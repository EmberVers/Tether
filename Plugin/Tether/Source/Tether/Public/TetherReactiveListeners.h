#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "TetherReactiveListeners.generated.h"

/**
 * UObject receivers for UE dynamic delegates the reactive adapters bind to.
 * Dynamic delegates (`DECLARE_DYNAMIC_MULTICAST_DELEGATE*`) can only invoke
 * UFUNCTIONs, not lambdas — so each adapter that binds a dynamic delegate
 * owns one or more listener instances declared here. Each listener keeps a
 * raw back-pointer to its plain-C++ adapter; the adapter owns the listener
 * via TStrongObjectPtr and clears the back-pointer before destruction.
 */

class FTetherMovementModeAdapter;
class FTetherAnimNotifyAdapter;
class FTetherActorLifecycleAdapter;
class FTetherInputActionAdapter;
class ACharacter;
class USkeletalMeshComponent;
class UEnhancedInputComponent;
class UInputAction;
struct FBranchingPointNotifyPayload;
struct FInputActionInstance;

UCLASS()
class TETHER_API UTetherMovementModeListener : public UObject
{
	GENERATED_BODY()
public:
	FTetherMovementModeAdapter* Adapter = nullptr;
	TWeakObjectPtr<ACharacter> BoundCharacter;

	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMode, uint8 PrevCustomMode);
};

UCLASS()
class TETHER_API UTetherAnimNotifyListener : public UObject
{
	GENERATED_BODY()
public:
	FTetherAnimNotifyAdapter* Adapter = nullptr;
	TWeakObjectPtr<USkeletalMeshComponent> BoundMesh;

	UFUNCTION()
	void OnNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);
};

UCLASS()
class TETHER_API UTetherActorLifecycleListener : public UObject
{
	GENERATED_BODY()
public:
	FTetherActorLifecycleAdapter* Adapter = nullptr;
	TWeakObjectPtr<AActor> BoundActor;

	UFUNCTION()
	void OnActorDestroyed(AActor* Actor);

	UFUNCTION()
	void OnActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
};

UCLASS()
class TETHER_API UTetherInputActionListener : public UObject
{
	GENERATED_BODY()
public:
	FTetherInputActionAdapter* Adapter = nullptr;
	TWeakObjectPtr<UEnhancedInputComponent> BoundComp;
	TWeakObjectPtr<UInputAction> BoundAction;
	/** ETriggerEvent cast to uint8 — avoid EnhancedInput include in this header. */
	uint8 TriggerEventValue = 0;

	UFUNCTION()
	void OnActionFired(const FInputActionInstance& Instance);
};
