#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TetherRigLibrary.generated.h"

USTRUCT(BlueprintType)
struct FTetherRigOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString AssetPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Error;
};

USTRUCT(BlueprintType)
struct FTetherRigTypeInfo
{
	GENERATED_BODY()

	/** ControlRigUnit, RigVMTemplate, IKSolver, or RetargetOp. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Kind;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString TypePath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Category;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") bool bDeprecated = false;
};

USTRUCT(BlueprintType)
struct FTetherRigPropertyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Path;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Type;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Value;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Category;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") bool bEditable = false;
};

USTRUCT(BlueprintType)
struct FTetherRigPropertyResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Value;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Type;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Error;
};

USTRUCT(BlueprintType)
struct FTetherRigValidationIssue
{
	GENERATED_BODY()

	/** Error, Warning, or Info. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Severity;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Code;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Subject;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Message;
};

USTRUCT(BlueprintType)
struct FTetherRigValidationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") bool bCompiled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") bool bSaved = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") int32 ErrorCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") int32 WarningCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") TArray<FTetherRigValidationIssue> Issues;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig") FString Error;
};

USTRUCT(BlueprintType)
struct FTetherControlRigInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") FString AssetPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") FString PreviewSkeletalMeshPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") FString GeneratedClassPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") int32 BoneCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") int32 ControlCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") int32 NullCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") int32 CurveCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") int32 ConnectorCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") int32 GraphCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") int32 NodeCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") bool bModularRig = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") bool bDirty = false;
};

USTRUCT(BlueprintType)
struct FTetherRigElementInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FString Type;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FString ParentName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FString ParentType;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FString ControlType;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FTransform InitialLocalTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FTransform InitialGlobalTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FTransform CurrentLocalTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") FTransform CurrentGlobalTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") TArray<FString> Tags;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Hierarchy") bool bImportedBone = false;
};

USTRUCT(BlueprintType)
struct FTetherRigVMGraphInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString NodePath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") int32 NodeCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") int32 LinkCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") bool bFunctionLibrary = false;
};

USTRUCT(BlueprintType)
struct FTetherRigVMPinInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString Path;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString Direction;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString CPPType;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString CPPTypeObjectPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString DefaultValue;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") bool bArray = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") bool bLinked = false;
};

USTRUCT(BlueprintType)
struct FTetherRigVMNodeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString Path;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString ClassName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FVector2D Position = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") TArray<FTetherRigVMPinInfo> Pins;
};

USTRUCT(BlueprintType)
struct FTetherRigVMLinkInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString SourcePinPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") FString TargetPinPath;
};

USTRUCT(BlueprintType)
struct FTetherRigLayoutResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") int32 NodesPositioned = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") int32 LayerCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|RigVM") TArray<FString> Warnings;
};

USTRUCT(BlueprintType)
struct FTetherRigNamedTransform
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Tether|Rig") FString Name;
	UPROPERTY(BlueprintReadWrite, Category = "Tether|Rig") FString Type;
	UPROPERTY(BlueprintReadWrite, Category = "Tether|Rig") FTransform Transform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct FTetherControlRigEvaluationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") FString EventName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") TArray<FTetherRigNamedTransform> Controls;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") TArray<FTetherRigNamedTransform> Bones;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|ControlRig") FString Error;
};

USTRUCT(BlueprintType)
struct FTetherIKRigInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString AssetPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString PreviewSkeletalMeshPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString SkeletonPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString RetargetRoot;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") int32 BoneCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") int32 SolverCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") int32 GoalCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") int32 ChainCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") bool bDirty = false;
};

USTRUCT(BlueprintType)
struct FTetherIKSolverInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString TypePath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString StartBone;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString EndBone;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") TArray<FString> Goals;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") TArray<FString> BonesWithSettings;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") bool bEnabled = false;
};

USTRUCT(BlueprintType)
struct FTetherIKGoalInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString BoneName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FTransform InitialTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FTransform CurrentTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") float PositionAlpha = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") float RotationAlpha = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") TArray<int32> ConnectedSolverIndices;
};

USTRUCT(BlueprintType)
struct FTetherIKChainInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString StartBone;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString EndBone;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") FString GoalName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") int32 BoneCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|IKRig") bool bValid = false;
};

USTRUCT(BlueprintType)
struct FTetherIKRetargeterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString AssetPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString SourceIKRigPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString TargetIKRigPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString SourcePreviewMeshPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString TargetPreviewMeshPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString CurrentSourcePose;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString CurrentTargetPose;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString CurrentProfile;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") int32 OpCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") int32 MappingCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") int32 UnmappedChainCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") int32 SourcePoseCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") int32 TargetPoseCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") bool bDirty = false;
};

USTRUCT(BlueprintType)
struct FTetherIKRetargetOpInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString TypePath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString ParentOpName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString TargetIKRigPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") bool bEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") bool bHasChainMapping = false;
};

USTRUCT(BlueprintType)
struct FTetherIKChainMappingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString OpName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString TargetChainName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString SourceChainName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") bool bMapped = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") bool bSettingsAtDefault = true;
};

USTRUCT(BlueprintType)
struct FTetherIKRetargetPoseInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString Side;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FVector RootTranslationOffset = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") int32 BoneRotationOffsetCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") bool bCurrent = false;
};

USTRUCT(BlueprintType)
struct FTetherRetargetBatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") TArray<FString> CreatedAssetPaths;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") TArray<FString> FailedSourceAssetPaths;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Retargeter") FString Error;
};

USTRUCT(BlueprintType)
struct FTetherAnimationBoneMetric
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") FString BoneName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") float MinimumRelativeHeight = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") float MaximumHorizontalSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") float MaximumAngularDeltaDegrees = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") bool bFootBone = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") bool bFlagged = false;
};

USTRUCT(BlueprintType)
struct FTetherAnimationQualityReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") FString AnimationPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") float Duration = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") int32 Samples = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") float MaximumRootSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") float MinimumFootHeight = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") float MaximumFootSlideSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") float MaximumJointAngularDeltaDegrees = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") TArray<FTetherAnimationBoneMetric> BoneMetrics;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") TArray<FTetherRigValidationIssue> Issues;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Rig|Quality") FString Error;
};

/** Complete Control Rig, IK Rig, IK Retargeter authoring and animation-delivery surface. */
UCLASS()
class TETHER_API UTetherRigLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig")
	static bool IsRigApiAvailable();

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig")
	static FString GetLastRigError();

	/** Discover ControlRigUnit, RigVMTemplate, IKSolver, and RetargetOp types. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig")
	static TArray<FTetherRigTypeInfo> ListRigTypes(const FString& Kind, const FString& Query, int32 MaxResults);

	// Control Rig asset and hierarchy --------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FTetherRigOperationResult CreateControlRig(const FString& AssetPath, const FString& SourceSkeletalAssetPath, bool bModularRig, bool bImportCurves);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FTetherControlRigInfo GetControlRigInfo(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static TArray<FTetherRigElementInfo> ListControlRigElements(const FString& AssetPath, const FString& ElementType);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static bool ImportControlRigHierarchy(const FString& AssetPath, const FString& SourceSkeletalAssetPath, bool bReplaceExisting, bool bImportCurves);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FString AddControlRigBone(const FString& AssetPath, const FString& Name, const FString& ParentName, const FString& ParentType, const FTransform& Transform, bool bGlobalTransform, bool bImportedBone);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FString AddControlRigNull(const FString& AssetPath, const FString& Name, const FString& ParentName, const FString& ParentType, const FTransform& Transform, bool bGlobalTransform);

	/** InitialValue uses UE export text, e.g. "True", "1.0", "(X=0,Y=0,Z=0)", or a Transform tuple. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FString AddControlRigControl(const FString& AssetPath, const FString& Name, const FString& ParentName, const FString& ParentType, const FString& ControlType, const FString& InitialValue, const FTransform& OffsetTransform, const FTransform& ShapeTransform, const FString& ShapeName, const FLinearColor& ShapeColor, bool bAnimatable);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FString AddControlRigCurve(const FString& AssetPath, const FString& Name, float Value);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FString AddControlRigConnector(const FString& AssetPath, const FString& Name, const FString& ConnectorType, const FString& Description, bool bOptional, bool bArray);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static bool RemoveControlRigElement(const FString& AssetPath, const FString& Name, const FString& ElementType);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FString RenameControlRigElement(const FString& AssetPath, const FString& Name, const FString& ElementType, const FString& NewName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static bool ReparentControlRigElement(const FString& AssetPath, const FString& Name, const FString& ElementType, const FString& ParentName, const FString& ParentType, bool bMaintainGlobalTransform);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static bool SetControlRigElementTransform(const FString& AssetPath, const FString& Name, const FString& ElementType, const FTransform& Transform, bool bGlobalTransform, bool bInitial, bool bAffectChildren);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static bool SetControlRigControlShape(const FString& AssetPath, const FString& ControlName, const FString& ShapeName, const FLinearColor& ShapeColor, bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static bool AddControlRigElementTag(const FString& AssetPath, const FString& Name, const FString& ElementType, const FString& Tag);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static bool RemoveControlRigElementTag(const FString& AssetPath, const FString& Name, const FString& ElementType, const FString& Tag);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static TArray<FTetherRigPropertyInfo> ListControlRigControlProperties(const FString& AssetPath, const FString& ControlName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FTetherRigPropertyResult GetControlRigControlProperty(const FString& AssetPath, const FString& ControlName, const FString& PropertyPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static bool SetControlRigControlProperty(const FString& AssetPath, const FString& ControlName, const FString& PropertyPath, const FString& Value);

	// RigVM graph ----------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static TArray<FTetherRigVMGraphInfo> ListControlRigGraphs(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static TArray<FTetherRigVMNodeInfo> ListControlRigNodes(const FString& AssetPath, const FString& GraphName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static TArray<FTetherRigVMLinkInfo> ListControlRigLinks(const FString& AssetPath, const FString& GraphName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static FString AddControlRigMemberVariable(const FString& AssetPath, const FString& Name, const FString& CPPType, const FString& DefaultValue, bool bPublic, bool bReadOnly);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static bool RemoveControlRigMemberVariable(const FString& AssetPath, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static FString AddControlRigUnitNode(const FString& AssetPath, const FString& GraphName, const FString& UnitStructPath, const FString& MethodName, const FVector2D& Position, const FString& NodeName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static FString AddControlRigTemplateNode(const FString& AssetPath, const FString& GraphName, const FString& Notation, const FVector2D& Position, const FString& NodeName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static FString AddControlRigVariableNode(const FString& AssetPath, const FString& GraphName, const FString& VariableName, const FString& CPPType, const FString& CPPTypeObjectPath, bool bGetter, const FString& DefaultValue, const FVector2D& Position, const FString& NodeName, bool bCreateMemberVariable);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static FString AddControlRigCommentNode(const FString& AssetPath, const FString& GraphName, const FString& CommentText, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color, const FString& NodeName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static FString AddControlRigBranchNode(const FString& AssetPath, const FString& GraphName, const FVector2D& Position, const FString& NodeName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static bool RemoveControlRigNode(const FString& AssetPath, const FString& GraphName, const FString& NodeName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static bool SetControlRigNodePosition(const FString& AssetPath, const FString& GraphName, const FString& NodeName, const FVector2D& Position);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static bool SetControlRigPinDefaultValue(const FString& AssetPath, const FString& GraphName, const FString& PinPath, const FString& DefaultValue, bool bResizeArrays);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static bool ConnectControlRigPins(const FString& AssetPath, const FString& GraphName, const FString& OutputPinPath, const FString& InputPinPath, bool bCreateCastNode);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static bool DisconnectControlRigPins(const FString& AssetPath, const FString& GraphName, const FString& OutputPinPath, const FString& InputPinPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|RigVM")
	static FTetherRigLayoutResult AutoLayoutControlRigGraph(const FString& AssetPath, const FString& GraphName, float HorizontalSpacing, float VerticalSpacing);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FTetherRigValidationReport CompileControlRig(const FString& AssetPath, bool bSave);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FTetherRigValidationReport ValidateControlRig(const FString& AssetPath, bool bSave);

	/** Evaluate an event on a transient Control Rig instance without changing the asset. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|ControlRig")
	static FTetherControlRigEvaluationResult EvaluateControlRig(const FString& AssetPath, const FString& EventName, const TArray<FTetherRigNamedTransform>& InputControls);

	// IK Rig ---------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static FTetherRigOperationResult CreateIKRig(const FString& AssetPath, const FString& SkeletalMeshPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static FTetherIKRigInfo GetIKRigInfo(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static TArray<FTetherIKSolverInfo> ListIKRigSolvers(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static TArray<FTetherIKGoalInfo> ListIKRigGoals(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static TArray<FTetherIKChainInfo> ListIKRigRetargetChains(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static int32 AddIKRigSolver(const FString& AssetPath, const FString& SolverTypePath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool RemoveIKRigSolver(const FString& AssetPath, int32 SolverIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool MoveIKRigSolver(const FString& AssetPath, int32 SolverIndex, int32 TargetIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool SetIKRigSolverEnabled(const FString& AssetPath, int32 SolverIndex, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool SetIKRigSolverBones(const FString& AssetPath, int32 SolverIndex, const FString& StartBone, const FString& EndBone);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static FString AddIKRigGoal(const FString& AssetPath, const FString& GoalName, const FString& BoneName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool RemoveIKRigGoal(const FString& AssetPath, const FString& GoalName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool ConnectIKRigGoalToSolver(const FString& AssetPath, const FString& GoalName, int32 SolverIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool DisconnectIKRigGoalFromSolver(const FString& AssetPath, const FString& GoalName, int32 SolverIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static FString AddIKRigRetargetChain(const FString& AssetPath, const FString& ChainName, const FString& StartBone, const FString& EndBone, const FString& GoalName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool RemoveIKRigRetargetChain(const FString& AssetPath, const FString& ChainName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static FString RenameIKRigRetargetChain(const FString& AssetPath, const FString& ChainName, const FString& NewName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool SetIKRigRetargetRoot(const FString& AssetPath, const FString& RootBoneName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool SetIKRigBoneExcluded(const FString& AssetPath, const FString& BoneName, bool bExcluded);

	/** Auto-generate a humanoid retarget definition and/or Full Body IK solver setup. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool ApplyIKRigAutoSetup(const FString& AssetPath, bool bRetargetDefinition, bool bFullBodyIK);

	/** TargetKind: Solver, Goal, GoalSettings, or BoneSettings. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static TArray<FTetherRigPropertyInfo> ListIKRigProperties(const FString& AssetPath, const FString& TargetKind, int32 SolverIndex, const FString& TargetName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static FTetherRigPropertyResult GetIKRigProperty(const FString& AssetPath, const FString& TargetKind, int32 SolverIndex, const FString& TargetName, const FString& PropertyPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static bool SetIKRigProperty(const FString& AssetPath, const FString& TargetKind, int32 SolverIndex, const FString& TargetName, const FString& PropertyPath, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|IKRig")
	static FTetherRigValidationReport ValidateIKRig(const FString& AssetPath, bool bSave);

	// IK Retargeter --------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static FTetherRigOperationResult CreateIKRetargeter(const FString& AssetPath, const FString& SourceIKRigPath, const FString& TargetIKRigPath, const FString& SourcePreviewMeshPath, const FString& TargetPreviewMeshPath, bool bAddDefaultOps);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static FTetherIKRetargeterInfo GetIKRetargeterInfo(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool ConfigureIKRetargeterAssets(const FString& AssetPath, const FString& SourceIKRigPath, const FString& TargetIKRigPath, const FString& SourcePreviewMeshPath, const FString& TargetPreviewMeshPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static TArray<FTetherIKRetargetOpInfo> ListIKRetargetOps(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static int32 AddIKRetargetOp(const FString& AssetPath, const FString& OpTypePath, const FString& OpName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool AddDefaultIKRetargetOps(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool RemoveIKRetargetOp(const FString& AssetPath, int32 OpIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool MoveIKRetargetOp(const FString& AssetPath, int32 OpIndex, int32 TargetIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool SetIKRetargetOpEnabled(const FString& AssetPath, int32 OpIndex, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool SetIKRetargetOpParent(const FString& AssetPath, const FString& ChildOpName, const FString& ParentOpName);

	/** MappingType: Exact, Fuzzy, or Clear. Empty OpName applies to all chain-mapping ops. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool AutoMapIKRetargetChains(const FString& AssetPath, const FString& MappingType, bool bForceRemap, const FString& OpName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool SetIKRetargetChainMapping(const FString& AssetPath, const FString& TargetChainName, const FString& SourceChainName, const FString& OpName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static TArray<FTetherIKChainMappingInfo> ListIKRetargetChainMappings(const FString& AssetPath, const FString& OpName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static TArray<FTetherRigPropertyInfo> ListIKRetargetOpProperties(const FString& AssetPath, int32 OpIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static FTetherRigPropertyResult GetIKRetargetOpProperty(const FString& AssetPath, int32 OpIndex, const FString& PropertyPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool SetIKRetargetOpProperty(const FString& AssetPath, int32 OpIndex, const FString& PropertyPath, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static TArray<FTetherIKRetargetPoseInfo> ListIKRetargetPoses(const FString& AssetPath, const FString& Side);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static FString CreateIKRetargetPose(const FString& AssetPath, const FString& PoseName, const FString& Side);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static FString DuplicateIKRetargetPose(const FString& AssetPath, const FString& PoseName, const FString& NewName, const FString& Side);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool RenameIKRetargetPose(const FString& AssetPath, const FString& PoseName, const FString& NewName, const FString& Side);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool RemoveIKRetargetPose(const FString& AssetPath, const FString& PoseName, const FString& Side);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool SetCurrentIKRetargetPose(const FString& AssetPath, const FString& PoseName, const FString& Side);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool SetIKRetargetPoseBoneRotation(const FString& AssetPath, const FString& Side, const FString& BoneName, const FQuat& RotationOffset);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool SetIKRetargetPoseRootOffset(const FString& AssetPath, const FString& Side, const FVector& TranslationOffset);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool ResetIKRetargetPose(const FString& AssetPath, const FString& Side, const FString& PoseName, const TArray<FString>& BoneNames);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool AutoAlignIKRetargetPose(const FString& AssetPath, const FString& Side, const TArray<FString>& BoneNames, const FString& Method);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static TArray<FString> ListIKRetargetProfiles(const FString& AssetPath);

	/** Snapshot the current op settings and optional source/target poses into a named runtime profile. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool SaveCurrentIKRetargetProfile(const FString& AssetPath, const FString& ProfileName, bool bApplySourcePose, bool bApplyTargetPose, bool bForceAllIKOff);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool RemoveIKRetargetProfile(const FString& AssetPath, const FString& ProfileName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static bool SetCurrentIKRetargetProfile(const FString& AssetPath, const FString& ProfileName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static FTetherRetargetBatchResult BatchRetargetAnimations(const FString& RetargeterPath, const TArray<FString>& SourceAssetPaths, const FString& SourceMeshPath, const FString& TargetMeshPath, const FString& DestinationFolder, const FString& Search, const FString& Replace, const FString& Prefix, const FString& Suffix, bool bIncludeReferencedAssets, bool bOverwriteExisting, bool bSave);

	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Retargeter")
	static FTetherRigValidationReport ValidateIKRetargeter(const FString& AssetPath, bool bInitializeProcessor, bool bSave);

	/** Sample a sequence for root spikes, foot sliding/penetration, and per-joint angular discontinuities. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Rig|Quality")
	static FTetherAnimationQualityReport AnalyzeAnimationQuality(const FString& AnimationPath, const TArray<FString>& FootBoneNames, int32 NumSamples, float ContactHeightTolerance, float FootSlideSpeedTolerance, float JointAngularDeltaToleranceDegrees, int32 MaxReportedBones);
};
