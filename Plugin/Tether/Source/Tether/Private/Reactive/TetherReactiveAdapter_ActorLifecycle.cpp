#include "TetherReactiveAdapter.h"
#include "TetherReactiveSubsystem.h"
#include "TetherReactiveListeners.h"
#include "Reactive/TetherReactiveShared.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherReactiveLife, Log, All);

namespace TetherReactiveAdapterImpl_Lifecycle
{
	FString EndPlayReasonName(EEndPlayReason::Type R)
	{
		switch (R)
		{
		case EEndPlayReason::Destroyed:           return TEXT("Destroyed");
		case EEndPlayReason::LevelTransition:     return TEXT("LevelTransition");
		case EEndPlayReason::EndPlayInEditor:     return TEXT("EndPlayInEditor");
		case EEndPlayReason::RemovedFromWorld:    return TEXT("RemovedFromWorld");
		case EEndPlayReason::Quit:                return TEXT("Quit");
		default:                                  return TEXT("Unknown");
		}
	}

	const FName EventDestroyed(TEXT("Destroyed"));
	const FName EventEndPlay(TEXT("EndPlay"));
}

/**
 * Binds AActor::OnDestroyed and AActor::OnEndPlay (both dynamic delegates).
 * One adapter handles both; selector encodes the event name. One listener
 * UObject per binding so unbinding is precise.
 */
class FTetherActorLifecycleAdapter : public ITetherReactiveAdapter
{
public:
	virtual ETetherTrigger GetTriggerType() const override { return ETetherTrigger::ActorLifecycle; }
	virtual FString GetTriggerName() const override { return TEXT("ActorLifecycle"); }

	virtual void OnHandlerAdded(const FTetherHandlerRecord& Record) override
	{
		AActor* Actor = Cast<AActor>(Record.Subject.Get());
		if (!Actor)
		{
			UE_LOG(LogTetherReactiveLife, Warning,
				TEXT("OnHandlerAdded %s: Subject is not an AActor"), *Record.HandlerId);
			return;
		}
		if (Record.Selector != TetherReactiveAdapterImpl_Lifecycle::EventDestroyed &&
			Record.Selector != TetherReactiveAdapterImpl_Lifecycle::EventEndPlay)
		{
			UE_LOG(LogTetherReactiveLife, Warning,
				TEXT("OnHandlerAdded %s: Selector must be 'Destroyed' or 'EndPlay', got '%s'"),
				*Record.HandlerId, *Record.Selector.ToString());
			return;
		}

		for (FBinding& B : Bindings)
		{
			if (B.Actor.Get() == Actor && B.Event == Record.Selector)
			{
				B.HandlerCount += 1;
				return;
			}
		}

		FBinding NB;
		NB.Actor = Actor;
		NB.Event = Record.Selector;
		NB.HandlerCount = 1;
		NB.Listener.Reset(NewObject<UTetherActorLifecycleListener>());
		NB.Listener->Adapter = this;
		NB.Listener->BoundActor = Actor;

		if (Record.Selector == TetherReactiveAdapterImpl_Lifecycle::EventDestroyed)
		{
			Actor->OnDestroyed.AddDynamic(NB.Listener.Get(),
				&UTetherActorLifecycleListener::OnActorDestroyed);
		}
		else
		{
			Actor->OnEndPlay.AddDynamic(NB.Listener.Get(),
				&UTetherActorLifecycleListener::OnActorEndPlay);
		}

		Bindings.Add(MoveTemp(NB));
	}

	virtual void OnHandlerRemoved(const FTetherHandlerRecord& Record) override
	{
		AActor* Actor = Cast<AActor>(Record.Subject.Get());
		if (!Actor)
		{
			// Dead subject (GC'd since registration): the binding's own weak
			// pointer is stale too, so there is nothing left to unbind. Treat
			// as "binding not found" instead of matching null==null below,
			// which would decrement the wrong binding.
			return;
		}
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.Actor.Get() == Actor && B.Event == Record.Selector)
			{
				B.HandlerCount -= 1;
				if (B.HandlerCount <= 0)
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
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),         TEXT("str — always 'actor_lifecycle'"));
		D.Add(TEXT("event"),           TEXT("str — 'Destroyed' or 'EndPlay'"));
		D.Add(TEXT("actor"),           TEXT("unreal.Actor"));
		D.Add(TEXT("end_play_reason"), TEXT("str — EEndPlayReason name; '' for Destroyed"));
		return D;
	}

	void HandleDestroyed(AActor* Actor)
	{
		FireDispatch(Actor, TetherReactiveAdapterImpl_Lifecycle::EventDestroyed, FString());
	}

	void HandleEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
	{
		FireDispatch(Actor, TetherReactiveAdapterImpl_Lifecycle::EventEndPlay,
			TetherReactiveAdapterImpl_Lifecycle::EndPlayReasonName(Reason));
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<AActor> Actor;
		FName Event;
		TStrongObjectPtr<UTetherActorLifecycleListener> Listener;
		int32 HandlerCount = 0;
	};

	void UnbindOne(FBinding& B)
	{
		if (B.Actor.IsValid() && B.Listener.IsValid())
		{
			if (B.Event == TetherReactiveAdapterImpl_Lifecycle::EventDestroyed)
			{
				B.Actor->OnDestroyed.RemoveDynamic(B.Listener.Get(),
					&UTetherActorLifecycleListener::OnActorDestroyed);
			}
			else if (B.Event == TetherReactiveAdapterImpl_Lifecycle::EventEndPlay)
			{
				B.Actor->OnEndPlay.RemoveDynamic(B.Listener.Get(),
					&UTetherActorLifecycleListener::OnActorEndPlay);
			}
		}
		if (B.Listener.IsValid())
		{
			B.Listener->Adapter = nullptr;
		}
	}

	void FireDispatch(AActor* Actor, FName Event, const FString& ReasonName)
	{
		UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
		if (!Sub || !Actor) return;
		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"), TEXT("'actor_lifecycle'"));
		Ctx.Add(TEXT("event"),   FString::Printf(TEXT("'%s'"), *Event.ToString()));
		Ctx.Add(TEXT("actor"),   TetherReactiveUtil::RenderPyObjectLiteral(Actor));
		Ctx.Add(TEXT("end_play_reason"),
			FString::Printf(TEXT("'%s'"),
				*TetherReactiveUtil::EscapePythonStringLiteral(ReasonName)));
		Sub->Dispatch(ETetherTrigger::ActorLifecycle, TWeakObjectPtr<UObject>(Actor), Event, Ctx);
	}

	TArray<FBinding> Bindings;
};

void UTetherActorLifecycleListener::OnActorDestroyed(AActor* Actor)
{
	if (Adapter) Adapter->HandleDestroyed(Actor);
}

void UTetherActorLifecycleListener::OnActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	if (Adapter) Adapter->HandleEndPlay(Actor, Reason);
}

namespace TetherReactiveAdapters
{
	TUniquePtr<ITetherReactiveAdapter> MakeActorLifecycleAdapter()
	{
		return MakeUnique<FTetherActorLifecycleAdapter>();
	}
}
