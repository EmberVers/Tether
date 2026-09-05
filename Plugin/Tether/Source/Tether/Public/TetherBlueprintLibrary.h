#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TetherBlueprintLibrary.generated.h"

// ─── Structs ─────────────────────────────────────────────────

/** Single entry in a class hierarchy chain. */
USTRUCT(BlueprintType)
struct FTetherClassInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString ClassName;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString ClassPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsNative = false;
};

/** Describes a single variable defined in a Blueprint. */
USTRUCT(BlueprintType)
struct FTetherVariableInfo
{
	GENERATED_BODY()

	/** Variable name */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	/** Type as displayed in the editor (e.g. "Float", "Vector", "MyStruct", "Array of Int") */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Type;

	/** Category assigned in the Blueprint editor */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Category;

	/** Whether this variable is marked Instance Editable (visible in Details panel) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bInstanceEditable = false;

	/** Whether this variable is marked Blueprint Read Only */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bBlueprintReadOnly = false;

	/** Default value as string (best-effort serialization) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString DefaultValue;

	/** Tooltip / description set in the editor */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Description;

	/** The replication condition (None, Replicated, RepNotify) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString ReplicationCondition;
};

/** Describes a single parameter of a function. */
USTRUCT(BlueprintType)
struct FTetherFunctionParam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Type;

	/** True if this is a return / output parameter */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsOutput = false;

	/** Declared default value when the function was exposed to Blueprint
	 *  (stored as `CPP_Default_<ParamName>` metadata on the UFunction).
	 *  Empty when no default was specified — the caller must wire a value.
	 *  Only populated by GetFunctionSignature; older APIs (GetFunctionCallSignature
	 *  / GetBlueprintFunctions) leave this empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString DefaultValue;

	/** True when the parameter is passed by reference (`UPARAM(ref)` / `Foo&`).
	 *  Relevant because Blueprint must wire a variable (not a literal) into
	 *  the pin. Only populated by GetFunctionSignature. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsReference = false;

	/** True when the parameter is `const` — some callers use this to decide
	 *  whether a pin default is legal. Only populated by GetFunctionSignature. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsConst = false;
};

/** Describes a function or event defined in a Blueprint. */
USTRUCT(BlueprintType)
struct FTetherFunctionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	/** "Function", "Event", "Override" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Kind;

	/** Access level: "Public", "Protected", "Private" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Access;

	/** Whether this is a pure function (no exec pin) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsPure = false;

	/** Whether this is a static function */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsStatic = false;

	/** Category assigned in editor */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Category;

	/** Input and output parameters */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FTetherFunctionParam> Params;

	/** Description / tooltip */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Description;
};

/** Describes a single component in a Blueprint's component tree. */
USTRUCT(BlueprintType)
struct FTetherComponentInfo
{
	GENERATED_BODY()

	/** Component variable name */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	/** Component class (e.g. "StaticMeshComponent", "CapsuleComponent") */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString ComponentClass;

	/** Parent component name (empty for root) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString ParentName;

	/** Whether this is the scene root component */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsRoot = false;

	/** Whether this component is inherited from a parent class */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsInherited = false;
};

/** Describes an interface implemented by a Blueprint. */
USTRUCT(BlueprintType)
struct FTetherInterfaceInfo
{
	GENERATED_BODY()

	/** Interface class name (e.g. "BPI_Interactable") */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString InterfaceName;

	/** Full class path */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString InterfacePath;

	/** Whether this interface is implemented via a Blueprint (vs C++) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsBlueprintImplemented = false;

	/** Function names declared by this interface */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FString> Functions;
};

/** A single call edge: "function A calls function B". */
USTRUCT(BlueprintType)
struct FTetherCallEdge
{
	GENERATED_BODY()

	/** The function/event being called */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString TargetName;

	/** Target class or object (e.g. "KismetMathLibrary", "Self", "OtherActor") */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString TargetClass;

	/** Whether the target is a function, event, or macro */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString TargetKind;
};

/** Result of WireEnhancedInputActionToFunction (B7). */
USTRUCT(BlueprintType)
struct FTetherWireIAResult
{
	GENERATED_BODY()

	/** GUID of the K2Node_EnhancedInputAction event node (new or reused). Empty on failure. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString EventNodeGuid;

	/** GUID of the K2Node_CallFunction node added for the target function. Empty on failure. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString CallNodeGuid;

	/** True iff the trigger-event exec pin → CallFunction's exec_in connection succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bWired = false;

	/** Diagnostic when bWired=false (e.g. "trigger pin 'Foo' not found", "schema rejected connection"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString FailureReason;
};

/** A single node in a Blueprint function graph. */
USTRUCT(BlueprintType)
struct FTetherNodeInfo
{
	GENERATED_BODY()

	/** Node title as shown in the editor (e.g. "Branch", "Print String", "Set Timer by Event") */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Title;

	/** Node type category: "FunctionCall", "VariableGet", "VariableSet", "Branch", "ForEach",
	    "Cast", "Event", "Macro", "Spawn", "Timeline", "Knot", "Other" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString NodeType;

	/** For function calls: the target class (e.g. "KismetSystemLibrary") */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString TargetClass;

	/** For variable nodes: the variable name */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString VariableName;

	/** Node comment if any */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Comment;

	/** NodeGuid (digits form, 32-hex); pass to connect_graph_pins / remove_graph_node / etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString NodeGuid;
};

// ─── Overview structs ───────────────────────────────────────

/** Compact variable entry for blueprint overview. */
USTRUCT(BlueprintType)
struct FTetherVariableSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Type;
};

/** Compact function entry for blueprint overview. */
USTRUCT(BlueprintType)
struct FTetherFunctionSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	/** "Function", "Event", "Override" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Kind;

	/** Compact signature, e.g. "(Int, Float) -> Bool" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Signature;
};

/** Compact component entry for blueprint overview. */
USTRUCT(BlueprintType)
struct FTetherComponentSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString ComponentClass;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString ParentName;
};

/** Full blueprint overview — one call replaces Variables + Functions + Components + Interfaces. */
USTRUCT(BlueprintType)
struct FTetherBlueprintOverview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString BlueprintName;

	/** First native ancestor class name: "Actor", "Character", "AnimInstance", "UserWidget", etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString BlueprintType;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FTetherClassInfo ParentClass;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FTetherVariableSummary> Variables;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FTetherFunctionSummary> Functions;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FTetherComponentSummary> Components;

	/** Interface class names */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FString> Interfaces;

	/** Event dispatcher names */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FString> EventDispatchers;

	/** All graph names (EventGraph, functions, macros) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FString> GraphNames;
};

// ─── Event dispatcher structs ───────────────────────────────

/** Describes an event dispatcher with its parameter signature. */
USTRUCT(BlueprintType)
struct FTetherEventDispatcherInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	/** Parameters of the dispatcher delegate */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FTetherFunctionParam> Params;
};

// ─── Graph listing structs ──────────────────────────────────

/** Lightweight description of a graph in a Blueprint. */
USTRUCT(BlueprintType)
struct FTetherGraphInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	/** "EventGraph", "Function", "Macro", "EventDispatcher" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString GraphType;
};

// ─── Execution flow structs ─────────────────────────────────

/** A single outgoing exec-pin connection from an execution step. */
USTRUCT(BlueprintType)
struct FTetherExecConnection
{
	GENERATED_BODY()

	/** Exec output pin name (e.g. "then", "True", "False", "Completed") */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString PinName;

	/** Index of the target step in the result array (-1 = unconnected) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	int32 TargetStepIndex = -1;
};

/** A single step in the execution flow of a function graph. */
USTRUCT(BlueprintType)
struct FTetherExecStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	int32 StepIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString NodeTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString NodeType;

	/** Extra context: called function, variable name, cast target, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Detail;

	/** Outgoing exec-pin connections (branching info) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FTetherExecConnection> ExecOutputs;
};

// ─── Pin connection structs ─────────────────────────────────

/** A single pin-to-pin connection between two nodes in a graph. */
USTRUCT(BlueprintType)
struct FTetherPinConnection
{
	GENERATED_BODY()

	/** Source node index (matches GetFunctionNodes order with empty filter) */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	int32 SourceNodeIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString SourcePinName;

	/** Target node index */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	int32 TargetNodeIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString TargetPinName;

	/** True for exec wires, false for data wires */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bIsExec = false;
};

// ─── Component property value ───────────────────────────────

/** A single non-default property value on a component or CDO. */
USTRUCT(BlueprintType)
struct FTetherPropertyValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Type;

	/** Value as export-text string */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Category;
};

// ─── Node search result ─────────────────────────────────────

/** A node found by cross-graph search. */
USTRUCT(BlueprintType)
struct FTetherNodeSearchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString GraphName;

	/** "EventGraph", "Function", "Macro" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString GraphType;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString NodeTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString NodeType;

	/** Variable name, function name, cast target, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Detail;
};

// ─── Timeline structs ───────────────────────────────────────

/** A single track in a Timeline. */
USTRUCT(BlueprintType)
struct FTetherTimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString TrackName;

	/** "Float", "Vector", "LinearColor", "Event" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString TrackType;
};

/** Describes a Timeline component in a Blueprint. */
USTRUCT(BlueprintType)
struct FTetherTimelineInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	float Length = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bAutoPlay = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bLoop = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bReplicated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	TArray<FTetherTimelineTrack> Tracks;
};

/** A single message produced by the Blueprint compiler. */
USTRUCT(BlueprintType)
struct FTetherCompileMessage
{
	GENERATED_BODY()

	/** "Error" | "Warning" | "Note" | "Info" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Severity;

	/** Plain-text message with object/node tokens flattened to names. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString Message;

	/** NodeGuid (digits) of the first graph node referenced by the message, or "" if none. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString NodeGuid;
};

USTRUCT(BlueprintType)
struct FTetherBreakpointInfo
{
	GENERATED_BODY()

	/** Graph name the node lives in (e.g. "EventGraph", "MyFunction"). Empty if graph unresolved. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString GraphName;

	/** NodeGuid (digits) of the node the breakpoint is attached to. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString NodeGuid;

	/** User-facing node title — best-effort, may be empty if the node was deleted. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	FString NodeTitle;

	/** True if the breakpoint is enabled (as requested by the user — ignores single-step transient state). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint")
	bool bEnabled = false;
};

// ─── Semantic summary structs (understanding layer) ──────────

/**
 * LLM-ready high-level digest of a Blueprint. Replaces the 5-6 round-trips
 * an agent would otherwise need (overview + variables + functions +
 * components + interfaces + dispatchers) with a single call that also
 * adds aggregate stats (variable categories, key referenced classes,
 * referenced assets) not available anywhere else.
 */
USTRUCT(BlueprintType)
struct FTetherBlueprintSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Path;

	/** Short name of immediate superclass. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString ParentClass;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString ParentClassPath;

	/** First native ancestor ("Actor", "Character", "UserWidget", etc.). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString BlueprintType;

	/** Interface class short names. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> Interfaces;

	/** Names of events this BP actually overrides / handles
	 *  (entry nodes found in UbergraphPages + AnimGraph). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> EventsHandled;

	/** Non-internal function names defined on this BP. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> PublicFunctions;

	/** Event dispatcher names declared on this BP. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> EventDispatchers;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 VariableCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 InstanceEditableCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 ReplicatedVariableCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 FunctionCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 MacroCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 ComponentCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 TimelineCount = 0;

	/** Sum of nodes across every graph (ubergraph + functions + macros). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 TotalNodeCount = 0;

	/** Deduped variable categories across all variables. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> VariableCategories;

	/** Most-called external classes (e.g. "KismetSystemLibrary",
	 *  "GameplayStatics"), sorted by call-site frequency, top 10. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> KeyReferencedClasses;

	/** Asset paths referenced by pin defaults + component class refs,
	 *  deduped, top 10. Gives a sense of what content this BP "uses". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> KeyReferencedAssets;
};

/** Per-function semantic digest — pre-formatted exec outline + aggregate
 *  reads/writes/calls/fires. Replaces GetFunctionNodes +
 *  GetFunctionExecutionFlow + GetNodePinConnections + manual assembly. */
USTRUCT(BlueprintType)
struct FTetherFunctionSemantics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Name;

	/** "Function" | "Event" | "Override" | "Macro" | "EventGraph". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Kind;

	/** "Public" | "Protected" | "Private". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Access;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsPure = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsOverride = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherFunctionParam> Params;

	/** Indented human-readable outline of the exec flow. Each entry is
	 *  one line, two-space indent per nesting level. e.g.
	 *    "Branch (IsValid(Target))"
	 *    "  True → Call GameplayStatics.ApplyDamage"
	 *    "  False → Log 'no target'" */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> ExecOutline;

	/** Variable names read in this function body (deduped). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> ReadsVariables;

	/** Variable names written (Set) in this function body. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> WritesVariables;

	/** Functions called (formatted "ClassName.FuncName", deduped). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> CallsFunctions;

	/** Event dispatchers fired (Call). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> FiresDispatchers;

	/** Classes spawned via SpawnActorFromClass / similar. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> SpawnsClasses;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 NodeCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bHasLoops = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bHasBranches = false;

	/** Text from UEdGraphNode_Comment boxes inside this graph. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> CommentBlocks;

	/** One-line tooltip / metadata description if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Description;
};

/** Positioning + sizing data for a Blueprint graph node. Used for layout.
 *
 *  ⚠️ Size caveat: NodeWidth / NodeHeight on UEdGraphNode are only populated
 *  after Slate has rendered the node at least once in an open graph panel.
 *  For Comment nodes (UEdGraphNode_Comment) sizes are user-authored and
 *  always authoritative. For everything else, expect StoredWidth/Height == 0
 *  on a freshly-loaded BP that isn't open; fall back to EstimatedWidth/
 *  Height (synthesised from title length + visible pin count).
 *  EffectiveWidth/Height picks Stored when available, else Estimated.
 */
USTRUCT(BlueprintType)
struct FTetherNodeLayout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 PosX = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 PosY = 0;

	/** Width as stored on UEdGraphNode::NodeWidth.
	 *  Authoritative for Comment nodes; 0 until Slate renders for others. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 StoredWidth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 StoredHeight = 0;

	/** Synthesised from title length + visible pin count when Stored is 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 EstimatedWidth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 EstimatedHeight = 0;

	/** Stored if nonzero, else Estimated. Used for corner calculations. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 EffectiveWidth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 EffectiveHeight = 0;

	/** All four corners in graph coordinates. TopLeft == (PosX, PosY). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D TopLeft      = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D TopRight     = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D BottomLeft   = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D BottomRight  = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D Center       = FVector2D::ZeroVector;

	/** True if this is a UEdGraphNode_Comment (sizes always authoritative). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsCommentBox = false;

	/** True when EffectiveWidth/Height came from StoredWidth/Height
	 *  (reliable). False when we had to estimate because Stored was 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bSizeIsAuthoritative = false;
};

/** Pixel-accurate pin position, queried from the live Slate SGraphPin
 *  widget. Only populated when the pin's owning node's graph is the
 *  active tab in an open Blueprint editor. Returned by
 *  GetRenderedNodeInfo. */
USTRUCT(BlueprintType)
struct FTetherRenderedPin
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Direction;  // "input" or "output"
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 DirectionIndex = 0;

	/** Pin position relative to its owning node's top-left corner. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D NodeOffset = FVector2D::ZeroVector;

	/** Pin position in graph coordinates (node graph pos + node offset). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D GraphPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsExec = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsHidden = false;
};

/** Pixel-accurate node geometry queried from the live SGraphNode widget.
 *  Size comes from the widget's cached desired size (what Slate will
 *  render at). Pin offsets come from SGraphPin::GetNodeOffset — the
 *  authoritative pixel position each wire endpoint actually lands at. */
USTRUCT(BlueprintType)
struct FTetherRenderedNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Title;

	/** Graph-space position (top-left). Matches UEdGraphNode::NodePosX/Y. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D GraphPosition = FVector2D::ZeroVector;

	/** Actual rendered size in graph units, from SGraphNode->GetDesiredSize(). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D Size = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherRenderedPin> Pins;

	/** True when Size & Pins were read from the live widget; false if the
	 *  node's widget couldn't be found (graph not open / not yet rendered). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsLive = false;
};

/** Estimated per-pin position for layout. Pin X is inset from node edges;
 *  pin Y is derived from the pin's direction-specific index × row height. */
USTRUCT(BlueprintType)
struct FTetherPinLayout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Name;

	/** "input" or "output". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Direction;

	/** Index among pins of the same direction on this node (0-based,
	 *  hidden pins skipped). Used to drive pin Y. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 DirectionIndex = 0;

	/** Position relative to the node's top-left origin. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D LocalOffset = FVector2D::ZeroVector;

	/** Absolute position in graph coordinates (node pos + local offset). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsExec = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsHidden = false;

	/** Always true for now — pin coordinates are synthesised. A running
	 *  Slate graph editor is required for pixel-accurate pin positions. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsEstimated = true;
};

/** Pre-spawn node size estimate. Same heuristic as FTetherNodeLayout's
 *  estimated path, but keyed off node *kind* + parameters rather than an
 *  existing UEdGraphNode — lets callers reserve space before spawning. */
USTRUCT(BlueprintType)
struct FTetherNodeSizeEstimate
{
	GENERATED_BODY()

	/** Predicted width in graph units. Matches EstimateNodeSize's output. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 Width = 180;

	/** Predicted height in graph units. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 Height = 60;

	/** Visible input pin count (exec + data, hidden pins excluded). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 InputPinCount = 0;

	/** Visible output pin count. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 OutputPinCount = 0;

	/** Echoes the input Kind — "function_call", "branch", etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Kind;

	/** Diagnostic notes: "function not found", "fallback default", etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Notes;

	/** True if the estimate succeeded; false if kind/params couldn't be
	 *  resolved and we fell back to a generic default size. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bResolved = false;
};

/** Result of AutoLayoutGraph. */
USTRUCT(BlueprintType)
struct FTetherLayoutResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bSucceeded = false;

	/** Number of nodes whose NodePosX/Y was updated. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 NodesPositioned = 0;

	/** Depth of the layered layout (layer 0 = leftmost sources). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 LayerCount = 0;

	/** Width of the final laid-out area in graph units. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 BoundsWidth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 BoundsHeight = 0;

	/** Per-node diagnostics: cycles broken, unreachable nodes, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> Warnings;
};

/** A single finding from LintBlueprint. `code` is a stable machine-readable
 *  tag; `message` is human text. Location fields are populated only when
 *  meaningful (e.g. NodeGuid for per-node issues, VariableName for per-var). */
USTRUCT(BlueprintType)
struct FTetherLintIssue
{
	GENERATED_BODY()

	/** "error" (compile-blocking), "warning" (quality issue), "info" (style). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Severity;

	/** Stable identifier (e.g. "OrphanNode", "OversizedFunction", "UnusedVariable"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Code;

	/** Plain-English description with the specific offending name(s) inlined. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Message;

	/** Graph where the issue lives ("" for BP-level issues like unused class vars). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString GraphName;

	/** Node GUID for per-node issues ("" if not applicable). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;

	/** Variable name for per-variable issues ("" if not applicable). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString VariableName;

	/** Function name for per-function issues ("" if not applicable). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString FunctionName;
};

/** Full pin description for one node pin (listed regardless of wire state). */
USTRUCT(BlueprintType)
struct FTetherPinInfo
{
	GENERATED_BODY()

	/** Pin's internal name (e.g. "then", "Center", "InString", "Condition"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Name;

	/** User-facing pin label shown in the graph editor (sometimes differs). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString DisplayName;

	/** Human-readable type. Examples: "Exec", "Bool", "Int", "Float",
	 *  "Vector", "String", "Actor", "Array of Int", "Class<PrimitiveComponent>",
	 *  "Enum<EMovementMode>". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Type;

	/** "input" or "output". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Direction;

	/** Raw FEdGraphPinType::PinCategory (e.g. "exec", "int", "object"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Category;

	/** Raw FEdGraphPinType::PinSubCategory (rarely useful directly;
	 *  see SubCategoryObjectPath for class/struct name). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString SubCategory;

	/** Path of the subcategory object (class for Object/SoftObject/Class
	 *  pins, struct for Struct pins, enum for Enum pins). Empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString SubCategoryObjectPath;

	/** Effective default value as a string. For object pins this is the
	 *  object path; for text pins the text contents; for literals the
	 *  string form. Empty when the pin is connected (default is ignored)
	 *  or when no default has been set. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString DefaultValue;

	/** True if the pin has an object reference default (distinct from
	 *  a literal DefaultValue string). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bHasDefaultObject = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsExec = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsConnected = false;

	/** Number of currently-linked pins on the other side. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 LinkCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsArray = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsSet = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsMap = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsReference = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsConst = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsHidden = false;

	/** True for the implicit "self" / target pin most K2 CallFunction nodes carry. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsSelfPin = false;

	/** "None" | "Array" | "Set" | "Map". Mirrors PinType.ContainerType so
	 *  callers don't have to read the bIsArray/bIsSet/bIsMap booleans above. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString ContainerKind;

	/** For Map pins: human-readable value-type (the V in Map<K, V>). Empty for
	 *  non-map pins. The Type field above already shows "Map<K, V>" combined;
	 *  these fields let callers see the V side without string surgery. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString MapValueType;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString MapValueCategory;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString MapValueSubCategory;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString MapValueSubCategoryObjectPath;

	/** Connected pins on the other side, each formatted `"<node_guid>:<pin_name>"`
	 *  where node_guid is the 32-hex Digits form and pin_name is the target
	 *  pin's internal FName. Empty if the pin has no links. Populated by
	 *  DescribeNode; other pin-returning APIs may leave this empty for cost
	 *  reasons (see their docs). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> LinkedTo;
};

/**
 * One entry in the Blueprint action database — represents a node that *could*
 * be spawned in a graph. Returned by ListSpawnableActions; the Key field is
 * what SpawnNodeByActionKey takes to materialize the node.
 */
USTRUCT(BlueprintType)
struct FTetherSpawnableAction
{
	GENERATED_BODY()

	/** Opaque, stable-within-an-editor-session identifier. Pass to
	 *  SpawnNodeByActionKey to spawn this exact node. Built from the spawner's
	 *  FBlueprintNodeSignature so it survives across registry walks but not
	 *  across editor restarts (re-list then re-spawn after restart). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Key;

	/** User-facing menu name (e.g. "Get Player Controller", "Print String",
	 *  "Set MyVariable"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Title;

	/** Editor menu category (e.g. "Math|Float", "Utilities|String", "Variables").
	 *  Pipe-separated for nested categories. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Tooltip;

	/** Searchable keyword text the editor surfaces in palette search. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Keywords;

	/** K2Node class short name this spawner produces (e.g. "K2Node_CallFunction",
	 *  "K2Node_VariableGet", "K2Node_IfThenElse"). Useful for filtering. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeType;

	/** Owning class / asset short name — for function spawners this is the class
	 *  declaring the function; for variable spawners the var-owner; for engine
	 *  intrinsics like Branch/Sequence this may be the node class itself. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString OwningClass;

	/** Full path of the owning class/asset when resolvable (e.g.
	 *  "/Script/Engine.KismetSystemLibrary"). Empty when not derivable. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString OwningClassPath;
};

/** A single reference site surfaced by Find* cross-reference queries. */
USTRUCT(BlueprintType)
struct FTetherReference
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString GraphName;

	/** "EventGraph" | "Function" | "Macro". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString GraphType;

	/** NodeGuid (digits). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeTitle;

	/** "read" | "write" | "call" | "bind" | "unbind" | "event". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Kind;
};

/** Single selected node in the currently-focused Blueprint editor. */
USTRUCT(BlueprintType)
struct FTetherSelectedNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeClass;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Title;
};

/** Snapshot of the user's current Blueprint-editor focus. Empty fields when
 *  no BP editor is active. */
USTRUCT(BlueprintType)
struct FTetherEditorFocusState
{
	GENERATED_BODY()

	/** Path of the BP currently shown in the focused editor tab. Empty when
	 *  no BP editor has focus. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString BlueprintPath;

	/** Name of the graph currently visible in the BP editor's main tab
	 *  (function / macro / event graph). Empty when not resolvable. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString FocusedGraphName;

	/** "EventGraph" | "Function" | "Macro" | "" (when not in that BP). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString FocusedGraphType;

	/** Selected nodes in the focused graph. Empty array when nothing's
	 *  selected or when no graph is focused. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherSelectedNode> SelectedNodes;

	/** Paths of every BP currently open in an asset-editor tab (including
	 *  non-focused ones). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> OpenBlueprintPaths;
};

/** Summary of a cross-Blueprint rename operation. */
USTRUCT(BlueprintType)
struct FTetherRenameReport
{
	GENERATED_BODY()

	/** true when the defining-BP rename succeeded. False results leave the
	 *  rest of the report empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bSuccess = false;

	/** Asset paths of BPs whose call/get/set sites were updated. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> UpdatedBlueprints;

	/** Total number of K2Node references rewritten across all BPs. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 UpdatedNodeCount = 0;

	/** Diagnostic text when bSuccess is false, or warnings when partial. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Message;
};

/** Per-operation result row in an ApplyGraphOps batch. */
USTRUCT(BlueprintType)
struct FTetherGraphOpResult
{
	GENERATED_BODY()

	/** Index into the input ops array (mirrors the caller's order). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 Index = 0;

	/** Op-kind string echoed from input ("add_call_function" etc.). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Op;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bSuccess = false;

	/** GUID of a newly-spawned node, for ops that spawn nodes; empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NewNodeGuid;

	/** Error detail on failure; empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Message;
};

/** Report of a replace-node-preserving-connections op. */
USTRUCT(BlueprintType)
struct FTetherReplaceNodeReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bSuccess = false;

	/** GUID of the new node, or empty on failure. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NewNodeGuid;

	/** Pins on the old node that were successfully rewired to the new node. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> ReconnectedPins;

	/** Pins on the old node whose links could not be carried over (no matching
	 *  pin on the new node, or types were incompatible). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FString> DroppedPins;

	/** Diagnostic text on failure. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Message;
};

/** Single node entry used in a graph diff's added/removed lists. */
USTRUCT(BlueprintType)
struct FTetherGraphDiffNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeClass;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Title;
};

/** Single wire entry used in a graph diff's added/removed lists. */
USTRUCT(BlueprintType)
struct FTetherGraphDiffWire
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString SrcNodeGuid;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString SrcPinName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString DstNodeGuid;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString DstPinName;
};

/** Result of comparing two graph snapshots. */
USTRUCT(BlueprintType)
struct FTetherGraphDiff
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherGraphDiffNode> AddedNodes;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherGraphDiffNode> RemovedNodes;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherGraphDiffWire> AddedWires;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherGraphDiffWire> RemovedWires;
};

/** One per-node hit count from get_pie_node_coverage. */
USTRUCT(BlueprintType)
struct FTetherNodeCoverageEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeTitle;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString GraphName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 HitCount = 0;

	/** FPlatformTime::Seconds() timestamp of the most recent sample. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") double LastHitTime = 0.0;
};

/** One param / local / return value captured when a breakpoint hits. */
USTRUCT(BlueprintType)
struct FTetherBreakpointHitValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Type;

	/** ExportText form of the value at the instant the breakpoint hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Value;

	/** "param" | "local" | "return" | "instance". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Kind;

	/** For Kind="instance": path of the UClass that declares the variable
	 *  (e.g. "/Game/Blueprints/BP_Hero.BP_Hero_C"). Empty for stack-frame
	 *  values (param/local/return) — those are scoped to a function, not a class. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString OwnerClass;
};

/** Snapshot of the most recent breakpoint hit for a Blueprint. */
USTRUCT(BlueprintType)
struct FTetherBreakpointHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bHasHit = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString BlueprintPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString FunctionName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString GraphName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeTitle;

	/** Path of the object whose execution hit the breakpoint. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString SelfPath;

	/** FPlatformTime::Seconds() timestamp of the hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") double HitTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherBreakpointHitValue> Values;
};

/** Single CDO override row for FindCdoVariableOverrides. */
USTRUCT(BlueprintType)
struct FTetherCdoOverride
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString BlueprintPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString VariableName;

	/** Parent CDO's value (ExportText form). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString ParentValue;

	/** Child CDO's value (ExportText form) — differs from ParentValue. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString ChildValue;
};

/** Reference site for a cross-Blueprint query — same fields as FTetherReference
 *  plus the asset path of the containing Blueprint. */
USTRUCT(BlueprintType)
struct FTetherGlobalReference
{
	GENERATED_BODY()

	/** Full object path of the containing Blueprint (e.g. "/Game/BP_X.BP_X"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString BlueprintPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString GraphName;

	/** "EventGraph" | "Function" | "Macro". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString GraphType;

	/** NodeGuid (digits). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeTitle;

	/** "call". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Kind;
};

/** One debug-print call site found by FindBlueprintDebugPrints. */
USTRUCT(BlueprintType)
struct FTetherDebugPrintSite
{
	GENERATED_BODY()

	/** Full object path of the containing Blueprint (e.g. "/Game/BP_X.BP_X"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString BlueprintPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString GraphName;

	/** "EventGraph" | "Function" | "Macro". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString GraphType;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeTitle;

	/** Short name of the KismetSystemLibrary function — "PrintString", "PrintText",
	 *  "PrintWarning". Lets callers bucket prints by flavour. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString FunctionName;

	/** Literal default of the `InString` / `InText` pin when it's not wired to
	 *  another node. Empty when the pin is connected (dynamic message) or when
	 *  the default is the engine's placeholder "Hello". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString StringLiteral;

	/** True when `InString` / `InText` is connected to another node — the
	 *  message is computed at runtime, not a static literal. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bHasConnectedInput = false;
};

/** Unified one-shot description of a single graph node. Combines title,
 *  position, classification, pins (with link targets), and the
 *  K2Node-subclass-specific fields a caller typically wants — all in a
 *  single tether round-trip. Returned by DescribeNode. */
USTRUCT(BlueprintType)
struct FTetherNodeDescription
{
	GENERATED_BODY()

	/** NodeGuid in 32-hex digits form. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeGuid;

	/** `GetNodeTitle(ENodeTitleType::ListView)` — the palette-style name. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Title;

	/** Coarse type from ClassifyNode: "FunctionCall", "VariableGet",
	 *  "VariableSet", "Branch", "Cast", "Event", "CustomEvent", "Macro",
	 *  "Spawn", "Timeline", "Knot", "MakeStruct", "BreakStruct", ... */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeType;

	/** Exact UE class name of the UEdGraphNode (e.g. "K2Node_CallFunction",
	 *  "K2Node_VariableGet", "K2Node_IfThenElse"). Stable across UE versions;
	 *  prefer this when dispatching on node type in code. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString NodeClass;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 PosX = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 PosY = 0;

	/** Rendered size if Slate has arranged the node; else estimated. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 Width = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 Height = 0;

	/** User-authored node comment (floating text above the node). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Comment;

	/** "Enabled" | "Disabled" | "DevelopmentOnly". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString EnabledState;

	// ── Subclass-specific fields (populated based on NodeClass) ──

	/** Function call: target class short name. Cast: target class.
	 *  Empty when not applicable. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString TargetClass;

	/** Function call: function internal name. Event / CustomEvent: event name.
	 *  Empty when not applicable. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString TargetName;

	/** VariableGet / VariableSet: the variable's internal name. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString VariableName;

	/** VariableGet / VariableSet: "member" (Blueprint class variable),
	 *  "local" (function local), or "external" (inherited / foreign class). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString VariableScope;

	/** VariableGet / VariableSet: resolved variable type (PinTypeToHuman form). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString VariableType;

	/** Function call: `true` if pure (no exec pin). Cast: true for pure cast. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsPure = false;

	/** Function call: true if the target UFunction is const. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsConst = false;

	/** MakeStruct / BreakStruct: struct class path. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString StructType;

	/** Literal nodes (MakeLiteralInt / Bool / etc.): current literal value
	 *  (from the corresponding pin's default). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString LiteralValue;

	/** Macro instance: the macro graph's display name, empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString MacroGraph;

	/** AddDelegate / RemoveDelegate / CallDelegate: the delegate property name. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString DelegateName;

	/** For fanout-style exec nodes (ExecutionSequence, Branch, Select):
	 *  number of wired exec outputs. 0 for others. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") int32 ExecOutCount = 0;

	/** All visible pins with full type info, default value, and LinkedTo
	 *  populated. Same ordering as GetNodePins. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherPinInfo> Pins;
};

/** UFunction signature query result: full parameter list with types and
 *  declared default values, plus the blueprint-facing flags (pure, const,
 *  static, latent). Returned by GetFunctionSignature. */
USTRUCT(BlueprintType)
struct FTetherFunctionSignature
{
	GENERATED_BODY()

	/** True when the function was found and the other fields are meaningful. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bFound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString FunctionName;

	/** Owning class short name (e.g. "KismetSystemLibrary", "Actor"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString OwningClass;

	/** Full class path (e.g. "/Script/Engine.KismetSystemLibrary"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString OwningClassPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsPure = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsConst = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsStatic = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsLatent = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsBlueprintCallable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsBlueprintPure = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") bool bIsNative = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Category;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") FString Tooltip;

	/** Ordered parameter list. Includes inputs, outputs, and the return
	 *  value (the return is marked with bIsOutput=true and Name="ReturnValue"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Blueprint") TArray<FTetherFunctionParam> Parameters;
};

// ─── Function Library ────────────────────────────────────────

UCLASS()
class TETHER_API UTetherBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ── Class hierarchy ──

	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool GetBlueprintParentClass(const FString& BlueprintPath, FTetherClassInfo& OutParentInfo);

	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherClassInfo> GetBlueprintClassHierarchy(const FString& BlueprintPath);

	// ── Variables ──

	/** Get all variables defined in a Blueprint (not inherited ones). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherVariableInfo> GetBlueprintVariables(const FString& BlueprintPath, bool bIncludeInherited = false);

	// ── Functions / Events ──

	/** Get all functions and events defined in a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherFunctionInfo> GetBlueprintFunctions(const FString& BlueprintPath, bool bIncludeInherited = false);

	// ── Components ──

	/** Get the component tree of an Actor Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherComponentInfo> GetBlueprintComponents(const FString& BlueprintPath);

	// ── Interfaces ──

	/** Get all interfaces implemented by a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherInterfaceInfo> GetBlueprintInterfaces(const FString& BlueprintPath);

	// ── Graph analysis ──

	/**
	 * Get the call graph of a specific function in a Blueprint.
	 * Returns only the outgoing call edges (what functions/events this function calls).
	 * Lightweight — no node details, just the call relationships.
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param FunctionName   Name of the function/event to analyze. Empty string = EventGraph.
	 * @return Array of call edges
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherCallEdge> GetFunctionCallGraph(const FString& BlueprintPath, const FString& FunctionName);

	/**
	 * Get all nodes in a specific function graph.
	 * Can be filtered by node type to reduce output size.
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param FunctionName   Name of the function/event. Empty string = EventGraph.
	 * @param NodeTypeFilter Optional filter: "FunctionCall", "VariableGet", "VariableSet",
	 *                       "Branch", "Cast", "Macro", "Event", etc. Empty = all nodes.
	 * @return Array of node info
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherNodeInfo> GetFunctionNodes(const FString& BlueprintPath, const FString& FunctionName, const FString& NodeTypeFilter);

	// ── Overview ──

	/**
	 * Get a compact overview of a Blueprint in a single call.
	 * Replaces separate calls to Variables + Functions + Components + Interfaces.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool GetBlueprintOverview(const FString& BlueprintPath, FTetherBlueprintOverview& OutOverview);

	// ── Event Dispatchers ──

	/** Get all event dispatchers defined in a Blueprint with their parameter signatures. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherEventDispatcherInfo> GetEventDispatchers(const FString& BlueprintPath);

	// ── Graph listing ──

	/** Get a lightweight list of all graphs (EventGraph, functions, macros, dispatchers). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherGraphInfo> GetGraphNames(const FString& BlueprintPath);

	// ── Execution flow ──

	/**
	 * Walk exec pins to produce an ordered execution flow for a function.
	 * Much more compact than GetFunctionNodes — only includes nodes on exec wires,
	 * with branching info preserved.
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param FunctionName   Name of function/event. Empty string = EventGraph.
	 * @return Ordered steps with branching info
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherExecStep> GetFunctionExecutionFlow(const FString& BlueprintPath, const FString& FunctionName);

	// ── Pin connections ──

	/**
	 * Get all pin-to-pin connections in a function graph.
	 * Node indices match the order of GetFunctionNodes(path, funcName, "").
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param FunctionName   Name of function/event. Empty string = EventGraph.
	 * @return All connections (exec + data wires)
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherPinConnection> GetNodePinConnections(const FString& BlueprintPath, const FString& FunctionName);

	// ── Component properties ──

	/**
	 * Get all non-default property values on a specific component.
	 * Only returns properties that differ from the component class CDO.
	 *
	 * @param BlueprintPath  Content path to the Blueprint
	 * @param ComponentName  Component variable name (from GetBlueprintComponents)
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherPropertyValue> GetComponentPropertyValues(const FString& BlueprintPath, const FString& ComponentName);

	// ── Cross-graph search ──

	/**
	 * Search for nodes across all graphs in a Blueprint.
	 * Matches node title, variable name, or function name against the query.
	 *
	 * @param BlueprintPath   Content path to the Blueprint
	 * @param Query           Search string (matched against title/detail, case-insensitive)
	 * @param NodeTypeFilter  Optional: "FunctionCall", "VariableGet", "VariableSet", etc. Empty = all.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherNodeSearchResult> SearchBlueprintNodes(const FString& BlueprintPath, const FString& Query, const FString& NodeTypeFilter);

	// ── Timelines ──

	/** Get all Timelines defined in a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherTimelineInfo> GetTimelineInfo(const FString& BlueprintPath);

	// ── Write operations ──

	/**
	 * Set the default value of a Blueprint variable.
	 * Use the same export-text format returned by GetBlueprintVariables DefaultValue field.
	 *
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetBlueprintVariableDefault(const FString& BlueprintPath, const FString& VariableName, const FString& Value);

	/**
	 * Set a property value on a Blueprint component template.
	 * Use export-text format for the value (same format as GetComponentPropertyValues).
	 *
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetComponentProperty(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName, const FString& Value);

	/**
	 * Add a new variable to a Blueprint.
	 * Type string: "Bool", "Int", "Float", "Double", "String", "Name", "Text",
	 *              "Vector", "Rotator", "Transform", "LinearColor",
	 *              or a class/struct name for object/struct references.
	 *              Prefix with "Array of " for arrays (e.g. "Array of Float").
	 *
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool AddBlueprintVariable(const FString& BlueprintPath, const FString& Name, const FString& TypeString, const FString& DefaultValue);

	/**
	 * Remove a member variable from a Blueprint by name. Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveBlueprintVariable(const FString& BlueprintPath, const FString& VariableName);

	/**
	 * Rename a member variable on a Blueprint. Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RenameBlueprintVariable(const FString& BlueprintPath, const FString& OldName, const FString& NewName);

	// ── Function-scope local variables ──
	//
	// Local variables live on UK2Node_FunctionEntry of a user-authored function
	// graph (not EventGraph, not macros). They're visible inside the function
	// body but not as class members; tooling that walks NewVariables won't see
	// them. These helpers operate on FunctionEntry.LocalVariables directly.

	/**
	 * List local variables declared inside a function graph. Returns the same
	 * FTetherVariableInfo shape used by GetBlueprintVariables — name, type,
	 * category, default value, instance-editable flag, tooltip description.
	 * Replication fields are always "None" (local vars can't be replicated).
	 *
	 * @param FunctionName  Function graph name as listed by GetGraphNames /
	 *                      GetBlueprintFunctions. Must be a Function graph;
	 *                      EventGraph and Macro graphs return an empty list.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherVariableInfo> GetFunctionLocalVariables(
		const FString& BlueprintPath, const FString& FunctionName);

	/**
	 * Add a local variable to a function graph. Same TypeString grammar as
	 * AddBlueprintVariable ("Bool" / "Int" / "Float" / "Vector" / class paths /
	 * "Array of X" prefix). Compiles on success so the variable becomes visible
	 * to subsequent variable-get/set node spawns.
	 *
	 * @return true if the variable was added; false if the function graph is
	 *         missing, the name is taken, or the TypeString couldn't be parsed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool AddFunctionLocalVariable(const FString& BlueprintPath,
		const FString& FunctionName, const FString& VariableName,
		const FString& TypeString, const FString& DefaultValue);

	/**
	 * Remove a local variable from a function graph. Any variable-get/set
	 * nodes that reference it inside the function become invalid after compile
	 * — delete or repoint them before removing if you care about a clean graph.
	 *
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveFunctionLocalVariable(const FString& BlueprintPath,
		const FString& FunctionName, const FString& VariableName);

	/**
	 * Rename a local variable within a function graph. Variable-get/set nodes
	 * in the function are updated to reference the new name.
	 *
	 * @return true on success; false if the old name doesn't exist or the new
	 *         name is already taken.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RenameFunctionLocalVariable(const FString& BlueprintPath,
		const FString& FunctionName, const FString& OldName, const FString& NewName);

	/**
	 * Set a local variable's default value. Uses the same export-text format
	 * as SetBlueprintVariableDefault (e.g. "true", "3.14", "(X=1,Y=2,Z=3)").
	 *
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetFunctionLocalVariableDefault(const FString& BlueprintPath,
		const FString& FunctionName, const FString& VariableName,
		const FString& Value);

	/**
	 * Add a variable-get / variable-set node for a function-scope local
	 * variable. Equivalent to AddVariableNode but resolves against the
	 * function's LocalVariables instead of BP->NewVariables — AddVariableNode
	 * can't spawn local-var nodes because the VariableReference has to carry
	 * the function scope, not the self-member scope.
	 *
	 * @return GUID of the new node, or "" if the variable is missing / the
	 *         graph isn't a function graph.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddFunctionLocalVariableNode(const FString& BlueprintPath,
		const FString& FunctionName, const FString& VariableName,
		bool bIsSet, int32 NodePosX, int32 NodePosY);

	/**
	 * Add an interface implementation to a Blueprint. InterfacePath can be a content path
	 * to a Blueprint interface (e.g. "/Game/BPI_Foo") or a native class path
	 * ("/Script/MyModule.UMyInterface"). Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool AddBlueprintInterface(const FString& BlueprintPath, const FString& InterfacePath);

	/**
	 * Remove an interface implementation from a Blueprint by class name or path. Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveBlueprintInterface(const FString& BlueprintPath, const FString& InterfaceNameOrPath);

	/**
	 * Add a new component to an Actor Blueprint's SimpleConstructionScript.
	 * ComponentClassPath: native class path (e.g. "/Script/Engine.StaticMeshComponent")
	 * or Blueprint component class path with trailing _C.
	 * ComponentName: variable name for the new component (must be unique within the BP).
	 * ParentComponentName: optional — attach under this component (empty = attach under root, or become root if none).
	 * Compiles on success.
	 * @return true on success
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool AddBlueprintComponent(const FString& BlueprintPath, const FString& ComponentClassPath, const FString& ComponentName, const FString& ParentComponentName);

	// ── Graph node write ops ──
	// Intentionally return minimal values (GUID or bool) to keep round-trip cost low.
	// None auto-compile; call CompileBlueprint once after a batch of graph edits.
	// Callers drive layout via explicit (X, Y) — recommended: 300px column, 150px row spacing.

	/**
	 * Add a Call-Function node to a graph.
	 * @param TargetClassPath  Empty for self (the Blueprint's own generated/parent class); otherwise a class path.
	 * @return node GUID on success, empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddCallFunctionNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& TargetClassPath, const FString& FunctionName, int32 NodePosX, int32 NodePosY);

	/**
	 * Add a VariableGet (bIsSet=false) or VariableSet (bIsSet=true) node for a self-member variable.
	 * Variable may be declared on the Blueprint or inherited from a parent.
	 * @return node GUID on success, empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddVariableNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& VariableName, bool bIsSet, int32 NodePosX, int32 NodePosY);

	/**
	 * 为显式外部类中当前蓝图可访问的属性添加 VariableGet（bIsSet=false）或 VariableSet（bIsSet=true）节点。
	 * Add a VariableGet (bIsSet=false) or VariableSet (bIsSet=true) node for a property that the
	 * current Blueprint may access on an explicit external class.
	 * @param OwnerClassPath 必须为非空类路径；继承属性会绑定到实际声明该属性的类，外部蓝图不能访问 BlueprintPrivate 属性。
	 *                       Must be a non-empty class path; inherited properties bind to their declaring class, and unrelated Blueprints cannot access BlueprintPrivate properties.
	 * @return 成功时返回节点 GUID，失败时记录诊断并返回空字符串。Node GUID on success; logs a diagnostic and returns an empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddExternalVariableNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& OwnerClassPath, const FString& VariableName, bool bIsSet,
		int32 NodePosX, int32 NodePosY);

	/**
	 * Connect two pins identified by node GUID + pin name. Uses the K2 schema's TryCreateConnection
	 * so it respects type coercion and exec-link rules.
	 * @return true on success; false when nodes/pins missing or types incompatible.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool ConnectGraphPins(const FString& BlueprintPath, const FString& GraphName,
		const FString& SourceNodeGuid, const FString& SourcePinName,
		const FString& TargetNodeGuid, const FString& TargetPinName);

	/** Remove a node by GUID, breaking all its pin links. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveGraphNode(const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid);

	/** Reposition a node by GUID for tidy layout. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetGraphNodePosition(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, int32 NodePosX, int32 NodePosY);

	/**
	 * Add a K2Node_Event that overrides a parent-class BlueprintImplementableEvent / BlueprintNativeEvent
	 * (e.g. "ReceiveTick", "ReceiveBeginPlay" on AActor). If a matching event already exists on the graph
	 * (including a "ghost" default event), its existing GUID is returned and it is re-enabled/repositioned.
	 * @param ParentClassPath  Empty = BP's ParentClass; otherwise full class path.
	 * @return node GUID on success, empty string on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddEventNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& ParentClassPath, const FString& EventName, int32 NodePosX, int32 NodePosY);

	/**
	 * Set a pin's default (literal) value via the K2 schema. Accepts the same text form the Details
	 * panel uses — e.g. "1.0", "(X=1,Y=0,Z=0)", "true".
	 * @return true on success; false if node/pin missing or the schema rejects the value.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetPinDefaultValue(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& PinName, const FString& NewDefaultValue);

	// ═══ P0 — Control-flow / basic nodes ═══════════════════════════

	/** Add a Branch (If-Then-Else) node. Returns node GUID or "" on failure. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddBranchNode(const FString& BlueprintPath, const FString& GraphName,
		int32 NodePosX, int32 NodePosY);

	/** Add a Sequence node with the given number of Then pins (clamped 2..16). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddSequenceNode(const FString& BlueprintPath, const FString& GraphName,
		int32 PinCount, int32 NodePosX, int32 NodePosY);

	/** Add a DynamicCast node. `bPure` → no exec pins. `TargetClassPath` can be a native or BP class path. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddCastNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& TargetClassPath, bool bPure, int32 NodePosX, int32 NodePosY);

	/** Add a Self reference node. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddSelfNode(const FString& BlueprintPath, const FString& GraphName,
		int32 NodePosX, int32 NodePosY);

	/** Add a Custom Event node (K2Node_CustomEvent) with the given name. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddCustomEventNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& EventName, int32 NodePosX, int32 NodePosY);

	// ═══ P0 — Function/event graph management ═══════════════════════

	/** Create an empty user-defined function graph (with default entry/return). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool CreateFunctionGraph(const FString& BlueprintPath, const FString& FunctionName);

	/** Remove a user-defined function graph by name. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveFunctionGraph(const FString& BlueprintPath, const FString& FunctionName);

	/** Rename a user-defined function graph. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RenameFunctionGraph(const FString& BlueprintPath, const FString& OldName, const FString& NewName);

	/**
	 * Add a parameter to a function graph.
	 * `bIsReturn=false` → input pin on FunctionEntry. `bIsReturn=true` → output pin on FunctionResult
	 * (result node auto-created if absent). TypeString uses the same format as AddBlueprintVariable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool AddFunctionParameter(const FString& BlueprintPath, const FString& FunctionName,
		const FString& ParamName, const FString& TypeString, bool bIsReturn);

	/**
	 * Set flags on a function (pure, const, category, access).
	 * AccessSpecifier: "public" | "protected" | "private" (empty = leave unchanged).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetFunctionMetadata(const FString& BlueprintPath, const FString& FunctionName,
		bool bPure, bool bConst, const FString& Category, const FString& AccessSpecifier);

	// ═══ P0 — Event Dispatcher write ops ════════════════════════════

	/** Add a new Event Dispatcher (multicast delegate) with no parameters. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool AddEventDispatcher(const FString& BlueprintPath, const FString& DispatcherName);

	/** Remove an Event Dispatcher by name. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveEventDispatcher(const FString& BlueprintPath, const FString& DispatcherName);

	/** Rename an Event Dispatcher. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RenameEventDispatcher(const FString& BlueprintPath, const FString& OldName, const FString& NewName);

	/** Add a Call (Broadcast) node for a self Event Dispatcher. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddDispatcherCallNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& DispatcherName, int32 NodePosX, int32 NodePosY);

	/** Add a Bind/Unbind node for a self Event Dispatcher. `bUnbind=true` → unbind. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddDispatcherBindNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& DispatcherName, bool bUnbind, int32 NodePosX, int32 NodePosY);

	// NOTE: for "assign custom event to dispatcher", use AddCustomEventNode and connect
	// its OutputDelegate to AddDispatcherBindNode's event pin manually.

	// ═══ P0 — Interface override ════════════════════════════════════

	/**
	 * Materialize an interface function as an editable graph on this Blueprint.
	 * No-op (returns true) if the function is already implemented or is an event-type member.
	 * The interface must already be added via AddBlueprintInterface.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool ImplementInterfaceFunction(const FString& BlueprintPath,
		const FString& InterfacePath, const FString& FunctionName);

	/** Add a K2Node_Message ("Call Function (Message)") for an interface method. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddInterfaceMessageNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& InterfacePath, const FString& FunctionName, int32 NodePosX, int32 NodePosY);

	// ═══ P0 — Variable metadata / type ══════════════════════════════

	/**
	 * Set common flags on a Blueprint member variable.
	 * ReplicationMode: ""|"None"|"Replicated"|"RepNotify" (empty = leave unchanged).
	 * Empty Category/Tooltip strings leave existing values untouched; pass " " (single space) to clear.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetVariableMetadata(const FString& BlueprintPath, const FString& VariableName,
		bool bInstanceEditable, bool bBlueprintReadOnly, bool bExposeOnSpawn, bool bPrivate,
		const FString& Category, const FString& Tooltip, const FString& ReplicationMode);

	/** Change the type (and container kind) of an existing member variable. Uses AddBlueprintVariable's type syntax. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetVariableType(const FString& BlueprintPath, const FString& VariableName,
		const FString& NewTypeString);

	// ═══ P0 — Compile feedback ══════════════════════════════════════

	/** Compile and return all messages (errors + warnings + notes). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherCompileMessage> GetCompileErrors(const FString& BlueprintPath);

	// ═══ P1 — Control-flow: loops / select / literal ════════════════

	/** Insert a ForEachLoop (or ForEachLoopWithBreak if bWithBreak) macro node. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddForeachNode(const FString& BlueprintPath, const FString& GraphName,
		bool bWithBreak, int32 X, int32 Y);

	/** Insert a ForLoop (or ForLoopWithBreak if bWithBreak) macro node. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddForLoopNode(const FString& BlueprintPath, const FString& GraphName,
		bool bWithBreak, int32 X, int32 Y);

	/** Insert a WhileLoop macro node. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddWhileLoopNode(const FString& BlueprintPath, const FString& GraphName,
		int32 X, int32 Y);

	/** Insert a Select node. Wildcard by default; discriminator/options wire up via pin connections. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddSelectNode(const FString& BlueprintPath, const FString& GraphName,
		int32 X, int32 Y);

	/** Insert a Make Literal <Type> call node. TypeString: Int|Int64|Float|Double|Bool|Byte|Name|String|Text. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddMakeLiteralNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& TypeString, const FString& Value, int32 X, int32 Y);

	// ═══ P1 — Graph layout ═════════════════════════════════════════

	/**
	 * Align / distribute nodes. Axis: Left, Right, Top, Bottom, CenterHorizontal,
	 * CenterVertical, DistributeHorizontal, DistributeVertical.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool AlignNodes(const FString& BlueprintPath, const FString& GraphName,
		const TArray<FString>& NodeGuids, const FString& Axis);

	/** Add a comment box wrapping the provided node guids (pass empty to just position). Returns new comment GUID. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddCommentBox(const FString& BlueprintPath, const FString& GraphName,
		const TArray<FString>& NodeGuids, const FString& Text,
		int32 X, int32 Y, int32 Width, int32 Height);

	/**
	 * Create a comment box that wraps the given set of nodes with the given
	 * comment text. Declarative sibling of AddCommentBox: no manual
	 * X/Y/Width/Height — the box is always sized to fit the nodes with a
	 * standard padding + title strip. NodeGuids must be non-empty (returns
	 * "" if empty, or if none of the guids resolve to a node in the graph).
	 *
	 * Use this when you know WHICH nodes you want wrapped and just want a
	 * box around them; use AddCommentBox when you need manual placement
	 * without any nodes (e.g. a free-floating section header).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString WrapNodesInCommentBox(const FString& BlueprintPath,
		const FString& GraphName, const TArray<FString>& NodeGuids,
		const FString& Text);

	/**
	 * Update an existing comment box in place: reshape it to wrap a new set
	 * of nodes and/or change its text. Both inputs are "optional" — pass an
	 * empty NodeGuids array to leave the box's position/size unchanged;
	 * pass an empty Text to leave the comment string unchanged. Pass both
	 * to change both in one go.
	 *
	 * Returns true when the comment was found and at least one field was
	 * updated; false if the guid didn't resolve to a comment box, or if
	 * both inputs were empty (nothing to do).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool UpdateCommentBox(const FString& BlueprintPath,
		const FString& GraphName, const FString& CommentGuid,
		const TArray<FString>& NodeGuids, const FString& Text);

	/** Insert a reroute (knot) node. Caller wires pins afterwards via ConnectGraphPins. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddRerouteNode(const FString& BlueprintPath, const FString& GraphName,
		int32 X, int32 Y);

	/** State: Enabled | Disabled | DevelopmentOnly. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetNodeEnabled(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& EnabledState);

	// ═══ P1 — Class settings ═══════════════════════════════════════

	/** Change a Blueprint's parent class. Recompiles. HIGH RISK — may discard incompatible components/vars. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool ReparentBlueprint(const FString& BlueprintPath, const FString& NewParentPath);

	/** Set BP display name / description / category / namespace. Empty string = leave unchanged. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetBlueprintMetadata(const FString& BlueprintPath,
		const FString& DisplayName, const FString& Description,
		const FString& Category, const FString& Namespace);

	// ═══ P1 — Component tree ═══════════════════════════════════════

	/** Move a component under a new parent (empty NewParentName = make it a scene root). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool ReparentComponent(const FString& BlueprintPath,
		const FString& ComponentName, const FString& NewParentName);

	/** Reorder a component within its current parent's child list (or root list). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool ReorderComponent(const FString& BlueprintPath,
		const FString& ComponentName, int32 NewIndex);

	/** Remove a component node from the SCS. Children are also removed (use carefully). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveComponent(const FString& BlueprintPath, const FString& ComponentName);

	// ═══ P1 — Dispatcher event node ════════════════════════════════

	/**
	 * Create a CustomEvent whose signature matches the dispatcher's. Caller still needs
	 * an AddDelegate node + a Self-typed target pin to actually bind it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddDispatcherEventNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& DispatcherName, int32 X, int32 Y);

	// ═══ P2 — CallFunction convenience wrappers ═════════════════════

	/** Insert a Delay latent node (KismetSystemLibrary::Delay) with Duration pin default. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddDelayNode(const FString& BlueprintPath, const FString& GraphName,
		float DurationSeconds, int32 X, int32 Y);

	/** Insert a "Set Timer by Function Name" node (K2_SetTimer) with pin defaults set. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddSetTimerByFunctionNameNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& FunctionName, float TimeSeconds, bool bLooping, int32 X, int32 Y);

	/** Insert a SpawnActorFromClass node with its Class pin defaulted to the given actor class. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddSpawnActorFromClassNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& ActorClassPath, int32 X, int32 Y);

	// ═══ P2 — Struct Make / Break ═══════════════════════════════════

	/** Insert a MakeStruct node for the given UScriptStruct path (e.g. /Script/CoreUObject.Vector). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddMakeStructNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& StructPath, int32 X, int32 Y);

	/** Insert a BreakStruct node for the given UScriptStruct path. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddBreakStructNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& StructPath, int32 X, int32 Y);

	// ═══ P2 — Graph extras ══════════════════════════════════════════

	/** Create an empty user-defined macro graph. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool CreateMacroGraph(const FString& BlueprintPath, const FString& MacroName);

	/** Create / toggle a debug breakpoint on a node (by GUID). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool AddBreakpoint(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, bool bEnabled);

	// ═══ P2 — Timeline ══════════════════════════════════════════════

	/**
	 * Insert a Timeline node. If TimelineTemplateName is empty, a unique name is chosen.
	 * The node auto-creates a new UTimelineTemplate on the Blueprint if none exists with that name.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddTimelineNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& TimelineTemplateName, int32 X, int32 Y);

	/**
	 * Update a timeline's template-level settings (length, auto-play, loop, replicated, ignore-time-dilation).
	 * Pass -1.0 for Length to leave unchanged. Syncs to any existing K2Node_Timeline instance that references this template.
	 * Returns false if the timeline template cannot be found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetTimelineProperties(const FString& BlueprintPath, const FString& TimelineName,
		float Length, bool bAutoPlay, bool bLoop, bool bReplicated, bool bIgnoreTimeDilation);

	// ═══ P2 — Macro / Debug management ══════════════════════════════

	/** Remove a user-defined macro graph by name. Returns false if not found. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveMacroGraph(const FString& BlueprintPath, const FString& MacroName);

	/** Remove a breakpoint previously set on a node (by GUID). Returns true if a breakpoint was removed. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveBreakpoint(const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid);

	/** Remove every breakpoint on the Blueprint. Returns the number of breakpoints cleared. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static int32 ClearAllBreakpoints(const FString& BlueprintPath);

	/** Enumerate all breakpoints on the Blueprint — graph name, node GUID, node title, enabled state. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherBreakpointInfo> GetBreakpoints(const FString& BlueprintPath);

	// ═══ P2 — Node utilities ════════════════════════════════════════

	/**
	 * Set the inline comment text shown above a graph node (the "Node Comment" in Details).
	 * Pass empty string to clear. bCommentBubbleVisible controls whether the bubble is pinned open.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetNodeComment(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& Comment, bool bCommentBubbleVisible);

	/**
	 * Duplicate a node in the same graph at (X, Y). The new node gets a fresh GUID and no pin
	 * connections (caller rewires via ConnectGraphPins). Returns new node GUID or "" on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString DuplicateGraphNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, int32 NodePosX, int32 NodePosY);

	/**
	 * Break every link on a single pin (by node GUID + pin name). Returns true if the pin was
	 * found and any links were broken (false if pin missing or already unlinked).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool DisconnectGraphPin(const FString& BlueprintPath, const FString& GraphName,
		const FString& NodeGuid, const FString& PinName);

	/**
	 * Insert a Make Array node (K2Node_MakeArray) — wildcard element type until a pin is connected.
	 * Returns node GUID on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddMakeArrayNode(const FString& BlueprintPath, const FString& GraphName,
		int32 NodePosX, int32 NodePosY);

	/**
	 * Insert a Literal Enum node (K2Node_EnumLiteral) for the given UEnum asset/native path.
	 * EnumPath: e.g. "/Script/Engine.EComponentMobility" or a user-defined enum path.
	 * ValueName: the short enum entry name (e.g. "Static"). Leave empty to use the first entry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddEnumLiteralNode(const FString& BlueprintPath, const FString& GraphName,
		const FString& EnumPath, const FString& ValueName, int32 NodePosX, int32 NodePosY);

	// ─── Understanding / semantic summary ──────────────────

	/**
	 * One-call high-level digest of a Blueprint. Aggregates parent class,
	 * interfaces, events handled, public functions, dispatchers, variable
	 * stats, component / timeline / macro counts, total node count, the
	 * variable categories in use, the most-called external classes, and the
	 * top referenced assets. Replaces 5-6 separate queries with ~500 bytes
	 * of structured output — the first thing an agent should call to "read"
	 * an unfamiliar BP.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool GetBlueprintSummary(const FString& BlueprintPath, FTetherBlueprintSummary& OutSummary);

	/**
	 * Per-function / per-event semantic digest. Walks the graph's exec chain
	 * from its entry node to produce an indented human-readable outline,
	 * plus aggregates: variables read/written, functions called, dispatchers
	 * fired, classes spawned, loop/branch flags, comment text. For event
	 * graphs, pass the event name (e.g. "ReceiveBeginPlay"); for functions,
	 * pass the function name; for macros, the macro name.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool GetFunctionSummary(const FString& BlueprintPath,
		const FString& FunctionName, FTetherFunctionSemantics& OutSemantics);

	/**
	 * All sites (graph + node guid) where `VariableName` is read or written
	 * in this Blueprint. Walks UbergraphPages + FunctionGraphs + MacroGraphs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherReference> FindVariableReferences(
		const FString& BlueprintPath, const FString& VariableName);

	/**
	 * All sites where `FunctionName` is called in this Blueprint.
	 * Matches any CallFunction node whose target function's name equals
	 * `FunctionName`, regardless of owning class.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherReference> FindFunctionCallSites(
		const FString& BlueprintPath, const FString& FunctionName);

	/**
	 * Cross-Blueprint search for all call sites of `FunctionName` across every
	 * Blueprint under `PackagePath` (e.g. "/Game" — recursive). Walks each BP's
	 * UbergraphPages + FunctionGraphs + MacroGraphs just like the single-BP
	 * variant, but returns refs keyed by `BlueprintPath` so an agent can see
	 * "who calls X" project-wide.
	 *
	 * @param FunctionName  Target function short name; matched against
	 *                      K2Node_CallFunction target-function name and
	 *                      K2Node_Message title (for interface calls).
	 * @param OwningClassFilter  Optional filter: when non-empty, only matches
	 *                      calls whose target function's OwnerClass name OR
	 *                      short name equals this (supports both
	 *                      "KismetSystemLibrary" and "UKismetSystemLibrary").
	 *                      Empty = match any owner.
	 * @param PackagePath   Content root to search (default "/Game" = project
	 *                      only). Use "/Engine" to include engine content, or
	 *                      a subfolder like "/Game/AI" to narrow the scope.
	 * @param MaxResults    Stop after this many hits (0 or negative = 1000).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherGlobalReference> FindFunctionCallSitesGlobal(
		const FString& FunctionName,
		const FString& OwningClassFilter,
		const FString& PackagePath,
		int32 MaxResults);

	/**
	 * Tech-debt audit helper. Scans Blueprints under `PackagePath` for every
	 * call to `KismetSystemLibrary::PrintString` / `PrintText` / `PrintWarning`
	 * and returns each call site with the literal string argument extracted
	 * (when static). Used by `scripts/audit_tech_debt.py` alongside a
	 * filesystem scan for `// TODO` / `// HACK` / `// FIXME` / `UE_LOG(LogTemp`
	 * to produce a pre-release cleanup checklist.
	 *
	 * @param PackagePath   Content root (default "/Game"). "/Engine" for engine BPs.
	 * @param MaxResults    Cap (0 or negative → 1000).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherDebugPrintSite> FindBlueprintDebugPrints(
		const FString& PackagePath,
		int32 MaxResults);

	/**
	 * Execute a Blueprint-callable function on a transient instance of the
	 * given Blueprint's generated class and return the result as JSON. Lets
	 * an agent verify a function's behavior without entering PIE.
	 *
	 * Execution:
	 *   - AActor-derived classes are spawned in the editor world with
	 *     RF_Transient and Destroy()'d after the call.
	 *   - Other UObject classes are NewObject'd in the transient package.
	 *
	 * Args JSON must be a JSON object keyed by parameter name. Struct
	 * parameters accept nested JSON objects (field names match UPROPERTY
	 * names). Arrays accept JSON arrays. Object-ref params accept the
	 * asset/object path string.
	 *
	 * Output JSON is a JSON object with one entry per out/return param.
	 * The return value (if any) is under the key "_return"; named out
	 * params keep their declared name.
	 *
	 * Safety gates — produces {"error":"..."} without calling the function
	 * when:
	 *   - Function is not found on the generated class
	 *   - Function lacks FUNC_BlueprintCallable and is not user-defined
	 *     on this Blueprint (rejects engine lifecycle events like BeginPlay)
	 *   - Function takes a latent action info (would never complete inline)
	 *
	 * @param BlueprintPath   Full BP object path (e.g. "/Game/BP_X.BP_X").
	 * @param FunctionName    Short name of the function to call.
	 * @param ArgsJson        JSON object string of parameter values; empty
	 *                        string = no args.
	 * @param OutResultJson   JSON object string. On success: one entry per
	 *                        out/return param (return value keyed as
	 *                        "_return"). On handled failure: {"error":"..."}.
	 *                        Always populated — callers should inspect for
	 *                        an "error" key rather than rely on the bool
	 *                        return.
	 * @param OutError        Duplicate of the error text (empty on success).
	 *                        Useful for C++ callers that branch on error;
	 *                        Python callers should use OutResultJson since
	 *                        the UFUNCTION binding strips out-params when
	 *                        a bool return is false.
	 * @return Always true for handled outcomes. Reserved as false only for
	 *         catastrophic failures that can't produce a JSON payload.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool InvokeBlueprintFunction(
		const FString& BlueprintPath,
		const FString& FunctionName,
		const FString& ArgsJson,
		FString& OutResultJson,
		FString& OutError);

	/**
	 * All event-handler / dispatcher bind sites matching `EventName`.
	 * Covers K2Node_Event / K2Node_CustomEvent entries (kind="event"),
	 * K2Node_CallDelegate (kind="call"), K2Node_AddDelegate (kind="bind"),
	 * K2Node_RemoveDelegate (kind="unbind").
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherReference> FindEventHandlerSites(
		const FString& BlueprintPath, const FString& EventName);

	// ─── Node pin introspection ────────────────────────────

	/**
	 * Read the default value of a specific pin on a specific node.
	 * For object-ref pins returns the object's path name; for text pins
	 * the text contents; for literals the string form. Returns empty
	 * string when the pin is connected (default ignored) or has no
	 * default set.
	 *
	 * Complements set_pin_default_value — the read half that was missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString GetPinDefaultValue(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid, const FString& PinName);

	/**
	 * List every pin on a node — including unconnected ones — with its
	 * type, direction, category, default value, and connection state.
	 * Unlike get_node_pin_connections (which only surfaces wired pins),
	 * this returns the full pin surface so an agent can see what's
	 * available to wire against without opening the BP editor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherPinInfo> GetNodePins(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid);

	/**
	 * One-shot description of a single node. Combines title, position,
	 * size, classification, K2Node-subclass fields (target class/function,
	 * variable name, macro graph, etc.), and every pin (with types, defaults,
	 * AND link targets) into one round-trip. Replaces the common pattern of
	 * calling get_function_nodes + get_node_pins + get_node_pin_connections +
	 * get_pin_default_value × N for a single node.
	 *
	 * Each pin's LinkedTo entries are formatted `"<node_guid>:<pin_name>"` so
	 * the caller can navigate to the connected side without joining a
	 * separate node list.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherNodeDescription DescribeNode(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid);

	/**
	 * Look up a UFunction's full signature — parameter names, types,
	 * declared default values (stored as `CPP_Default_<ParamName>` metadata
	 * on the UFunction), ref / const / out flags, and blueprint-facing
	 * flags (pure / const / static / latent / callable / pure / native).
	 *
	 * @param ClassPath      Either a full path (`/Script/Engine.KismetSystemLibrary`)
	 *                       or a short name (`KismetSystemLibrary`) — the
	 *                       short name is resolved via `FindFirstObject<UClass>`.
	 * @param FunctionName   Internal FName (e.g. "PrintString", "GetActorLocation").
	 *                       Not the display name.
	 *
	 * Returns bFound=false when the class or function can't be resolved.
	 * Use this before spawning a CallFunction node so the caller knows
	 * exactly which pins to wire and which defaults to leave untouched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherFunctionSignature GetFunctionSignature(const FString& ClassPath,
		const FString& FunctionName);

	// ─── Node layout (position / size / corners) ───────────

	/**
	 * Node origin, stored + estimated sizes, and all four corners in
	 * graph coordinates. See FTetherNodeLayout for the size caveat —
	 * stored sizes require the BP to have been rendered in an open
	 * graph panel at least once (Comment nodes always have authoritative
	 * sizes because user authored them).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherNodeLayout GetNodeLayout(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid);

	/**
	 * Estimated local + absolute positions of every visible pin on a node.
	 * Useful for placing reroute knots between two nodes, lining up new
	 * nodes next to specific pins, or computing the "where does this wire
	 * want to go" point.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherPinLayout> GetNodePinLayouts(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid);

	/**
	 * Open the named function/event/macro graph in the Blueprint editor and
	 * bring it to the front. Focuses Slate's attention on that graph so the
	 * SGraphNode / SGraphPin widgets get created, which (after a Slate tick)
	 * populates UEdGraphNode::NodeWidth/NodeHeight with accurate rendered
	 * dimensions. Without this, those fields stay 0 and size queries fall
	 * back to the heuristic estimator.
	 *
	 * Returns true if the graph was opened. After calling this, yield
	 * control (return from the current tether exec so Slate can tick), then
	 * re-query GetNodeLayout / GetNodePinLayouts in a second exec.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool OpenFunctionGraphForRender(const FString& BlueprintPath,
		const FString& GraphName);

	/**
	 * Query pixel-accurate node sizes and pin positions from the live Slate
	 * SGraphNode / SGraphPin widgets. Returns one entry per node in the
	 * graph; each entry's bIsLive flag tells you whether the widget was
	 * actually rendered and queried (vs. not yet rendered, in which case
	 * Size is zero and Pins is empty).
	 *
	 * Prerequisite: OpenFunctionGraphForRender must have been called in a
	 * PREVIOUS tether exec AND at least one Slate tick must have elapsed
	 * (i.e. a small wait + new exec). Slate can't render while an exec is
	 * holding the game thread, so a single-shot query on a freshly-opened
	 * graph will return bIsLive=false for everything.
	 *
	 * Use this instead of GetNodeLayout / GetNodePinLayouts when you need
	 * the ACTUAL rendered geometry (e.g. to drive pin-accurate auto-layout
	 * for nodes whose widths depend on compact-mode rendering, localised
	 * labels, or custom Slate overrides).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherRenderedNode> GetRenderedNodeInfo(
		const FString& BlueprintPath, const FString& GraphName);

	/**
	 * Predict the rendered size of a node *before* it is spawned, so callers
	 * can reserve space / avoid overlap without a two-round-trip spawn→measure
	 * dance. Uses the same width/height heuristic as FTetherNodeLayout's
	 * estimated path (header + pin rows + char-width title).
	 *
	 * @param Kind  Node kind discriminator. Supported:
	 *   - "function_call"  ParamA=ClassPath, ParamB=FunctionName
	 *   - "variable_get"   ParamA=BlueprintPath (or ""), ParamB=VariableName
	 *   - "variable_set"   ParamA=BlueprintPath (or ""), ParamB=VariableName
	 *   - "event"          ParamA=ClassPath, ParamB=EventName
	 *   - "custom_event"   ParamInt=output-pin count (data params)
	 *   - "branch"         (no params)
	 *   - "sequence"       ParamInt=then-pin count (≥1)
	 *   - "cast"           ParamA=TargetClassPath (for title length)
	 *   - "self"           (no params)
	 *   - "reroute"        (no params)
	 *   - "delay"          (no params)
	 *   - "foreach"        (no params)
	 *   - "forloop"        (no params)
	 *   - "whileloop"      (no params)
	 *   - "select"         ParamInt=option count (≥2)
	 *   - "make_array"     ParamInt=element count (≥1)
	 *   - "make_struct"    ParamA=StructPath (for title + member count)
	 *   - "break_struct"   ParamA=StructPath
	 *   - "enum_literal"   ParamA=EnumPath
	 *   - "make_literal"   ParamA=TypeString (e.g. "Vector")
	 *   - "spawn_actor"    ParamA=ActorClassPath
	 *   - "dispatcher_call"|"dispatcher_bind"|"dispatcher_event"  ParamA=BlueprintPath, ParamB=DispatcherName
	 *
	 * Unknown Kind returns the 180×60 fallback with bResolved=false.
	 *
	 * @param ParamInt  Used by kinds that take a count (see above). Clamped ≥ 0.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherNodeSizeEstimate PredictNodeSize(const FString& Kind,
		const FString& ParamA, const FString& ParamB, int32 ParamInt);

	/**
	 * Reflow the graph with a layered, exec-flow-driven auto-layout.
	 *
	 * Strategies:
	 *   - "exec_flow" (default): Sugiyama-lite. Topologically layer nodes by
	 *     exec dependency, position each layer left-to-right with H_SPACING
	 *     gaps, stack nodes vertically within a layer with V_SPACING between
	 *     them, then barycentrically re-order each layer to reduce wire
	 *     crossings. Pure / data-only nodes attach to their first exec
	 *     consumer's layer − 1. Positions nodes at layer-center Y — exec pins
	 *     are not pixel-aligned; follow up with StraightenExecChain for that.
	 *
	 *   - "pin_aligned": exec backbone placed pin-to-pin (each node's input
	 *     exec pin Y matches its primary predecessor's output exec pin Y, so
	 *     exec wires render perfectly horizontal), then data producers pulled
	 *     right-to-left into the exec nodes they feed, aligning their output
	 *     pin Y to the consumer's input pin Y. Chained data nodes cascade
	 *     further left. Matches the "polished BP" look without requiring a
	 *     separate straightening pass.
	 *
	 *   Reserved: "data_flow", "event_grouped".
	 *
	 * Doesn't modify wires — positions only. Safe to call multiple times;
	 * idempotent on stable topology. Doesn't compile the Blueprint.
	 * Comment boxes are skipped (moving them breaks their enclose-intent).
	 *
	 * @param AnchorNodeGuid  Optional. If set, the anchor node's position is
	 *   preserved and everything else is translated so the anchor lands where
	 *   it was. Otherwise the layout origin defaults to the current top-left
	 *   of the graph's bounding box.
	 * @param HorizontalSpacing  Gap between layers. Pass 0 or negative for
	 *   the default (80).
	 * @param VerticalSpacing  Gap between nodes within a layer. Pass 0 or
	 *   negative for the default (40).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherLayoutResult AutoLayoutGraph(const FString& BlueprintPath,
		const FString& GraphName, const FString& Strategy,
		const FString& AnchorNodeGuid,
		int32 HorizontalSpacing, int32 VerticalSpacing);

	// ─── Universal node spawner (FBlueprintActionDatabase) ─────────

	/**
	 * List nodes that *could* be spawned in the given graph, with palette
	 * search/filter applied. Walks the editor's FBlueprintActionDatabase —
	 * the same source the right-click "All Actions" menu uses — so any node
	 * the user could pick from the editor can be enumerated here, including
	 * function calls, variable get/set, control flow (Branch/Sequence/Switch/
	 * loops), spawn / cast / message, async / latent nodes, MakeArray /
	 * MakeStruct, custom-event templates, etc.
	 *
	 * Filters are AND'd together (all non-empty must match):
	 * @param Keyword            Case-insensitive substring matched against
	 *                           Title + Tooltip + Keywords + Category.
	 * @param CategoryContains   Case-insensitive substring on the menu category
	 *                           (e.g. "Math" picks "Math|Float", "Math|Int").
	 * @param OwningClassPath    Exact path filter, e.g.
	 *                           "/Script/Engine.KismetSystemLibrary" — keeps
	 *                           only spawners owned by that class/asset.
	 * @param NodeType           Exact K2Node class short name filter
	 *                           (e.g. "K2Node_CallFunction").
	 * @param MaxResults         Hard cap on returned entries (clamped to >= 1).
	 *                           Walk stops once this many matches accumulate.
	 *
	 * Returns the matched actions; pass FTetherSpawnableAction.Key into
	 * SpawnNodeByActionKey to materialize one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherSpawnableAction> ListSpawnableActions(
		const FString& BlueprintPath, const FString& GraphName,
		const FString& Keyword, const FString& CategoryContains,
		const FString& OwningClassPath, const FString& NodeType,
		int32 MaxResults);

	/**
	 * Spawn a node in the graph by its action key (from ListSpawnableActions).
	 * Walks the action database, finds the matching spawner by signature, and
	 * invokes it at (X, Y). Returns the new node's GUID, or "" on failure
	 * (key not found, graph missing, spawner rejected the target graph).
	 *
	 * No auto-compile; call CompileBlueprint after a batch of edits.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString SpawnNodeByActionKey(const FString& BlueprintPath,
		const FString& GraphName, const FString& ActionKey,
		int32 NodePosX, int32 NodePosY);

	// ─── Fine-grained pin link control ─────────────────────────────

	/**
	 * Break exactly one link between (SourceNode.SourcePin) and
	 * (TargetNode.TargetPin). Unlike DisconnectGraphPin (which clears every
	 * link on a pin), this leaves other links on the same pin intact —
	 * essential when a single output drives multiple consumers and you only
	 * want to disconnect one.
	 *
	 * Direction is symmetric: order of (Source, Target) doesn't matter as long
	 * as the two pins are linked. Returns true if the link existed and was
	 * broken; false if either pin was missing or the two weren't linked.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool DisconnectPinLink(const FString& BlueprintPath, const FString& GraphName,
		const FString& SourceNodeGuid, const FString& SourcePinName,
		const FString& TargetNodeGuid, const FString& TargetPinName);

	// ─── Lint / quality review ─────────────────────────────────────

	/**
	 * Run a quality-review pass over a Blueprint and return structured
	 * findings. Designed for AI agents to self-review generated graphs before
	 * calling the task done. Checks are intentionally cheap (single BP load,
	 * no compile required).
	 *
	 * Current checks (v1):
	 *   • OrphanNode            — node has zero connections on every pin
	 *   • OversizedFunction     — function graph exceeds the node threshold
	 *   • UnnamedCustomEvent    — CustomEvent has default name (Event_N etc.)
	 *   • UnnamedFunction       — user-authored function named NewFunction_N
	 *   • InstanceEditableNoCategory — editable var lacks a custom Category
	 *   • InstanceEditableNoTooltip  — editable var lacks a tooltip
	 *   • UnusedVariable        — class variable with zero references
	 *   • UnusedLocalVariable   — function local var with zero references
	 *   • LargeUncommentedGraph — graph has many nodes and no comment boxes
	 *   • LongExecChain         — unbroken exec chain suggests extraction
	 *   • HardcodedStringLiteral — PrintString / literal nodes with prod-looking defaults
	 *
	 * @param SeverityFilter  "" = all, or one of "error"/"warning"/"info"
	 * @param OversizedFunctionThreshold  Node-count ceiling for functions.
	 *                                    -1 uses the default of 20.
	 * @param LongExecChainThreshold  Linear-chain length that triggers the
	 *                                extraction hint. -1 uses default 15.
	 * @param LargeGraphThreshold     Node count above which a graph must
	 *                                have at least one comment box. -1 = 10.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherLintIssue> LintBlueprint(
		const FString& BlueprintPath,
		const FString& SeverityFilter,
		int32 OversizedFunctionThreshold,
		int32 LongExecChainThreshold,
		int32 LargeGraphThreshold);

	// ─── Collapse to function / macro ─────────────────────────────

	/**
	 * Extract a selection of nodes into a new Function graph and replace the
	 * selection with a CallFunction gateway node wired to the same external
	 * pins. Mirrors the engine's "Collapse to Function" command but callable
	 * without the editor UI.
	 *
	 * Algorithm (matches FBlueprintEditor::CollapseNodesIntoGraph):
	 *   1. Create a new function graph (unique name derived from NewName)
	 *   2. Move the selected nodes into it
	 *   3. For each pin on a selected node whose LinkedTo crosses the
	 *      selection boundary, create a matching pin on the FunctionEntry
	 *      (inputs) or FunctionResult (outputs), plus a corresponding pin on
	 *      the gateway CallFunction node in the source graph
	 *   4. Rewire: external side ↔ gateway; internal side ↔ entry/result
	 *
	 * The selection must all live in the same graph. Comment boxes in the
	 * selection are moved along with the nodes. Orphan Events / Function
	 * entries in the selection are refused (they can't be extracted).
	 *
	 * @param NewFunctionName  Desired name; gets uniquified if taken.
	 * @param OutNewGraphName  Set to the actual function graph name on success.
	 * @return GUID of the gateway CallFunction node in the source graph, or ""
	 *         on failure. Check OutNewGraphName to locate the new function.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString CollapseNodesToFunction(const FString& BlueprintPath,
		const FString& SourceGraphName, const TArray<FString>& NodeGuids,
		const FString& NewFunctionName, FString& OutNewGraphName);

	// ─── Straighten exec rail ─────────────────────────────────────

	/**
	 * Walk a linear exec chain starting from StartNodeGuid and align each
	 * downstream node's Y so its exec-input pin sits at exactly the same Y
	 * as the upstream exec-output pin. Produces the "clean straight rail"
	 * look that auto_layout_graph alone can't give because layout works at
	 * layer-center granularity, not pin granularity.
	 *
	 * Stops at the first branch (multiple exec outs with links) or merge
	 * (exec in pin with > 1 links) — use multiple calls to straighten each
	 * branch independently.
	 *
	 * @return number of nodes whose Y was adjusted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static int32 StraightenExecChain(const FString& BlueprintPath,
		const FString& GraphName, const FString& StartNodeGuid,
		const FString& StartExecPinName);

	// ─── Comment-box color + node tint ─────────────────────────────

	/**
	 * Set the background color of a Comment box. Accepts a hex string
	 * ("#RRGGBB" or "#RRGGBBAA") or one of these presets:
	 *   "Section"    — neutral grey
	 *   "Validation" — yellow
	 *   "Danger"     — red
	 *   "Network"    — purple
	 *   "UI"         — teal
	 *   "Debug"      — green
	 *   "Setup"      — blue
	 *
	 * @return true if the node is a Comment and the color parsed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetCommentBoxColor(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid,
		const FString& ColorOrPreset);

	/**
	 * Tint an individual node with a custom color override (the little
	 * "Enable Node Color" path). Uses the same preset names / hex grammar as
	 * SetCommentBoxColor. Pass an empty string to clear the override.
	 *
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetNodeColor(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid,
		const FString& ColorOrPreset);

	// ─── Auto-insert reroute knots to break overlaps ───────────────

	/**
	 * Walk every wire in the graph; if a wire's straight line from source
	 * pin to destination pin crosses any *other* node's bounding box
	 * (non-endpoint), insert a reroute (knot) node at a clear Y above or
	 * below the obstruction so the wire no longer passes through it.
	 *
	 * Only operates on wires that are problematic — untouched wires stay
	 * untouched. Safe to call multiple times.
	 *
	 * @return number of reroute knots inserted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static int32 AutoInsertReroutes(const FString& BlueprintPath,
		const FString& GraphName);

	// ─── Typed pin helpers (#11) ────────────────────────────────────

	/**
	 * Set a `FDataTableRowHandle` pin to reference a specific DataTable row.
	 * Emits the exported-text form `(DataTable="/Game/...",RowName="Row")`
	 * so callers don't have to hand-format it.
	 *
	 * @param DataTablePath  Object path of the DataTable asset.
	 * @param RowName        FName of the row to reference (empty = NAME_None).
	 * @return true if the pin was found, was of type FDataTableRowHandle, and
	 *         the default value parsed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetDataTableRowHandlePin(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid,
		const FString& PinName,
		const FString& DataTablePath, const FString& RowName);

	// ─── Struct pin split / recombine (#12) ─────────────────────────

	/**
	 * Split a struct pin into its subfield pins on the node (the editor's
	 * "Split Struct Pin" right-click action). Works on any pin whose
	 * PinType is a struct recognised by the K2 schema.
	 *
	 * @return true if the pin was split. false if the pin was not found,
	 *         not a struct, or not splittable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SplitStructPin(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid,
		const FString& PinName);

	/**
	 * Recombine a previously-split struct pin back into a single pin.
	 * Pass the name of any one of the sub-pins; the schema locates the
	 * parent pin and recombines.
	 *
	 * @return true if recombined.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RecombineStructPin(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid,
		const FString& SubPinName);

	// ─── Promote-to-Variable (#9) ───────────────────────────────────

	/**
	 * Create a new Blueprint variable matching the pin's type, then wire a
	 * Get/Set node for that variable to the pin. Mirrors the editor's
	 * "Promote to Variable" right-click command.
	 *
	 * - Input pin (including unconnected)  → spawns a Get node (for pure/
	 *   data pins) or a Set node (for exec-flow input pins that expect a
	 *   stored value) and wires it in.
	 * - Output pin (data)  → spawns a Set node so the pin's value is
	 *   captured into the new variable.
	 *
	 * @param VariableName    Desired name (uniquified against existing vars).
	 * @param bToMemberVariable  true = add to BP's class-level variables;
	 *                           false = add as a local variable on the
	 *                           containing function graph.
	 * @param OutNewVariableName  On success, the actual (possibly
	 *                            uniquified) variable name.
	 * @param OutNewNodeGuid      On success, the GUID of the spawned
	 *                            Get/Set node.
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool PromotePinToVariable(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeGuid,
		const FString& PinName,
		const FString& VariableName, bool bToMemberVariable,
		FString& OutNewVariableName, FString& OutNewNodeGuid);

	// ─── Function-signature editing (#14) ───────────────────────────

	/**
	 * Remove a user-defined parameter from a function graph. Deletes the
	 * pin on FunctionEntry (for input params) or FunctionResult (for
	 * return / out params), then recompiles.
	 *
	 * @return true when a pin with that name was removed from either
	 *         scaffolding node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool RemoveFunctionParameter(const FString& BlueprintPath,
		const FString& FunctionName, const FString& ParamName);

	/**
	 * Reorder a user-defined parameter in a function graph. Moves the
	 * named pin to `NewIndex` in the owning node's UserDefinedPins array
	 * (inputs on FunctionEntry or outputs on FunctionResult). Pass -1 or
	 * past-the-end to move to last.
	 *
	 * @return true when the pin was located + moved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool ReorderFunctionParameter(const FString& BlueprintPath,
		const FString& FunctionName, const FString& ParamName,
		int32 NewIndex);

	// ─── Collapse-to-Macro (#10) ────────────────────────────────────

	/**
	 * Companion to CollapseNodesToFunction — bundles the selection into a
	 * new **macro** graph instead of a function graph. Useful for
	 * control-flow patterns where a function's fixed exec in/out isn't
	 * flexible enough (multi-out custom loops, exec-branching helpers).
	 *
	 * Same selection semantics as CollapseNodesToFunction: nodes must live
	 * in the same graph; FunctionEntry / FunctionResult / Tunnel nodes
	 * can't be collapsed.
	 *
	 * @param NewMacroName   Desired name; uniquified if taken.
	 * @param OutNewGraphName  Set to the actual macro graph name on success.
	 * @return GUID of the MacroInstance node spawned in the source graph,
	 *         or "" on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString CollapseNodesToMacro(const FString& BlueprintPath,
		const FString& SourceGraphName, const TArray<FString>& NodeGuids,
		const FString& NewMacroName, FString& OutNewGraphName);

	// ─── Async-action node creation (#13) ───────────────────────────

	/**
	 * Spawn a `K2Node_AsyncAction` wired to a factory function on a
	 * `UBlueprintAsyncActionBase` subclass. Reliable alternative to
	 * `SpawnNodeByActionKey` for async tasks whose action-key signatures
	 * change across sessions.
	 *
	 * Typical factory classes:
	 *   - `UAbilityAsync_WaitGameplayEvent` (GAS)
	 *   - `UMoviePlayerAsyncTask`           (media)
	 *   - Any user-defined
	 *     `UBlueprintAsyncActionBase`-derived class.
	 *
	 * @param FactoryClassPath     `/Script/<Module>.<ClassName>` or short
	 *                             class name.
	 * @param FactoryFunctionName  Short name of the static factory UFUNCTION
	 *                             on the factory class.
	 * @return GUID of the new node, or "" on failure (bad class / function).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddAsyncActionNode(const FString& BlueprintPath,
		const FString& GraphName, const FString& FactoryClassPath,
		const FString& FactoryFunctionName, int32 NodePosX, int32 NodePosY);

	// ─── Custom K2Node by class name (#19) ──────────────────────────

	/**
	 * Spawn an arbitrary K2Node subclass by class name. Fallback for
	 * project-private nodes where `SpawnNodeByActionKey` is unstable
	 * across sessions because action-key strings aren't guaranteed stable.
	 *
	 * @param NodeClassPath  `/Script/<Module>.<Class>` or short class name;
	 *                       resolved via `FindFirstObject<UClass>`. Must
	 *                       resolve to a `UK2Node` subclass.
	 * @return GUID of the new node, or "" on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddNodeByClassName(const FString& BlueprintPath,
		const FString& GraphName, const FString& NodeClassPath,
		int32 NodePosX, int32 NodePosY);

	// ─── Enhanced Input authoring ───────────────────────────────────

	/**
	 * Spawn a UK2Node_EnhancedInputAction event node bound to a specific
	 * UInputAction asset. The asset reference is set BEFORE AllocateDefaultPins
	 * so the node ships with its full pin set: 5 exec out pins
	 * (Triggered/Started/Ongoing/Canceled/Completed) + ActionValue (typed by
	 * the IA's ValueType) + ElapsedSeconds + TriggeredSeconds + InputAction.
	 *
	 * Reuses an existing node bound to the same IA if one already lives on
	 * the graph (matches UInputActionEventNodeSpawner::FindExistingNode).
	 *
	 * The Blueprint must support input events (Pawn/Actor/PlayerController/...)
	 * and the graph must not be a construction script — caller's
	 * responsibility; this function does not preflight.
	 *
	 * @return GUID of the new (or reused) node, or "" on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddEnhancedInputActionEventNode(const FString& BlueprintPath,
		const FString& GraphName, const FString& InputActionPath,
		int32 NodePosX, int32 NodePosY);

	/**
	 * Spawn a UK2Node_GetInputActionValue (a pure "Get Input Action Value"
	 * node — not the event node). Output value pin is typed by the IA's
	 * ValueType: Boolean → bool, Axis1D → double, Axis2D → Vector2D,
	 * Axis3D → Vector. Use this when you need the IA's current value
	 * inside a non-event graph (functions, custom events, etc.) instead
	 * of routing through the EnhancedInputAction event node.
	 *
	 * @return GUID of the new node, or "" on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddGetInputActionValueNode(const FString& BlueprintPath,
		const FString& GraphName, const FString& InputActionPath,
		int32 NodePosX, int32 NodePosY);

	/**
	 * Composite helper: place a K2Node_EnhancedInputAction event node for
	 * `InputActionPath` (or reuse an existing one), place a K2Node_CallFunction
	 * for `TargetFunctionName` on `TargetClassPath` (empty = self/this BP),
	 * and wire the event's `TriggerEventPin` exec pin to the CallFunction's
	 * exec_in pin. The C4 Pawn-scaffold inner loop, exposed standalone for
	 * direct use.
	 *
	 * `TriggerEventPin` is one of "Triggered" / "Started" / "Ongoing" /
	 * "Canceled" / "Completed" — case-sensitive, matches the exec pin name.
	 *
	 * Note: UEnhancedInputComponent::BindAction is not BlueprintCallable.
	 * The K2Node_EnhancedInputAction event node IS the BP equivalent of a
	 * BindAction call — the BP compiler expands it into BindAction during
	 * compile via UK2Node_EnhancedInputAction::ExpandNode. So this helper
	 * does NOT add a literal "BindAction" function call node; it adds the
	 * event node + a CallFunction for your handler, and wires them.
	 *
	 * Returns both GUIDs + a bWired flag. On failure the bWired flag is
	 * false and FailureReason carries the diagnostic. Either GUID may be
	 * empty if its node creation failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherWireIAResult WireEnhancedInputActionToFunction(
		const FString& BlueprintPath, const FString& GraphName,
		const FString& InputActionPath, const FString& TriggerEventPin,
		const FString& TargetClassPath, const FString& TargetFunctionName,
		int32 EventNodeX, int32 EventNodeY,
		int32 CallNodeX,  int32 CallNodeY,
		bool bAutoWireActionValue = true);

	// ─── B2/B3/B4/B5 — Legacy InputAction / InputAxis / InputKey K2Nodes ─

	/** Spawn a UK2Node_InputAction event node bound to a legacy ActionMapping name (FName). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddLegacyInputActionEventNode(const FString& BlueprintPath,
		const FString& GraphName, const FString& ActionName,
		int32 NodePosX, int32 NodePosY);

	/** Spawn a UK2Node_InputAxisEvent for a legacy AxisMapping name. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddLegacyInputAxisEventNode(const FString& BlueprintPath,
		const FString& GraphName, const FString& AxisName,
		int32 NodePosX, int32 NodePosY);

	/** Spawn a UK2Node_InputKey for a raw FKey (e.g. "SpaceBar"). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddInputKeyEventNode(const FString& BlueprintPath,
		const FString& GraphName, const FString& KeyName,
		int32 NodePosX, int32 NodePosY);

	/** Spawn a UK2Node_InputAxisKeyEvent for a raw axis FKey (e.g. "Mouse X"). */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString AddInputAxisKeyEventNode(const FString& BlueprintPath,
		const FString& GraphName, const FString& AxisKeyName,
		int32 NodePosX, int32 NodePosY);

	/**
	 * Build the standard "BeginPlay → AddMappingContext" graph fragment on a
	 * Pawn's EventGraph: Event ReceiveBeginPlay → Get Player Controller →
	 * Get Enhanced Input Local Player Subsystem → AddMappingContext(IMC, priority).
	 * Returns true on success.
	 *
	 * Reuses existing BeginPlay if one is already on the graph.
	 *
	 * @param BlueprintPath  Pawn-derived BP.
	 * @param IMCPath        UInputMappingContext asset path.
	 * @param Priority       AddMappingContext priority.
	 * @param OriginX/Y      Top-left position of the new chain (the BeginPlay event node).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool AddPawnInputBeginPlaySetup(const FString& BlueprintPath,
		const FString& IMCPath, int32 Priority,
		int32 OriginX, int32 OriginY);

	// ─── Editor focus-state query (#17) ─────────────────────────────

	/**
	 * Snapshot the active Blueprint editor's state: which BP is focused,
	 * which graph is showing, and which nodes are selected. Used by
	 * AI-assisted editing flows ("wrap the selection into a function",
	 * "disable the selected nodes") where the agent needs the user's
	 * current attention point, not a hard-coded graph name.
	 *
	 * Returns an empty struct (empty BlueprintPath / FocusedGraphName) when
	 * no BP editor has focus.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherEditorFocusState GetEditorFocusState();

	// ─── Cross-BP rename (#16) ──────────────────────────────────────

	/**
	 * Rename a member variable on the defining Blueprint and rewrite every
	 * reference in every Blueprint under `PackagePath` (default "/Game").
	 * Walks Get/Set nodes on every BP's graphs; for each match with
	 * `VariableReference.MemberName == OldName` and a matching owner class
	 * (the defining BP's generated class or a subclass), the reference is
	 * rewritten to the new name and the node is reconstructed.
	 *
	 * Each updated BP is recompiled + marked dirty so the caller can save
	 * with `save_asset`. The defining BP is also recompiled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherRenameReport RenameMemberVariableGlobal(
		const FString& DefiningBlueprintPath,
		const FString& OldName, const FString& NewName,
		const FString& PackagePath);

	/**
	 * Rename a user-defined function on the defining Blueprint and rewrite
	 * every CallFunction / Message reference across Blueprints under
	 * `PackagePath`. Uses the same scan strategy as
	 * find_function_call_sites_global.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherRenameReport RenameFunctionGlobal(
		const FString& DefiningBlueprintPath,
		const FString& OldName, const FString& NewName,
		const FString& PackagePath);

	// ─── Variable-type change with explicit ref-fix report (#15) ────

	/**
	 * Change a member variable's type and return the list of K2 nodes
	 * whose references were dropped as a result. Existing
	 * `set_variable_type` relies on UE's `ChangeMemberVariableType` which
	 * reconforms most Get/Set nodes but silently breaks wires on
	 * incompatible type changes (e.g. int → bool). This wrapper compiles
	 * after the change and reports every reference node that ended up with
	 * broken pins so the agent can fix them up.
	 *
	 * @param OutBrokenNodeGuids  Node GUIDs (across all graphs) whose pins
	 *                            became orphaned after the type change.
	 *                            Empty when the change was pin-compatible.
	 * @return true on a successful type change; false when the variable
	 *         didn't exist or the type string didn't parse.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool ChangeVariableTypeWithReport(const FString& BlueprintPath,
		const FString& VariableName, const FString& NewTypeString,
		TArray<FString>& OutBrokenNodeGuids);

	// ─── Graph fingerprint + snapshot + diff ────────────────────────

	/**
	 * Stable hash of a graph's shape. Encodes every node's class / position /
	 * K2Node-subclass fields and every wire's endpoints, in a canonical
	 * (guid-sorted) order, then SHA-1s the encoding. Cheap "did this graph
	 * change since I last looked?" check that doesn't require a full re-read.
	 *
	 * @return 40-hex-char SHA-1, or empty string if the graph isn't found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString GetGraphFingerprint(const FString& BlueprintPath,
		const FString& GraphName);

	/**
	 * Serialize a graph's nodes and wires to a canonical JSON string. The
	 * output is deterministic (guid-sorted), so two calls on the same graph
	 * produce byte-identical output. Pair with DiffGraphSnapshots to reason
	 * about what changed between two edit states.
	 *
	 * Schema:
	 *   {"nodes":[{"guid":"...","class":"K2Node_CallFunction","title":"...",
	 *              "x":0,"y":0}, ...],
	 *    "wires":[{"src":"guid","src_pin":"then","dst":"guid",
	 *              "dst_pin":"execute"}, ...]}
	 *
	 * @return JSON string, or empty on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FString SnapshotGraphJson(const FString& BlueprintPath,
		const FString& GraphName);

	/**
	 * Compute the set difference between two graph snapshots. Node identity
	 * is the NodeGuid; wire identity is (src_guid, src_pin, dst_guid, dst_pin).
	 * Treats reordered nodes / wires as unchanged since order has no semantic
	 * meaning.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherGraphDiff DiffGraphSnapshots(const FString& BeforeJson,
		const FString& AfterJson);

	// ─── Entry-friction fix (#4) ────────────────────────────────────

	/**
	 * Wire the first FunctionEntry's exec-out ("then") to the first
	 * FunctionResult's exec-in ("execute") inside a function graph, if both
	 * exist and the wire doesn't already exist. Returns false if no exec
	 * wiring was necessary (already wired, missing Result, or the graph is
	 * not a function graph).
	 *
	 * Workaround for the trap where CreateFunctionGraph + AddFunctionParameter
	 * leave the graph with isolated Entry/Result nodes; calling the function
	 * compiles clean but does nothing at runtime. Call this after shaping
	 * the signature to guarantee a walkable exec path.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool EnsureFunctionExecWired(const FString& BlueprintPath,
		const FString& FunctionName);

	// ─── Refactor primitives (#2) ───────────────────────────────────

	/**
	 * Split an existing wire A→B by inserting a third node X such that
	 * A→X and X→B carry the original wire's data. Breaks the original link
	 * and adds the two new ones in a single call.
	 *
	 * @param SrcNodeGuid / SrcPinName  The upstream endpoint of the existing wire.
	 * @param DstNodeGuid / DstPinName  The downstream endpoint of the existing wire.
	 * @param InsertNodeGuid  The node to sit between them.
	 * @param InsertInPinName  Pin on the insert node that takes the upstream side.
	 * @param InsertOutPinName Pin on the insert node that feeds the downstream side.
	 * @return true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool InsertNodeOnWire(const FString& BlueprintPath,
		const FString& GraphName,
		const FString& SrcNodeGuid, const FString& SrcPinName,
		const FString& DstNodeGuid, const FString& DstPinName,
		const FString& InsertNodeGuid,
		const FString& InsertInPinName, const FString& InsertOutPinName);

	/**
	 * Swap a node's UClass while carrying over as many pin connections as
	 * possible. Pins are matched by exact name between old and new node;
	 * only pins whose PinType is schema-compatible get rewired. Dropped
	 * pins + reconnected pins are reported so the caller can fix orphans.
	 *
	 * @param OldNodeGuid  Node to replace.
	 * @param NewNodeClassPath  `/Script/<Module>.<Class>` or short class name
	 *                          of a UK2Node subclass.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherReplaceNodeReport ReplaceNodePreservingConnections(
		const FString& BlueprintPath, const FString& GraphName,
		const FString& OldNodeGuid, const FString& NewNodeClassPath);

	// ─── Batch graph ops (#1) ───────────────────────────────────────

	/**
	 * Execute a sequence of graph mutations on one Blueprint in a single
	 * tether round-trip. Compiles the BP once at the end instead of once
	 * per op.
	 *
	 * Input JSON: `[{"op":"...", ...args}, ...]`. Supported ops:
	 *   - `{"op":"add_call_function", "graph":"<name>",
	 *       "target_class":"<path or empty for self>",
	 *       "function_name":"...", "x":0, "y":0}` → NewNodeGuid
	 *   - `{"op":"add_variable_node", "graph":"<name>",
	 *       "variable":"...", "is_set":false, "x":0, "y":0}` → NewNodeGuid
	 *   - `{"op":"add_node_by_class", "graph":"<name>",
	 *       "class":"K2Node_IfThenElse", "x":0, "y":0}` → NewNodeGuid
	 *   - `{"op":"connect_pins", "graph":"<name>",
	 *       "src_node":"<guid or $N>", "src_pin":"...",
	 *       "dst_node":"<guid or $N>", "dst_pin":"..."}`
	 *   - `{"op":"set_pin_default", "graph":"<name>",
	 *       "node":"<guid or $N>", "pin":"...", "value":"..."}`
	 *   - `{"op":"remove_node", "graph":"<name>", "node":"<guid or $N>"}`
	 *
	 * **Back-references.** Any `*_node` / `node` field accepts the literal
	 * string `$N` where N is the zero-based index of an earlier op that
	 * produced a NewNodeGuid. Lets a single batch build up a tree of
	 * connected new nodes without knowing their guids in advance.
	 *
	 * Ops run sequentially. A failed op records its error in its result row
	 * and the remaining ops continue; callers that want all-or-nothing
	 * semantics should check each result before using the BP.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherGraphOpResult> ApplyGraphOps(
		const FString& BlueprintPath, const FString& OpsJson);

	// ─── CDO override query (#5) ────────────────────────────────────

	/**
	 * Find every Blueprint under `PackagePath` whose CDO value for
	 * `VariableName` differs from the parent BP's CDO value for the same
	 * variable. Use case: "show me all BP_Enemy children where MaxHealth is
	 * overridden".
	 *
	 * @param DefiningBlueprintPath  The parent BP that owns the variable.
	 * @param VariableName  The member variable to check.
	 * @param PackagePath   Content root to scan (default "/Game" when empty).
	 * @return One row per overriding child BP. The parent BP itself is not
	 *         included even though it has a CDO value.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherCdoOverride> FindCdoVariableOverrides(
		const FString& DefiningBlueprintPath,
		const FString& VariableName,
		const FString& PackagePath);

	// ─── PIE node coverage (#3 / runtime observability) ─────────────

	/**
	 * Aggregate per-node hit counts from UE's script-trace ring buffer.
	 * The trace buffer is a live snapshot of the last ~1024 script-execution
	 * samples; this walks that buffer, keeps samples whose owning UFunction
	 * belongs to the given Blueprint's generated class (or a subclass), and
	 * aggregates per-node hit-count + last-hit time.
	 *
	 * Use case: "which nodes actually ran during my last PIE session?"
	 * Combine with `find_variable_references` / `find_function_call_sites`
	 * to spot dead branches an AI-generated BP left behind.
	 *
	 * **Limits.** The underlying ring is 1024 samples total across the whole
	 * editor, so very busy scripts overwrite earlier samples. Call soon after
	 * the scenario of interest stops ticking, and scope to a focused BP.
	 * Samples persist across PIE start/stop but reset on editor restart.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static TArray<FTetherNodeCoverageEntry> GetPIENodeCoverage(
		const FString& BlueprintPath);

	/**
	 * Set which instance of a Blueprint the editor's trace-stack machinery
	 * samples from. **Must be called before running a scenario** — UE's
	 * `FKismetDebugUtilities` only records trace samples for
	 * `ObjectBeingDebugged`, so without this, `get_pie_node_coverage`
	 * returns empty.
	 *
	 * @param ActorName  Actor label in the editor world (or PIE world when
	 *                   PIE is running) to pin as the debug object. Pass
	 *                   empty string to clear.
	 * @return true if the actor was found and set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static bool SetBlueprintDebugObject(const FString& BlueprintPath,
		const FString& ActorName);

	// ─── Breakpoint-hit snapshot (#6 / runtime var capture) ────────

	/**
	 * Return the most recent breakpoint hit captured for this Blueprint.
	 * The tether subscribes to `FBlueprintCoreDelegates::OnScriptException`
	 * at module startup; every time a breakpoint fires on a BP's graph, the
	 * active function name, node GUID, self-path, and every param / local /
	 * return value (serialized to ExportText) are snapshotted into a
	 * per-BP slot.
	 *
	 * Pair with `add_breakpoint` to turn the existing breakpoint API into a
	 * true debug loop: set a breakpoint, enter PIE, trigger, then pull the
	 * snapshot to see what the actor's state was at that moment.
	 *
	 * @return `bHasHit = false` when no breakpoint has fired for this BP
	 *         since editor start (or since `clear_last_breakpoint_hit`).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static FTetherBreakpointHit GetLastBreakpointHit(const FString& BlueprintPath);

	/** Clear the stored snapshot so the next breakpoint hit starts fresh. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static void ClearLastBreakpointHit(const FString& BlueprintPath);

	/**
	 * Resume script execution after a breakpoint hit by requesting an abort
	 * on the debugger. Useful for unsticking PIE / editor when a test
	 * triggered a breakpoint and needs to continue. Does nothing if no
	 * breakpoint is currently held.
	 *
	 * **Important:** if your exec call triggered the break, this won't reach
	 * the GameThread (Python queue is blocked). Use the server-level
	 * `tether.py resume` CLI instead, which bypasses the Python exec queue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static void ResumeScriptExecution();

	/**
	 * Sweep every Blueprint under `PackagePath` (default "/Game") and delete
	 * all breakpoints. Preventive hygiene for automated test flows — a stray
	 * breakpoint from a prior session can freeze the editor when an
	 * AI-driven test invokes a function that hits it, and the tether can't
	 * recover from the freeze (see ResumeScriptExecution note). Call this
	 * at the start of any unattended run.
	 *
	 * @return number of breakpoints removed across all scanned BPs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Blueprint")
	static int32 ClearProjectBreakpoints(const FString& PackagePath);
};
