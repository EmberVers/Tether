#include "TetherReactiveAdapter.h"
#include "TetherReactiveSubsystem.h"
#include "TetherReactiveListeners.h"
#include "TetherReactiveShared.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherReactiveAnim, Log, All);

/**
 * Binds UAnimInstance::OnPlayMontageNotifyBegin (dynamic). The dynamic delegate
 * lives on UAnimInstance, not on USkeletalMeshComponent — so Subject is the
 * AnimInstance. One listener per AnimInstance; per-notify filtering happens at
 * dispatch time via Selector.
 */
class FTetherAnimNotifyAdapter : public ITetherReactiveAdapter
{
public:
	virtual ETetherTrigger GetTriggerType() const override { return ETetherTrigger::AnimNotify; }
	virtual FString GetTriggerName() const override { return TEXT("AnimNotify"); }

	virtual void OnHandlerAdded(const FTetherHandlerRecord& Record) override
	{
		UAnimInstance* AI = Cast<UAnimInstance>(Record.Subject.Get());
		if (!AI)
		{
			UE_LOG(LogTetherReactiveAnim, Warning,
				TEXT("OnHandlerAdded %s: Subject is not a UAnimInstance"), *Record.HandlerId);
			return;
		}

		for (FBinding& B : Bindings)
		{
			if (B.AnimInstance.Get() == AI)
			{
				B.HandlerCount += 1;
				return;
			}
		}

		FBinding NB;
		NB.AnimInstance = AI;
		NB.HandlerCount = 1;
		NB.Listener.Reset(NewObject<UTetherAnimNotifyListener>());
		NB.Listener->Adapter = this;
		// BoundMesh kept for diagnostics; not strictly required.
		NB.Listener->BoundMesh = AI->GetSkelMeshComponent();
		AI->OnPlayMontageNotifyBegin.AddDynamic(NB.Listener.Get(),
			&UTetherAnimNotifyListener::OnNotifyBegin);
		Bindings.Add(MoveTemp(NB));
	}

	virtual void OnHandlerRemoved(const FTetherHandlerRecord& Record) override
	{
		UAnimInstance* AI = Cast<UAnimInstance>(Record.Subject.Get());
		if (!AI)
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
			if (B.AnimInstance.Get() == AI)
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
		D.Add(TEXT("trigger"),        TEXT("str — always 'anim_notify'"));
		D.Add(TEXT("notify_name"),    TEXT("str — the FName of the notify that fired"));
		D.Add(TEXT("anim_instance"),  TEXT("unreal.AnimInstance"));
		D.Add(TEXT("mesh_component"), TEXT("unreal.SkeletalMeshComponent | None"));
		D.Add(TEXT("owner_actor"),    TEXT("unreal.Actor | None — the SkelMesh's owner"));
		D.Add(TEXT("montage"),        TEXT("unreal.AnimMontage | None — non-null when notify fires from a montage"));
		D.Add(TEXT("source_asset"),   TEXT("unreal.AnimSequenceBase | None — raw SequenceAsset from the payload"));
		return D;
	}

	void HandleNotify(UAnimInstance* AI, FName NotifyName, UAnimSequenceBase* SourceAsset)
	{
		UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
		if (!Sub || !AI) return;

		USkeletalMeshComponent* Mesh = AI->GetSkelMeshComponent();
		AActor* OwnerActor = Mesh ? Mesh->GetOwner() : nullptr;
		UAnimMontage* Montage = Cast<UAnimMontage>(SourceAsset);

		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"),        TEXT("'anim_notify'"));
		Ctx.Add(TEXT("notify_name"),    FString::Printf(TEXT("'%s'"),
			*TetherReactiveUtil::EscapePythonStringLiteral(NotifyName.ToString())));
		Ctx.Add(TEXT("anim_instance"),  TetherReactiveUtil::RenderPyObjectLiteral(AI));
		Ctx.Add(TEXT("mesh_component"), TetherReactiveUtil::RenderPyObjectLiteral(Mesh));
		Ctx.Add(TEXT("owner_actor"),    TetherReactiveUtil::RenderPyObjectLiteral(OwnerActor));
		Ctx.Add(TEXT("montage"),        TetherReactiveUtil::RenderPyObjectLiteral(Montage));
		Ctx.Add(TEXT("source_asset"),   TetherReactiveUtil::RenderPyObjectLiteral(SourceAsset));

		Sub->Dispatch(ETetherTrigger::AnimNotify,
			TWeakObjectPtr<UObject>(AI), NotifyName, Ctx);
	}

private:
	struct FBinding
	{
		TWeakObjectPtr<UAnimInstance> AnimInstance;
		TStrongObjectPtr<UTetherAnimNotifyListener> Listener;
		int32 HandlerCount = 0;
	};

	void UnbindOne(FBinding& B)
	{
		if (B.AnimInstance.IsValid() && B.Listener.IsValid())
		{
			B.AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(B.Listener.Get(),
				&UTetherAnimNotifyListener::OnNotifyBegin);
		}
		if (B.Listener.IsValid())
		{
			B.Listener->Adapter = nullptr;
		}
	}

	TArray<FBinding> Bindings;
};

void UTetherAnimNotifyListener::OnNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	UAnimInstance* AI = nullptr;
	if (Payload.SkelMeshComponent)
	{
		AI = Payload.SkelMeshComponent->GetAnimInstance();
	}
	if (Adapter)
	{
		Adapter->HandleNotify(AI, NotifyName, Payload.SequenceAsset);
	}
}

namespace TetherReactiveAdapters
{
	TUniquePtr<ITetherReactiveAdapter> MakeAnimNotifyAdapter()
	{
		return MakeUnique<FTetherAnimNotifyAdapter>();
	}
}
