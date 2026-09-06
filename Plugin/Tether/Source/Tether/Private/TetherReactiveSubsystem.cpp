#include "TetherReactiveSubsystem.h"
#include "TetherReactiveAdapter.h"
#include "TetherReactiveLibrary.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Base64.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherReactive, Log, All);

// Forward decls for adapter factories, defined in adapter translation units.
namespace TetherReactiveAdapters
{
	TUniquePtr<ITetherReactiveAdapter> MakeGameplayEventAdapter();
	TUniquePtr<ITetherReactiveAdapter> MakeAttributeChangedAdapter();
	TUniquePtr<ITetherReactiveAdapter> MakeActorLifecycleAdapter();
	TUniquePtr<ITetherReactiveAdapter> MakeMovementModeAdapter();
	TUniquePtr<ITetherReactiveAdapter> MakeAnimNotifyAdapter();
	TUniquePtr<ITetherReactiveAdapter> MakeInputActionAdapter();
	TUniquePtr<ITetherReactiveAdapter> MakeTimerAdapter();
	TUniquePtr<ITetherReactiveAdapter> MakeAssetEventAdapter();
	TUniquePtr<ITetherReactiveAdapter> MakePieStateAdapter();
	TUniquePtr<ITetherReactiveAdapter> MakeBpCompiledAdapter();
}

namespace TetherReactiveImpl
{
	/** Hard cap on synchronous handler re-entrancy (handler A fires event → handler B → …). */
	constexpr int32 MaxDispatchDepth = 16;
	/** Emit a warning at this depth before hitting the hard cap. */
	constexpr int32 WarnDispatchDepth = 8;

	FString TriggerTypeName(ETetherTrigger T)
	{
		switch (T)
		{
		case ETetherTrigger::GameplayEvent:        return TEXT("GameplayEvent");
		case ETetherTrigger::AnimNotify:           return TEXT("AnimNotify");
		case ETetherTrigger::AttributeChanged:     return TEXT("AttributeChanged");
		case ETetherTrigger::MovementModeChanged:  return TEXT("MovementModeChanged");
		case ETetherTrigger::InputAction:          return TEXT("InputAction");
		case ETetherTrigger::ActorLifecycle:       return TEXT("ActorLifecycle");
		case ETetherTrigger::Timer:                return TEXT("Timer");
		case ETetherTrigger::AssetEvent:           return TEXT("AssetEvent");
		case ETetherTrigger::PieEvent:             return TEXT("PieEvent");
		case ETetherTrigger::BpCompiled:           return TEXT("BpCompiled");
		default:                                   return TEXT("None");
		}
	}

	FString LifetimeName(ETetherHandlerLifetime L)
	{
		switch (L)
		{
		case ETetherHandlerLifetime::Permanent:         return TEXT("Permanent");
		case ETetherHandlerLifetime::Once:              return TEXT("Once");
		case ETetherHandlerLifetime::Count:             return TEXT("Count");
		case ETetherHandlerLifetime::WhilePIE:          return TEXT("WhilePIE");
		case ETetherHandlerLifetime::WhileSubjectAlive: return TEXT("WhileSubjectAlive");
		default:                                        return TEXT("");
		}
	}

	FString ErrorPolicyName(ETetherErrorPolicy E)
	{
		switch (E)
		{
		case ETetherErrorPolicy::LogContinue:   return TEXT("LogContinue");
		case ETetherErrorPolicy::LogUnregister: return TEXT("LogUnregister");
		case ETetherErrorPolicy::Throw:         return TEXT("Throw");
		default:                                return TEXT("");
		}
	}

	FString EscapePythonStringLiteral(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 2);
		for (TCHAR C : In)
		{
			if (C == TEXT('\\') || C == TEXT('\''))
			{
				Out.AppendChar(TEXT('\\'));
				Out.AppendChar(C);
			}
			else if (C == TEXT('\n'))
			{
				Out.Append(TEXT("\\n"));
			}
			else if (C == TEXT('\r'))
			{
				Out.Append(TEXT("\\r"));
			}
			else
			{
				Out.AppendChar(C);
			}
		}
		return Out;
	}

	FString BuildSummaryTrigger(ETetherTrigger T, const FName& Selector)
	{
		if (Selector.IsNone())
		{
			return TriggerTypeName(T);
		}
		return FString::Printf(TEXT("%s:%s"), *TriggerTypeName(T), *Selector.ToString());
	}

	FString BuildSubjectPath(const TWeakObjectPtr<UObject>& Subject)
	{
		if (UObject* Obj = Subject.Get())
		{
			return Obj->GetPathName();
		}
		if (Subject.IsExplicitlyNull())
		{
			return FString();
		}
		return TEXT("<invalid>");
	}

	void ApplyLifetimeDecrement(FTetherHandlerRecord& R, bool& bShouldRemove)
	{
		bShouldRemove = false;
		switch (R.Lifetime)
		{
		case ETetherHandlerLifetime::Once:
			bShouldRemove = true;
			break;
		case ETetherHandlerLifetime::Count:
			if (R.RemainingCalls > 0)
			{
				--R.RemainingCalls;
				if (R.RemainingCalls <= 0)
				{
					bShouldRemove = true;
				}
			}
			else
			{
				bShouldRemove = true;
			}
			break;
		default:
			break;
		}
	}
} // namespace TetherReactiveImpl

// ─── Subsystem lifecycle ────────────────────────────────────────

UTetherReactiveSubsystem* UTetherReactiveSubsystem::Get()
{
	if (!GEditor)
	{
		return nullptr;
	}
	return GEditor->GetEditorSubsystem<UTetherReactiveSubsystem>();
}

void UTetherReactiveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// World cleanup: purge handlers whose subject belonged to the
	// cleaned world. Fires on PIE end, map change, and editor shutdown.
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UTetherReactiveSubsystem::HandleWorldCleanup);

	// PIE end: remove WhilePIE-scoped handlers.
	PieEndedHandle = FEditorDelegates::EndPIE.AddUObject(
		this, &UTetherReactiveSubsystem::HandlePieEnded);

	// PIE start: retry any DeferredHandlers whose Subject needs a live PIE world.
	PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddUObject(
		this, &UTetherReactiveSubsystem::HandlePostPIEStarted);

	// Persistence debounce ticker: saves ~100 ms after the last mutation.
	PersistenceDebounceHandle = FTSTicker::GetCoreTicker().AddTicker(
		TEXT("TetherReactive.PersistenceDebounce"),
		0.1f,
		[this](float /*Dt*/) -> bool
		{
			if (bPersistenceDirty)
			{
				bPersistenceDirty = false;
				SaveAllHandlers();
			}
			return true;
		});

	// Deferred-exec ticker drains any snippets queued via DeferToNextTick.
	DeferTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		TEXT("TetherReactive.DeferTicker"),
		0.0f,
		[this](float /*Dt*/) -> bool
		{
			TArray<FString> Drain;
			Swap(Drain, DeferredScripts);
			for (const FString& S : Drain)
			{
				FString Err;
				if (!ExecutePythonScript(S, Err))
				{
					UE_LOG(LogTetherReactive, Warning,
						TEXT("deferred script failed: %s"), *Err);
				}
			}
			return true; // keep ticking
		});

	// Register built-in adapters (trigger types currently implemented).
	RegisterAdapter(TetherReactiveAdapters::MakeGameplayEventAdapter());
	RegisterAdapter(TetherReactiveAdapters::MakeAttributeChangedAdapter());
	RegisterAdapter(TetherReactiveAdapters::MakeActorLifecycleAdapter());
	RegisterAdapter(TetherReactiveAdapters::MakeMovementModeAdapter());
	RegisterAdapter(TetherReactiveAdapters::MakeAnimNotifyAdapter());
	RegisterAdapter(TetherReactiveAdapters::MakeInputActionAdapter());
	RegisterAdapter(TetherReactiveAdapters::MakeTimerAdapter());
	// Editor-scope adapters (P5).
	RegisterAdapter(TetherReactiveAdapters::MakeAssetEventAdapter());
	RegisterAdapter(TetherReactiveAdapters::MakePieStateAdapter());
	RegisterAdapter(TetherReactiveAdapters::MakeBpCompiledAdapter());

	UE_LOG(LogTetherReactive, Log,
		TEXT("UTetherReactiveSubsystem initialized (%d adapter(s))."), Adapters.Num());

	// Load persisted handlers AFTER adapters register so re-register's
	// OnHandlerAdded call lands on a live adapter. PIE-tied subjects that
	// don't resolve yet are parked in DeferredHandlers.
	LoadAllHandlers();
}

void UTetherReactiveSubsystem::Deinitialize()
{
	// Flush any pending save so we don't lose the last mutation on quit.
	if (bPersistenceDirty)
	{
		bPersistenceDirty = false;
		SaveAllHandlers();
	}

	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	FEditorDelegates::EndPIE.Remove(PieEndedHandle);
	FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(DeferTickerHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(PersistenceDebounceHandle);

	// Unregister all persistent tickers (sticky-input, Timer adapter's multiplexer, …).
	for (FTSTicker::FDelegateHandle& H : PersistentTickers)
	{
		if (H.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(H);
		}
	}
	PersistentTickers.Reset();

	// Tear down adapters first (they unbind UE delegates they still hold),
	// then drop handler records.
	for (TUniquePtr<ITetherReactiveAdapter>& A : Adapters)
	{
		if (A.IsValid())
		{
			A->Shutdown();
		}
	}
	Adapters.Reset();
	Handlers.Reset();
	DeferredScripts.Reset();

	Super::Deinitialize();
}

void UTetherReactiveSubsystem::RegisterAdapter(TUniquePtr<ITetherReactiveAdapter> Adapter)
{
	if (Adapter.IsValid())
	{
		Adapters.Add(MoveTemp(Adapter));
	}
}

ITetherReactiveAdapter* UTetherReactiveSubsystem::FindAdapter(ETetherTrigger TriggerType) const
{
	for (const TUniquePtr<ITetherReactiveAdapter>& A : Adapters)
	{
		if (A.IsValid() && A->GetTriggerType() == TriggerType)
		{
			return A.Get();
		}
	}
	return nullptr;
}

TMap<FString, FString> UTetherReactiveSubsystem::DescribeTriggerContext(const FString& TriggerTypeName) const
{
	for (const TUniquePtr<ITetherReactiveAdapter>& A : Adapters)
	{
		if (A.IsValid() &&
			TetherReactiveImpl::TriggerTypeName(A->GetTriggerType()) == TriggerTypeName)
		{
			return A->DescribeContext();
		}
	}
	return TMap<FString, FString>();
}

// ─── Handler id issuance ────────────────────────────────────────

FString UTetherReactiveSubsystem::IssueHandlerId(const FString& Scope)
{
	int32& Seq = (Scope == TEXT("editor")) ? EditorSeq : RuntimeSeq;
	++Seq;
	// Short random tag reduces confusion if the agent retains a handler_id string
	// across an editor restart — the seq resets but rand4 won't collide.
	const uint32 Rand = FGuid::NewGuid().A ^ FGuid::NewGuid().B;
	const FString Prefix = (Scope == TEXT("editor")) ? TEXT("ed") : TEXT("rt");
	return FString::Printf(TEXT("%s_%04d_%04x"), *Prefix, Seq, Rand & 0xffff);
}

// ─── Registration ───────────────────────────────────────────────

FString UTetherReactiveSubsystem::RegisterHandler(FTetherHandlerRecord&& Record)
{
	if (Record.TaskName.IsEmpty())
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RegisterHandler refused: task_name is required."));
		return FString();
	}
	if (Record.Description.IsEmpty())
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RegisterHandler refused: description is required."));
		return FString();
	}
	if (Record.Script.IsEmpty())
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RegisterHandler refused: script is empty."));
		return FString();
	}
	if (Record.TriggerType == ETetherTrigger::None)
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RegisterHandler refused: trigger type is None."));
		return FString();
	}

	ITetherReactiveAdapter* Adapter = FindAdapter(Record.TriggerType);
	if (!Adapter)
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RegisterHandler refused: no adapter for trigger '%s' (not yet implemented?)."),
			*TetherReactiveImpl::TriggerTypeName(Record.TriggerType));
		return FString();
	}

	if (Record.Scope.IsEmpty())
	{
		Record.Scope = TEXT("runtime");
	}
	Record.HandlerId = IssueHandlerId(Record.Scope);
	Record.CreatedAt = FDateTime::UtcNow();

	TSharedRef<FTetherHandlerRecord> Shared = MakeShared<FTetherHandlerRecord>(MoveTemp(Record));
	const FString Id = Shared->HandlerId;
	Handlers.Add(Id, Shared);

	Adapter->OnHandlerAdded(*Shared);

	UE_LOG(LogTetherReactive, Log,
		TEXT("registered handler %s '%s' (%s)"),
		*Id, *Shared->TaskName,
		*TetherReactiveImpl::BuildSummaryTrigger(Shared->TriggerType, Shared->Selector));
	MarkDirty();
	return Id;
}

bool UTetherReactiveSubsystem::UnregisterHandler(const FString& HandlerId)
{
	TSharedRef<FTetherHandlerRecord>* Found = Handlers.Find(HandlerId);
	if (!Found)
	{
		return false;
	}
	TSharedRef<FTetherHandlerRecord> Ref = *Found;
	if (ITetherReactiveAdapter* Adapter = FindAdapter(Ref->TriggerType))
	{
		Adapter->OnHandlerRemoved(*Ref);
	}
	Handlers.Remove(HandlerId);
	UE_LOG(LogTetherReactive, Log, TEXT("unregistered handler %s"), *HandlerId);
	MarkDirty();
	return true;
}

void UTetherReactiveSubsystem::RemoveByIdInternal(const FString& HandlerId)
{
	UnregisterHandler(HandlerId);
}

bool UTetherReactiveSubsystem::PauseHandler(const FString& HandlerId)
{
	if (TSharedRef<FTetherHandlerRecord>* R = Handlers.Find(HandlerId))
	{
		(*R)->bPaused = true;
		return true;
	}
	return false;
}

bool UTetherReactiveSubsystem::ResumeHandler(const FString& HandlerId)
{
	if (TSharedRef<FTetherHandlerRecord>* R = Handlers.Find(HandlerId))
	{
		(*R)->bPaused = false;
		// A user Resume() also clears the Throw-policy auto-pause marker —
		// explicit intent to run the handler again.
		(*R)->Stats.bPausedByErrorPolicy = false;
		return true;
	}
	return false;
}

int32 UTetherReactiveSubsystem::ClearHandlers(const FString& Scope)
{
	const bool bAll = (Scope == TEXT("all") || Scope.IsEmpty());
	TArray<FString> ToRemove;
	for (const auto& Pair : Handlers)
	{
		if (bAll || Pair.Value->Scope == Scope)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FString& Id : ToRemove)
	{
		UnregisterHandler(Id);
	}
	return ToRemove.Num();
}

// ─── Introspection ──────────────────────────────────────────────

TArray<FTetherHandlerSummary> UTetherReactiveSubsystem::ListAllHandlers(
	const FString& FilterScope,
	const FString& FilterTriggerTypeName,
	const FString& FilterTag) const
{
	TArray<FTetherHandlerSummary> Results;
	Results.Reserve(Handlers.Num());

	for (const auto& Pair : Handlers)
	{
		const FTetherHandlerRecord& R = *Pair.Value;

		if (!FilterScope.IsEmpty() && R.Scope != FilterScope)
		{
			continue;
		}
		if (!FilterTriggerTypeName.IsEmpty() &&
			TetherReactiveImpl::TriggerTypeName(R.TriggerType) != FilterTriggerTypeName)
		{
			continue;
		}
		// Tag filter: literal text matches by exact equality (covers existing
		// callers); patterns containing '*' or '?' use FString::MatchesWildcard.
		// Detection is char-scan over the filter, not the per-handler tag list,
		// so it's O(filter_len) once per call.
		if (!FilterTag.IsEmpty())
		{
			const bool bWildcard = FilterTag.Contains(TEXT("*")) || FilterTag.Contains(TEXT("?"));
			const bool bMatch = bWildcard
				? R.Tags.ContainsByPredicate([&FilterTag](const FString& T)
				  {
					  return T.MatchesWildcard(FilterTag);
				  })
				: R.Tags.Contains(FilterTag);
			if (!bMatch)
			{
				continue;
			}
		}

		FTetherHandlerSummary S;
		S.HandlerId = R.HandlerId;
		S.Scope = R.Scope;
		S.TaskName = R.TaskName;
		S.Description = R.Description;
		S.TriggerSummary = TetherReactiveImpl::BuildSummaryTrigger(R.TriggerType, R.Selector);
		S.SubjectPath = TetherReactiveImpl::BuildSubjectPath(R.Subject);
		S.ScriptPath = R.ScriptPath;
		S.Tags = R.Tags;
		S.Lifetime = TetherReactiveImpl::LifetimeName(R.Lifetime);
		S.bPaused = R.bPaused;
		S.Stats = R.Stats;
		Results.Add(MoveTemp(S));
	}

	Results.Sort([](const FTetherHandlerSummary& A, const FTetherHandlerSummary& B)
	{
		return A.HandlerId < B.HandlerId;
	});
	return Results;
}

bool UTetherReactiveSubsystem::GetHandler(const FString& HandlerId, FTetherHandlerDetail& OutDetail) const
{
	const TSharedRef<FTetherHandlerRecord>* Found = Handlers.Find(HandlerId);
	if (!Found)
	{
		return false;
	}
	const FTetherHandlerRecord& R = **Found;

	FTetherHandlerSummary S;
	S.HandlerId = R.HandlerId;
	S.Scope = R.Scope;
	S.TaskName = R.TaskName;
	S.Description = R.Description;
	S.TriggerSummary = TetherReactiveImpl::BuildSummaryTrigger(R.TriggerType, R.Selector);
	S.SubjectPath = TetherReactiveImpl::BuildSubjectPath(R.Subject);
	S.ScriptPath = R.ScriptPath;
	S.Tags = R.Tags;
	S.Lifetime = TetherReactiveImpl::LifetimeName(R.Lifetime);
	S.bPaused = R.bPaused;
	S.Stats = R.Stats;

	OutDetail.Summary = MoveTemp(S);
	OutDetail.Script = R.Script;
	OutDetail.ErrorPolicy = TetherReactiveImpl::ErrorPolicyName(R.ErrorPolicy);
	OutDetail.ThrottleMs = R.ThrottleMs;
	OutDetail.RemainingCalls =
		(R.Lifetime == ETetherHandlerLifetime::Count) ? R.RemainingCalls : -1;
	OutDetail.CreatedAt = R.CreatedAt.ToIso8601();
	return true;
}

bool UTetherReactiveSubsystem::GetStats(const FString& HandlerId, FTetherHandlerStats& OutStats) const
{
	if (const TSharedRef<FTetherHandlerRecord>* R = Handlers.Find(HandlerId))
	{
		OutStats = (*R)->Stats;
		return true;
	}
	return false;
}

// ─── Deferred execution ─────────────────────────────────────────

void UTetherReactiveSubsystem::DeferToNextTick(const FString& Script)
{
	if (!Script.IsEmpty())
	{
		DeferredScripts.Add(Script);
	}
}

// ─── Dispatch ───────────────────────────────────────────────────

void UTetherReactiveSubsystem::Dispatch(
	ETetherTrigger TriggerType,
	TWeakObjectPtr<UObject> Subject,
	FName Selector,
	const TMap<FString, FString>& ContextLiterals)
{
	DispatchLocked(TriggerType, Subject, Selector, ContextLiterals);
}

void UTetherReactiveSubsystem::DispatchLocked(
	ETetherTrigger TriggerType,
	const TWeakObjectPtr<UObject>& Subject,
	const FName& Selector,
	const TMap<FString, FString>& ContextLiterals)
{
	if (DispatchDepth >= TetherReactiveImpl::MaxDispatchDepth)
	{
		UE_LOG(LogTetherReactive, Error,
			TEXT("dispatch depth %d exceeded cap (%d) — aborting this fire"),
			DispatchDepth, TetherReactiveImpl::MaxDispatchDepth);
		return;
	}
	if (DispatchDepth >= TetherReactiveImpl::WarnDispatchDepth)
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("dispatch depth %d (warn threshold); check for recursive event chains"),
			DispatchDepth);
	}

	// Snapshot matching handler ids. Handlers may register/unregister
	// during dispatch without invalidating iteration.
	TArray<FString> SnapshotIds;
	SnapshotIds.Reserve(Handlers.Num());
	for (const auto& Pair : Handlers)
	{
		const FTetherHandlerRecord& R = *Pair.Value;
		if (R.TriggerType != TriggerType)
		{
			continue;
		}
		// Subject match: a global event (explicitly-null Subject) matches only
		// global handlers; a live event subject matches the same object.
		// Stale per-subject records (dead weak ptr) must NOT match a global
		// event — pointer equality used to equate both "dead" with "null".
		const bool bEventIsGlobal = Subject.IsExplicitlyNull() && !Subject.IsValid();
		const bool bRecordIsGlobal = R.Subject.IsExplicitlyNull() && !R.Subject.IsValid();
		if (bEventIsGlobal != bRecordIsGlobal)
		{
			continue;
		}
		if (!bEventIsGlobal)
		{
			UObject* RecSubject = R.Subject.Get();
			UObject* EvtSubject = Subject.Get();
			if (!RecSubject || !EvtSubject || RecSubject != EvtSubject)
			{
				continue; // expired record subject: skip, never fire
			}
		}
		if (!R.Selector.IsNone() && R.Selector != Selector)
		{
			continue;
		}
		SnapshotIds.Add(Pair.Key);
	}
	if (SnapshotIds.Num() == 0)
	{
		return;
	}

	++DispatchDepth;
	TArray<FString> ToRemoveAfter;

	for (const FString& Id : SnapshotIds)
	{
		TSharedRef<FTetherHandlerRecord>* Found = Handlers.Find(Id);
		if (!Found)
		{
			continue; // removed mid-snapshot
		}
		ExecuteHandlerOnce(**Found, ContextLiterals, ToRemoveAfter);
	}

	--DispatchDepth;

	for (const FString& Id : ToRemoveAfter)
	{
		RemoveByIdInternal(Id);
	}
}

void UTetherReactiveSubsystem::DispatchOne(
	const FString& HandlerId,
	const TMap<FString, FString>& ContextLiterals)
{
	if (DispatchDepth >= TetherReactiveImpl::MaxDispatchDepth)
	{
		UE_LOG(LogTetherReactive, Error,
			TEXT("DispatchOne: depth %d exceeded cap (%d) — aborting"),
			DispatchDepth, TetherReactiveImpl::MaxDispatchDepth);
		return;
	}
	if (DispatchDepth >= TetherReactiveImpl::WarnDispatchDepth)
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("DispatchOne: depth %d (warn threshold)"), DispatchDepth);
	}

	TSharedRef<FTetherHandlerRecord>* Found = Handlers.Find(HandlerId);
	if (!Found) return;

	++DispatchDepth;
	TArray<FString> ToRemoveAfter;
	ExecuteHandlerOnce(**Found, ContextLiterals, ToRemoveAfter);
	--DispatchDepth;

	for (const FString& Id : ToRemoveAfter)
	{
		RemoveByIdInternal(Id);
	}
}

void UTetherReactiveSubsystem::ExecuteHandlerOnce(
	FTetherHandlerRecord& R,
	const TMap<FString, FString>& ContextLiterals,
	TArray<FString>& OutToRemove)
{
	if (R.bPaused)
	{
		return;
	}

	// Subject liveness check for WhileSubjectAlive lifetime.
	if (R.Lifetime == ETetherHandlerLifetime::WhileSubjectAlive &&
		!R.Subject.IsExplicitlyNull() && !R.Subject.IsValid())
	{
		OutToRemove.Add(R.HandlerId);
		return;
	}

	// Throttle.
	if (R.ThrottleMs > 0)
	{
		const double Now = FPlatformTime::Seconds();
		const double Delta = (Now - R.LastFirePlatformSeconds) * 1000.0;
		if (Delta < static_cast<double>(R.ThrottleMs))
		{
			return;
		}
		R.LastFirePlatformSeconds = Now;
	}
	else
	{
		R.LastFirePlatformSeconds = FPlatformTime::Seconds();
	}

	// Build + execute.
	const FString WrappedScript = BuildWrappedScript(R, ContextLiterals);
	const double T0 = FPlatformTime::Seconds();
	FString Error;
	const bool bOk = ExecutePythonScript(WrappedScript, Error);
	const int64 ElapsedUs = static_cast<int64>((FPlatformTime::Seconds() - T0) * 1'000'000.0);

	// Stats.
	R.Stats.Calls += 1;
	R.Stats.TotalMicroseconds += ElapsedUs;
	R.Stats.MaxMicroseconds = FMath::Max(R.Stats.MaxMicroseconds, ElapsedUs);
	if (GEditor)
	{
		if (UWorld* W = GEditor->GetEditorWorldContext().World())
		{
			R.Stats.LastFireTimeSeconds = W->GetTimeSeconds();
		}
	}
	if (!bOk)
	{
		R.Stats.ErrorCount += 1;
		R.Stats.LastError = Error;
		UE_LOG(LogTetherReactive, Warning,
			TEXT("handler %s '%s' failed: %s"),
			*R.HandlerId, *R.TaskName, *Error);

		if (R.ErrorPolicy == ETetherErrorPolicy::Throw)
		{
			// Throw escalates beyond LogContinue: log at Error severity AND
			// pause the handler so it stops firing (a raising handler is
			// presumably broken; triage before more calls). Python can observe
			// the pause via stats.bPausedByErrorPolicy / summary.bPaused and
			// undo it with Resume().
			R.bPaused = true;
			R.Stats.bPausedByErrorPolicy = true;
			UE_LOG(LogTetherReactive, Error,
				TEXT("handler %s '%s' (Throw policy): %s — handler paused (resume with tether_resume_handler)"),
				*R.HandlerId, *R.TaskName, *Error);
			// No early return: a failed Throw invocation still burns the
			// Once/Count budget below (A-P3-9's "failed calls count too"), so
			// a Once handler that raised is removed, not left paused forever.
		}
		else if (R.ErrorPolicy == ETetherErrorPolicy::LogUnregister)
		{
			OutToRemove.Add(R.HandlerId);
			return;
		}
	}

	// Lifetime decrement. NOTE: this runs after failed invocations too (any
	// path that didn't return above — LogContinue, and Throw which only pauses
	// the handler) — a Count/Once handler that keeps raising still burns its
	// budget. ErrorCount/LastError are the stats to consult when a handler's
	// fire count doesn't match its success count.
	bool bShouldRemove = false;
	TetherReactiveImpl::ApplyLifetimeDecrement(R, bShouldRemove);
	if (bShouldRemove)
	{
		OutToRemove.Add(R.HandlerId);
	}
}

// ─── Persistent ticker registry ─────────────────────────────────

FTSTicker::FDelegateHandle UTetherReactiveSubsystem::RegisterPersistentTicker(
	TFunction<bool(float)> Callback,
	const FString& DebugName)
{
	if (!Callback)
	{
		return FTSTicker::FDelegateHandle();
	}
	FTSTicker::FDelegateHandle Handle = FTSTicker::GetCoreTicker().AddTicker(
		*FString::Printf(TEXT("TetherReactive.Persistent.%s"), *DebugName),
		0.0f,
		[CB = MoveTemp(Callback)](float Dt) -> bool { return CB(Dt); });
	PersistentTickers.Add(Handle);
	return Handle;
}

void UTetherReactiveSubsystem::UnregisterPersistentTicker(FTSTicker::FDelegateHandle& Handle)
{
	if (!Handle.IsValid()) return;
	FTSTicker::GetCoreTicker().RemoveTicker(Handle);
	PersistentTickers.RemoveAll([&Handle](const FTSTicker::FDelegateHandle& H){ return H == Handle; });
	Handle.Reset();
}

// ─── Script assembly + execution ────────────────────────────────

FString UTetherReactiveSubsystem::BuildWrappedScript(
	const FTetherHandlerRecord& Record,
	const TMap<FString, FString>& ContextLiterals)
{
	// Build the ctx dict body from the adapter-supplied literals. Values are
	// already Python source expressions (quoted strings, numeric literals,
	// unreal.load_object(...) calls, etc.) — the adapter is responsible for
	// quoting/escaping its string values.
	FString CtxBody;
	for (const auto& Pair : ContextLiterals)
	{
		CtxBody += FString::Printf(TEXT("    '%s': %s,\n"),
			*TetherReactiveImpl::EscapePythonStringLiteral(Pair.Key),
			*Pair.Value);
	}

	const FString TaskNameEsc = TetherReactiveImpl::EscapePythonStringLiteral(Record.TaskName);
	const FString HandlerIdEsc = TetherReactiveImpl::EscapePythonStringLiteral(Record.HandlerId);

	// Preamble sets up ctx + convenience names + state dicts + log/defer helpers,
	// then runs the user script inside a try/except that prints the traceback on
	// failure and re-raises to make ExecPythonCommandEx return false.
	//
	// Note: unlike the tether server's sync exec path we don't capture the
	// handler's stdout — its output just goes to the editor log via the user's
	// own unreal.log() calls. The base64 transport below is only about
	// preserving the script's exact bytes, not capturing output.
	FString Script;
	Script.Append(TEXT("import sys as _sys\n"));
	Script.Append(TEXT("import unreal\n"));
	Script.Append(TEXT("import base64 as _b64\n"));
	Script.Append(TEXT("_mod = _sys.modules.setdefault('_tether_reactive_state', type(_sys)('_tether_reactive_state'))\n"));
	Script.Append(TEXT("if not hasattr(_mod, 'shared'):  _mod.shared = {}\n"));
	Script.Append(TEXT("if not hasattr(_mod, 'private'): _mod.private = {}\n"));
	Script.Append(FString::Printf(TEXT("handler_id = '%s'\n"), *HandlerIdEsc));
	Script.Append(FString::Printf(TEXT("handler_task_name = '%s'\n"), *TaskNameEsc));
	Script.Append(TEXT("tether_state = _mod.shared\n"));
	Script.Append(TEXT("state = _mod.private.setdefault(handler_id, {})\n"));
	Script.Append(TEXT("ctx = {\n"));
	Script.Append(CtxBody);
	Script.Append(TEXT("}\n"));
	// Hoist the most common ctx keys to local names so handler scripts stay terse.
	Script.Append(TEXT("for _k, _v in list(ctx.items()):\n"));
	Script.Append(TEXT("    globals()[_k] = _v\n"));
	// Helpers.
	Script.Append(TEXT("def log(_msg):\n"));
	Script.Append(TEXT("    unreal.log('[reactive:' + handler_id + '|' + handler_task_name + '] ' + str(_msg))\n"));
	Script.Append(TEXT("def defer_to_next_tick(_src):\n"));
	Script.Append(TEXT("    unreal.TetherReactiveLibrary.defer_to_next_tick(_src)\n"));
	// User script is base64-encoded instead of being indented into the
	// try-block. The old line-by-line "    " prefix permanently altered the
	// content of multi-line strings (triple-quoted literals, backslash
	// continuations) — a data-correctness bug for handlers that build code /
	// JSON / templates. Base64 sidesteps quoting and indentation entirely;
	// the same pattern the tether server's sync exec path already uses.
	const FTCHARToUTF8 ScriptUtf8(*Record.Script);
	const FString ScriptB64 = FBase64::Encode(
		reinterpret_cast<const uint8*>(ScriptUtf8.Get()),
		ScriptUtf8.Length());
	Script.Append(FString::Printf(
		TEXT("_src = _b64.b64decode('%s').decode('utf-8')\n"), *ScriptB64));
	// User script inside a try/except that reports and re-raises.
	Script.Append(TEXT("try:\n"));
	Script.Append(TEXT("    exec(compile(_src, '<tether-handler>', 'exec'))\n"));
	Script.Append(TEXT("except Exception:\n"));
	Script.Append(TEXT("    import traceback as _tb\n"));
	Script.Append(TEXT("    _err = _tb.format_exc()\n"));
	Script.Append(TEXT("    unreal.log_error('[reactive:' + handler_id + '|' + handler_task_name + '] ' + _err)\n"));
	Script.Append(TEXT("    raise\n"));
	return Script;
}

bool UTetherReactiveSubsystem::ExecutePythonScript(const FString& FullScript, FString& OutError)
{
	OutError.Reset();

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		OutError = TEXT("PythonScriptPlugin unavailable");
		return false;
	}

	FPythonCommandEx CommandEx;
	CommandEx.Command = FullScript;
	CommandEx.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	CommandEx.FileExecutionScope = EPythonFileExecutionScope::Public;

	const bool bOk = PythonPlugin->ExecPythonCommandEx(CommandEx);
	if (!bOk)
	{
		// Concatenate any Error-severity entries so callers can surface
		// the Python traceback to stats / agent.
		for (const FPythonLogOutputEntry& Entry : CommandEx.LogOutput)
		{
			if (Entry.Type == EPythonLogOutputType::Error)
			{
				if (!OutError.IsEmpty())
				{
					OutError += TEXT("\n");
				}
				OutError += Entry.Output;
			}
		}
		if (OutError.IsEmpty())
		{
			OutError = CommandEx.CommandResult.IsEmpty()
				? FString(TEXT("python exec failed (no error text)"))
				: CommandEx.CommandResult;
		}
	}
	return bOk;
}

// ─── Cleanup hooks ──────────────────────────────────────────────

void UTetherReactiveSubsystem::HandleWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	if (!World)
	{
		return;
	}
	TArray<FString> ToDelete;     // WhilePIE: purge entirely (user declared ephemeral)
	TArray<FString> ToDefer;      // Others: move to DeferredHandlers so they survive the session gap
	for (const auto& Pair : Handlers)
	{
		const FTetherHandlerRecord& R = *Pair.Value;
		UObject* Subj = R.Subject.Get();
		if (!Subj) continue;
		if (Subj->GetWorld() != World) continue;
		if (R.Lifetime == ETetherHandlerLifetime::WhilePIE)
		{
			ToDelete.Add(Pair.Key);
		}
		else
		{
			ToDefer.Add(Pair.Key);
		}
	}
	for (const FString& Id : ToDelete)
	{
		UnregisterHandler(Id);
	}
	for (const FString& Id : ToDefer)
	{
		MoveHandlerToDeferred(Id);
	}
	if (ToDelete.Num() > 0 || ToDefer.Num() > 0)
	{
		UE_LOG(LogTetherReactive, Log,
			TEXT("world cleanup on %s: deleted %d ephemeral, deferred %d for next session"),
			*World->GetName(), ToDelete.Num(), ToDefer.Num());
	}
}

void UTetherReactiveSubsystem::MoveHandlerToDeferred(const FString& HandlerId)
{
	TSharedRef<FTetherHandlerRecord>* Found = Handlers.Find(HandlerId);
	if (!Found) return;
	TSharedRef<FTetherHandlerRecord> Ref = *Found;
	if (ITetherReactiveAdapter* Adapter = FindAdapter(Ref->TriggerType))
	{
		Adapter->OnHandlerRemoved(*Ref);
	}
	// Copy the record for the deferred queue. Stale Subject weak ptr is cleared
	// so ResolveForRestore rebinds fresh from RegistrationContext.
	FTetherHandlerRecord Copy = *Ref;
	Copy.Subject.Reset();
	DeferredHandlers.Add(MoveTemp(Copy));
	Handlers.Remove(HandlerId);
	// Intentionally no MarkDirty: the record is still persisted via
	// DeferredHandlers (BuildPersistenceJson iterates both sets), so nothing
	// on disk needs to change.
}

void UTetherReactiveSubsystem::HandlePieEnded(bool /*bIsSimulating*/)
{
	TArray<FString> ToRemove;
	for (const auto& Pair : Handlers)
	{
		if (Pair.Value->Lifetime == ETetherHandlerLifetime::WhilePIE)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FString& Id : ToRemove)
	{
		UnregisterHandler(Id);
	}
	if (ToRemove.Num() > 0)
	{
		UE_LOG(LogTetherReactive, Log,
			TEXT("PIE ended: removed %d WhilePIE handler(s)"), ToRemove.Num());
	}
}

// ─── Persistence (P6.B3) ──────────────────────────────────────────

void UTetherReactiveSubsystem::MarkDirty()
{
	bPersistenceDirty = true;
}

FString UTetherReactiveSubsystem::GetPersistencePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(),
		TEXT("Tether"), TEXT("reactive-handlers.json"));
}

int32 UTetherReactiveSubsystem::GetDeferredHandlerCount() const
{
	return DeferredHandlers.Num();
}

FString UTetherReactiveSubsystem::RestoreSingleRecord(FTetherHandlerRecord&& Record)
{
	if (Record.HandlerId.IsEmpty())
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RestoreSingleRecord refused: empty HandlerId"));
		return FString();
	}
	ITetherReactiveAdapter* Adapter = FindAdapter(Record.TriggerType);
	if (!Adapter)
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RestoreSingleRecord refused: no adapter for trigger '%s'"),
			*TetherReactiveImpl::TriggerTypeName(Record.TriggerType));
		return FString();
	}
	// Caller is expected to have preserved HandlerId + pre-resolved Subject/
	// Selector/AdapterPayload. CreatedAt is preserved from JSON; fall back
	// to now() if it wasn't serialised (older schemas).
	if (Record.CreatedAt.GetTicks() == 0)
	{
		Record.CreatedAt = FDateTime::UtcNow();
	}
	TSharedRef<FTetherHandlerRecord> Shared = MakeShared<FTetherHandlerRecord>(MoveTemp(Record));
	const FString Id = Shared->HandlerId;
	// Sanity: reject if the id is already live (double-restore).
	if (Handlers.Contains(Id))
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RestoreSingleRecord refused: HandlerId '%s' already present"), *Id);
		return FString();
	}
	Handlers.Add(Id, Shared);
	Adapter->OnHandlerAdded(*Shared);
	return Id;
}

void UTetherReactiveSubsystem::HandlePostPIEStarted(bool /*bIsSimulating*/)
{
	if (DeferredHandlers.Num() > 0)
	{
		RetryDeferredHandlers();
	}
}

void UTetherReactiveSubsystem::RetryDeferredHandlers()
{
	TArray<FTetherHandlerRecord> StillDeferred;
	int32 Restored = 0;
	for (FTetherHandlerRecord& R : DeferredHandlers)
	{
		if (UTetherReactiveLibrary::ResolveForRestore(R))
		{
			if (!RestoreSingleRecord(MoveTemp(R)).IsEmpty())
			{
				++Restored;
				continue;
			}
		}
		StillDeferred.Add(MoveTemp(R));
	}
	DeferredHandlers = MoveTemp(StillDeferred);
	if (Restored > 0)
	{
		UE_LOG(LogTetherReactive, Log,
			TEXT("PostPIEStarted: restored %d deferred handler(s); %d still waiting"),
			Restored, DeferredHandlers.Num());
	}
}

// JSON helpers — build a JSON handler array + seq counters.
namespace TetherReactivePersistenceImpl
{
	TSharedPtr<FJsonObject> RecordToJson(const FTetherHandlerRecord& R)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("handler_id"),     R.HandlerId);
		O->SetStringField(TEXT("scope"),          R.Scope);
		O->SetStringField(TEXT("task_name"),      R.TaskName);
		O->SetStringField(TEXT("description"),    R.Description);
		TArray<TSharedPtr<FJsonValue>> TagsJson;
		for (const FString& T : R.Tags) TagsJson.Add(MakeShared<FJsonValueString>(T));
		O->SetArrayField(TEXT("tags"), TagsJson);
		O->SetStringField(TEXT("script"),         R.Script);
		O->SetStringField(TEXT("script_path"),    R.ScriptPath);
		O->SetStringField(TEXT("trigger_type"),   TetherReactiveImpl::TriggerTypeName(R.TriggerType));
		TSharedPtr<FJsonObject> Reg = MakeShared<FJsonObject>();
		for (const auto& Pair : R.RegistrationContext)
		{
			Reg->SetStringField(Pair.Key, Pair.Value);
		}
		O->SetObjectField(TEXT("registration_context"), Reg);
		O->SetStringField(TEXT("lifetime"),       TetherReactiveImpl::LifetimeName(R.Lifetime));
		O->SetNumberField(TEXT("remaining_calls"), R.RemainingCalls);
		O->SetStringField(TEXT("error_policy"),   TetherReactiveImpl::ErrorPolicyName(R.ErrorPolicy));
		O->SetNumberField(TEXT("throttle_ms"),    R.ThrottleMs);
		O->SetBoolField(TEXT("paused"),           R.bPaused);
		// Persist the Throw-policy auto-pause marker so a restarted editor can
		// still tell an error-policy pause apart from a manual Pause() (F3);
		// absent on pre-schema files and defaults to false on load.
		O->SetBoolField(TEXT("paused_by_error_policy"), R.Stats.bPausedByErrorPolicy);
		O->SetStringField(TEXT("created_at"),     R.CreatedAt.ToIso8601());
		return O;
	}

	ETetherTrigger ParseTriggerType(const FString& S)
	{
		if (S == TEXT("GameplayEvent"))       return ETetherTrigger::GameplayEvent;
		if (S == TEXT("AttributeChanged"))    return ETetherTrigger::AttributeChanged;
		if (S == TEXT("ActorLifecycle"))      return ETetherTrigger::ActorLifecycle;
		if (S == TEXT("MovementModeChanged")) return ETetherTrigger::MovementModeChanged;
		if (S == TEXT("AnimNotify"))          return ETetherTrigger::AnimNotify;
		if (S == TEXT("InputAction"))         return ETetherTrigger::InputAction;
		if (S == TEXT("Timer"))               return ETetherTrigger::Timer;
		if (S == TEXT("AssetEvent"))          return ETetherTrigger::AssetEvent;
		if (S == TEXT("PieEvent"))            return ETetherTrigger::PieEvent;
		if (S == TEXT("BpCompiled"))          return ETetherTrigger::BpCompiled;
		return ETetherTrigger::None;
	}

	ETetherHandlerLifetime ParseLifetime(const FString& S)
	{
		if (S == TEXT("Permanent"))         return ETetherHandlerLifetime::Permanent;
		if (S == TEXT("Once"))              return ETetherHandlerLifetime::Once;
		if (S == TEXT("Count"))             return ETetherHandlerLifetime::Count;
		if (S == TEXT("WhilePIE"))          return ETetherHandlerLifetime::WhilePIE;
		if (S == TEXT("WhileSubjectAlive")) return ETetherHandlerLifetime::WhileSubjectAlive;
		return ETetherHandlerLifetime::Permanent;
	}

	ETetherErrorPolicy ParseErrorPolicy(const FString& S)
	{
		if (S == TEXT("LogContinue"))   return ETetherErrorPolicy::LogContinue;
		if (S == TEXT("LogUnregister")) return ETetherErrorPolicy::LogUnregister;
		if (S == TEXT("Throw"))         return ETetherErrorPolicy::Throw;
		return ETetherErrorPolicy::LogContinue;
	}

	bool JsonToRecord(const TSharedPtr<FJsonObject>& O, FTetherHandlerRecord& Out)
	{
		if (!O.IsValid()) return false;
		Out.HandlerId     = O->GetStringField(TEXT("handler_id"));
		Out.Scope         = O->GetStringField(TEXT("scope"));
		Out.TaskName      = O->GetStringField(TEXT("task_name"));
		Out.Description   = O->GetStringField(TEXT("description"));
		const TArray<TSharedPtr<FJsonValue>>* TagsArr = nullptr;
		if (O->TryGetArrayField(TEXT("tags"), TagsArr) && TagsArr)
		{
			for (const TSharedPtr<FJsonValue>& V : *TagsArr)
			{
				if (V.IsValid()) Out.Tags.Add(V->AsString());
			}
		}
		Out.Script        = O->GetStringField(TEXT("script"));
		Out.ScriptPath    = O->GetStringField(TEXT("script_path"));
		Out.TriggerType   = ParseTriggerType(O->GetStringField(TEXT("trigger_type")));
		const TSharedPtr<FJsonObject>* RegObj = nullptr;
		if (O->TryGetObjectField(TEXT("registration_context"), RegObj) && RegObj && RegObj->IsValid())
		{
			for (const auto& Pair : (*RegObj)->Values)
			{
				if (Pair.Value.IsValid() && Pair.Value->Type == EJson::String)
				{
					// FJsonObject::Values became TMap<UE::FSharedString,...> in 5.8;
					// `*Pair.Key` is const TCHAR* on every version (FString::operator*
					// pre-5.8, FSharedString::operator* on 5.8+).
					Out.RegistrationContext.Add(FString(*Pair.Key), Pair.Value->AsString());
				}
			}
		}
		Out.Lifetime      = ParseLifetime(O->GetStringField(TEXT("lifetime")));
		Out.RemainingCalls = static_cast<int32>(O->GetNumberField(TEXT("remaining_calls")));
		Out.ErrorPolicy   = ParseErrorPolicy(O->GetStringField(TEXT("error_policy")));
		Out.ThrottleMs    = static_cast<int32>(O->GetNumberField(TEXT("throttle_ms")));
		// Paused state survives restarts; absent on pre-schema files (false).
		O->TryGetBoolField(TEXT("paused"), Out.bPaused);
		// Throw-policy auto-pause marker (F3): absent on pre-schema files
		// (false) — backward compatible with older persistence files.
		O->TryGetBoolField(TEXT("paused_by_error_policy"), Out.Stats.bPausedByErrorPolicy);
		FString Created;
		if (O->TryGetStringField(TEXT("created_at"), Created) && !Created.IsEmpty())
		{
			FDateTime::ParseIso8601(*Created, Out.CreatedAt);
		}
		return !Out.HandlerId.IsEmpty() && Out.TriggerType != ETetherTrigger::None;
	}
}

FString UTetherReactiveSubsystem::BuildPersistenceJson() const
{
	using namespace TetherReactivePersistenceImpl;
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetNumberField(TEXT("runtime_seq"), RuntimeSeq);
	Root->SetNumberField(TEXT("editor_seq"),  EditorSeq);

	TArray<TSharedPtr<FJsonValue>> HandlersArr;
	// Live handlers: persist non-WhilePIE.
	for (const auto& Pair : Handlers)
	{
		const FTetherHandlerRecord& R = *Pair.Value;
		if (R.Lifetime == ETetherHandlerLifetime::WhilePIE) continue;
		HandlersArr.Add(MakeShared<FJsonValueObject>(RecordToJson(R)));
	}
	// Deferred handlers: keep them persisted too, so a session that never
	// starts PIE doesn't lose its pending restore records.
	for (const FTetherHandlerRecord& R : DeferredHandlers)
	{
		if (R.Lifetime == ETetherHandlerLifetime::WhilePIE) continue;
		HandlersArr.Add(MakeShared<FJsonValueObject>(RecordToJson(R)));
	}
	Root->SetArrayField(TEXT("handlers"), HandlersArr);

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out, 0);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

bool UTetherReactiveSubsystem::SaveAllHandlers()
{
	const FString Path = GetPersistencePath();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*FPaths::GetPath(Path));
	const FString Content = BuildPersistenceJson();
	if (!FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("SaveAllHandlers: write failed (%s)"), *Path);
		return false;
	}
	UE_LOG(LogTetherReactive, Verbose,
		TEXT("SaveAllHandlers: wrote %d handler(s) to %s"),
		Handlers.Num() + DeferredHandlers.Num(), *Path);
	return true;
}

int32 UTetherReactiveSubsystem::RestoreFromJson(const FString& JsonText)
{
	using namespace TetherReactivePersistenceImpl;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		// The file-level rename (when a path is known) is handled by the
		// caller, LoadAllHandlers, before this parse is retried.
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RestoreFromJson: invalid JSON — skipping (caller renames the file when a path is known)"));
		return 0;
	}

	const int32 Version = static_cast<int32>(Root->GetNumberField(TEXT("version")));
	if (Version != 1)
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("RestoreFromJson: unsupported schema version %d (expected 1); skipping"),
			Version);
		return 0;
	}

	// Seq counters: keep whichever is larger so new IDs never collide.
	const int32 SavedRuntimeSeq = static_cast<int32>(Root->GetNumberField(TEXT("runtime_seq")));
	const int32 SavedEditorSeq  = static_cast<int32>(Root->GetNumberField(TEXT("editor_seq")));
	RuntimeSeq = FMath::Max(RuntimeSeq, SavedRuntimeSeq);
	EditorSeq  = FMath::Max(EditorSeq,  SavedEditorSeq);

	const TArray<TSharedPtr<FJsonValue>>* HandlersArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("handlers"), HandlersArr) || !HandlersArr)
	{
		return 0;
	}

	int32 RestoredNow = 0;
	int32 Deferred = 0;
	for (const TSharedPtr<FJsonValue>& V : *HandlersArr)
	{
		if (!V.IsValid() || V->Type != EJson::Object) continue;
		FTetherHandlerRecord R;
		if (!JsonToRecord(V->AsObject(), R)) continue;
		if (R.Lifetime == ETetherHandlerLifetime::WhilePIE) continue;

		if (UTetherReactiveLibrary::ResolveForRestore(R))
		{
			if (!RestoreSingleRecord(MoveTemp(R)).IsEmpty())
			{
				++RestoredNow;
				continue;
			}
		}
		DeferredHandlers.Add(MoveTemp(R));
		++Deferred;
	}

	if (RestoredNow > 0 || Deferred > 0)
	{
		UE_LOG(LogTetherReactive, Log,
			TEXT("RestoreFromJson: restored %d handler(s); %d deferred (await PIE start)"),
			RestoredNow, Deferred);
	}
	return RestoredNow;
}

int32 UTetherReactiveSubsystem::LoadAllHandlers()
{
	const FString Path = GetPersistencePath();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.FileExists(*Path))
	{
		return 0;
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTetherReactive, Warning,
			TEXT("LoadAllHandlers: read failed (%s)"), *Path);
		return 0;
	}

	// Corrupt file: parse will fail in RestoreFromJson. Save the evidence —
	// rename the file aside with a UTC-timestamp suffix instead of leaving it
	// in place (a bad file in place re-fails the parse on every startup and
	// blocks legitimate saves).
	{
		TSharedPtr<FJsonObject> ProbeRoot;
		TSharedRef<TJsonReader<>> ProbeReader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(ProbeReader, ProbeRoot) || !ProbeRoot.IsValid())
		{
			const FString CorruptPath = Path + TEXT(".corrupt-") +
				FDateTime::UtcNow().ToString(TEXT("%Y%m%d%H%M%S"));
			if (PF.MoveFile(*CorruptPath, *Path))
			{
				UE_LOG(LogTetherReactive, Warning,
					TEXT("LoadAllHandlers: invalid JSON in '%s' — renamed to '%s' and starting fresh"),
					*Path, *CorruptPath);
			}
			else
			{
				UE_LOG(LogTetherReactive, Warning,
					TEXT("LoadAllHandlers: invalid JSON in '%s' — rename to '%s' failed (check permissions); starting fresh"),
					*Path, *CorruptPath);
			}
			return 0;
		}
	}

	// Clear existing registry first so load is idempotent.
	if (Handlers.Num() > 0)
	{
		TArray<FString> Existing;
		Handlers.GetKeys(Existing);
		for (const FString& Id : Existing) { UnregisterHandler(Id); }
	}
	DeferredHandlers.Reset();

	const int32 Restored = RestoreFromJson(Text);

	// Don't let this pre-existing-file load trigger an immediate re-save.
	bPersistenceDirty = false;
	return Restored;
}
