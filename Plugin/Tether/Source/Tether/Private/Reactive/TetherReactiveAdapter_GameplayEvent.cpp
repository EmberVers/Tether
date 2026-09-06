#include "TetherReactiveAdapter.h"
#include "TetherReactiveSubsystem.h"
#include "Reactive/TetherReactiveShared.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherReactiveGE, Log, All);

namespace TetherReactiveAdapterImpl_GE
{
	FString RenderTagContainerLiteral(const FGameplayTagContainer& Tags)
	{
		if (Tags.Num() == 0)
		{
			return TEXT("[]");
		}
		FString Out = TEXT("[");
		bool bFirst = true;
		for (const FGameplayTag& Tag : Tags)
		{
			if (!bFirst) Out += TEXT(", ");
			bFirst = false;
			Out += FString::Printf(TEXT("'%s'"),
				*TetherReactiveUtil::EscapePythonStringLiteral(Tag.ToString()));
		}
		Out += TEXT("]");
		return Out;
	}

	/** True for PIE / PIE-server worlds. Editor-world ASCs (asset previews) excluded. */
	bool IsPieWorld(const UWorld* W)
	{
		if (!W) return false;
		const EWorldType::Type T = W->WorldType;
		return T == EWorldType::PIE || T == EWorldType::Game;
	}
}

/**
 * Binds to UAbilitySystemComponent::GenericGameplayEventCallbacks[Tag]. Two
 * registration shapes share one binding cache:
 *
 * - **Per-subject** handlers (Subject = ASC) bind a lambda when first added on
 *   that (ASC, Tag), refcounted by HandlerCount.
 * - **Global** handlers (Subject = nullptr, AdapterPayload = "global") fire on
 *   any live ASC for the matching Tag. The adapter walks live PIE-world ASCs
 *   at register time, ensures a (ASC, Tag) binding exists for each (refcounted
 *   by GlobalRefCount), and watches UWorld::OnActorSpawned to bind to
 *   late-spawning ASCs. PlayerState-attached ASCs are typically not present
 *   on the actor at OnActorSpawned, so the spawn callback also schedules a
 *   one-tick deferred re-resolve via FTSTicker.
 *
 * Lambda dispatches twice on fire: once with Subject = ASC (per-subject
 * handlers match), once with Subject = nullptr (global handlers match — the
 * subsystem's matcher already excludes per-subject records when the event
 * Subject is null because pointer equality fails).
 */
class FTetherGameplayEventAdapter : public ITetherReactiveAdapter
{
public:
	virtual ETetherTrigger GetTriggerType() const override { return ETetherTrigger::GameplayEvent; }
	virtual FString GetTriggerName() const override { return TEXT("GameplayEvent"); }

	virtual void OnHandlerAdded(const FTetherHandlerRecord& Record) override
	{
		if (Record.Selector.IsNone())
		{
			UE_LOG(LogTetherReactiveGE, Warning,
				TEXT("OnHandlerAdded %s: Selector (tag) required"), *Record.HandlerId);
			return;
		}
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Record.Selector, /*bErrorIfNotFound=*/false);
		if (!Tag.IsValid())
		{
			UE_LOG(LogTetherReactiveGE, Warning,
				TEXT("OnHandlerAdded %s: tag '%s' is not registered"),
				*Record.HandlerId, *Record.Selector.ToString());
			return;
		}

		const bool bGlobal = Record.AdapterPayload == TEXT("global");
		if (bGlobal)
		{
			GlobalTagCounts.FindOrAdd(Tag) += 1;
			SnapshotGlobalBindingsForTag(Tag);
			EnsureWorldWatchers();
			return;
		}

		// Per-subject path.
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC)
		{
			UE_LOG(LogTetherReactiveGE, Warning,
				TEXT("OnHandlerAdded %s: Subject is not an ASC"), *Record.HandlerId);
			return;
		}
		EnsureBinding(ASC, Tag, /*bForGlobal=*/false);
	}

	virtual void OnHandlerRemoved(const FTetherHandlerRecord& Record) override
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Record.Selector, false);
		if (!Tag.IsValid())
		{
			return;
		}

		const bool bGlobal = Record.AdapterPayload == TEXT("global");
		if (bGlobal)
		{
			int32* CountPtr = GlobalTagCounts.Find(Tag);
			if (!CountPtr) return;
			--(*CountPtr);
			if (*CountPtr <= 0)
			{
				GlobalTagCounts.Remove(Tag);
				// Drop the global refcount on every binding for this tag.
				for (int32 i = Bindings.Num() - 1; i >= 0; --i)
				{
					FBinding& B = Bindings[i];
					if (B.Tag != Tag) continue;
					if (B.GlobalRefCount > 0)
					{
						B.GlobalRefCount = 0;
					}
					if (B.HandlerCount + B.GlobalRefCount <= 0)
					{
						UnbindOne(B);
						Bindings.RemoveAtSwap(i);
					}
				}
			}
			TeardownWorldWatchersIfIdle();
			return;
		}

		// Per-subject path.
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC) return;
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.ASC.Get() == ASC && B.Tag == Tag)
			{
				B.HandlerCount -= 1;
				if (B.HandlerCount + B.GlobalRefCount <= 0)
				{
					UnbindOne(B);
					Bindings.RemoveAtSwap(i);
				}
				return;
			}
		}
	}

	virtual void Shutdown() override
	{
		for (FBinding& B : Bindings)
		{
			UnbindOne(B);
		}
		Bindings.Reset();
		GlobalTagCounts.Reset();
		// One-shot deferred-rebind tickers capture raw `this`; drop them before
		// the adapter is destroyed so a late tick can't touch freed memory.
		for (FTSTicker::FDelegateHandle& H : PendingTickers)
		{
			if (H.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(H);
			}
		}
		PendingTickers.Reset();
		for (auto& Pair : WorldSpawnHandles)
		{
			if (UWorld* W = Pair.Key.Get())
			{
				W->RemoveOnActorSpawnedHandler(Pair.Value);
			}
		}
		WorldSpawnHandles.Reset();
		if (PostPieStartedHandle.IsValid())
		{
			FEditorDelegates::PostPIEStarted.Remove(PostPieStartedHandle);
			PostPieStartedHandle.Reset();
		}
		if (PieEndHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(PieEndHandle);
			PieEndHandle.Reset();
		}
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),                 TEXT("str — always 'gameplay_event'"));
		D.Add(TEXT("tag"),                     TEXT("str — the GameplayTag that fired"));
		D.Add(TEXT("source_asc"),              TEXT("unreal.AbilitySystemComponent | None — the ASC that received the event (useful for global handlers to know which ASC fired)"));
		D.Add(TEXT("event_instigator"),        TEXT("unreal.Object | None — FGameplayEventData::Instigator"));
		D.Add(TEXT("event_target"),            TEXT("unreal.Object | None — FGameplayEventData::Target"));
		D.Add(TEXT("event_optional_object"),   TEXT("unreal.Object | None"));
		D.Add(TEXT("event_optional_object2"),  TEXT("unreal.Object | None"));
		D.Add(TEXT("event_magnitude"),         TEXT("float — FGameplayEventData::EventMagnitude"));
		D.Add(TEXT("event_instigator_tags"),   TEXT("list[str]"));
		D.Add(TEXT("event_target_tags"),       TEXT("list[str]"));
		return D;
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FGameplayTag Tag;
		FDelegateHandle Handle;
		int32 HandlerCount = 0;       // per-subject handler refcount
		int32 GlobalRefCount = 0;     // 1 if any global handler covers this (ASC, Tag), else 0
	};
	TArray<FBinding> Bindings;

	/** Tag → number of global handlers registered for that tag. */
	TMap<FGameplayTag, int32> GlobalTagCounts;

	/** Worlds we've subscribed to OnActorSpawned for. */
	TMap<TWeakObjectPtr<UWorld>, FDelegateHandle> WorldSpawnHandles;

	/** PostPIEStarted subscription so we (re)snapshot ASCs when PIE comes up after registration. */
	FDelegateHandle PostPieStartedHandle;
	/** EndPIE subscription to clean up bindings whose ASC just got torn down. */
	FDelegateHandle PieEndHandle;

	/** One-shot deferred-rebind tickers from OnPieActorSpawned; removed on Shutdown. */
	TArray<FTSTicker::FDelegateHandle> PendingTickers;

	/** Bind the ASC delegate if not already; bump the requested refcount. */
	void EnsureBinding(UAbilitySystemComponent* ASC, const FGameplayTag& Tag, bool bForGlobal)
	{
		if (!ASC) return;
		for (FBinding& B : Bindings)
		{
			if (B.ASC.Get() == ASC && B.Tag == Tag)
			{
				if (bForGlobal)
				{
					// Global contributes at most 1 refcount per (ASC, Tag) — multiple
					// global handlers on the same tag share the binding.
					if (B.GlobalRefCount == 0) B.GlobalRefCount = 1;
				}
				else
				{
					B.HandlerCount += 1;
				}
				return;
			}
		}

		// First handler for (ASC, Tag).
		FBinding NB;
		NB.ASC = ASC;
		NB.Tag = Tag;
		NB.HandlerCount = bForGlobal ? 0 : 1;
		NB.GlobalRefCount = bForGlobal ? 1 : 0;

		TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;
		const FGameplayTag CaptureTag = Tag;

		NB.Handle = ASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddLambda(
			[WeakASC, CaptureTag](const FGameplayEventData* EventData)
			{
				UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
				if (!Sub) return;

				UAbilitySystemComponent* SourceASC = WeakASC.Get();

				TMap<FString, FString> Ctx;
				Ctx.Add(TEXT("trigger"),    TEXT("'gameplay_event'"));
				Ctx.Add(TEXT("tag"),        FString::Printf(TEXT("'%s'"),
					*TetherReactiveUtil::EscapePythonStringLiteral(CaptureTag.ToString())));
				Ctx.Add(TEXT("source_asc"), TetherReactiveUtil::RenderPyObjectLiteral(SourceASC));

				if (EventData)
				{
					Ctx.Add(TEXT("event_instigator"),
						TetherReactiveUtil::RenderPyObjectLiteral(EventData->Instigator.Get()));
					Ctx.Add(TEXT("event_target"),
						TetherReactiveUtil::RenderPyObjectLiteral(EventData->Target.Get()));
					Ctx.Add(TEXT("event_optional_object"),
						TetherReactiveUtil::RenderPyObjectLiteral(EventData->OptionalObject.Get()));
					Ctx.Add(TEXT("event_optional_object2"),
						TetherReactiveUtil::RenderPyObjectLiteral(EventData->OptionalObject2.Get()));
					Ctx.Add(TEXT("event_magnitude"),
						FString::Printf(TEXT("%f"), EventData->EventMagnitude));
					Ctx.Add(TEXT("event_instigator_tags"),
						TetherReactiveAdapterImpl_GE::RenderTagContainerLiteral(EventData->InstigatorTags));
					Ctx.Add(TEXT("event_target_tags"),
						TetherReactiveAdapterImpl_GE::RenderTagContainerLiteral(EventData->TargetTags));
				}
				else
				{
					Ctx.Add(TEXT("event_instigator"),        TEXT("None"));
					Ctx.Add(TEXT("event_target"),            TEXT("None"));
					Ctx.Add(TEXT("event_optional_object"),   TEXT("None"));
					Ctx.Add(TEXT("event_optional_object2"),  TEXT("None"));
					Ctx.Add(TEXT("event_magnitude"),         TEXT("0.0"));
					Ctx.Add(TEXT("event_instigator_tags"),   TEXT("[]"));
					Ctx.Add(TEXT("event_target_tags"),       TEXT("[]"));
				}

				// Per-subject handlers match (Subject = ASC).
				Sub->Dispatch(ETetherTrigger::GameplayEvent,
					TWeakObjectPtr<UObject>(SourceASC), CaptureTag.GetTagName(), Ctx);

				// Global handlers match (Subject = nullptr). The subsystem's
				// pointer-equality test excludes per-subject records on this
				// pass — no double-firing.
				Sub->Dispatch(ETetherTrigger::GameplayEvent,
					TWeakObjectPtr<UObject>(), CaptureTag.GetTagName(), Ctx);
			});

		Bindings.Add(NB);

		UE_LOG(LogTetherReactiveGE, Verbose,
			TEXT("bound GameplayEvent lambda for ASC=%s Tag=%s (HandlerCount=%d GlobalRef=%d)"),
			*ASC->GetPathName(), *Tag.ToString(), NB.HandlerCount, NB.GlobalRefCount);
	}

	void UnbindOne(FBinding& B)
	{
		if (B.ASC.IsValid())
		{
			B.ASC->GenericGameplayEventCallbacks.FindOrAdd(B.Tag).Remove(B.Handle);
		}
	}

	/** Walk live PIE-world actors, ensure a global binding exists for (ASC, Tag) on each. */
	void SnapshotGlobalBindingsForTag(const FGameplayTag& Tag)
	{
		if (!GEditor) return;
		for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
		{
			UWorld* W = Ctx.World();
			if (!TetherReactiveAdapterImpl_GE::IsPieWorld(W)) continue;
			for (TActorIterator<AActor> It(W); It; ++It)
			{
				if (UAbilitySystemComponent* ASC = TetherReactiveUtil::ResolveActorASC(*It))
				{
					EnsureBinding(ASC, Tag, /*bForGlobal=*/true);
				}
			}
		}
	}

	/** Subscribe to spawn events for every PIE world; subscribe to PostPIEStarted / EndPIE. */
	void EnsureWorldWatchers()
	{
		if (!GEditor) return;

		// Per-world spawn hooks.
		for (const FWorldContext& WC : GEditor->GetWorldContexts())
		{
			UWorld* W = WC.World();
			if (!TetherReactiveAdapterImpl_GE::IsPieWorld(W)) continue;
			TWeakObjectPtr<UWorld> Key(W);
			if (WorldSpawnHandles.Contains(Key)) continue;

			FDelegateHandle H = W->AddOnActorSpawnedHandler(
				FOnActorSpawned::FDelegate::CreateLambda(
					[this](AActor* SpawnedActor) { OnPieActorSpawned(SpawnedActor); }));
			WorldSpawnHandles.Add(Key, H);
		}

		if (!PostPieStartedHandle.IsValid())
		{
			PostPieStartedHandle = FEditorDelegates::PostPIEStarted.AddLambda(
				[this](const bool /*bIsSimulating*/)
				{
					// New PIE world spun up — snapshot ASCs for every tracked global tag,
					// and add a spawn watcher for the new world.
					EnsureWorldWatchers();
					for (const auto& Pair : GlobalTagCounts)
					{
						SnapshotGlobalBindingsForTag(Pair.Key);
					}
				});
		}
		if (!PieEndHandle.IsValid())
		{
			PieEndHandle = FEditorDelegates::EndPIE.AddLambda(
				[this](const bool /*bIsSimulating*/)
				{
					// PIE world is going away. Drop bindings whose ASC is invalid OR
					// whose ASC's owning world is now invalid. The lambda capture's
					// WeakObjectPtr would also stop firing, but stale entries in
					// Bindings would leak across PIE sessions if we didn't sweep.
					for (int32 i = Bindings.Num() - 1; i >= 0; --i)
					{
						UAbilitySystemComponent* ASC = Bindings[i].ASC.Get();
						if (!ASC || !TetherReactiveAdapterImpl_GE::IsPieWorld(ASC->GetWorld()))
						{
							// Best-effort unbind; if ASC is already gone the FindOrAdd
							// fallback in UnbindOne is a no-op on the stale handle.
							UnbindOne(Bindings[i]);
							Bindings.RemoveAtSwap(i);
						}
					}
					// Drop spawn handles for dead worlds.
					for (auto It = WorldSpawnHandles.CreateIterator(); It; ++It)
					{
						if (!It.Key().IsValid())
						{
							It.RemoveCurrent();
						}
					}
				});
		}
	}

	void TeardownWorldWatchersIfIdle()
	{
		if (GlobalTagCounts.Num() > 0) return;
		// No global handlers left — drop spawn watchers and PIE delegates.
		for (auto& Pair : WorldSpawnHandles)
		{
			if (UWorld* W = Pair.Key.Get())
			{
				W->RemoveOnActorSpawnedHandler(Pair.Value);
			}
		}
		WorldSpawnHandles.Reset();
		if (PostPieStartedHandle.IsValid())
		{
			FEditorDelegates::PostPIEStarted.Remove(PostPieStartedHandle);
			PostPieStartedHandle.Reset();
		}
		if (PieEndHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(PieEndHandle);
			PieEndHandle.Reset();
		}
	}

	void OnPieActorSpawned(AActor* SpawnedActor)
	{
		if (!SpawnedActor || GlobalTagCounts.Num() == 0) return;
		if (UAbilitySystemComponent* ASC = TetherReactiveUtil::ResolveActorASC(SpawnedActor))
		{
			BindAllGlobalTagsToASC(ASC);
			return;
		}
		// PlayerState ASCs aren't on the actor at OnActorSpawned. Re-check next tick.
		// The ticker is one-shot (returns false) but is registered in
		// PendingTickers anyway: if the subsystem deinits before it fires, the
		// lambda would otherwise tick with a dangling `this`.
		TWeakObjectPtr<AActor> WeakActor(SpawnedActor);
		FTSTicker::FDelegateHandle Handle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([this, WeakActor](float) -> bool
			{
				if (AActor* A = WeakActor.Get())
				{
					if (UAbilitySystemComponent* ASC = TetherReactiveUtil::ResolveActorASC(A))
					{
						BindAllGlobalTagsToASC(ASC);
					}
				}
				return false; // one-shot
			}),
			0.0f);
		// Prune fired one-shot tickers before appending: FTSTicker::FDelegateHandle
		// is a TWeakPtr to the ticker element, so it goes invalid once the
		// one-shot fired (or was removed) — without this sweep PendingTickers
		// only ever grew over the editor session's lifetime.
		PendingTickers.RemoveAll(
			[](const FTSTicker::FDelegateHandle& H) { return !H.IsValid(); });
		PendingTickers.Add(Handle);
	}

	void BindAllGlobalTagsToASC(UAbilitySystemComponent* ASC)
	{
		if (!ASC) return;
		for (const auto& Pair : GlobalTagCounts)
		{
			EnsureBinding(ASC, Pair.Key, /*bForGlobal=*/true);
		}
	}
};

namespace TetherReactiveAdapters
{
	TUniquePtr<ITetherReactiveAdapter> MakeGameplayEventAdapter()
	{
		return MakeUnique<FTetherGameplayEventAdapter>();
	}
}
