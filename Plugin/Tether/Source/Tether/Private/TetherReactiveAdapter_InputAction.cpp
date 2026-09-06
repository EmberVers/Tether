#include "TetherReactiveAdapter.h"
#include "TetherReactiveSubsystem.h"
#include "TetherReactiveListeners.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherReactiveInput, Log, All);

namespace TetherReactiveAdapterImpl_Input
{
	FString EscapeSingleQuoted(const FString& In)
	{
		FString Out; Out.Reserve(In.Len() + 2);
		for (TCHAR C : In)
		{
			if (C == TEXT('\\') || C == TEXT('\'')) Out.AppendChar(TEXT('\\'));
			Out.AppendChar(C);
		}
		return Out;
	}

	FString TriggerEventName(ETriggerEvent E)
	{
		switch (E)
		{
		case ETriggerEvent::Triggered: return TEXT("Triggered");
		case ETriggerEvent::Started:   return TEXT("Started");
		case ETriggerEvent::Ongoing:   return TEXT("Ongoing");
		case ETriggerEvent::Canceled:  return TEXT("Canceled");
		case ETriggerEvent::Completed: return TEXT("Completed");
		case ETriggerEvent::None:      return TEXT("None");
		default:                       return TEXT("Unknown");
		}
	}

	/**
	 * Case-sensitive parse of a trigger-event name (Selector's tail segment).
	 * The library entry point canonicalizes casing before it ever reaches the
	 * Selector, so an unmatched name here means corrupted data — the caller
	 * warns and bails rather than silently falling back to another event.
	 */
	bool ParseTriggerEvent(const FString& EventName, ETriggerEvent& Out)
	{
		if      (EventName == TEXT("Triggered")) { Out = ETriggerEvent::Triggered; return true; }
		else if (EventName == TEXT("Started"))   { Out = ETriggerEvent::Started;   return true; }
		else if (EventName == TEXT("Ongoing"))   { Out = ETriggerEvent::Ongoing;   return true; }
		else if (EventName == TEXT("Canceled"))  { Out = ETriggerEvent::Canceled;  return true; }
		else if (EventName == TEXT("Completed")) { Out = ETriggerEvent::Completed; return true; }
		return false;
	}
}

/**
 * Binds UEnhancedInputComponent::BindAction(Action, TriggerEvent, listener,
 * &OnActionFired). One listener per (InputComp, IA, TriggerEvent) so the
 * binding handle is unambiguous on unbind. Selector encodes
 * "<IAName>:<TriggerEvent>" so handlers are matched precisely.
 */
class FTetherInputActionAdapter : public ITetherReactiveAdapter
{
public:
	virtual ETetherTrigger GetTriggerType() const override { return ETetherTrigger::InputAction; }

	virtual void OnHandlerAdded(const FTetherHandlerRecord& Record) override
	{
		UEnhancedInputComponent* Comp = Cast<UEnhancedInputComponent>(Record.Subject.Get());
		if (!Comp)
		{
			UE_LOG(LogTetherReactiveInput, Warning,
				TEXT("OnHandlerAdded %s: Subject is not a UEnhancedInputComponent"), *Record.HandlerId);
			return;
		}
		if (Record.Selector.IsNone())
		{
			UE_LOG(LogTetherReactiveInput, Warning,
				TEXT("OnHandlerAdded %s: Selector required ('IAName:TriggerEvent')"), *Record.HandlerId);
			return;
		}

		// Selector format: "<IAName>:<TriggerEvent>". AdapterPayload carries the full IA asset path.
		FString IAName, EventName;
		if (!Record.Selector.ToString().Split(TEXT(":"), &IAName, &EventName))
		{
			UE_LOG(LogTetherReactiveInput, Warning,
				TEXT("OnHandlerAdded %s: bad Selector '%s'"), *Record.HandlerId, *Record.Selector.ToString());
			return;
		}
		ETriggerEvent Event;
		if (!TetherReactiveAdapterImpl_Input::ParseTriggerEvent(EventName, Event))
		{
			// Casing is canonicalized at registration, so this is corrupt data.
			// No silent fallback — a defaulted event would silently fire the
			// handler at the wrong time.
			UE_LOG(LogTetherReactiveInput, Warning,
				TEXT("OnHandlerAdded %s: unknown trigger event '%s' in Selector '%s'"),
				*Record.HandlerId, *EventName, *Record.Selector.ToString());
			return;
		}
		UInputAction* IA = Cast<UInputAction>(StaticLoadObject(
			UInputAction::StaticClass(), nullptr, *Record.AdapterPayload));
		if (!IA)
		{
			UE_LOG(LogTetherReactiveInput, Warning,
				TEXT("OnHandlerAdded %s: UInputAction '%s' failed to load"),
				*Record.HandlerId, *Record.AdapterPayload);
			return;
		}

		const FName NewIAName(*IA->GetName());
		for (FBinding& B : Bindings)
		{
			// Dedup by (Comp, IAName, Event) rather than by the UInputAction
			// pointer: the previously bound IA asset may have been GC'd since
			// (B.Action stale) while a new IA object was loaded for the same
			// asset path. Pointer-only matching used to create a second
			// binding whose unregister then decremented the FIRST (stale)
			// entry, leaving the live one bound forever.
			if (B.Comp.Get() == Comp && B.IAName == NewIAName && B.Event == Event)
			{
				if (B.Action.Get() != IA)
				{
					// Same (Comp, IAName, Event) but a different IA object —
					// the old delegate binding is dead (its IA was GC'd);
					// rebind against the freshly loaded IA.
					UnbindOne(B);
					BindOne(B, IA, Event);
				}
				B.HandlerCount += 1;
				return;
			}
		}

		FBinding NB;
		NB.Comp = Comp;
		NB.HandlerCount = 1;
		BindOne(NB, IA, Event);
		Bindings.Add(MoveTemp(NB));
	}

	virtual void OnHandlerRemoved(const FTetherHandlerRecord& Record) override
	{
		UEnhancedInputComponent* Comp = Cast<UEnhancedInputComponent>(Record.Subject.Get());
		FString IAName, EventName;
		if (!Record.Selector.ToString().Split(TEXT(":"), &IAName, &EventName))
		{
			UE_LOG(LogTetherReactiveInput, Warning,
				TEXT("OnHandlerRemoved %s: bad Selector '%s'"), *Record.HandlerId, *Record.Selector.ToString());
			return;
		}
		ETriggerEvent Event;
		if (!TetherReactiveAdapterImpl_Input::ParseTriggerEvent(EventName, Event))
		{
			// If the Selector never parsed at registration either, no binding
			// exists for it — nothing to remove.
			UE_LOG(LogTetherReactiveInput, Warning,
				TEXT("OnHandlerRemoved %s: unknown trigger event '%s' in Selector '%s'"),
				*Record.HandlerId, *EventName, *Record.Selector.ToString());
			return;
		}
		const FName IANameF(*IAName);

		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			// Match by the cached IAName (recorded at bind time) rather than
			// dereferencing B.Action — the UInputAction asset may have been
			// GC'd since binding, which used to strand the binding until
			// Shutdown because B.Action->GetName() was unreachable.
			if (B.Comp.Get() == Comp && B.Event == Event && B.IAName == IANameF)
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
		for (FBinding& B : Bindings) UnbindOne(B);
		Bindings.Reset();
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),         TEXT("str — always 'input_action'"));
		D.Add(TEXT("action_path"),     TEXT("str — the UInputAction's path name"));
		D.Add(TEXT("action_name"),     TEXT("str — short name (e.g. 'IA_Jump')"));
		D.Add(TEXT("trigger_event"),   TEXT("str — Triggered|Started|Ongoing|Canceled|Completed"));
		D.Add(TEXT("value_type"),      TEXT("str — Boolean|Axis1D|Axis2D|Axis3D"));
		D.Add(TEXT("value_bool"),      TEXT("bool — populated for Boolean actions"));
		D.Add(TEXT("value_axis1d"),    TEXT("float — populated for Axis1D"));
		D.Add(TEXT("value_axis2d_x"),  TEXT("float"));
		D.Add(TEXT("value_axis2d_y"),  TEXT("float"));
		D.Add(TEXT("value_axis3d_x"),  TEXT("float"));
		D.Add(TEXT("value_axis3d_y"),  TEXT("float"));
		D.Add(TEXT("value_axis3d_z"),  TEXT("float"));
		D.Add(TEXT("elapsed_seconds"), TEXT("float — Instance.GetElapsedTime()"));
		return D;
	}

	void HandleFire(UEnhancedInputComponent* Comp, const FInputActionInstance& Instance, const UInputAction* Action, ETriggerEvent Event)
	{
		UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
		if (!Sub || !Action) return;

		const FInputActionValue Value = Instance.GetValue();
		const EInputActionValueType VT = Value.GetValueType();

		auto VTName = [VT]() -> FString
		{
			switch (VT)
			{
			case EInputActionValueType::Boolean: return TEXT("Boolean");
			case EInputActionValueType::Axis1D:  return TEXT("Axis1D");
			case EInputActionValueType::Axis2D:  return TEXT("Axis2D");
			case EInputActionValueType::Axis3D:  return TEXT("Axis3D");
			default:                             return TEXT("Unknown");
			}
		};

		const FString IAName = Action->GetName();
		const FString IAPath = Action->GetPathName();
		const FString EventName = TetherReactiveAdapterImpl_Input::TriggerEventName(Event);
		const FName Selector(*FString::Printf(TEXT("%s:%s"), *IAName, *EventName));

		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"),       TEXT("'input_action'"));
		Ctx.Add(TEXT("action_path"),   FString::Printf(TEXT("'%s'"),
			*TetherReactiveAdapterImpl_Input::EscapeSingleQuoted(IAPath)));
		Ctx.Add(TEXT("action_name"),   FString::Printf(TEXT("'%s'"),
			*TetherReactiveAdapterImpl_Input::EscapeSingleQuoted(IAName)));
		Ctx.Add(TEXT("trigger_event"), FString::Printf(TEXT("'%s'"), *EventName));
		Ctx.Add(TEXT("value_type"),    FString::Printf(TEXT("'%s'"), *VTName()));
		Ctx.Add(TEXT("value_bool"),    Value.Get<bool>() ? TEXT("True") : TEXT("False"));
		const float V1 = (VT == EInputActionValueType::Axis1D) ? Value.Get<float>() : 0.f;
		Ctx.Add(TEXT("value_axis1d"),  FString::Printf(TEXT("%f"), V1));
		const FVector2D V2 = (VT == EInputActionValueType::Axis2D) ? Value.Get<FVector2D>() : FVector2D::ZeroVector;
		Ctx.Add(TEXT("value_axis2d_x"), FString::Printf(TEXT("%f"), V2.X));
		Ctx.Add(TEXT("value_axis2d_y"), FString::Printf(TEXT("%f"), V2.Y));
		const FVector V3 = (VT == EInputActionValueType::Axis3D) ? Value.Get<FVector>() : FVector::ZeroVector;
		Ctx.Add(TEXT("value_axis3d_x"), FString::Printf(TEXT("%f"), V3.X));
		Ctx.Add(TEXT("value_axis3d_y"), FString::Printf(TEXT("%f"), V3.Y));
		Ctx.Add(TEXT("value_axis3d_z"), FString::Printf(TEXT("%f"), V3.Z));
		Ctx.Add(TEXT("elapsed_seconds"), FString::Printf(TEXT("%f"), Instance.GetElapsedTime()));

		Sub->Dispatch(ETetherTrigger::InputAction,
			TWeakObjectPtr<UObject>(Comp), Selector, Ctx);
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UEnhancedInputComponent> Comp;
		TWeakObjectPtr<UInputAction> Action;
		/** Cached short name of the bound IA — outlives the asset for GC-safe unbind matching. */
		FName IAName;
		ETriggerEvent Event = ETriggerEvent::Triggered;
		uint32 BindingHandle = 0;
		TStrongObjectPtr<UTetherInputActionListener> Listener;
		int32 HandlerCount = 0;
	};

	void UnbindOne(FBinding& B)
	{
		if (B.Comp.IsValid())
		{
			B.Comp->RemoveBindingByHandle(B.BindingHandle);
		}
		if (B.Listener.IsValid())
		{
			B.Listener->Adapter = nullptr;
		}
	}

	/** (Re)bind a binding's EnhancedInput delegate against a live IA object. */
	void BindOne(FBinding& B, UInputAction* InIA, ETriggerEvent InEvent)
	{
		UEnhancedInputComponent* Comp = B.Comp.Get();
		if (!Comp || !InIA)
		{
			return;
		}
		B.Action = InIA;
		B.IAName = FName(*InIA->GetName());
		B.Event = InEvent;
		if (!B.Listener.IsValid())
		{
			B.Listener.Reset(NewObject<UTetherInputActionListener>());
			B.Listener->Adapter = this;
		}
		B.Listener->BoundComp = Comp;
		B.Listener->BoundAction = InIA;
		B.Listener->TriggerEventValue = static_cast<uint8>(InEvent);
		FEnhancedInputActionEventBinding& EBind = Comp->BindAction(
			InIA, InEvent, B.Listener.Get(),
			GET_FUNCTION_NAME_CHECKED(UTetherInputActionListener, OnActionFired));
		B.BindingHandle = EBind.GetHandle();
	}

	TArray<FBinding> Bindings;
};

void UTetherInputActionListener::OnActionFired(const FInputActionInstance& Instance)
{
	if (!Adapter) return;
	UEnhancedInputComponent* Comp = BoundComp.Get();
	UInputAction* IA = BoundAction.Get();
	const ETriggerEvent E = static_cast<ETriggerEvent>(TriggerEventValue);
	Adapter->HandleFire(Comp, Instance, IA, E);
}

namespace TetherReactiveAdapters
{
	TUniquePtr<ITetherReactiveAdapter> MakeInputActionAdapter()
	{
		return MakeUnique<FTetherInputActionAdapter>();
	}
}
