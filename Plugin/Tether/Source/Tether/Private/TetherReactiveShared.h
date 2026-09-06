#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TetherWorldSelection.h"
#include "UObject/UnrealType.h"

class AActor;
class ACharacter;
class AController;
class APlayerController;
class APlayerState;
class APawn;
class UWorld;

/**
 * Shared helpers for the whole reactive family: TetherReactiveLibrary.cpp,
 * TetherReactiveSubsystem.cpp and every TetherReactiveAdapter_*.cpp.
 * Header-only (inline) so each translation unit gets its own copy without
 * introducing a new .cpp to the module's file list. Previously named
 * TetherReactiveAdapterImpl_Attr and holding only ResolveAttribute; now the
 * single home for helpers that used to be copy-pasted per adapter.
 */
namespace TetherReactiveUtil
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

	/**
	 * Escape a string for embedding inside a single-quoted Python literal:
	 * backslashes and quotes are backslash-escaped, newlines become \n / \r.
	 * The adapter copies (EscapeSingleQuoted) only escaped backslash + quote;
	 * the subsystem version also handled \n / \r — this is the conservative
	 * superset, identical on every string the adapters actually pass (object
	 * paths, FNames, tags — none of which contain raw newlines).
	 */
	inline FString EscapePythonStringLiteral(const FString& In)
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

	/**
	 * Render a UObject* as a Python source expression:
	 * "unreal.load_object(None, '/path')" or "None" when null.
	 */
	inline FString RenderPyObjectLiteral(const UObject* Obj)
	{
		if (!Obj)
		{
			return TEXT("None");
		}
		const FString Path = Obj->GetPathName();
		return FString::Printf(TEXT("unreal.load_object(None, '%s')"),
			*EscapePythonStringLiteral(Path));
	}

	/**
	 * Resolve a UAbilitySystemComponent from an actor across the common GAS
	 * placement patterns: directly on the actor (single-player), on its
	 * PlayerState (multiplayer-friendly, this project's pattern), or on its
	 * Controller. Walks via IAbilitySystemInterface first, falls back to
	 * component lookup at each level.
	 */
	inline UAbilitySystemComponent* ResolveActorASC(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		auto FromObject = [](UObject* Obj) -> UAbilitySystemComponent*
		{
			if (!Obj) return nullptr;
			if (IAbilitySystemInterface* I = Cast<IAbilitySystemInterface>(Obj))
			{
				if (UAbilitySystemComponent* ASC = I->GetAbilitySystemComponent())
				{
					return ASC;
				}
			}
			if (AActor* A = Cast<AActor>(Obj))
			{
				return A->FindComponentByClass<UAbilitySystemComponent>();
			}
			return nullptr;
		};

		if (UAbilitySystemComponent* ASC = FromObject(Actor)) return ASC;

		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			if (APlayerState* PS = Pawn->GetPlayerState())
			{
				if (UAbilitySystemComponent* ASC = FromObject(PS)) return ASC;
			}
			if (AController* Ctrl = Pawn->GetController())
			{
				if (UAbilitySystemComponent* ASC = FromObject(Ctrl)) return ASC;
				if (APlayerController* PC = Cast<APlayerController>(Ctrl))
				{
					if (UAbilitySystemComponent* ASC = FromObject(PC->PlayerState)) return ASC;
				}
			}
		}
		return nullptr;
	}

	/**
	 * First begun-play PIE world in editor context order (delegates to
	 * TetherWorldSelection — single implementation of PIE world selection).
	 */
	inline UWorld* FindPIEWorld()
	{
		return GEditor ? TetherAgentImpl::SelectFirstValidPIEWorld(GEditor->GetWorldContexts())
			: nullptr;
	}
}
