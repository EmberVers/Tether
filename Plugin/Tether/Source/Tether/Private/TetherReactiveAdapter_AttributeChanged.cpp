#include "TetherReactiveShared.h"
#include "TetherReactiveAdapter.h"
#include "TetherReactiveSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherReactiveAttr, Log, All);

// Shared attribute resolver + Python-literal helpers live in
// TetherReactiveShared.h (namespace TetherReactiveUtil).

/**
 * Binds to UAbilitySystemComponent::GetGameplayAttributeValueChangeDelegate(Attr).
 * Non-dynamic multicast delegate — AddLambda works directly, no listener UObject
 * required. One binding per (ASC, Attribute) shared across handlers.
 */
class FTetherAttributeChangedAdapter : public ITetherReactiveAdapter
{
public:
	virtual ETetherTrigger GetTriggerType() const override { return ETetherTrigger::AttributeChanged; }
	virtual FString GetTriggerName() const override { return TEXT("AttributeChanged"); }

	virtual void OnHandlerAdded(const FTetherHandlerRecord& Record) override
	{
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC || Record.Selector.IsNone())
		{
			UE_LOG(LogTetherReactiveAttr, Warning,
				TEXT("OnHandlerAdded %s: Subject must be an ASC and Selector (attribute name) required"),
				*Record.HandlerId);
			return;
		}

		const FString AttrString = Record.Selector.ToString();
		const FGameplayAttribute Attr = TetherReactiveUtil::ResolveAttribute(ASC, AttrString);
		// The library entry point pre-checks this, but restore paths can reach
		// the adapter with a stale ASC state — guard the delegate bind too.
		if (!Attr.IsValid())
		{
			UE_LOG(LogTetherReactiveAttr, Warning,
				TEXT("OnHandlerAdded %s: attribute '%s' not found on ASC's spawned sets"),
				*Record.HandlerId, *AttrString);
			return;
		}

		for (FBinding& B : Bindings)
		{
			if (B.ASC.Get() == ASC && B.AttrName == Record.Selector)
			{
				B.HandlerCount += 1;
				return;
			}
		}

		FBinding NB;
		NB.ASC = ASC;
		NB.AttrName = Record.Selector;
		NB.Attribute = Attr;
		NB.HandlerCount = 1;

		TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;
		const FName AttrFName = Record.Selector;
		const FString AttrStrCapture = AttrString;

		NB.Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attr).AddLambda(
			[WeakASC, AttrFName, AttrStrCapture](const FOnAttributeChangeData& Data)
			{
				UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
				if (!Sub) return;
				TMap<FString, FString> Ctx;
				Ctx.Add(TEXT("trigger"),        TEXT("'attribute_changed'"));
				Ctx.Add(TEXT("attribute_name"), FString::Printf(TEXT("'%s'"),
					*TetherReactiveUtil::EscapePythonStringLiteral(AttrStrCapture)));
				Ctx.Add(TEXT("new_value"),      FString::Printf(TEXT("%f"), Data.NewValue));
				Ctx.Add(TEXT("old_value"),      FString::Printf(TEXT("%f"), Data.OldValue));
				Ctx.Add(TEXT("delta"),          FString::Printf(TEXT("%f"), Data.NewValue - Data.OldValue));
				Sub->Dispatch(
					ETetherTrigger::AttributeChanged,
					TWeakObjectPtr<UObject>(WeakASC.Get()),
					AttrFName,
					Ctx);
			});

		Bindings.Add(NB);
	}

	virtual void OnHandlerRemoved(const FTetherHandlerRecord& Record) override
	{
		UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Record.Subject.Get());
		if (!ASC) return;
		for (int32 i = 0; i < Bindings.Num(); ++i)
		{
			FBinding& B = Bindings[i];
			if (B.ASC.Get() == ASC && B.AttrName == Record.Selector)
			{
				B.HandlerCount -= 1;
				if (B.HandlerCount <= 0)
				{
					if (B.ASC.IsValid())
					{
						B.ASC->GetGameplayAttributeValueChangeDelegate(B.Attribute).Remove(B.Handle);
					}
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
			if (B.ASC.IsValid())
			{
				B.ASC->GetGameplayAttributeValueChangeDelegate(B.Attribute).Remove(B.Handle);
			}
		}
		Bindings.Reset();
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),        TEXT("str — always 'attribute_changed'"));
		D.Add(TEXT("attribute_name"), TEXT("str — 'AttrSet.Field' or the bare 'Field'"));
		D.Add(TEXT("new_value"),      TEXT("float — post-change numeric value"));
		D.Add(TEXT("old_value"),      TEXT("float — pre-change numeric value"));
		D.Add(TEXT("delta"),          TEXT("float — new_value - old_value"));
		return D;
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FName AttrName;
		FGameplayAttribute Attribute;
		FDelegateHandle Handle;
		int32 HandlerCount = 0;
	};
	TArray<FBinding> Bindings;
};

namespace TetherReactiveAdapters
{
	TUniquePtr<ITetherReactiveAdapter> MakeAttributeChangedAdapter()
	{
		return MakeUnique<FTetherAttributeChangedAdapter>();
	}
}
