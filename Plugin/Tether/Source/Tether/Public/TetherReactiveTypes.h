#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "TetherReactiveTypes.generated.h"

/** Which UE event source a handler binds to. */
UENUM(BlueprintType)
enum class ETetherTrigger : uint8
{
	None,
	GameplayEvent,        // UAbilitySystemComponent::GenericGameplayEventCallbacks (P2)
	AnimNotify,           // P3
	AttributeChanged,     // P3
	MovementModeChanged,  // P3
	InputAction,          // P3 (reactive, distinct from sticky-inject)
	ActorLifecycle,       // P3 (BeginPlay/EndPlay/Destroyed)
	Timer,                // P4
	AssetEvent,           // P5
	PieEvent,             // P5
	BpCompiled,           // P5
};

/** When the handler should auto-deregister. */
UENUM(BlueprintType)
enum class ETetherHandlerLifetime : uint8
{
	/** Never auto-deregister. */
	Permanent,
	/** Fire once, then auto-deregister. */
	Once,
	/** Fire N times, then auto-deregister (RemainingCalls counts down). */
	Count,
	/** Auto-deregister on PIE end. */
	WhilePIE,
	/** Auto-deregister when Subject becomes invalid. */
	WhileSubjectAlive,
};

/** What to do when the handler script raises an exception. */
UENUM(BlueprintType)
enum class ETetherErrorPolicy : uint8
{
	/** Log error, keep handler active. (default) */
	LogContinue,
	/** Log error, auto-deregister the handler. */
	LogUnregister,
	/** Log at Error severity (still keeps handler active). */
	Throw,
};

/** Per-handler runtime statistics, surfaced via GetHandlerStats. */
USTRUCT(BlueprintType)
struct FTetherHandlerStats
{
	GENERATED_BODY()

	/** Total times this handler was invoked. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive")
	int64 Calls = 0;

	/** Cumulative wall-time spent inside ExecPythonCommandEx, microseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive")
	int64 TotalMicroseconds = 0;

	/** Worst-case single invocation, microseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive")
	int64 MaxMicroseconds = 0;

	/** Number of invocations that returned bSuccess=false. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive")
	int64 ErrorCount = 0;

	/** Most recent error string (empty if none). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive")
	FString LastError;

	/**
	 * True when the handler was auto-paused by the Throw error policy after a
	 * failing invocation (script raised an exception). Distinct from a user
	 * Pause() call — this is Python-observable signal that the handler errored
	 * out and stopped firing until Resume().
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive")
	bool bPausedByErrorPolicy = false;

	/** UE world time (Editor world) of most recent fire. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive")
	double LastFireTimeSeconds = 0.0;
};

/** Compact summary for ListAllHandlers — does not include the script source. */
USTRUCT(BlueprintType)
struct FTetherHandlerSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString HandlerId;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString Scope;            // "runtime" or "editor"
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString TaskName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString Description;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString TriggerSummary;   // e.g. "GameplayEvent:Event.Combat.Hit"
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString SubjectPath;      // resolved path or "" for global / dead
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString ScriptPath;       // optional, agent-provided
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") TArray<FString> Tags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString Lifetime;         // ETetherHandlerLifetime as string
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") bool bPaused = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FTetherHandlerStats Stats;
};

/** Full record returned by GetHandler — includes script source. */
USTRUCT(BlueprintType)
struct FTetherHandlerDetail
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FTetherHandlerSummary Summary;

	/** Inline Python source the handler will execute. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString Script;

	/** ErrorPolicy as string. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString ErrorPolicy;

	/** Throttle interval in milliseconds (0 = unlimited). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") int32 ThrottleMs = 0;

	/** For Lifetime=Count: invocations remaining (-1 if not Count). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") int32 RemainingCalls = -1;

	/** ISO-8601 timestamp string of registration. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Reactive") FString CreatedAt;
};
