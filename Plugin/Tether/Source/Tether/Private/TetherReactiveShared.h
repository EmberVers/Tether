#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "UObject/UnrealType.h"

/**
 * Small helpers shared between TetherReactiveLibrary.cpp and the reactive
 * adapters. Header-only (inline) so each translation unit gets its own copy
 * without introducing a new .cpp to the module's file list.
 */
namespace TetherReactiveAdapterImpl_Attr
{
	/**
	 * Resolve FGameplayAttribute from "AttrSet.Field" or bare "Field" against
	 * an ASC's spawned sets. Returns an invalid FGameplayAttribute when the
	 * attribute does not exist on any spawned set (bad name, wrong set name,
	 * or no sets spawned).
	 *
	 * Used by both the library entry point (pre-registration validation, so a
	 * bad attribute name fails fast with an empty HandlerId instead of
	 * silently registering a handler that never fires) and the adapter
	 * (binding-time resolution on restore paths).
	 */
	inline FGameplayAttribute ResolveAttribute(UAbilitySystemComponent* ASC, const FString& Needle)
	{
		if (!ASC)
		{
			return FGameplayAttribute();
		}
		FString SetName, BareName;
		if (!Needle.Split(TEXT("."), &SetName, &BareName))
		{
			BareName = Needle;
		}
		for (const UAttributeSet* AS : ASC->GetSpawnedAttributes())
		{
			if (!AS) continue;
			if (!SetName.IsEmpty() && AS->GetClass()->GetName() != SetName) continue;
			for (TFieldIterator<FStructProperty> It(AS->GetClass()); It; ++It)
			{
				FStructProperty* P = *It;
				if (!P || P->Struct != FGameplayAttributeData::StaticStruct()) continue;
				if (P->GetName() == BareName)
				{
					return FGameplayAttribute(P);
				}
			}
		}
		return FGameplayAttribute();
	}
}
