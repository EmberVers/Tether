#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TetherSmartObjectLibrary.generated.h"

USTRUCT(BlueprintType)
struct FTetherSmartObjectCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString AssetPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Error;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectValidationMessage
{
	GENERATED_BODY()

	/** Info, Warning, Error, or CriticalError. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Severity;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Message;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FTetherSmartObjectValidationMessage> Messages;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Error;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectDefinitionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString AssetPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString UserTagFilterJson;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> ActivityTags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString UserTagsFilteringPolicy;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ActivityTagsMergingPolicy;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString WorldConditionSchemaClassPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString PreviewObjectActorClassPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString PreviewObjectMeshPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString PreviewUserActorClassPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString PreviewValidationFilterClassPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString RootBindableId;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ParametersBindableId;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 SlotCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 DefaultBehaviorCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 DefinitionDataCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 ObjectConditionCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 ParameterCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 BindingCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bHasBeenValidated = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bDirty = false;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FVector Offset = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bEnabled = true;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString UserTagFilterJson;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> ActivityTags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> RuntimeTags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 BehaviorCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 DefinitionDataCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 ConditionCount = 0;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectTypeInfo
{
	GENERATED_BODY()

	/** Behavior, DefinitionData, Annotation, or WorldCondition. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Kind;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString TypePath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bAllowedBySchema = true;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bAllowedAtDefinition = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bAllowedAtSlot = false;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectBehaviorInfo
{
	GENERATED_BODY()

	/** Empty means a definition-level default behavior. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotId;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ObjectPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ClassPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 PropertyCount = 0;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectDefinitionDataInfo
{
	GENERATED_BODY()

	/** Empty means definition-level data; a GUID means slot data/annotation. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotId;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString TypePath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bAnnotation = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bHasTransform = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 PropertyCount = 0;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectPropertyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Path;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Type;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Value;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Category;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bEditable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bBindable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bInherited = false;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectPropertyResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Value;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Type;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Error;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectWorldConditionInfo
{
	GENERATED_BODY()

	/** Empty means object preconditions; a GUID means one slot's preconditions. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotId;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString TypePath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Operator;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 ExpressionDepth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bInvert = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 PropertyCount = 0;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectParameterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Type;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Value;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectBindableStructInfo
{
	GENERATED_BODY()

	/** Root, Parameters, Slot, or DefinitionData. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Kind;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString TypePath;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectBindingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SourceId;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SourcePath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString TargetPath;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectComponentInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ComponentPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ComponentName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString OwnerPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString OwnerLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString WorldType;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString BaseDefinitionPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString AppliedDefinitionPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString RegisteredHandle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString RegistrationType;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FTransform Transform;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FVector BoundsMin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FVector BoundsMax = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bBoundToSimulation = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bCanBePartOfCollection = false;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectCollectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ActorPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ActorLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString WorldType;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 EntryCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FVector BoundsMin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FVector BoundsMax = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bRegistered = false;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectCollectionEntryInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SmartObjectHandle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ComponentPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString DefinitionPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FTransform Transform;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FVector BoundsMin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FVector BoundsMax = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> Tags;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectQueryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 Rank = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") float Distance = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SmartObjectHandle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotHandle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ComponentPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString OwnerPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString DefinitionPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FTransform SlotTransform;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotState;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> ActivityTags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> RuntimeTags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> BehaviorClassPaths;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bCanBeClaimed = false;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectRuntimeSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SmartObjectHandle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotHandle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") int32 SlotIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotState;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString DefinitionPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ComponentPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString OwnerPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FTransform SlotTransform;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> ActivityTags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> RuntimeTags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bCanBeClaimed = false;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectClaimResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bSuccess = false;
	/** Opaque editor-session token used by occupy/release; never persist it in an asset. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString ClaimToken;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SmartObjectHandle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotHandle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString UserActorPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Priority;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotState;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString BehaviorObjectPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Error;
};

/** Options shared by live entrance lookup and offline definition entrance validation. */
USTRUCT(BlueprintType)
struct FTetherSmartObjectEntranceRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") FString UserActorPath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") FString ValidationFilterClassPath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") FVector SearchLocation = FVector::ZeroVector;
	/** First or NearestToSearchLocation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") FString SelectionMethod = TEXT("First");
	/** Entry or Exit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") FString LocationType = TEXT("Entry");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") float CapsuleRadius = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") float CapsuleHeight = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") float CapsuleStepHeight = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") bool bProjectNavigationLocation = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") bool bTraceGroundLocation = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") bool bCheckTransitionTrajectory = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") bool bCheckEntranceLocationOverlap = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") bool bCheckSlotLocationOverlap = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") bool bUseSlotLocationAsFallback = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|SmartObject") bool bUseUpAxisLockedRotation = false;
};

USTRUCT(BlueprintType)
struct FTetherSmartObjectEntranceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString SlotHandle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") TArray<FString> Tags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") bool bHasNavigationNode = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|SmartObject") FString Error;
};

/**
 * Smart Object definition authoring, condition/binding editing, world collection
 * management, spatial queries, claim lifecycle, entrance validation, and debug control.
 *
 * The functional implementation is available on UE 5.7+. Older supported engines
 * keep the reflected class and return safe defaults with a descriptive last error.
 */
UCLASS()
class TETHER_API UTetherSmartObjectLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Capability and definition lifecycle

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool IsSmartObjectApiAvailable();

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString GetLastSmartObjectError();

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectCreateResult CreateSmartObjectDefinition(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectDefinitionInfo GetSmartObjectDefinitionInfo(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectValidationResult ValidateSmartObjectDefinition(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectPropertyInfo> ListSmartObjectDefinitionProperties(
		const FString& AssetPath, bool bIncludeInherited = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectPropertyResult GetSmartObjectDefinitionProperty(
		const FString& AssetPath, const FString& PropertyPath);

	/** Value uses Unreal export-text syntax. Structural arrays have dedicated APIs and are rejected here. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectDefinitionProperty(
		const FString& AssetPath, const FString& PropertyPath, const FString& Value);

	/** SlotId empty targets the definition query. Returns Epic's FGameplayTagQuery expression JSON. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString GetSmartObjectTagQueryJson(const FString& AssetPath, const FString& SlotId = TEXT(""));

	/** Empty QueryJson clears the query. SlotId empty targets the definition query. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectTagQueryJson(const FString& AssetPath, const FString& QueryJson,
		const FString& SlotId = TEXT(""));

	/** TagSet is Activity, or Runtime for slots. The full set is replaced. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectTags(const FString& AssetPath, const TArray<FString>& Tags,
		const FString& SlotId = TEXT(""), const FString& TagSet = TEXT("Activity"));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectTagPolicies(const FString& AssetPath,
		const FString& UserTagsFilteringPolicy, const FString& ActivityTagsMergingPolicy);

	// Slots

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectSlotInfo> ListSmartObjectSlots(const FString& AssetPath);

	/** InsertIndex -1 appends. Returns the new stable editor GUID. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString AddSmartObjectSlot(const FString& AssetPath, const FString& Name,
		const FVector& Offset, const FRotator& Rotation, bool bEnabled = true, int32 InsertIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString DuplicateSmartObjectSlot(const FString& AssetPath, const FString& SourceSlotId,
		const FString& NewName = TEXT(""), int32 InsertIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool RemoveSmartObjectSlot(const FString& AssetPath, const FString& SlotId);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool MoveSmartObjectSlot(const FString& AssetPath, const FString& SlotId, int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectPropertyInfo> ListSmartObjectSlotProperties(
		const FString& AssetPath, const FString& SlotId);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectPropertyResult GetSmartObjectSlotProperty(
		const FString& AssetPath, const FString& SlotId, const FString& PropertyPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectSlotProperty(const FString& AssetPath, const FString& SlotId,
		const FString& PropertyPath, const FString& Value);

	// Behavior definitions

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectTypeInfo> ListSmartObjectBehaviorTypes();

	/** SlotId empty lists definition-level fallback behaviors. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectBehaviorInfo> ListSmartObjectBehaviorDefinitions(
		const FString& AssetPath, const FString& SlotId = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString AddSmartObjectBehaviorDefinition(const FString& AssetPath, const FString& BehaviorClassPath,
		const FString& SlotId = TEXT(""), int32 InsertIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool RemoveSmartObjectBehaviorDefinition(const FString& AssetPath, const FString& BehaviorObjectPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool MoveSmartObjectBehaviorDefinition(const FString& AssetPath,
		const FString& BehaviorObjectPath, int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectPropertyInfo> ListSmartObjectBehaviorProperties(
		const FString& AssetPath, const FString& BehaviorObjectPath, bool bIncludeInherited = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectPropertyResult GetSmartObjectBehaviorProperty(const FString& AssetPath,
		const FString& BehaviorObjectPath, const FString& PropertyPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectBehaviorProperty(const FString& AssetPath, const FString& BehaviorObjectPath,
		const FString& PropertyPath, const FString& Value);

	// Definition data and slot annotations

	/** SlotId empty discovers definition-level data; a slot GUID discovers slot data/annotations. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectTypeInfo> ListSmartObjectDefinitionDataTypes(
		const FString& AssetPath, const FString& SlotId = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectDefinitionDataInfo> ListSmartObjectDefinitionData(
		const FString& AssetPath, const FString& SlotId = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString AddSmartObjectDefinitionData(const FString& AssetPath, const FString& StructTypePath,
		const FString& SlotId = TEXT(""), int32 InsertIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool RemoveSmartObjectDefinitionData(const FString& AssetPath, const FString& DataId);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool MoveSmartObjectDefinitionData(const FString& AssetPath, const FString& DataId, int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectPropertyInfo> ListSmartObjectDefinitionDataProperties(
		const FString& AssetPath, const FString& DataId, bool bIncludeInherited = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectPropertyResult GetSmartObjectDefinitionDataProperty(const FString& AssetPath,
		const FString& DataId, const FString& PropertyPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectDefinitionDataProperty(const FString& AssetPath, const FString& DataId,
		const FString& PropertyPath, const FString& Value);

	// Object and slot selection conditions

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectTypeInfo> ListSmartObjectWorldConditionTypes(
		const FString& AssetPath, const FString& SlotId = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectWorldConditionInfo> ListSmartObjectWorldConditions(
		const FString& AssetPath, const FString& SlotId = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static int32 AddSmartObjectWorldCondition(const FString& AssetPath, const FString& ConditionStructPath,
		const FString& SlotId = TEXT(""), const FString& Operator = TEXT("And"),
		int32 ExpressionDepth = 0, bool bInvert = false, int32 InsertIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool RemoveSmartObjectWorldCondition(const FString& AssetPath,
		const FString& SlotId, int32 ConditionIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool MoveSmartObjectWorldCondition(const FString& AssetPath,
		const FString& SlotId, int32 ConditionIndex, int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectWorldConditionExpression(const FString& AssetPath, const FString& SlotId,
		int32 ConditionIndex, const FString& Operator, int32 ExpressionDepth, bool bInvert);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectPropertyInfo> ListSmartObjectWorldConditionProperties(
		const FString& AssetPath, const FString& SlotId, int32 ConditionIndex, bool bIncludeInherited = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectPropertyResult GetSmartObjectWorldConditionProperty(const FString& AssetPath,
		const FString& SlotId, int32 ConditionIndex, const FString& PropertyPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectWorldConditionProperty(const FString& AssetPath, const FString& SlotId,
		int32 ConditionIndex, const FString& PropertyPath, const FString& Value);

	// Definition parameters and property bindings

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectParameterInfo> ListSmartObjectParameters(const FString& AssetPath);

	/** Type syntax matches StateTree property bags, e.g. Float or Object:/Script/Engine.Actor. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString AddSmartObjectParameter(const FString& AssetPath, const FString& Name,
		const FString& Type, const FString& DefaultValue = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool RemoveSmartObjectParameter(const FString& AssetPath, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool RenameSmartObjectParameter(const FString& AssetPath,
		const FString& OldName, const FString& NewName);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectParameterValue(const FString& AssetPath,
		const FString& Name, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectBindableStructInfo> ListSmartObjectBindableStructs(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectBindingInfo> ListSmartObjectBindings(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool AddSmartObjectBinding(const FString& AssetPath, const FString& SourceId,
		const FString& SourcePath, const FString& TargetId, const FString& TargetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool RemoveSmartObjectBinding(const FString& AssetPath,
		const FString& TargetId, const FString& TargetPath);

	// Loaded world components and persistent collections

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectComponentInfo> ListSmartObjectComponents(bool bPIEOnly = false);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectComponentInfo GetSmartObjectComponentInfo(const FString& ComponentPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString AddSmartObjectComponent(const FString& ActorPath, const FString& DefinitionAssetPath,
		const FString& ComponentName = TEXT("SmartObject"), bool bCanBePartOfCollection = false,
		bool bRegisterWithSubsystem = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool RemoveSmartObjectComponent(const FString& ComponentPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectComponentDefinition(const FString& ComponentPath,
		const FString& DefinitionAssetPath, bool bRegisterWithSubsystem = true);

	/** Action: Register, Unregister, RemoveFromSimulation, Refresh, Enable, or Disable. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool ControlSmartObjectComponent(const FString& ComponentPath, const FString& Action);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectCollectionInfo> ListPersistentSmartObjectCollections(bool bPIEOnly = false);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString CreatePersistentSmartObjectCollection(const FString& ActorLabel = TEXT("SmartObjectPersistentCollection"));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool DestroyPersistentSmartObjectCollection(const FString& CollectionActorPath);

	/** Action: Rebuild, Clear, Register, or Unregister. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool ControlPersistentSmartObjectCollection(const FString& CollectionActorPath, const FString& Action);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectCollectionEntryInfo> ListPersistentSmartObjectCollectionEntries(
		const FString& CollectionActorPath);

	// Runtime query, lifecycle, tags, and entrance validation

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectQueryResult> QuerySmartObjects(const FVector& Center, const FVector& Extent,
		const TArray<FString>& UserTags, const TArray<FString>& ActivityTags,
		const TArray<FString>& BehaviorClassPaths, const FString& ActivityMatch = TEXT("All"),
		const FString& ClaimPriority = TEXT("Normal"), bool bEvaluateConditions = true,
		bool bIncludeClaimedSlots = false, bool bIncludeDisabledSlots = false,
		const FString& UserActorPath = TEXT(""), bool bSortByDistance = true, int32 MaxResults = 0);

	/** Exactly one of SmartObjectHandle or ComponentPath should normally be supplied. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectRuntimeSlotInfo> ListSmartObjectRuntimeSlots(
		const FString& SmartObjectHandle = TEXT(""), const FString& ComponentPath = TEXT(""),
		const FString& ClaimPriority = TEXT("Normal"));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FString CreateRuntimeSmartObject(const FString& DefinitionAssetPath, const FTransform& Transform,
		const FString& OwnerActorPath = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool DestroyRuntimeSmartObject(const FString& SmartObjectHandle);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectClaimResult ClaimSmartObjectSlot(const FString& SlotHandle,
		const FString& UserActorPath = TEXT(""), const FString& ClaimPriority = TEXT("Normal"));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectClaimResult OccupySmartObjectClaim(const FString& ClaimToken,
		const FString& BehaviorClassPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool ReleaseSmartObjectClaim(const FString& ClaimToken);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectClaimResult> ListSmartObjectClaims();

	/** Scope is Object or Slot. Existing runtime tags are replaced when bReplace is true. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectRuntimeTags(const FString& Handle, const FString& Scope,
		const TArray<FString>& Tags, bool bReplace = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectRuntimeEnabled(const FString& SmartObjectHandle, bool bEnabled,
		const FString& ReasonTag = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SetSmartObjectRuntimeSlotEnabled(const FString& SlotHandle, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool SendSmartObjectSlotEvent(const FString& SlotHandle, const FString& EventTag);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static FTetherSmartObjectEntranceResult FindSmartObjectEntrance(const FString& SlotHandle,
		const FTetherSmartObjectEntranceRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static TArray<FTetherSmartObjectEntranceResult> ValidateSmartObjectDefinitionEntrances(
		const FString& AssetPath, const FTransform& OwnerTransform,
		const FTetherSmartObjectEntranceRequest& Request, const FString& SkipActorPath = TEXT(""));

	/** Action: InitializeRuntime, CleanupRuntime, RegisterAll, or UnregisterAll. */
	UFUNCTION(BlueprintCallable, Category = "Tether|SmartObject")
	static bool DebugSmartObjectSubsystem(const FString& Action);
};
