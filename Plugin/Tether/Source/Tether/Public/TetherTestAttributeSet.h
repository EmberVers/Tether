#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "TetherTestAttributeSet.generated.h"

/**
 * Minimal UAttributeSet used by the reactive framework smoke tests to
 * exercise the AttributeChanged adapter end-to-end. Not intended for
 * gameplay — the project should ship its own UAttributeSet subclasses.
 *
 * Health and Mana are plain FGameplayAttributeData fields so
 * FTetherAttributeChangedAdapter's field-name walker picks them up
 * without needing gameplay-effect plumbing.
 */
UCLASS()
class TETHER_API UTetherTestAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "TetherTest")
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "TetherTest")
	FGameplayAttributeData Mana;
};
