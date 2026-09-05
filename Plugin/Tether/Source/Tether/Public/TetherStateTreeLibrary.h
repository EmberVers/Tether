#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TetherStateTreeLibrary.generated.h"

USTRUCT(BlueprintType)
struct FTetherStateTreeCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Error;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeCompileMessage
{
	GENERATED_BODY()

	/** "Info", "Warning", "Error", or "CriticalError". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Severity;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString StateId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString StatePath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString ItemName;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeCompileResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bReadyToRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	TArray<FTetherStateTreeCompileMessage> Messages;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Error;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeAssetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString SchemaClassPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString EditorSchemaClassPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString RootParametersId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bReadyToRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bDirty = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 RootStateCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 StateCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 NodeCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 TransitionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 BindingCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 ParameterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 BreakpointCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int64 EditorDataHash = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int64 LastCompiledEditorDataHash = 0;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeStateInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString ParentId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Path;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString SelectionBehavior;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString TasksCompletion;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Tag;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString LinkedStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString LinkedAssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bExpanded = true;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 Depth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 Index = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 ChildCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 TaskCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 EnterConditionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 ConsiderationCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 TransitionCount = 0;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeNodeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Id;

	/** Empty for evaluators/global tasks; transition GUID for transition conditions; state GUID otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString OwnerId;

	/** Evaluator, GlobalTask, EnterCondition, Task, SingleTask, Consideration, or TransitionCondition. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Scope;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString NodeTypePath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString InstanceTypePath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString ExecutionRuntimeDataTypePath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString ExpressionOperand;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 ExpressionIndent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 Index = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bBlueprintNode = false;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeNodeTypeInfo
{
	GENERATED_BODY()

	/** Evaluator, Task, Condition, or Consideration. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Kind;

	/** Pass this value to AddStateTreeNode. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString TypePath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString InstanceTypePath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bBlueprintClass = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bAllowedBySchema = false;
};

USTRUCT(BlueprintType)
struct FTetherStateTreePropertyInfo
{
	GENERATED_BODY()

	/** Node, Instance, or ExecutionRuntimeData. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString DataSource;

	/** Property path accepted by the node property and binding APIs. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Path;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Type;

	/** Current value in Unreal export-text syntax. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Value;

	/** Invalid, Context, Input, Parameter, or Output. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Usage;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bEditable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bBindable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bInherited = false;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeTransitionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString StateId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 Index = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Trigger;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString RequiredEventTag;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString TargetType;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString TargetStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString TargetStateName;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Priority;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bHasDelay = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	float DelayDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	float DelayRandomVariance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	int32 ConditionCount = 0;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeBindingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString SourcePath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString TargetId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString TargetPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bOutputBinding = false;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeBindableStructInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString TypePath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Section;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeParameterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bOverridden = false;
};

USTRUCT(BlueprintType)
struct FTetherStateTreePropertyResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString TypePath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Error;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeBreakpointInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString ItemId;

	/** OnEnter, OnExit, or OnTransition. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString Type;
};

USTRUCT(BlueprintType)
struct FTetherStateTreeComponentInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString ComponentPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString ComponentName;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString OwnerPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString OwnerLabel;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString WorldType;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString StateTreeAssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	FString RunStatus;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	bool bPaused = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|StateTree")
	TArray<FString> ActiveStateNames;
};

/**
 * StateTree authoring, inspection, binding, compilation, debugger, and runtime control.
 *
 * The implementation uses StateTree's editor data model and compiler directly. The
 * full API is available on UE 5.7+; older supported engines return safe stubs and a
 * descriptive error from GetLastStateTreeError().
 */
UCLASS()
class TETHER_API UTetherStateTreeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Capability and asset lifecycle

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool IsStateTreeApiAvailable();

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FString GetLastStateTreeError();

	/** Create a StateTree with the requested UStateTreeSchema subclass. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FTetherStateTreeCreateResult CreateStateTree(const FString& AssetPath, const FString& SchemaClassPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FTetherStateTreeAssetInfo GetStateTreeInfo(const FString& AssetPath);

	/** Run StateTree's safety validation/fixup pass without compiling. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool ValidateStateTree(const FString& AssetPath);

	/** Compile editor data into runnable data and return the complete compiler log. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FTetherStateTreeCompileResult CompileStateTree(const FString& AssetPath, bool bRunValidation = true);

	// State hierarchy

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreeStateInfo> ListStateTreeStates(const FString& AssetPath);

	/** ParentStateId empty creates a root/subtree state. InsertIndex -1 appends. Returns the new GUID. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FString AddStateTreeState(const FString& AssetPath, const FString& ParentStateId, const FString& Name,
		const FString& StateType = TEXT("State"), int32 InsertIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool RemoveStateTreeState(const FString& AssetPath, const FString& StateId);

	/** Move a state to a new parent (empty = root) and insertion index (-1 = append). */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool MoveStateTreeState(const FString& AssetPath, const FString& StateId,
		const FString& NewParentStateId, int32 InsertIndex = -1);

	/** Set State, Group, or Subtree. Use the dedicated link APIs for linked types. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeStateType(const FString& AssetPath, const FString& StateId,
		const FString& StateType);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FTetherStateTreePropertyResult GetStateTreeStateProperty(const FString& AssetPath,
		const FString& StateId, const FString& PropertyPath);

	/** Value uses Unreal export-text syntax. Nested paths and array indices are supported. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeStateProperty(const FString& AssetPath, const FString& StateId,
		const FString& PropertyPath, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeLinkedState(const FString& AssetPath, const FString& StateId,
		const FString& LinkedStateId);

	/** Empty LinkedAssetPath clears the link and restores a regular State. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeLinkedAsset(const FString& AssetPath, const FString& StateId,
		const FString& LinkedAssetPath);

	// Nodes

	/** List native node structs and Blueprint node classes known to StateTree's node cache. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreeNodeTypeInfo> ListStateTreeNodeTypes(const FString& AssetPath,
		const FString& Kind = TEXT(""), bool bIncludeDisallowed = false);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreeNodeInfo> ListStateTreeNodes(const FString& AssetPath,
		const FString& Scope = TEXT(""));

	/** Discover top-level properties and current values for one node data source. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreePropertyInfo> ListStateTreeNodeProperties(const FString& AssetPath,
		const FString& NodeId, const FString& DataSource = TEXT("Instance"), bool bIncludeInherited = true);

	/**
	 * Add a native node struct or Blueprint node class. Scope is Evaluator, GlobalTask,
	 * EnterCondition, Task, SingleTask, Consideration, or TransitionCondition. OwnerId
	 * is empty for global scopes, a state GUID for state scopes, and a transition GUID
	 * for TransitionCondition. InsertIndex -1 appends. Returns the new node GUID.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FString AddStateTreeNode(const FString& AssetPath, const FString& OwnerId,
		const FString& Scope, const FString& TypePath, int32 InsertIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool RemoveStateTreeNode(const FString& AssetPath, const FString& NodeId);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool MoveStateTreeNode(const FString& AssetPath, const FString& NodeId, int32 NewIndex);

	/** DataSource is Node, Instance, or ExecutionRuntimeData. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FTetherStateTreePropertyResult GetStateTreeNodeProperty(const FString& AssetPath,
		const FString& NodeId, const FString& DataSource, const FString& PropertyPath);

	/** DataSource is Node, Instance, or ExecutionRuntimeData; value uses export-text syntax. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeNodeProperty(const FString& AssetPath, const FString& NodeId,
		const FString& DataSource, const FString& PropertyPath, const FString& Value);

	// Transitions

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreeTransitionInfo> ListStateTreeTransitions(const FString& AssetPath,
		const FString& StateId = "");

	/** Trigger accepts flag names joined by '|'. TargetType is GotoState/NextState/etc. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FString AddStateTreeTransition(const FString& AssetPath, const FString& StateId,
		const FString& Trigger, const FString& TargetType, const FString& TargetStateId = TEXT(""),
		const FString& RequiredEventTag = TEXT(""), int32 InsertIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool RemoveStateTreeTransition(const FString& AssetPath, const FString& TransitionId);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool MoveStateTreeTransition(const FString& AssetPath, const FString& TransitionId,
		int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FTetherStateTreePropertyResult GetStateTreeTransitionProperty(const FString& AssetPath,
		const FString& TransitionId, const FString& PropertyPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeTransitionProperty(const FString& AssetPath, const FString& TransitionId,
		const FString& PropertyPath, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeTransitionTarget(const FString& AssetPath, const FString& TransitionId,
		const FString& TargetType, const FString& TargetStateId = TEXT(""));

	// Property bindings

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreeBindingInfo> ListStateTreeBindings(const FString& AssetPath);

	/** TargetId scopes which sources are accessible; empty returns the generally bindable set. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreeBindableStructInfo> ListStateTreeBindableStructs(const FString& AssetPath,
		const FString& TargetId = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool AddStateTreeBinding(const FString& AssetPath, const FString& SourceId,
		const FString& SourcePath, const FString& TargetId, const FString& TargetPath,
		bool bOutputBinding = false);

	/** Remove the exact binding identified by target GUID and target property path. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool RemoveStateTreeBinding(const FString& AssetPath, const FString& TargetId,
		const FString& TargetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static int32 ClearStateTreeBindingsForItem(const FString& AssetPath, const FString& ItemId);

	// Parameters

	/** ScopeId empty lists root parameters; a state GUID lists that state's parameter bag. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreeParameterInfo> ListStateTreeParameters(const FString& AssetPath,
		const FString& ScopeId = TEXT(""));

	/** Type examples: Bool, Float, Struct:/Script/CoreUObject.Vector, Array<Object:/Script/Engine.Actor>. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FString AddStateTreeRootParameter(const FString& AssetPath, const FString& Name,
		const FString& Type, const FString& DefaultValue = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool RemoveStateTreeRootParameter(const FString& AssetPath, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool RenameStateTreeRootParameter(const FString& AssetPath, const FString& OldName,
		const FString& NewName);

	/** ScopeId empty targets root parameters; a state GUID targets its parameter bag. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeParameterValue(const FString& AssetPath, const FString& ScopeId,
		const FString& Name, const FString& Value, bool bMarkOverridden = true);

	// Debugger breakpoints

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreeBreakpointInfo> ListStateTreeBreakpoints(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeBreakpoint(const FString& AssetPath, const FString& ItemId,
		const FString& BreakpointType, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool ClearStateTreeBreakpoints(const FString& AssetPath);

	// Runtime StateTreeComponent control

	/** Enumerate live editor and PIE StateTreeComponents. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static TArray<FTetherStateTreeComponentInfo> ListStateTreeComponents(bool bPIEOnly = false);

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static FTetherStateTreeComponentInfo GetStateTreeComponentInfo(const FString& ComponentPath);

	/** Set the component asset while its logic is stopped. Empty AssetPath clears it. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SetStateTreeComponentAsset(const FString& ComponentPath, const FString& AssetPath);

	/** Action is Start, Restart, Stop, Pause, or Resume. */
	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool ControlStateTreeComponent(const FString& ComponentPath, const FString& Action,
		const FString& Reason = TEXT("Tether"));

	UFUNCTION(BlueprintCallable, Category = "Tether|StateTree")
	static bool SendStateTreeComponentEvent(const FString& ComponentPath, const FString& EventTag,
		const FString& Origin = TEXT("Tether"));
};
