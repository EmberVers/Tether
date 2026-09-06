#include "TetherReactiveLibrary.h"
#include "TetherReactiveSubsystem.h"
#include "TetherReactiveShared.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EnhancedInputComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherReactiveLib, Log, All);

namespace TetherReactiveLibImpl
{
	// ResolveActorASC lives in TetherReactiveShared.h (namespace
	// TetherReactiveUtil) so the GameplayEvent adapter and this library share
	// one implementation.

	/** Find an actor by FName or label across all editor world contexts (PIE first). */
	AActor* FindActorByName(const FString& NameOrLabel)
	{
		if (!GEditor)
		{
			return nullptr;
		}

		auto SearchWorld = [&NameOrLabel](UWorld* World) -> AActor*
		{
			if (!World) return nullptr;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* A = *It;
				if (!A) continue;
				if (A->GetName() == NameOrLabel || A->GetActorNameOrLabel() == NameOrLabel)
				{
					return A;
				}
			}
			return nullptr;
		};

		// PIE worlds first — agent-owned actors with ASCs typically live there.
		for (const FWorldContext& Ctx : GEditor->GetWorldContexts())
		{
			if (Ctx.WorldType == EWorldType::PIE)
			{
				if (AActor* A = SearchWorld(Ctx.World())) return A;
			}
		}
		// Fall back to the editor world.
		return SearchWorld(GEditor->GetEditorWorldContext().World());
	}

	/** "Permanent" | "Once" | "Count:N" | "WhilePIE" | "WhileSubjectAlive". */
	bool ParseLifetime(const FString& In, ETetherHandlerLifetime& OutLifetime, int32& OutCount)
	{
		OutCount = -1;
		const FString S = In.IsEmpty() ? TEXT("Permanent") : In;
		if (S.Equals(TEXT("Permanent"), ESearchCase::IgnoreCase))
		{
			OutLifetime = ETetherHandlerLifetime::Permanent; return true;
		}
		if (S.Equals(TEXT("Once"), ESearchCase::IgnoreCase))
		{
			OutLifetime = ETetherHandlerLifetime::Once; return true;
		}
		if (S.Equals(TEXT("WhilePIE"), ESearchCase::IgnoreCase))
		{
			OutLifetime = ETetherHandlerLifetime::WhilePIE; return true;
		}
		if (S.Equals(TEXT("WhileSubjectAlive"), ESearchCase::IgnoreCase))
		{
			OutLifetime = ETetherHandlerLifetime::WhileSubjectAlive; return true;
		}
		if (S.StartsWith(TEXT("Count:"), ESearchCase::IgnoreCase))
		{
			const FString NStr = S.Mid(6);
			const int32 N = FCString::Atoi(*NStr);
			if (N <= 0)
			{
				return false;
			}
			OutLifetime = ETetherHandlerLifetime::Count;
			OutCount = N;
			return true;
		}
		return false;
	}

	bool ParseErrorPolicy(const FString& In, ETetherErrorPolicy& Out)
	{
		const FString S = In.IsEmpty() ? TEXT("LogContinue") : In;
		if (S.Equals(TEXT("LogContinue"), ESearchCase::IgnoreCase))   { Out = ETetherErrorPolicy::LogContinue;   return true; }
		if (S.Equals(TEXT("LogUnregister"), ESearchCase::IgnoreCase)) { Out = ETetherErrorPolicy::LogUnregister; return true; }
		if (S.Equals(TEXT("Throw"), ESearchCase::IgnoreCase))         { Out = ETetherErrorPolicy::Throw;         return true; }
		return false;
	}

	/** Populate common fields on the record. Returns false if parse failed. */
	bool FillCommonRecordFields(
		FTetherHandlerRecord& Record,
		const FString& TaskName,
		const FString& Description,
		const FString& Script,
		const FString& ScriptPath,
		const TArray<FString>& Tags,
		const FString& Lifetime,
		const FString& ErrorPolicy,
		int32 ThrottleMs,
		const TCHAR* CallerTag)
	{
		Record.Scope = TEXT("runtime");
		Record.TaskName = TaskName;
		Record.Description = Description;
		Record.Tags = Tags;
		Record.ScriptPath = ScriptPath;
		Record.Script = Script;
		Record.ThrottleMs = FMath::Max(0, ThrottleMs);

		int32 ParsedCount = -1;
		if (!ParseLifetime(Lifetime, Record.Lifetime, ParsedCount))
		{
			UE_LOG(LogTetherReactiveLib, Warning,
				TEXT("%s: bad Lifetime '%s'"), CallerTag, *Lifetime);
			return false;
		}
		Record.RemainingCalls = ParsedCount;

		if (!ParseErrorPolicy(ErrorPolicy, Record.ErrorPolicy))
		{
			UE_LOG(LogTetherReactiveLib, Warning,
				TEXT("%s: bad ErrorPolicy '%s'"), CallerTag, *ErrorPolicy);
			return false;
		}
		return true;
	}
}

FString UTetherReactiveLibrary::RegisterRuntimeGameplayEvent(
	const FString& TaskName,
	const FString& Description,
	const FString& TargetActorName,
	const FString& EventTag,
	const FString& Script,
	const FString& ScriptPath,
	const TArray<FString>& Tags,
	const FString& Lifetime,
	const FString& ErrorPolicy,
	int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub)
	{
		UE_LOG(LogTetherReactiveLib, Warning, TEXT("RegisterRuntimeGameplayEvent: subsystem unavailable"));
		return FString();
	}

	// Empty TargetActorName => global handler: fires whenever ANY live ASC
	// receives EventTag. The adapter walks current PIE-world ASCs at register
	// time and watches OnActorSpawned for late arrivals.
	const bool bGlobal = TargetActorName.IsEmpty();
	UAbilitySystemComponent* ASC = nullptr;
	if (!bGlobal)
	{
		AActor* TargetActor = TetherReactiveLibImpl::FindActorByName(TargetActorName);
		if (!TargetActor)
		{
			UE_LOG(LogTetherReactiveLib, Warning,
				TEXT("RegisterRuntimeGameplayEvent: actor '%s' not found in PIE or editor world"),
				*TargetActorName);
			return FString();
		}
		ASC = TetherReactiveUtil::ResolveActorASC(TargetActor);
		if (!ASC)
		{
			UE_LOG(LogTetherReactiveLib, Warning,
				TEXT("RegisterRuntimeGameplayEvent: no ASC on actor '%s'"), *TargetActor->GetPathName());
			return FString();
		}
	}

	if (EventTag.IsEmpty())
	{
		UE_LOG(LogTetherReactiveLib, Warning, TEXT("RegisterRuntimeGameplayEvent: EventTag is empty"));
		return FString();
	}
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*EventTag), /*bErrorIfNotFound=*/false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeGameplayEvent: tag '%s' is not registered"), *EventTag);
		return FString();
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeGameplayEvent")))
	{
		return FString();
	}
	Record.TriggerType = ETetherTrigger::GameplayEvent;
	Record.Subject = bGlobal ? TWeakObjectPtr<UObject>() : TWeakObjectPtr<UObject>(ASC);
	Record.Selector = Tag.GetTagName();
	Record.AdapterPayload = bGlobal ? TEXT("global") : FString();

	Record.RegistrationContext.Add(TEXT("target_actor_name"), TargetActorName);
	Record.RegistrationContext.Add(TEXT("event_tag"),         EventTag);

	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── Helpers shared across runtime register_* entry points ──────

namespace TetherReactiveLibImpl
{
	/** Find a UEnhancedInputComponent on the actor or its (player) controller. */
	UEnhancedInputComponent* ResolveInputComponent(AActor* Actor)
	{
		if (!Actor) return nullptr;
		if (UEnhancedInputComponent* C = Cast<UEnhancedInputComponent>(Actor->InputComponent))
		{
			return C;
		}
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			if (AController* Ctrl = Pawn->GetController())
			{
				if (UEnhancedInputComponent* C = Cast<UEnhancedInputComponent>(Ctrl->InputComponent))
				{
					return C;
				}
			}
		}
		return nullptr;
	}
}

// ─── AttributeChanged ───────────────────────────────────────────

FString UTetherReactiveLibrary::RegisterRuntimeAttributeChanged(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& AttributeName,
	const FString& Script, const FString& ScriptPath,
	const TArray<FString>& Tags, const FString& Lifetime,
	const FString& ErrorPolicy, int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = TetherReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeAttributeChanged: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	UAbilitySystemComponent* ASC = TetherReactiveUtil::ResolveActorASC(Actor);
	if (!ASC)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeAttributeChanged: no ASC on actor '%s'"), *Actor->GetPathName());
		return FString();
	}
	if (AttributeName.IsEmpty())
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeAttributeChanged: AttributeName is empty"));
		return FString();
	}
	// Pre-check the attribute actually exists on the ASC's spawned sets.
	// Without this, a typo'd attribute name would register + persist a handler
	// whose adapter-side binding silently fails (it can only log), so the
	// handler never fires. Refuse registration instead: empty HandlerId,
	// nothing stored, nothing persisted.
	if (!TetherReactiveUtil::ResolveAttribute(ASC, AttributeName).IsValid())
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeAttributeChanged: attribute '%s' not found on ASC's spawned sets (actor '%s')"),
			*AttributeName, *Actor->GetPathName());
		return FString();
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeAttributeChanged")))
	{
		return FString();
	}
	Record.TriggerType = ETetherTrigger::AttributeChanged;
	Record.Subject = TWeakObjectPtr<UObject>(ASC);
	Record.Selector = FName(*AttributeName);
	Record.RegistrationContext.Add(TEXT("target_actor_name"), TargetActorName);
	Record.RegistrationContext.Add(TEXT("attribute_name"),    AttributeName);
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── ActorLifecycle ─────────────────────────────────────────────

FString UTetherReactiveLibrary::RegisterRuntimeActorLifecycle(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& EventType,
	const FString& Script, const FString& ScriptPath,
	const TArray<FString>& Tags, const FString& Lifetime,
	const FString& ErrorPolicy, int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = TetherReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeActorLifecycle: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	if (!EventType.Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase) &&
		!EventType.Equals(TEXT("EndPlay"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeActorLifecycle: EventType must be 'Destroyed' or 'EndPlay'"));
		return FString();
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeActorLifecycle")))
	{
		return FString();
	}
	Record.TriggerType = ETetherTrigger::ActorLifecycle;
	Record.Subject = TWeakObjectPtr<UObject>(Actor);
	Record.Selector = FName(EventType.Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase)
		? TEXT("Destroyed") : TEXT("EndPlay"));
	Record.RegistrationContext.Add(TEXT("target_actor_name"), TargetActorName);
	Record.RegistrationContext.Add(TEXT("event_type"),        EventType);
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── MovementModeChanged ────────────────────────────────────────

FString UTetherReactiveLibrary::RegisterRuntimeMovementModeChanged(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& Script,
	const FString& ScriptPath, const TArray<FString>& Tags,
	const FString& Lifetime, const FString& ErrorPolicy, int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = TetherReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeMovementModeChanged: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	ACharacter* Char = Cast<ACharacter>(Actor);
	if (!Char)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeMovementModeChanged: '%s' is not an ACharacter"), *Actor->GetPathName());
		return FString();
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeMovementModeChanged")))
	{
		return FString();
	}
	Record.TriggerType = ETetherTrigger::MovementModeChanged;
	Record.Subject = TWeakObjectPtr<UObject>(Char);
	Record.Selector = NAME_None;
	Record.RegistrationContext.Add(TEXT("target_actor_name"), TargetActorName);
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── AnimNotify ─────────────────────────────────────────────────

FString UTetherReactiveLibrary::RegisterRuntimeAnimNotify(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& NotifyName,
	const FString& Script, const FString& ScriptPath,
	const TArray<FString>& Tags, const FString& Lifetime,
	const FString& ErrorPolicy, int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = TetherReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeAnimNotify: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeAnimNotify: actor '%s' has no USkeletalMeshComponent"),
			*Actor->GetPathName());
		return FString();
	}
	UAnimInstance* AI = Mesh->GetAnimInstance();
	if (!AI)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeAnimNotify: skeletal mesh on '%s' has no AnimInstance (AnimBP not set?)"),
			*Actor->GetPathName());
		return FString();
	}
	if (NotifyName.IsEmpty())
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeAnimNotify: NotifyName is empty"));
		return FString();
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeAnimNotify")))
	{
		return FString();
	}
	Record.TriggerType = ETetherTrigger::AnimNotify;
	Record.Subject = TWeakObjectPtr<UObject>(AI);
	Record.Selector = FName(*NotifyName);
	Record.RegistrationContext.Add(TEXT("target_actor_name"), TargetActorName);
	Record.RegistrationContext.Add(TEXT("notify_name"),       NotifyName);
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── InputAction ────────────────────────────────────────────────

FString UTetherReactiveLibrary::RegisterRuntimeInputAction(
	const FString& TaskName, const FString& Description,
	const FString& TargetActorName, const FString& InputActionPath,
	const FString& TriggerEvent, const FString& Script,
	const FString& ScriptPath, const TArray<FString>& Tags,
	const FString& Lifetime, const FString& ErrorPolicy, int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub) return FString();

	AActor* Actor = TetherReactiveLibImpl::FindActorByName(TargetActorName);
	if (!Actor)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeInputAction: actor '%s' not found"), *TargetActorName);
		return FString();
	}
	UEnhancedInputComponent* Comp = TetherReactiveLibImpl::ResolveInputComponent(Actor);
	if (!Comp)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeInputAction: no UEnhancedInputComponent on actor or its controller (not yet possessed?)"));
		return FString();
	}
	UInputAction* IA = Cast<UInputAction>(StaticLoadObject(
		UInputAction::StaticClass(), nullptr, *InputActionPath));
	if (!IA)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeInputAction: UInputAction '%s' failed to load"), *InputActionPath);
		return FString();
	}

	// Validate TriggerEvent early, remembering the canonical casing. The
	// Selector and RegistrationContext must store the canonical form — the
	// adapter parses the event name case-sensitively and Dispatch matches
	// the Selector exactly, so persisting the caller's raw casing ("started")
	// would create a handler that never fires.
	static const TCHAR* Known[] = { TEXT("Triggered"), TEXT("Started"), TEXT("Ongoing"),
		TEXT("Canceled"), TEXT("Completed") };
	const TCHAR* CanonicalEvent = nullptr;
	for (const TCHAR* K : Known)
	{
		if (TriggerEvent.Equals(K, ESearchCase::IgnoreCase))
		{
			CanonicalEvent = K;
			break;
		}
	}
	if (!CanonicalEvent)
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeInputAction: bad TriggerEvent '%s'"), *TriggerEvent);
		return FString();
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeInputAction")))
	{
		return FString();
	}
	Record.TriggerType = ETetherTrigger::InputAction;
	Record.Subject = TWeakObjectPtr<UObject>(Comp);
	Record.Selector = FName(*FString::Printf(TEXT("%s:%s"), *IA->GetName(), CanonicalEvent));
	Record.AdapterPayload = InputActionPath;
	Record.RegistrationContext.Add(TEXT("target_actor_name"),  TargetActorName);
	Record.RegistrationContext.Add(TEXT("input_action_path"),  InputActionPath);
	Record.RegistrationContext.Add(TEXT("trigger_event"),      FString(CanonicalEvent));
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── Timer ──────────────────────────────────────────────────────

FString UTetherReactiveLibrary::RegisterRuntimeTimer(
	const FString& TaskName, const FString& Description,
	float IntervalSeconds, const FString& Script,
	const FString& ScriptPath, const TArray<FString>& Tags,
	const FString& Lifetime, const FString& ErrorPolicy, int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub) return FString();

	if (!(IntervalSeconds > 0.0f))
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterRuntimeTimer: IntervalSeconds must be > 0 (got %f)"), IntervalSeconds);
		return FString();
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterRuntimeTimer")))
	{
		return FString();
	}
	Record.TriggerType = ETetherTrigger::Timer;
	Record.Subject = TWeakObjectPtr<UObject>();   // global — no subject
	Record.Selector = NAME_None;
	Record.AdapterPayload = FString::SanitizeFloat(IntervalSeconds);
	Record.RegistrationContext.Add(TEXT("interval_seconds"),
		FString::SanitizeFloat(IntervalSeconds));
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ─── Editor-domain registration (P5) ─────────────────────────────

namespace TetherReactiveLibImpl
{
	bool ValidateAssetEventFilter(const FString& EventFilter, FName& OutSelector)
	{
		if (EventFilter.IsEmpty())
		{
			OutSelector = NAME_None;
			return true;
		}
		static const TCHAR* kEvents[] = {
			TEXT("Added"), TEXT("Removed"), TEXT("Renamed"), TEXT("Updated") };
		for (const TCHAR* E : kEvents)
		{
			if (EventFilter.Equals(E, ESearchCase::IgnoreCase))
			{
				OutSelector = FName(E);
				return true;
			}
		}
		return false;
	}

	bool ValidatePieEventFilter(const FString& PhaseFilter, FName& OutSelector)
	{
		if (PhaseFilter.IsEmpty())
		{
			OutSelector = NAME_None;
			return true;
		}
		static const TCHAR* kPhases[] = {
			TEXT("PreBeginPIE"), TEXT("BeginPIE"), TEXT("PostPIEStarted"),
			TEXT("PrePIEEnded"), TEXT("EndPIE"),
			TEXT("PausePIE"), TEXT("ResumePIE"), TEXT("SingleStepPIE") };
		for (const TCHAR* P : kPhases)
		{
			if (PhaseFilter.Equals(P, ESearchCase::IgnoreCase))
			{
				OutSelector = FName(P);
				return true;
			}
		}
		return false;
	}
}

FString UTetherReactiveLibrary::RegisterEditorAssetEvent(
	const FString& TaskName, const FString& Description,
	const FString& EventFilter, const FString& Script,
	const FString& ScriptPath, const TArray<FString>& Tags,
	const FString& Lifetime, const FString& ErrorPolicy, int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub) return FString();

	FName Selector;
	if (!TetherReactiveLibImpl::ValidateAssetEventFilter(EventFilter, Selector))
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterEditorAssetEvent: unknown EventFilter '%s' (expected '', 'Added', 'Removed', 'Renamed', or 'Updated')"),
			*EventFilter);
		return FString();
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterEditorAssetEvent")))
	{
		return FString();
	}
	Record.Scope = TEXT("editor");
	Record.TriggerType = ETetherTrigger::AssetEvent;
	Record.Subject = TWeakObjectPtr<UObject>();   // global
	Record.Selector = Selector;
	Record.RegistrationContext.Add(TEXT("event_filter"), EventFilter);
	return Sub->RegisterHandler(MoveTemp(Record));
}

FString UTetherReactiveLibrary::RegisterEditorPieEvent(
	const FString& TaskName, const FString& Description,
	const FString& PhaseFilter, const FString& Script,
	const FString& ScriptPath, const TArray<FString>& Tags,
	const FString& Lifetime, const FString& ErrorPolicy, int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub) return FString();

	FName Selector;
	if (!TetherReactiveLibImpl::ValidatePieEventFilter(PhaseFilter, Selector))
	{
		UE_LOG(LogTetherReactiveLib, Warning,
			TEXT("RegisterEditorPieEvent: unknown PhaseFilter '%s' (expected '', or one of PreBeginPIE/BeginPIE/PostPIEStarted/PrePIEEnded/EndPIE/PausePIE/ResumePIE/SingleStepPIE)"),
			*PhaseFilter);
		return FString();
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterEditorPieEvent")))
	{
		return FString();
	}
	Record.Scope = TEXT("editor");
	Record.TriggerType = ETetherTrigger::PieEvent;
	Record.Subject = TWeakObjectPtr<UObject>();
	Record.Selector = Selector;
	Record.RegistrationContext.Add(TEXT("phase_filter"), PhaseFilter);
	return Sub->RegisterHandler(MoveTemp(Record));
}

FString UTetherReactiveLibrary::RegisterEditorBpCompiled(
	const FString& TaskName, const FString& Description,
	const FString& BlueprintPathFilter, const FString& Script,
	const FString& ScriptPath, const TArray<FString>& Tags,
	const FString& Lifetime, const FString& ErrorPolicy, int32 ThrottleMs)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	if (!Sub) return FString();

	TWeakObjectPtr<UObject> Subject;   // default: explicit null → global mode
	if (!BlueprintPathFilter.IsEmpty())
	{
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPathFilter);
		if (!BP)
		{
			UE_LOG(LogTetherReactiveLib, Warning,
				TEXT("RegisterEditorBpCompiled: blueprint '%s' did not load"),
				*BlueprintPathFilter);
			return FString();
		}
		Subject = BP;
	}

	FTetherHandlerRecord Record;
	if (!TetherReactiveLibImpl::FillCommonRecordFields(Record, TaskName, Description,
		Script, ScriptPath, Tags, Lifetime, ErrorPolicy, ThrottleMs,
		TEXT("RegisterEditorBpCompiled")))
	{
		return FString();
	}
	Record.Scope = TEXT("editor");
	Record.TriggerType = ETetherTrigger::BpCompiled;
	Record.Subject = Subject;
	Record.Selector = NAME_None;
	// Record the registration intent ("global" vs per-subject) so the adapter
	// can route OnHandlerRemoved correctly even after the subject BP is GC'd —
	// matching the GameplayEvent adapter's existing convention.
	Record.AdapterPayload = BlueprintPathFilter.IsEmpty() ? TEXT("global") : TEXT("per_subject");
	Record.RegistrationContext.Add(TEXT("blueprint_path_filter"), BlueprintPathFilter);
	return Sub->RegisterHandler(MoveTemp(Record));
}

// ────────────────────────────────────────────────────────────────

bool UTetherReactiveLibrary::Unregister(const FString& HandlerId)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	return Sub ? Sub->UnregisterHandler(HandlerId) : false;
}

TArray<FTetherHandlerSummary> UTetherReactiveLibrary::ListAllHandlers(
	const FString& FilterScope,
	const FString& FilterTriggerType,
	const FString& FilterTag)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	return Sub ? Sub->ListAllHandlers(FilterScope, FilterTriggerType, FilterTag)
	           : TArray<FTetherHandlerSummary>();
}

FTetherHandlerDetail UTetherReactiveLibrary::GetHandler(const FString& HandlerId)
{
	FTetherHandlerDetail Detail;
	if (UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get())
	{
		Sub->GetHandler(HandlerId, Detail);
	}
	return Detail;
}

FTetherHandlerStats UTetherReactiveLibrary::GetHandlerStats(const FString& HandlerId)
{
	FTetherHandlerStats Stats;
	if (UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get())
	{
		Sub->GetStats(HandlerId, Stats);
	}
	return Stats;
}

bool UTetherReactiveLibrary::Pause(const FString& HandlerId)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	return Sub ? Sub->PauseHandler(HandlerId) : false;
}

bool UTetherReactiveLibrary::Resume(const FString& HandlerId)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	return Sub ? Sub->ResumeHandler(HandlerId) : false;
}

int32 UTetherReactiveLibrary::ClearAll(const FString& Scope)
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	return Sub ? Sub->ClearHandlers(Scope) : 0;
}

void UTetherReactiveLibrary::DeferToNextTick(const FString& Script)
{
	if (UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get())
	{
		Sub->DeferToNextTick(Script);
	}
}

TMap<FString, FString> UTetherReactiveLibrary::DescribeTriggerContext(const FString& TriggerType)
{
	if (UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get())
	{
		return Sub->DescribeTriggerContext(TriggerType);
	}
	return TMap<FString, FString>();
}

// ─── Persistence (P6.B3) ─────────────────────────────────────────

bool UTetherReactiveLibrary::SaveAllHandlers()
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	return Sub ? Sub->SaveAllHandlers() : false;
}

int32 UTetherReactiveLibrary::LoadAllHandlers()
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	return Sub ? Sub->LoadAllHandlers() : 0;
}

FString UTetherReactiveLibrary::GetPersistencePath()
{
	return UTetherReactiveSubsystem::GetPersistencePath();
}

int32 UTetherReactiveLibrary::GetDeferredHandlerCount()
{
	UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
	return Sub ? Sub->GetDeferredHandlerCount() : 0;
}

bool UTetherReactiveLibrary::ResolveForRestore(FTetherHandlerRecord& Record)
{
	// Returns false when subject resolution fails in a way the caller should
	// defer (e.g. PIE-tied actor not yet spawned). Returns false permanently
	// for bad data (missing required params) — caller treats that same as a
	// deferred resolution attempt; a subsequent save will prune it.

	using namespace TetherReactiveLibImpl;
	using namespace TetherReactiveUtil;
	const TMap<FString, FString>& Ctx = Record.RegistrationContext;

	auto GetOrEmpty = [&Ctx](const TCHAR* Key) -> FString
	{
		if (const FString* V = Ctx.Find(Key)) return *V;
		return FString();
	};

	switch (Record.TriggerType)
	{
	case ETetherTrigger::GameplayEvent:
	{
		const FString Target = GetOrEmpty(TEXT("target_actor_name"));
		const FString Tag    = GetOrEmpty(TEXT("event_tag"));
		if (Tag.IsEmpty()) return false;
		Record.Selector = FName(*Tag);
		if (Target.IsEmpty())
		{
			// Global — no subject needed.
			Record.Subject = TWeakObjectPtr<UObject>();
			Record.AdapterPayload = TEXT("global");
			return true;
		}
		AActor* Actor = FindActorByName(Target);
		if (!Actor) return false;
		UAbilitySystemComponent* ASC = ResolveActorASC(Actor);
		if (!ASC) return false;
		Record.Subject = TWeakObjectPtr<UObject>(ASC);
		Record.AdapterPayload = FString();
		return true;
	}
	case ETetherTrigger::AttributeChanged:
	{
		const FString Target = GetOrEmpty(TEXT("target_actor_name"));
		const FString Attr   = GetOrEmpty(TEXT("attribute_name"));
		if (Target.IsEmpty() || Attr.IsEmpty()) return false;
		AActor* Actor = FindActorByName(Target);
		if (!Actor) return false;
		UAbilitySystemComponent* ASC = ResolveActorASC(Actor);
		if (!ASC) return false;
		Record.Subject = TWeakObjectPtr<UObject>(ASC);
		Record.Selector = FName(*Attr);
		return true;
	}
	case ETetherTrigger::ActorLifecycle:
	{
		const FString Target = GetOrEmpty(TEXT("target_actor_name"));
		const FString Event  = GetOrEmpty(TEXT("event_type"));
		if (Target.IsEmpty() || Event.IsEmpty()) return false;
		AActor* Actor = FindActorByName(Target);
		if (!Actor) return false;
		Record.Subject = TWeakObjectPtr<UObject>(Actor);
		Record.Selector = FName(Event.Equals(TEXT("Destroyed"), ESearchCase::IgnoreCase)
			? TEXT("Destroyed") : TEXT("EndPlay"));
		return true;
	}
	case ETetherTrigger::MovementModeChanged:
	{
		const FString Target = GetOrEmpty(TEXT("target_actor_name"));
		if (Target.IsEmpty()) return false;
		AActor* Actor = FindActorByName(Target);
		if (!Actor) return false;
		ACharacter* Char = Cast<ACharacter>(Actor);
		if (!Char) return false;
		Record.Subject = TWeakObjectPtr<UObject>(Char);
		Record.Selector = NAME_None;
		return true;
	}
	case ETetherTrigger::AnimNotify:
	{
		const FString Target = GetOrEmpty(TEXT("target_actor_name"));
		const FString Notify = GetOrEmpty(TEXT("notify_name"));
		if (Target.IsEmpty() || Notify.IsEmpty()) return false;
		AActor* Actor = FindActorByName(Target);
		if (!Actor) return false;
		USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
		if (!Mesh) return false;
		UAnimInstance* AI = Mesh->GetAnimInstance();
		if (!AI) return false;
		Record.Subject = TWeakObjectPtr<UObject>(AI);
		Record.Selector = FName(*Notify);
		return true;
	}
	case ETetherTrigger::InputAction:
	{
		const FString Target   = GetOrEmpty(TEXT("target_actor_name"));
		const FString IAPath   = GetOrEmpty(TEXT("input_action_path"));
		const FString Trigger  = GetOrEmpty(TEXT("trigger_event"));
		if (Target.IsEmpty() || IAPath.IsEmpty() || Trigger.IsEmpty()) return false;
		// Re-canonicalize the trigger event name: pre-fix registrations (and
		// hand-edited JSON) may have stored the caller's raw casing, which
		// never matches the adapter's case-sensitive parse or Dispatch's
		// exact Selector comparison.
		const TCHAR* Canonical = *Trigger;
		static const TCHAR* kEvents[] = { TEXT("Triggered"), TEXT("Started"), TEXT("Ongoing"),
			TEXT("Canceled"), TEXT("Completed") };
		for (const TCHAR* K : kEvents)
		{
			if (Trigger.Equals(K, ESearchCase::IgnoreCase))
			{
				Canonical = K;
				break;
			}
		}
		AActor* Actor = FindActorByName(Target);
		if (!Actor) return false;
		UEnhancedInputComponent* Comp = ResolveInputComponent(Actor);
		if (!Comp) return false;
		UInputAction* IA = Cast<UInputAction>(StaticLoadObject(
			UInputAction::StaticClass(), nullptr, *IAPath));
		if (!IA) return false;
		Record.Subject = TWeakObjectPtr<UObject>(Comp);
		Record.Selector = FName(*FString::Printf(TEXT("%s:%s"), *IA->GetName(), Canonical));
		Record.AdapterPayload = IAPath;
		return true;
	}
	case ETetherTrigger::Timer:
	{
		const FString Interval = GetOrEmpty(TEXT("interval_seconds"));
		if (Interval.IsEmpty()) return false;
		const double Secs = FCString::Atod(*Interval);
		if (!(Secs > 0.0)) return false;
		Record.Subject = TWeakObjectPtr<UObject>();
		Record.Selector = NAME_None;
		Record.AdapterPayload = Interval;
		return true;
	}
	case ETetherTrigger::AssetEvent:
	{
		const FString Filter = GetOrEmpty(TEXT("event_filter"));
		Record.Subject = TWeakObjectPtr<UObject>();
		Record.Selector = Filter.IsEmpty() ? NAME_None : FName(*Filter);
		return true;
	}
	case ETetherTrigger::PieEvent:
	{
		const FString Filter = GetOrEmpty(TEXT("phase_filter"));
		Record.Subject = TWeakObjectPtr<UObject>();
		Record.Selector = Filter.IsEmpty() ? NAME_None : FName(*Filter);
		return true;
	}
	case ETetherTrigger::BpCompiled:
	{
		const FString Path = GetOrEmpty(TEXT("blueprint_path_filter"));
		if (Path.IsEmpty())
		{
			// Global mode — leave Subject explicit-null.
			Record.Subject = TWeakObjectPtr<UObject>();
			Record.Selector = NAME_None;
			Record.AdapterPayload = TEXT("global");
			return true;
		}
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Path);
		if (!BP) return false;
		Record.Subject = TWeakObjectPtr<UObject>(BP);
		Record.Selector = NAME_None;
		Record.AdapterPayload = TEXT("per_subject");
		return true;
	}
	default:
		return false;
	}
}
