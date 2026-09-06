#include "TetherReactiveLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "TetherReactiveSubsystem.h"
#include "TetherTestAttributeSet.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace TetherReactiveTests
{
	/**
	 * 在真实编辑器世界里生成带 ASC 与 UTetherTestAttributeSet 的瞬态命名 Actor。
	 * RegisterRuntimeAttributeChanged 走生产 FindActorByName 路径：编辑器世界是该查找的兜底分支，
	 * 不依赖任何 PIE 会话，因此自动化测试无需真正启动 PIE。
	 * Spawns a transient named actor with an ASC and UTetherTestAttributeSet in the real editor
	 * world. RegisterRuntimeAttributeChanged goes through the production FindActorByName path,
	 * whose editor-world fallback branch finds the actor without needing a live PIE session.
	 */
	class FScopedEditorWorldActor
	{
	public:
		explicit FScopedEditorWorldActor(const TCHAR* BaseName)
		{
			UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
			if (!EditorWorld)
			{
				return;
			}

			// Unique per-run name so repeated runs never collide on the
			// GetActorNameOrLabel comparison.
			ActorName = FString::Printf(TEXT("%s_%s"),
				BaseName, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
			FActorSpawnParameters Params;
			Params.Name = FName(*ActorName);
			Params.ObjectFlags = RF_Transient;
			Params.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Actor = EditorWorld->SpawnActor<AActor>(AActor::StaticClass(), Params);
			if (!Actor)
			{
				return;
			}

			ASC = NewObject<UAbilitySystemComponent>(Actor, TEXT("TetherReactiveTestASC"));
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Actor, Actor);
			UTetherTestAttributeSet* Set = NewObject<UTetherTestAttributeSet>(Actor);
			ASC->AddSpawnedAttribute(Set);
		}

		~FScopedEditorWorldActor()
		{
			if (ASC)
			{
				// 先解除 ASC 注册，避免销毁期间组件委托再次进入适配器。
				// Unregister the ASC first so component delegates cannot re-enter the adapter during teardown.
				ASC->UnregisterComponent();
			}
			if (Actor)
			{
				Actor->Destroy();
			}
		}

		bool IsValid() const { return Actor != nullptr && ASC != nullptr; }
		const FString& GetActorName() const { return ActorName; }
		UAbilitySystemComponent* GetASC() const { return ASC; }

	private:
		FString ActorName;
		AActor* Actor = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
	};

	/**
	 * 持久化文件守卫：进入时把现有文件移到备份名，退出时删除测试产物并恢复原文件。
	 * 持久化往返与损坏文件测试必须直接操作真实路径，守卫保证测试不破坏编辑器会话中的既有 handler 状态。
	 * Persistence-file guard: moves an existing file aside on entry, deletes
	 * test artifacts and restores the original on exit so round-trip and
	 * corrupt-file tests never destroy the editor session's persisted state.
	 */
	class FScopedPersistenceFile
	{
	public:
		FScopedPersistenceFile()
			: Path(UTetherReactiveSubsystem::GetPersistencePath())
		{
			bExisted = FPaths::FileExists(Path);
			if (bExisted)
			{
				BackupPath = Path + TEXT(".reactive-tests-backup");
				IFileManager::Get().Move(*BackupPath, *Path);
			}
		}

		~FScopedPersistenceFile()
		{
			IFileManager::Get().Delete(*Path, false, true);
			// Clean up any .corrupt-* artifacts the tests renamed aside.
			TArray<FString> CorruptFiles;
			IFileManager::Get().FindFiles(
				CorruptFiles,
				*(FPaths::Combine(FPaths::GetPath(Path), TEXT("reactive-handlers.json.corrupt-*"))),
				true, false);
			for (const FString& File : CorruptFiles)
			{
				IFileManager::Get().Delete(
					*FPaths::Combine(FPaths::GetPath(Path), File), false, true);
			}
			if (bExisted)
			{
				IFileManager::Get().Move(*Path, *BackupPath);
			}
		}

		const FString& GetPath() const { return Path; }

	private:
		FString Path;
		FString BackupPath;
		bool bExisted = false;
	};

	/**
	 * 只移除本测试注册的 handler，避免 ClearHandlers("all") 把编辑器会话
	 * 里的既有 handler 一并清掉并持久化到磁盘。
	 * Removes only the handlers a test registered; ClearHandlers("all") would
	 * also wipe the live editor session's own handlers and persist that.
	 */
	void UnregisterIfKnown(const FString& Id)
	{
		if (!Id.IsEmpty())
		{
			UTetherReactiveLibrary::Unregister(Id);
		}
	}
}

// ─── ① 注册原子性：A-P1-1 回归 ────────────────────────────────────
// Registration atomicity: an unknown attribute must be refused outright —
// empty HandlerId, nothing stored, nothing persisted.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherReactiveAttributeRegistrationAtomicityTest,
	"Tether.Reactive.Registration.AttributeRefusedWhenMissing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherReactiveAttributeRegistrationAtomicityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!TestNotNull(TEXT("reactive subsystem is available in the editor"), Sub))
	{
		return false;
	}

	TetherReactiveTests::FScopedEditorWorldActor Fixture(TEXT("ReactiveAtomicity"));
	if (!TestTrue(TEXT("editor-world actor with ASC was spawned"), Fixture.IsValid()))
	{
		return false;
	}

	const TArray<FString> NoTags;
	const FString NoScriptPath;

	// Register against a non-existent attribute: the library-level pre-check
	// must refuse the whole registration — empty id AND nothing stored.
	const FString BadId = UTetherReactiveLibrary::RegisterRuntimeAttributeChanged(
		TEXT("ReactiveBadAttr"),
		TEXT("registration must be refused for an unknown attribute"),
		Fixture.GetActorName(),
		TEXT("NoSuchAttribute"),
		TEXT("pass"),
		NoScriptPath, NoTags, TEXT("Permanent"), TEXT("LogContinue"), 0);
	TestTrue(TEXT("unknown attribute is refused with an empty HandlerId"), BadId.IsEmpty());

	TArray<FTetherHandlerSummary> All = UTetherReactiveLibrary::ListAllHandlers(
		TEXT("runtime"), TEXT("AttributeChanged"), TEXT(""));
	TestEqual(TEXT("refused registration stores no AttributeChanged handler"), All.Num(), 0);

	// Control: the real Health attribute registers fine.
	FString GoodId = UTetherReactiveLibrary::RegisterRuntimeAttributeChanged(
		TEXT("ReactiveGoodAttr"),
		TEXT("registration succeeds for a known attribute"),
		Fixture.GetActorName(),
		TEXT("Health"),
		TEXT("pass"),
		NoScriptPath, NoTags, TEXT("Permanent"), TEXT("LogContinue"), 0);
	ON_SCOPE_EXIT { TetherReactiveTests::UnregisterIfKnown(GoodId); };
	TestFalse(TEXT("known attribute registers with a non-empty HandlerId"), GoodId.IsEmpty());

	FTetherHandlerDetail Detail;
	TestTrue(TEXT("registered handler is retrievable"), Sub->GetHandler(GoodId, Detail));
	TestEqual(TEXT("registered handler keeps its task name"),
		Detail.Summary.TaskName, TEXT("ReactiveGoodAttr"));

	return true;
}

// ─── ② Add/Remove 对称性：A-P2-2 / A-P3-2 回归（Subject 失效路径）──────
// A handler whose Subject died between Add and Remove must unregister
// without decrementing a binding it never owned (dead-subject = "binding
// not found"), and a stale per-subject record must not be matched by a
// global event (A-P3-1).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherReactiveDeadSubjectUnregisterTest,
	"Tether.Reactive.Lifecycle.UnregisterWithDeadSubject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherReactiveDeadSubjectUnregisterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!TestNotNull(TEXT("reactive subsystem is available in the editor"), Sub))
	{
		return false;
	}

	// The subject lives in its own transient package so the test controls
	// exactly when it becomes garbage (nothing else references it).
	const FString PackagePath = FString::Printf(
		TEXT("/Temp/TetherReactiveDeadSubject_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	UPackage* TransientPackage = CreatePackage(*PackagePath);
	if (!TestNotNull(TEXT("transient package was created"), TransientPackage))
	{
		return false;
	}
	TransientPackage->SetFlags(RF_Transient);

	// Keep the subject alive through a strong pointer until the GC step.
	TStrongObjectPtr<AActor> Subject(NewObject<AActor>(
		TransientPackage, TEXT("DeadSubjectActor"), RF_Transient));
	if (!TestTrue(TEXT("transient subject actor was created"), Subject.IsValid()))
	{
		TransientPackage->MarkAsGarbage();
		return false;
	}

	// The handler id is declared before the scope exit so an early bail still
	// unregisters exactly this handler (never ClearHandlers("all")).
	FString Id;
	ON_SCOPE_EXIT
	{
		TetherReactiveTests::UnregisterIfKnown(Id);
		TransientPackage->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
	};

	// Per-subject ActorLifecycle handler with the registration intent
	// recorded the way the library entry point does.
	FTetherHandlerRecord Record;
	Record.Scope = TEXT("runtime");
	Record.TaskName = TEXT("DeadSubjectHandler");
	Record.Description = TEXT("per-subject handler whose subject dies before unregister");
	Record.Script = TEXT("pass");
	Record.TriggerType = ETetherTrigger::ActorLifecycle;
	Record.Subject = TWeakObjectPtr<UObject>(Subject.Get());
	Record.Selector = FName(TEXT("Destroyed"));
	Id = Sub->RegisterHandler(MoveTemp(Record));
	TestFalse(TEXT("per-subject handler registered with a non-empty HandlerId"), Id.IsEmpty());

	// A-P3-1 regression first: while the subject is dead but the record is
	// still live, a GLOBAL ActorLifecycle event must not fire the stale
	// per-subject handler. Dispatch with an explicitly-null subject; the
	// script "pass" cannot fail, but the stale record must not run — assert
	// via stats that its call count stays zero.
	Subject.Reset();
	TransientPackage->MarkAsGarbage();
	CollectGarbage(RF_NoFlags, true);

	FTetherHandlerStats Stats;
	Sub->GetStats(Id, Stats);
	TestEqual<int64>(TEXT("stale per-subject handler has not fired"), Stats.Calls, 0);
	Sub->Dispatch(ETetherTrigger::ActorLifecycle, TWeakObjectPtr<UObject>(),
		FName(TEXT("Destroyed")), TMap<FString, FString>());
	Sub->GetStats(Id, Stats);
	TestEqual<int64>(TEXT("global event does not fire a stale per-subject handler"), Stats.Calls, 0);

	// A-P3-2 regression: unregister with a dead subject must remove the
	// record cleanly (binding not found, no wrong-binding decrement).
	FTetherHandlerDetail Before;
	TestTrue(TEXT("handler still listed after subject death"), Sub->GetHandler(Id, Before));
	const bool bRemoved = Sub->UnregisterHandler(Id);
	TestTrue(TEXT("unregister with dead subject removes the handler"), bRemoved);
	FTetherHandlerDetail After;
	TestFalse(TEXT("removed handler is no longer retrievable"), Sub->GetHandler(Id, After));

	return true;
}

// ─── ③ 持久化往返 + 损坏文件 rename：A-P2-4 回归 ───────────────────
// Persistence round trip: paused state survives save/load. Corrupt file:
// renamed aside with a timestamp, and the normal path becomes writable again.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherReactivePersistenceRoundTripTest,
	"Tether.Reactive.Persistence.RoundTripPreservesFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherReactivePersistenceRoundTripTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!TestNotNull(TEXT("reactive subsystem is available in the editor"), Sub))
	{
		return false;
	}

	TetherReactiveTests::FScopedPersistenceFile FileGuard;
	if (!TestFalse(TEXT("persistence path is non-empty"), FileGuard.GetPath().IsEmpty()))
	{
		return false;
	}

	// Register a global BpCompiled handler (no subject, always resolvable)
	// so the save/load round trip restores it without PIE.
	FString Id = UTetherReactiveLibrary::RegisterEditorBpCompiled(
		TEXT("RoundTripHandler"),
		TEXT("persistence round trip"),
		FString(),  // "" → global
		TEXT("pass"),
		FString(), TArray<FString>(), TEXT("Permanent"), TEXT("LogContinue"), 0);
	ON_SCOPE_EXIT { TetherReactiveTests::UnregisterIfKnown(Id); };
	TestFalse(TEXT("handler registered with a non-empty HandlerId"), Id.IsEmpty());

	// Pause it, then save: the paused flag must survive the round trip (A-P3-4).
	TestTrue(TEXT("handler paused"), UTetherReactiveLibrary::Pause(Id));
	TestTrue(TEXT("explicit save succeeds"), Sub->SaveAllHandlers());
	TestTrue(TEXT("persistence file exists after save"), FPaths::FileExists(FileGuard.GetPath()));

	// Load replaces the in-memory registry from disk.
	const int32 Restored = Sub->LoadAllHandlers();
	TestEqual(TEXT("load restores the persisted handler"), Restored, 1);

	FTetherHandlerDetail Detail;
	TestTrue(TEXT("restored handler is retrievable"), Sub->GetHandler(Id, Detail));
	TestTrue(TEXT("restored handler keeps its paused state"), Detail.Summary.bPaused);
	TestEqual(TEXT("restored handler keeps its task name"),
		Detail.Summary.TaskName, TEXT("RoundTripHandler"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetherReactivePersistenceCorruptFileTest,
	"Tether.Reactive.Persistence.CorruptFileRenamedAside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTetherReactivePersistenceCorruptFileTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!TestNotNull(TEXT("reactive subsystem is available in the editor"), Sub))
	{
		return false;
	}

	TetherReactiveTests::FScopedPersistenceFile FileGuard;
	if (!TestFalse(TEXT("persistence path is non-empty"), FileGuard.GetPath().IsEmpty()))
	{
		return false;
	}

	// No handler is registered in this test; the corrupt file means the
	// registry stays empty after load. The persistence-file guard restores
	// the on-disk state on exit.
	// Write a deliberately truncated JSON payload.
	TestTrue(TEXT("corrupt payload was written"),
		FFileHelper::SaveStringToFile(TEXT("{\"version\":1,\"handlers\":[{"),
			*FileGuard.GetPath(), FFileHelper::EEncodingOptions::ForceUTF8));

	// Load must reject it, rename it aside, and start fresh.
	const int32 Restored = Sub->LoadAllHandlers();
	TestEqual(TEXT("corrupt file restores zero handlers"), Restored, 0);

	// The original path must be free again and a .corrupt-<timestamp> file
	// must exist next to it.
	TestFalse(TEXT("original path no longer holds the corrupt file"), FPaths::FileExists(FileGuard.GetPath()));

	TArray<FString> CorruptFiles;
	IFileManager::Get().FindFiles(
		CorruptFiles,
		*(FPaths::Combine(FPaths::GetPath(FileGuard.GetPath()), TEXT("reactive-handlers.json.corrupt-*"))),
		true, false);
	TestTrue(TEXT("corrupt file was renamed aside with a timestamp suffix"), CorruptFiles.Num() > 0);

	// The normal path must be writable again.
	TestTrue(TEXT("save after corruption succeeds"), Sub->SaveAllHandlers());
	TestTrue(TEXT("fresh persistence file exists after re-save"), FPaths::FileExists(FileGuard.GetPath()));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
