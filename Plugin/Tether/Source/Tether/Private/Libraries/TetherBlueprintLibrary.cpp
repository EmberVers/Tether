#include "TetherBlueprintLibrary.h"
#include "TetherBlueprintImpl_Shared.h"
#include "Misc/EngineVersionComparison.h"
#include "Shared/TetherCompat.h"
#include "Shared/TetherTypeParse.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "UObject/UnrealType.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Self.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_Message.h"
#include "K2Node_Timeline.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Select.h"
#include "K2Node_Knot.h"
#include "EdGraphNode_Comment.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/UObjectToken.h"
#include "GameFramework/Actor.h"
#include "Engine/TimelineTemplate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditor.h"
#include "GraphEditor.h"
#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "SGraphPin.h"
#include "Layout/ArrangedChildren.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/Breakpoint.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeArray.h"
#include "K2Node_EnumLiteral.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintNodeSignature.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
#include "K2Node.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "JsonObjectConverter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Engine/LatentActionManager.h"
#include "Engine/DataTable.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Composite.h"
#include "Misc/SecureHash.h"
#include "Misc/ScopeExit.h"
#include "UObject/Script.h"
#include "UObject/Stack.h"
#include "ScopedTransaction.h"
#if !UE_VERSION_OLDER_THAN(5, 4, 0)
#include "Blueprint/BlueprintExceptionInfo.h"
#endif
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_GetInputActionValue.h"
#include "K2Node_InputAction.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_InputKey.h"
#include "K2Node_InputAxisKeyEvent.h"
#include "K2Node_GetSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherBlueprintGraph, Log, All);

// ─── Helpers ─────────────────────────────────────────────────

static FTetherClassInfo MakeClassInfo(const UClass* InClass)
{
	FTetherClassInfo Info;
	if (!InClass) return Info;

	Info.ClassName = InClass->GetName();
	Info.ClassPath = InClass->GetPathName();
	Info.bIsNative = !InClass->IsChildOf<UBlueprintGeneratedClass>()
	              && !InClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
	return Info;
}

/** Convert an FProperty's type to a human-readable string. */
FString PropertyTypeToString(const FProperty* Prop)
{
	if (!Prop) return TEXT("Unknown");

	// Array
	if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		return FString::Printf(TEXT("Array of %s"), *PropertyTypeToString(ArrayProp->Inner));
	}
	// Set
	if (const FSetProperty* SetProp = CastField<FSetProperty>(Prop))
	{
		return FString::Printf(TEXT("Set of %s"), *PropertyTypeToString(SetProp->ElementProp));
	}
	// Map
	if (const FMapProperty* MapProp = CastField<FMapProperty>(Prop))
	{
		return FString::Printf(TEXT("Map<%s, %s>"),
			*PropertyTypeToString(MapProp->KeyProp),
			*PropertyTypeToString(MapProp->ValueProp));
	}
	// Object reference
	if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
	{
		UClass* ObjClass = ObjProp->PropertyClass;
		return ObjClass ? ObjClass->GetName() : TEXT("Object");
	}
	// Struct
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		return StructProp->Struct ? StructProp->Struct->GetName() : TEXT("Struct");
	}
	// Enum
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		UEnum* Enum = EnumProp->GetEnum();
		return Enum ? Enum->GetName() : TEXT("Enum");
	}
	if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		if (ByteProp->Enum)
		{
			return ByteProp->Enum->GetName();
		}
		return TEXT("Byte");
	}
	// Class/SoftClass reference
	if (const FClassProperty* ClassProp = CastField<FClassProperty>(Prop))
	{
		UClass* MetaClass = ClassProp->MetaClass;
		return FString::Printf(TEXT("Class<%s>"), MetaClass ? *MetaClass->GetName() : TEXT("Object"));
	}
	// Delegate
	if (CastField<FDelegateProperty>(Prop))
	{
		return TEXT("Delegate");
	}
	if (CastField<FMulticastDelegateProperty>(Prop))
	{
		return TEXT("MulticastDelegate");
	}

	// Primitives
	if (CastField<FBoolProperty>(Prop))       return TEXT("Bool");
	if (CastField<FIntProperty>(Prop))         return TEXT("Int");
	if (CastField<FInt64Property>(Prop))       return TEXT("Int64");
	if (CastField<FFloatProperty>(Prop))       return TEXT("Float");
	if (CastField<FDoubleProperty>(Prop))      return TEXT("Double");
	if (CastField<FStrProperty>(Prop))         return TEXT("String");
	if (CastField<FNameProperty>(Prop))        return TEXT("Name");
	if (CastField<FTextProperty>(Prop))        return TEXT("Text");

	return Prop->GetCPPType();
}

/** Best-effort export of a property's default value from a CDO. */
static FString GetDefaultValueString(const FProperty* Prop, const UObject* CDO)
{
	if (!Prop || !CDO) return FString();

	FString Result;
	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(CDO);
	Prop->ExportTextItem_Direct(Result, ValuePtr, nullptr, nullptr, PPF_None);
	return Result;
}

// ─── Class Hierarchy (existing) ──────────────────────────────

bool UTetherBlueprintLibrary::GetBlueprintParentClass(
	const FString& BlueprintPath, FTetherClassInfo& OutParentInfo)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return false;

	UClass* Super = BP->GeneratedClass->GetSuperClass();
	if (!Super) return false;

	OutParentInfo = MakeClassInfo(Super);
	return true;
}

TArray<FTetherClassInfo> UTetherBlueprintLibrary::GetBlueprintClassHierarchy(
	const FString& BlueprintPath)
{
	TArray<FTetherClassInfo> Hierarchy;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return Hierarchy;

	for (const UClass* Cur = BP->GeneratedClass; Cur; Cur = Cur->GetSuperClass())
	{
		Hierarchy.Add(MakeClassInfo(Cur));
	}
	return Hierarchy;
}

// ─── Variables ───────────────────────────────────────────────

TArray<FTetherVariableInfo> UTetherBlueprintLibrary::GetBlueprintVariables(
	const FString& BlueprintPath, bool bIncludeInherited)
{
	TArray<FTetherVariableInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return Result;

	const UClass* GenClass = BP->GeneratedClass;
	const UObject* CDO = GenClass->GetDefaultObject();

	// Collect the set of "new variables" names defined by this BP
	TSet<FName> OwnVariableNames;
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		OwnVariableNames.Add(Var.VarName);
	}

	// Iterate the BP's NewVariables for metadata, match with FProperty for type/default
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		const FProperty* Prop = GenClass->FindPropertyByName(Var.VarName);

		FTetherVariableInfo Info;
		Info.Name = Var.VarName.ToString();
		Info.Type = Prop ? PropertyTypeToString(Prop) : Var.VarType.PinCategory.ToString();
		Info.Category = Var.Category.ToString();
		if (Var.HasMetaData(TEXT("tooltip")))
		{
			Info.Description = Var.GetMetaData(TEXT("tooltip"));
		}
		Info.DefaultValue = Prop ? GetDefaultValueString(Prop, CDO) : Var.DefaultValue;
		Info.bInstanceEditable = Var.PropertyFlags & CPF_Edit ? true : false;
		Info.bBlueprintReadOnly = Var.PropertyFlags & CPF_BlueprintReadOnly ? true : false;

		if (Var.PropertyFlags & CPF_Net)
		{
			Info.ReplicationCondition = (Var.PropertyFlags & CPF_RepNotify)
				? TEXT("RepNotify")
				: TEXT("Replicated");
		}
		else
		{
			Info.ReplicationCondition = TEXT("None");
		}

		Result.Add(Info);
	}

	// Optionally include inherited variables
	if (bIncludeInherited)
	{
		const UClass* SuperClass = GenClass->GetSuperClass();
		for (TFieldIterator<FProperty> It(SuperClass); It; ++It)
		{
			const FProperty* Prop = *It;
			if (!Prop->HasAnyPropertyFlags(CPF_Parm) && !OwnVariableNames.Contains(Prop->GetFName()))
			{
				// Skip internal/hidden properties
				if (Prop->HasAnyPropertyFlags(CPF_DisableEditOnInstance) && !Prop->HasAnyPropertyFlags(CPF_Edit))
				{
					continue;
				}

				FTetherVariableInfo Info;
				Info.Name = Prop->GetName();
				Info.Type = PropertyTypeToString(Prop);

				if (const FString* Cat = Prop->FindMetaData(TEXT("Category")))
				{
					Info.Category = *Cat;
				}

				const UObject* SuperCDO = SuperClass->GetDefaultObject();
				Info.DefaultValue = GetDefaultValueString(Prop, SuperCDO);
				Info.bInstanceEditable = Prop->HasAnyPropertyFlags(CPF_Edit);
				Info.bBlueprintReadOnly = Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly);

				if (Prop->HasAnyPropertyFlags(CPF_Net))
				{
					Info.ReplicationCondition = Prop->HasAnyPropertyFlags(CPF_RepNotify)
						? TEXT("RepNotify") : TEXT("Replicated");
				}
				else
				{
					Info.ReplicationCondition = TEXT("None");
				}

				Result.Add(Info);
			}
		}
	}

	return Result;
}

// ─── Functions / Events ──────────────────────────────────────

TArray<FTetherFunctionInfo> UTetherBlueprintLibrary::GetBlueprintFunctions(
	const FString& BlueprintPath, bool bIncludeInherited)
{
	TArray<FTetherFunctionInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return Result;

	const UClass* GenClass = BP->GeneratedClass;
	const UClass* SuperClass = GenClass->GetSuperClass();

	for (TFieldIterator<UFunction> It(GenClass, bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		const UFunction* Func = *It;
		if (!Func) continue;

		// Skip hidden/internal functions
		FString FuncName = Func->GetName();
		if (FuncName.StartsWith(TEXT("ExecuteUbergraph"))) continue;
		if (FuncName.StartsWith(TEXT("UserConstructionScript")) && !bIncludeInherited) continue;

		FTetherFunctionInfo Info;
		Info.Name = FuncName;

		// Determine kind
		if (Func->HasAnyFunctionFlags(FUNC_Event | FUNC_BlueprintEvent))
		{
			// Check if it's an override of a parent event
			if (SuperClass && SuperClass->FindFunctionByName(Func->GetFName()))
			{
				Info.Kind = TEXT("Override");
			}
			else
			{
				Info.Kind = TEXT("Event");
			}
		}
		else
		{
			Info.Kind = TEXT("Function");
		}

		// Access
		if (Func->HasAnyFunctionFlags(FUNC_Public))
			Info.Access = TEXT("Public");
		else if (Func->HasAnyFunctionFlags(FUNC_Protected))
			Info.Access = TEXT("Protected");
		else
			Info.Access = TEXT("Private");

		Info.bIsPure = Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
		Info.bIsStatic = Func->HasAnyFunctionFlags(FUNC_Static);

		// Category
		Info.Category = Func->GetMetaData(TEXT("Category"));

		// Description
		Info.Description = Func->GetMetaData(TEXT("ToolTip"));

		// Parameters — only include real function params, not BP graph locals
		for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
		{
			const FProperty* Param = *ParamIt;
			if (!Param->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}

			FTetherFunctionParam P;
			P.Name = Param->GetName();
			P.Type = PropertyTypeToString(Param);
			P.bIsOutput = Param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm);
			Info.Params.Add(P);
		}

		Result.Add(Info);
	}

	// Fallback: include user-authored function graphs that haven't been
	// compiled yet (no UFunction on GeneratedClass exists for them). Without
	// this, create_function_graph followed by get_blueprint_functions would
	// silently return [] until the next recompile_blueprint.
	if (!bIncludeInherited)
	{
		TSet<FString> AlreadyListed;
		for (const FTetherFunctionInfo& Info : Result)
		{
			AlreadyListed.Add(Info.Name);
		}

		for (const UEdGraph* Graph : BP->FunctionGraphs)
		{
			if (!Graph) continue;

			const FString GraphName = Graph->GetFName().ToString();
			if (GraphName.StartsWith(TEXT("UserConstructionScript"))) continue;
			if (AlreadyListed.Contains(GraphName)) continue;

			FTetherFunctionInfo Info;
			Info.Name = GraphName;
			Info.Kind = TEXT("Function");
			Info.Access = TEXT("Public");
			Info.Description = TEXT("(uncompiled — call recompile_blueprint to refresh signature/params)");
			Result.Add(Info);
		}
	}

	return Result;
}

// ─── Components ──────────────────────────────────────────────

TArray<FTetherComponentInfo> UTetherBlueprintLibrary::GetBlueprintComponents(
	const FString& BlueprintPath)
{
	TArray<FTetherComponentInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	// Components from SimpleConstructionScript (this BP's own components)
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	if (SCS)
	{
		const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
		for (const USCS_Node* Node : AllNodes)
		{
			if (!Node || !Node->ComponentClass) continue;

			FTetherComponentInfo Info;
			Info.Name = Node->GetVariableName().ToString();
			Info.ComponentClass = Node->ComponentClass->GetName();
			Info.bIsInherited = false;

			// Find parent
			if (Node->ParentComponentOrVariableName != NAME_None)
			{
				Info.ParentName = Node->ParentComponentOrVariableName.ToString();
			}
			else
			{
				// Check if it's a root node
				const TArray<USCS_Node*>& RootNodes = SCS->GetRootNodes();
				Info.bIsRoot = RootNodes.Contains(Node);
			}

			Result.Add(Info);
		}
	}

	// Inherited components from parent CDO
	const UClass* SuperClass = BP->ParentClass;
	if (SuperClass)
	{
		const UObject* SuperCDO = SuperClass->GetDefaultObject();
		if (SuperCDO)
		{
			TArray<UActorComponent*> InheritedComponents;
			// Get components from the parent CDO
			if (const AActor* ActorCDO = Cast<AActor>(SuperCDO))
			{
				ActorCDO->GetComponents(InheritedComponents);
				for (const UActorComponent* Comp : InheritedComponents)
				{
					FTetherComponentInfo Info;
					Info.Name = Comp->GetName();
					Info.ComponentClass = Comp->GetClass()->GetName();
					Info.bIsInherited = true;
					Info.bIsRoot = (Comp == ActorCDO->GetRootComponent());
					Result.Add(Info);
				}
			}
		}
	}

	return Result;
}

// ─── Interfaces ──────────────────────────────────────────────

TArray<FTetherInterfaceInfo> UTetherBlueprintLibrary::GetBlueprintInterfaces(
	const FString& BlueprintPath)
{
	TArray<FTetherInterfaceInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	for (const FBPInterfaceDescription& Iface : BP->ImplementedInterfaces)
	{
		FTetherInterfaceInfo Info;

		UClass* IfaceClass = Iface.Interface;
		if (!IfaceClass) continue;

		Info.InterfaceName = IfaceClass->GetName();
		Info.InterfacePath = IfaceClass->GetPathName();

		// Check if the interface itself is Blueprint-generated
		Info.bIsBlueprintImplemented = IfaceClass->IsChildOf<UBlueprintGeneratedClass>()
			|| IfaceClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);

		// List functions declared by this interface
		for (TFieldIterator<UFunction> It(IfaceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			const UFunction* Func = *It;
			if (Func && !Func->GetName().StartsWith(TEXT("ExecuteUbergraph")))
			{
				Info.Functions.Add(Func->GetName());
			}
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── Graph helpers ───────────────────────────────────────────

/** Find graphs matching a function name in a Blueprint. Empty name = EventGraph / UbergraphPages. */
static TArray<UEdGraph*> FindGraphs(UBlueprint* BP, const FString& FunctionName)
{
	TArray<UEdGraph*> Graphs;

	if (FunctionName.IsEmpty())
	{
		Graphs = BP->UbergraphPages;
	}
	else
	{
		for (UEdGraph* Graph : BP->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				Graphs.Add(Graph);
				return Graphs;
			}
		}
		for (UEdGraph* Graph : BP->UbergraphPages)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				Graphs.Add(Graph);
				return Graphs;
			}
		}
		for (UEdGraph* Graph : BP->MacroGraphs)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				Graphs.Add(Graph);
				return Graphs;
			}
		}
	}

	return Graphs;
}

/** Classify a node into a simple type string. */
static FString ClassifyNode(const UEdGraphNode* Node)
{
	if (Cast<UK2Node_CallFunction>(Node))       return TEXT("FunctionCall");
	if (Cast<UK2Node_VariableGet>(Node))        return TEXT("VariableGet");
	if (Cast<UK2Node_VariableSet>(Node))        return TEXT("VariableSet");
	if (Cast<UK2Node_IfThenElse>(Node))         return TEXT("Branch");
	if (Cast<UK2Node_DynamicCast>(Node))        return TEXT("Cast");
	if (Cast<UK2Node_MacroInstance>(Node))       return TEXT("Macro");
	if (Cast<UK2Node_CustomEvent>(Node))        return TEXT("Event");
	if (Cast<UK2Node_Event>(Node))              return TEXT("Event");
	if (Cast<UK2Node_FunctionEntry>(Node))      return TEXT("FunctionEntry");

	FString ClassName = Node->GetClass()->GetName();
	ClassName.RemoveFromStart(TEXT("K2Node_"));
	return ClassName;
}

// ─── GetFunctionCallGraph ────────────────────────────────────

TArray<FTetherCallEdge> UTetherBlueprintLibrary::GetFunctionCallGraph(
	const FString& BlueprintPath, const FString& FunctionName)
{
	TArray<FTetherCallEdge> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);
	TSet<FString> Seen;

	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				FTetherCallEdge Edge;
				Edge.TargetName = CallNode->GetFunctionName().ToString();
				Edge.TargetKind = TEXT("Function");

				UFunction* TargetFunc = CallNode->GetTargetFunction();
				if (TargetFunc && TargetFunc->GetOwnerClass())
				{
					Edge.TargetClass = TargetFunc->GetOwnerClass()->GetName();
				}

				FString Key = Edge.TargetClass + TEXT("::") + Edge.TargetName;
				if (!Seen.Contains(Key))
				{
					Seen.Add(Key);
					Result.Add(Edge);
				}
			}
			else if (const UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
			{
				FTetherCallEdge Edge;
				UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
				Edge.TargetName = MacroGraph ? MacroGraph->GetName() : TEXT("Unknown");
				Edge.TargetClass = TEXT("Macro");
				Edge.TargetKind = TEXT("Macro");

				FString Key = Edge.TargetKind + TEXT("::") + Edge.TargetName;
				if (!Seen.Contains(Key))
				{
					Seen.Add(Key);
					Result.Add(Edge);
				}
			}
		}
	}

	return Result;
}

// ─── GetFunctionNodes ────────────────────────────────────────

TArray<FTetherNodeInfo> UTetherBlueprintLibrary::GetFunctionNodes(
	const FString& BlueprintPath, const FString& FunctionName, const FString& NodeTypeFilter)
{
	TArray<FTetherNodeInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);

	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			FString NodeType = ClassifyNode(Node);

			if (!NodeTypeFilter.IsEmpty() && NodeType != NodeTypeFilter)
			{
				continue;
			}

			FTetherNodeInfo Info;
			Info.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			Info.NodeType = NodeType;
			Info.Comment = Node->NodeComment;
			Info.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);

			if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				UFunction* TargetFunc = CallNode->GetTargetFunction();
				if (TargetFunc && TargetFunc->GetOwnerClass())
				{
					Info.TargetClass = TargetFunc->GetOwnerClass()->GetName();
				}
			}
			else if (const UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
			{
				Info.VariableName = VarNode->GetVarNameString();
			}
			else if (const UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
			{
				if (CastNode->TargetType)
				{
					Info.TargetClass = CastNode->TargetType->GetName();
				}
			}
			else if (const UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
			{
				UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
				if (MacroGraph)
				{
					Info.Title = MacroGraph->GetName();
				}
			}

			Result.Add(Info);
		}
	}

	return Result;
}

// ─── GetBlueprintOverview ───────────────────────────────────

bool UTetherBlueprintLibrary::GetBlueprintOverview(
	const FString& BlueprintPath, FTetherBlueprintOverview& OutOverview)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return false;

	const UClass* GenClass = BP->GeneratedClass;
	const UClass* Super = GenClass->GetSuperClass();

	OutOverview.BlueprintName = BP->GetName();
	OutOverview.ParentClass = MakeClassInfo(Super);

	// Determine BP type from first native ancestor
	const UClass* NativeBase = Super;
	while (NativeBase
		&& (NativeBase->IsChildOf<UBlueprintGeneratedClass>()
			|| NativeBase->HasAnyClassFlags(CLASS_CompiledFromBlueprint)))
	{
		NativeBase = NativeBase->GetSuperClass();
	}
	OutOverview.BlueprintType = NativeBase ? NativeBase->GetName() : TEXT("Object");

	// ── Compact variables (skip event dispatchers) ──
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		const FProperty* Prop = GenClass->FindPropertyByName(Var.VarName);
		if (Prop && CastField<FMulticastDelegateProperty>(Prop))
			continue;

		FTetherVariableSummary VS;
		VS.Name = Var.VarName.ToString();
		VS.Type = Prop ? PropertyTypeToString(Prop) : Var.VarType.PinCategory.ToString();
		OutOverview.Variables.Add(VS);
	}

	// ── Compact functions ──
	for (TFieldIterator<UFunction> It(GenClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		const UFunction* Func = *It;
		if (!Func) continue;
		FString FuncName = Func->GetName();
		if (FuncName.StartsWith(TEXT("ExecuteUbergraph"))) continue;
		if (FuncName.Contains(TEXT("__DelegateSignature"))) continue;

		FTetherFunctionSummary FS;
		FS.Name = FuncName;

		if (Func->HasAnyFunctionFlags(FUNC_Event | FUNC_BlueprintEvent))
		{
			FS.Kind = (Super && Super->FindFunctionByName(Func->GetFName()))
				? TEXT("Override") : TEXT("Event");
		}
		else
		{
			FS.Kind = TEXT("Function");
		}

		// Build compact signature
		TArray<FString> InParams, OutParams;
		for (TFieldIterator<FProperty> PIt(Func); PIt; ++PIt)
		{
			const FProperty* Param = *PIt;
			if (!Param->HasAnyPropertyFlags(CPF_Parm)) continue;
			FString T = PropertyTypeToString(Param);
			if (Param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
				OutParams.Add(T);
			else
				InParams.Add(T);
		}
		FS.Signature = TEXT("(") + FString::Join(InParams, TEXT(", ")) + TEXT(")");
		if (OutParams.Num() > 0)
			FS.Signature += TEXT(" -> ") + FString::Join(OutParams, TEXT(", "));

		OutOverview.Functions.Add(FS);
	}

	// ── Compact components ──
	if (USimpleConstructionScript* SCS = BP->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : SCS->GetAllNodes())
		{
			if (!Node || !Node->ComponentClass) continue;
			FTetherComponentSummary CS;
			CS.Name = Node->GetVariableName().ToString();
			CS.ComponentClass = Node->ComponentClass->GetName();
			if (Node->ParentComponentOrVariableName != NAME_None)
				CS.ParentName = Node->ParentComponentOrVariableName.ToString();
			OutOverview.Components.Add(CS);
		}
	}

	// ── Interface names ──
	for (const FBPInterfaceDescription& Iface : BP->ImplementedInterfaces)
	{
		if (Iface.Interface)
			OutOverview.Interfaces.Add(Iface.Interface->GetName());
	}

	// ── Event dispatcher names ──
	for (UEdGraph* SigGraph : BP->DelegateSignatureGraphs)
	{
		if (!SigGraph) continue;
		FString Name = SigGraph->GetName();
		static const FString DelegateSuffix = TEXT("__DelegateSignature");
		if (Name.EndsWith(DelegateSuffix))
			Name = Name.LeftChop(DelegateSuffix.Len());
		OutOverview.EventDispatchers.Add(Name);
	}

	// ── Graph names ──
	for (UEdGraph* G : BP->UbergraphPages)
		if (G) OutOverview.GraphNames.Add(G->GetName());
	for (UEdGraph* G : BP->FunctionGraphs)
		if (G) OutOverview.GraphNames.Add(G->GetName());
	for (UEdGraph* G : BP->MacroGraphs)
		if (G) OutOverview.GraphNames.Add(G->GetName());

	return true;
}

// ─── GetEventDispatchers ────────────────────────────────────

TArray<FTetherEventDispatcherInfo> UTetherBlueprintLibrary::GetEventDispatchers(
	const FString& BlueprintPath)
{
	TArray<FTetherEventDispatcherInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return Result;

	const UClass* GenClass = BP->GeneratedClass;

	for (UEdGraph* SigGraph : BP->DelegateSignatureGraphs)
	{
		if (!SigGraph) continue;

		FTetherEventDispatcherInfo Info;
		FString GraphName = SigGraph->GetName();
		static const FString DelegateSuffix = TEXT("__DelegateSignature");
		if (GraphName.EndsWith(DelegateSuffix))
			Info.Name = GraphName.LeftChop(DelegateSuffix.Len());
		else
			Info.Name = GraphName;

		// Get params from the delegate property's signature function
		FProperty* Prop = GenClass->FindPropertyByName(FName(*Info.Name));
		FMulticastDelegateProperty* MCDProp = CastField<FMulticastDelegateProperty>(Prop);
		if (MCDProp && MCDProp->SignatureFunction)
		{
			for (TFieldIterator<FProperty> PIt(MCDProp->SignatureFunction); PIt; ++PIt)
			{
				const FProperty* Param = *PIt;
				if (!Param->HasAnyPropertyFlags(CPF_Parm)) continue;

				FTetherFunctionParam P;
				P.Name = Param->GetName();
				P.Type = PropertyTypeToString(Param);
				P.bIsOutput = Param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm);
				Info.Params.Add(P);
			}
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── GetGraphNames ──────────────────────────────────────────

TArray<FTetherGraphInfo> UTetherBlueprintLibrary::GetGraphNames(
	const FString& BlueprintPath)
{
	TArray<FTetherGraphInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		if (!Graph) continue;
		FTetherGraphInfo Info;
		Info.Name = Graph->GetName();
		Info.GraphType = TEXT("EventGraph");
		Result.Add(Info);
	}

	for (UEdGraph* Graph : BP->FunctionGraphs)
	{
		if (!Graph) continue;
		FTetherGraphInfo Info;
		Info.Name = Graph->GetName();
		Info.GraphType = TEXT("Function");
		Result.Add(Info);
	}

	for (UEdGraph* Graph : BP->MacroGraphs)
	{
		if (!Graph) continue;
		FTetherGraphInfo Info;
		Info.Name = Graph->GetName();
		Info.GraphType = TEXT("Macro");
		Result.Add(Info);
	}

	for (UEdGraph* Graph : BP->DelegateSignatureGraphs)
	{
		if (!Graph) continue;
		FTetherGraphInfo Info;
		FString Name = Graph->GetName();
		static const FString DelegateSuffix = TEXT("__DelegateSignature");
		if (Name.EndsWith(DelegateSuffix))
			Name = Name.LeftChop(DelegateSuffix.Len());
		Info.Name = Name;
		Info.GraphType = TEXT("EventDispatcher");
		Result.Add(Info);
	}

	return Result;
}

// ─── Exec flow helper ───────────────────────────────────────

/** Get a compact detail string for a node (function name, variable, cast target, etc.). */
static FString GetNodeDetail(const UEdGraphNode* Node)
{
	if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
	{
		UFunction* Func = CallNode->GetTargetFunction();
		if (Func && Func->GetOwnerClass())
			return Func->GetOwnerClass()->GetName() + TEXT("::") + CallNode->GetFunctionName().ToString();
		return CallNode->GetFunctionName().ToString();
	}
	if (const UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
	{
		return VarNode->GetVarNameString();
	}
	if (const UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
	{
		return CastNode->TargetType ? CastNode->TargetType->GetName() : FString();
	}
	if (const UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
	{
		UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
		return MacroGraph ? MacroGraph->GetName() : FString();
	}
	if (const UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(Node))
	{
		return EventNode->CustomFunctionName.ToString();
	}
	if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		return EventNode->GetFunctionName().ToString();
	}
	return FString();
}

// ─── GetFunctionExecutionFlow ───────────────────────────────

TArray<FTetherExecStep> UTetherBlueprintLibrary::GetFunctionExecutionFlow(
	const FString& BlueprintPath, const FString& FunctionName)
{
	TArray<FTetherExecStep> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);

	// Collect all nodes across matched graphs
	TArray<UEdGraphNode*> AllNodes;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph)
			AllNodes.Append(Graph->Nodes);
	}

	// Find entry points: FunctionEntry, Event, CustomEvent
	TArray<UEdGraphNode*> EntryNodes;
	for (UEdGraphNode* Node : AllNodes)
	{
		if (!Node) continue;
		if (Cast<UK2Node_FunctionEntry>(Node)
			|| Cast<UK2Node_Event>(Node)
			|| Cast<UK2Node_CustomEvent>(Node))
		{
			EntryNodes.Add(Node);
		}
	}

	// BFS along exec pins from all entry points
	TMap<const UEdGraphNode*, int32> NodeToStep;
	TArray<UEdGraphNode*> Ordered;
	TArray<UEdGraphNode*> Queue;
	int32 QueueHead = 0;

	for (UEdGraphNode* Entry : EntryNodes)
		Queue.AddUnique(Entry);

	while (QueueHead < Queue.Num())
	{
		UEdGraphNode* Current = Queue[QueueHead++];
		if (NodeToStep.Contains(Current)) continue;

		int32 Idx = Ordered.Num();
		NodeToStep.Add(Current, Idx);
		Ordered.Add(Current);

		// Enqueue nodes connected via exec output pins
		for (const UEdGraphPin* Pin : Current->Pins)
		{
			if (Pin->Direction != EGPD_Output) continue;
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;

			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				UEdGraphNode* Target = Linked ? Linked->GetOwningNode() : nullptr;
				if (Target && !NodeToStep.Contains(Target))
					Queue.AddUnique(Target);
			}
		}
	}

	// Build result array
	for (int32 i = 0; i < Ordered.Num(); i++)
	{
		const UEdGraphNode* Node = Ordered[i];

		FTetherExecStep Step;
		Step.StepIndex = i;
		Step.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		Step.NodeType = ClassifyNode(Node);
		Step.Detail = GetNodeDetail(Node);

		// Record exec output connections with branching
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction != EGPD_Output) continue;
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;

			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				const UEdGraphNode* Target = Linked ? Linked->GetOwningNode() : nullptr;
				if (Target && NodeToStep.Contains(Target))
				{
					FTetherExecConnection Conn;
					Conn.PinName = Pin->PinName.ToString();
					Conn.TargetStepIndex = NodeToStep[Target];
					Step.ExecOutputs.Add(Conn);
				}
			}
		}

		Result.Add(Step);
	}

	return Result;
}

// ─── GetNodePinConnections ──────────────────────────────────

TArray<FTetherPinConnection> UTetherBlueprintLibrary::GetNodePinConnections(
	const FString& BlueprintPath, const FString& FunctionName)
{
	TArray<FTetherPinConnection> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);

	// Build node index map (same iteration order as GetFunctionNodes with empty filter)
	TMap<const UEdGraphNode*, int32> NodeIndexMap;
	int32 CurrentIndex = 0;
	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
				NodeIndexMap.Add(Node, CurrentIndex++);
		}
	}

	// Collect all output→input connections (output side only to avoid duplicates)
	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			const int32* SourceIdxPtr = NodeIndexMap.Find(Node);
			if (!SourceIdxPtr) continue;

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction != EGPD_Output) continue;

				const bool bExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);

				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin) continue;
					const UEdGraphNode* TargetNode = LinkedPin->GetOwningNode();
					if (!TargetNode) continue;

					const int32* TargetIdxPtr = NodeIndexMap.Find(TargetNode);
					if (!TargetIdxPtr) continue;

					FTetherPinConnection Conn;
					Conn.SourceNodeIndex = *SourceIdxPtr;
					Conn.SourcePinName = Pin->PinName.ToString();
					Conn.TargetNodeIndex = *TargetIdxPtr;
					Conn.TargetPinName = LinkedPin->PinName.ToString();
					Conn.bIsExec = bExec;
					Result.Add(Conn);
				}
			}
		}
	}

	return Result;
}

// ─── GetComponentPropertyValues ─────────────────────────────

TArray<FTetherPropertyValue> UTetherBlueprintLibrary::GetComponentPropertyValues(
	const FString& BlueprintPath, const FString& ComponentName)
{
	TArray<FTetherPropertyValue> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	UActorComponent* Template = nullptr;

	// Search SCS components
	if (BP->SimpleConstructionScript)
	{
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				Template = Node->ComponentTemplate;
				break;
			}
		}
	}

	// Search inherited components on CDO
	if (!Template && BP->GeneratedClass)
	{
		if (AActor* ActorCDO = Cast<AActor>(BP->GeneratedClass->GetDefaultObject()))
		{
			TArray<UActorComponent*> Components;
			ActorCDO->GetComponents(Components);
			for (UActorComponent* Comp : Components)
			{
				if (Comp && Comp->GetName() == ComponentName)
				{
					Template = Comp;
					break;
				}
			}
		}
	}

	if (!Template) return Result;

	// Compare against component class CDO
	const UObject* CompCDO = Template->GetClass()->GetDefaultObject();

	for (TFieldIterator<FProperty> It(Template->GetClass()); It; ++It)
	{
		const FProperty* Prop = *It;
		if (!Prop) continue;
		if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient)) continue;

		const void* TemplateVal = Prop->ContainerPtrToValuePtr<void>(Template);
		const void* CDOVal = Prop->ContainerPtrToValuePtr<void>(CompCDO);

		if (!Prop->Identical(TemplateVal, CDOVal))
		{
			FTetherPropertyValue PV;
			PV.Name = Prop->GetName();
			PV.Type = PropertyTypeToString(Prop);
			Prop->ExportTextItem_Direct(PV.Value, TemplateVal, nullptr, nullptr, PPF_None);

			if (const FString* Cat = Prop->FindMetaData(TEXT("Category")))
				PV.Category = *Cat;

			Result.Add(PV);
		}
	}

	return Result;
}

// ─── SearchBlueprintNodes ───────────────────────────────────

TArray<FTetherNodeSearchResult> UTetherBlueprintLibrary::SearchBlueprintNodes(
	const FString& BlueprintPath, const FString& Query, const FString& NodeTypeFilter)
{
	TArray<FTetherNodeSearchResult> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	FString QueryLower = Query.ToLower();

	// Helper: collect graphs with type labels
	struct FGraphEntry { UEdGraph* Graph; FString Type; };
	TArray<FGraphEntry> AllGraphs;

	for (UEdGraph* G : BP->UbergraphPages)
		if (G) AllGraphs.Add({G, TEXT("EventGraph")});
	for (UEdGraph* G : BP->FunctionGraphs)
		if (G) AllGraphs.Add({G, TEXT("Function")});
	for (UEdGraph* G : BP->MacroGraphs)
		if (G) AllGraphs.Add({G, TEXT("Macro")});

	for (const FGraphEntry& Entry : AllGraphs)
	{
		for (const UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (!Node) continue;

			FString NType = ClassifyNode(Node);
			if (!NodeTypeFilter.IsEmpty() && NType != NodeTypeFilter) continue;

			FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			FString Detail = GetNodeDetail(Node);

			// Match query against title or detail (case-insensitive)
			bool bMatch = QueryLower.IsEmpty()
				|| Title.ToLower().Contains(QueryLower)
				|| Detail.ToLower().Contains(QueryLower);

			if (bMatch)
			{
				FTetherNodeSearchResult SR;
				SR.GraphName = Entry.Graph->GetName();
				SR.GraphType = Entry.Type;
				SR.NodeTitle = Title;
				SR.NodeType = NType;
				SR.Detail = Detail;
				Result.Add(SR);
			}
		}
	}

	return Result;
}

// ─── GetTimelineInfo ────────────────────────────────────────

TArray<FTetherTimelineInfo> UTetherBlueprintLibrary::GetTimelineInfo(
	const FString& BlueprintPath)
{
	TArray<FTetherTimelineInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;

	for (const UTimelineTemplate* TL : BP->Timelines)
	{
		if (!TL) continue;

		FTetherTimelineInfo Info;
		Info.Name = TL->GetName();
		// Strip trailing "_Template" if present
		static const FString TemplateSuffix = TEXT("_Template");
		if (Info.Name.EndsWith(TemplateSuffix))
			Info.Name = Info.Name.LeftChop(TemplateSuffix.Len());

		Info.Length = TL->TimelineLength;
		Info.bAutoPlay = TL->bAutoPlay;
		Info.bLoop = TL->bLoop;
		Info.bReplicated = TL->bReplicated;

		for (const FTTFloatTrack& Track : TL->FloatTracks)
		{
			FTetherTimelineTrack T;
			T.TrackName = Track.GetTrackName().ToString();
			T.TrackType = TEXT("Float");
			Info.Tracks.Add(T);
		}
		for (const FTTVectorTrack& Track : TL->VectorTracks)
		{
			FTetherTimelineTrack T;
			T.TrackName = Track.GetTrackName().ToString();
			T.TrackType = TEXT("Vector");
			Info.Tracks.Add(T);
		}
		for (const FTTLinearColorTrack& Track : TL->LinearColorTracks)
		{
			FTetherTimelineTrack T;
			T.TrackName = Track.GetTrackName().ToString();
			T.TrackType = TEXT("LinearColor");
			Info.Tracks.Add(T);
		}
		for (const FTTEventTrack& Track : TL->EventTracks)
		{
			FTetherTimelineTrack T;
			T.TrackName = Track.GetTrackName().ToString();
			T.TrackType = TEXT("Event");
			Info.Tracks.Add(T);
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── Type string parser ─────────────────────────────────────
// (Moved to TetherTypeParse.{h,cpp} so TetherStructLibrary
//  can reuse the same parse/serialize logic for UserDefinedStruct fields.)

// ─── SetBlueprintVariableDefault ────────────────────────────

bool UTetherBlueprintLibrary::SetBlueprintVariableDefault(
	const FString& BlueprintPath, const FString& VariableName, const FString& Value)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	FName VarFName(*VariableName);

	// Try CDO path first (best: validates the value)
	if (BP->GeneratedClass)
	{
		UObject* CDO = BP->GeneratedClass->GetDefaultObject();
		FProperty* Prop = BP->GeneratedClass->FindPropertyByName(VarFName);
		if (Prop && CDO)
		{
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(CDO);
			if (Prop->ImportText_Direct(*Value, ValuePtr, CDO, PPF_None))
			{
				FString ExportedValue;
				Prop->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
				for (FBPVariableDescription& Var : BP->NewVariables)
				{
					if (Var.VarName == VarFName)
					{
						Var.DefaultValue = ExportedValue;
						break;
					}
				}
				FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
				return true;
			}
		}
	}

	// Fallback: update metadata directly (e.g. after AddVariable before compile)
	for (FBPVariableDescription& Var : BP->NewVariables)
	{
		if (Var.VarName == VarFName)
		{
			Var.DefaultValue = Value;
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
			return true;
		}
	}

	return false;
}

// ─── SetComponentProperty ───────────────────────────────────

bool UTetherBlueprintLibrary::SetComponentProperty(
	const FString& BlueprintPath, const FString& ComponentName,
	const FString& PropertyName, const FString& Value)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UActorComponent* Template = nullptr;

	if (BP->SimpleConstructionScript)
	{
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				Template = Node->ComponentTemplate;
				break;
			}
		}
	}

	if (!Template) return false;

	FProperty* Prop = Template->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop) return false;

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Template);
	if (!Prop->ImportText_Direct(*Value, ValuePtr, Template, PPF_None))
		return false;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

// ─── AddBlueprintVariable ───────────────────────────────────

bool UTetherBlueprintLibrary::AddBlueprintVariable(
	const FString& BlueprintPath, const FString& Name,
	const FString& TypeString, const FString& DefaultValue)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	FName VarName(*Name);

	// Check for existing variable with same name
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		if (Var.VarName == VarName)
			return false;
	}

	FEdGraphPinType PinType;
	if (!TetherTypeParseImpl::ParseTypeString(TypeString, PinType))
		return false;

	bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(BP, VarName, PinType, DefaultValue);
	if (bSuccess)
	{
		FKismetEditorUtilities::CompileBlueprint(BP);
	}

	return bSuccess;
}

// ─── RemoveBlueprintVariable ────────────────────────────────

bool UTetherBlueprintLibrary::RemoveBlueprintVariable(
	const FString& BlueprintPath, const FString& VariableName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	const FName VarName(*VariableName);
	const int32 Idx = FBlueprintEditorUtils::FindNewVariableIndex(BP, VarName);
	if (Idx == INDEX_NONE) return false;

	FBlueprintEditorUtils::RemoveMemberVariable(BP, VarName);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── RenameBlueprintVariable ────────────────────────────────

bool UTetherBlueprintLibrary::RenameBlueprintVariable(
	const FString& BlueprintPath, const FString& OldName, const FString& NewName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	const FName OldVar(*OldName);
	const FName NewVar(*NewName);
	if (OldVar == NewVar || NewVar.IsNone()) return false;

	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, OldVar) == INDEX_NONE) return false;
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, NewVar) != INDEX_NONE) return false;

	FBlueprintEditorUtils::RenameMemberVariable(BP, OldVar, NewVar);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── Function-scope local variables ─────────────────────────

// ─── Interface helpers ──────────────────────────────────────

// ─── AddBlueprintInterface ──────────────────────────────────

// ─── RemoveBlueprintInterface ───────────────────────────────

// ─── AddBlueprintComponent ──────────────────────────────────

bool UTetherBlueprintLibrary::AddBlueprintComponent(
	const FString& BlueprintPath,
	const FString& ComponentClassPath,
	const FString& ComponentName,
	const FString& ParentComponentName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	if (!SCS) return false;

	// Resolve component class.
	UClass* CompClass = FindObject<UClass>(nullptr, *ComponentClassPath);
	if (!CompClass)
	{
		CompClass = LoadObject<UClass>(nullptr, *ComponentClassPath);
	}
	if (!CompClass)
	{
		CompClass = LoadObject<UClass>(nullptr, *(ComponentClassPath + TEXT("_C")));
	}
	if (!CompClass || !CompClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return false;
	}

	const FName DesiredName(*ComponentName);
	if (DesiredName.IsNone()) return false;

	// Reject duplicate names.
	for (USCS_Node* Existing : SCS->GetAllNodes())
	{
		if (Existing && Existing->GetVariableName() == DesiredName)
		{
			return false;
		}
	}

	USCS_Node* NewNode = SCS->CreateNode(CompClass, DesiredName);
	if (!NewNode) return false;

	USCS_Node* ParentNode = nullptr;
	if (!ParentComponentName.IsEmpty())
	{
		const FName ParentName(*ParentComponentName);
		for (USCS_Node* Candidate : SCS->GetAllNodes())
		{
			if (Candidate && Candidate->GetVariableName() == ParentName)
			{
				ParentNode = Candidate;
				break;
			}
		}
	}

	if (ParentNode)
	{
		ParentNode->AddChildNode(NewNode);
	}
	else
	{
		// Attach under an existing root, or become the root if none yet.
		const TArray<USCS_Node*> Roots = SCS->GetRootNodes();
		if (Roots.Num() > 0 && Roots[0])
		{
			Roots[0]->AddChildNode(NewNode);
		}
		else
		{
			SCS->AddNode(NewNode);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}

// ─── Graph node write ops ──────────────────────────────────

// ═════════════════════════════════════════════════════════════════
//   P0 extensions: control flow, functions, dispatchers, interface,
//   variable metadata, compile feedback
// ═════════════════════════════════════════════════════════════════

// ─── Control-flow / basic nodes ─────────────────────────────────

// ─── Function/event graph management ────────────────────────────

// ─── Event Dispatcher write ops ─────────────────────────────────

// ─── Interface override ─────────────────────────────────────────

// ─── Variable metadata / type ───────────────────────────────────

// ─── Compile feedback ───────────────────────────────────────────

TArray<FTetherCompileMessage> UTetherBlueprintLibrary::GetCompileErrors(const FString& BlueprintPath)
{
	TArray<FTetherCompileMessage> Out;
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return Out;

	FCompilerResultsLog Log;
	Log.SetSourcePath(BP->GetPathName());
	Log.BeginEvent(TEXT("Compile"));
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
	Log.EndEvent();

	auto Sev = [](EMessageSeverity::Type S) -> const TCHAR*
	{
		switch (S)
		{
		case EMessageSeverity::Error:             return TEXT("Error");
		case EMessageSeverity::PerformanceWarning: // fallthrough
		case EMessageSeverity::Warning:           return TEXT("Warning");
		case EMessageSeverity::Info:              return TEXT("Info");
		default:                                  return TEXT("Note");
		}
	};

	for (const TSharedRef<FTokenizedMessage>& Msg : Log.Messages)
	{
		FTetherCompileMessage Entry;
		Entry.Severity = Sev(Msg->GetSeverity());
		Entry.Message  = Msg->ToText().ToString();
		// Try to extract a node guid from token objects.
		for (const TSharedRef<IMessageToken>& Tok : Msg->GetMessageTokens())
		{
			if (Tok->GetType() == EMessageToken::Object)
			{
				const TSharedRef<FUObjectToken> ObjTok = StaticCastSharedRef<FUObjectToken>(Tok);
				if (const UObject* Obj = ObjTok->GetObject().Get())
				{
					if (const UEdGraphNode* N = Cast<UEdGraphNode>(Obj))
					{
						Entry.NodeGuid = N->NodeGuid.ToString(EGuidFormats::Digits);
						break;
					}
				}
			}
		}
		Out.Add(MoveTemp(Entry));
	}
	return Out;
}

// ═══════════════════════════════════════════════════════════════════
//   P1 helpers
// ═══════════════════════════════════════════════════════════════════

// ─── Control-flow: loops / select / literal ──────────────────────

// ─── Graph layout ────────────────────────────────────────────────

// ─── Class settings ──────────────────────────────────────────────

// ─── Component tree ──────────────────────────────────────────────

// ─── Dispatcher event node ───────────────────────────────────────

// ═══ P2 implementation ═══════════════════════════════════════════

// ─── Batch A: CallFunction wrappers ────────────────────────────

// ─── Batch B: Struct Make / Break ─────────────────────────────

// ─── Batch C: Graph extras ─────────────────────────────────────

// ─── Batch D: Timeline ─────────────────────────────────────────

// ─── Batch E: Macro / Debug management ─────────────────────────

// ─── Batch F: Node utilities ───────────────────────────────────

// ═════════════════════════════════════════════════════════════════
// Semantic summary layer (agent-understanding helpers)
// ═════════════════════════════════════════════════════════════════

namespace TetherBPSummaryImpl
{
	/** Classify a graph against a loaded BP. */
	static FString ClassifyGraph(const UBlueprint* BP, const UEdGraph* Graph)
	{
		if (!BP || !Graph) return TEXT("Unknown");
		UEdGraph* G = const_cast<UEdGraph*>(Graph);
		if (BP->UbergraphPages.Contains(G)) return TEXT("EventGraph");
		if (BP->FunctionGraphs.Contains(G))  return TEXT("Function");
		if (BP->MacroGraphs.Contains(G))     return TEXT("Macro");
		return TEXT("Unknown");
	}

	TArray<FAllGraphs> CollectAllGraphs(UBlueprint* BP)
	{
		TArray<FAllGraphs> Out;
		for (UEdGraph* G : BP->UbergraphPages) if (G) Out.Add({G, TEXT("EventGraph")});
		for (UEdGraph* G : BP->FunctionGraphs)  if (G) Out.Add({G, TEXT("Function")});
		for (UEdGraph* G : BP->MacroGraphs)     if (G) Out.Add({G, TEXT("Macro")});
		return Out;
	}

	/** Pull an object-ref asset path out of a pin's default value if it's an
	 *  asset reference; returns empty if not an object pin or unset. */
	static FString PinAssetReference(const UEdGraphPin* Pin)
	{
		if (!Pin) return FString();
		if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object &&
			Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_SoftObject &&
			Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Class &&
			Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_SoftClass)
		{
			return FString();
		}
		if (Pin->DefaultObject)
		{
			return Pin->DefaultObject->GetPathName();
		}
		// Plain path defaults (soft refs etc.)
		if (!Pin->DefaultValue.IsEmpty() &&
			(Pin->DefaultValue.StartsWith(TEXT("/")) || Pin->DefaultValue.Contains(TEXT("."))))
		{
			return Pin->DefaultValue;
		}
		return FString();
	}

	/** Sort a name→count map into a TArray of names by descending count, cap N. */
	static TArray<FString> TopN(const TMap<FString, int32>& Counter, int32 N)
	{
		TArray<TPair<FString, int32>> Pairs;
		for (const auto& Pair : Counter) { Pairs.Add({Pair.Key, Pair.Value}); }
		Pairs.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
		{
			return A.Value > B.Value;
		});
		TArray<FString> Out;
		for (int32 i = 0; i < FMath::Min(N, Pairs.Num()); ++i) { Out.Add(Pairs[i].Key); }
		return Out;
	}
}

bool UTetherBlueprintLibrary::GetBlueprintSummary(
	const FString& BlueprintPath, FTetherBlueprintSummary& OutSummary)
{
	using namespace TetherBPSummaryImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return false;

	const UClass* GenClass = BP->GeneratedClass;
	const UClass* Super = GenClass->GetSuperClass();

	OutSummary.Name = BP->GetName();
	OutSummary.Path = BP->GetPathName();
	if (Super)
	{
		OutSummary.ParentClass = Super->GetName();
		OutSummary.ParentClassPath = Super->GetPathName();
	}

	// First native ancestor.
	const UClass* NativeBase = Super;
	while (NativeBase
		&& (NativeBase->IsChildOf<UBlueprintGeneratedClass>()
			|| NativeBase->HasAnyClassFlags(CLASS_CompiledFromBlueprint)))
	{
		NativeBase = NativeBase->GetSuperClass();
	}
	OutSummary.BlueprintType = NativeBase ? NativeBase->GetName() : TEXT("Object");

	// Variables.
	TSet<FString> CategorySet;
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		const FProperty* Prop = GenClass->FindPropertyByName(Var.VarName);
		if (Prop && CastField<FMulticastDelegateProperty>(Prop)) continue;

		OutSummary.VariableCount += 1;
		if (Prop && Prop->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_Edit))
		{
			OutSummary.InstanceEditableCount += 1;
		}
		if (Var.ReplicationCondition != COND_None || (Prop && Prop->HasAnyPropertyFlags(CPF_Net)))
		{
			OutSummary.ReplicatedVariableCount += 1;
		}
		const FString Cat = Var.Category.ToString();
		if (!Cat.IsEmpty() && Cat != TEXT("Default")) CategorySet.Add(Cat);
	}
	OutSummary.VariableCategories = CategorySet.Array();
	OutSummary.VariableCategories.Sort();

	// Functions + events handled.
	TSet<FString> EventsSet;
	for (TFieldIterator<UFunction> It(GenClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		const UFunction* Func = *It;
		if (!Func) continue;
		const FString FuncName = Func->GetName();
		if (FuncName.StartsWith(TEXT("ExecuteUbergraph"))) continue;
		if (FuncName.Contains(TEXT("__DelegateSignature"))) continue;
		OutSummary.FunctionCount += 1;

		const bool bIsEventOrOverride = Func->HasAnyFunctionFlags(FUNC_Event | FUNC_BlueprintEvent);
		const bool bIsOverride = Super && Super->FindFunctionByName(Func->GetFName()) != nullptr;
		if (bIsEventOrOverride && bIsOverride) EventsSet.Add(FuncName);

		OutSummary.PublicFunctions.Add(FuncName);
	}

	// Event nodes in Ubergraph are another source of "events handled".
	for (UEdGraph* G : BP->UbergraphPages)
	{
		if (!G) continue;
		for (UEdGraphNode* N : G->Nodes)
		{
			if (const UK2Node_Event* Evt = Cast<UK2Node_Event>(N))
			{
				EventsSet.Add(Evt->GetFunctionName().ToString());
			}
		}
	}
	OutSummary.EventsHandled = EventsSet.Array();
	OutSummary.EventsHandled.Sort();
	OutSummary.PublicFunctions.Sort();

	// Macros.
	for (UEdGraph* G : BP->MacroGraphs)
	{
		if (G) OutSummary.MacroCount += 1;
	}

	// Components.
	if (USimpleConstructionScript* SCS = BP->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->ComponentClass) OutSummary.ComponentCount += 1;
		}
	}

	// Timelines.
	OutSummary.TimelineCount = BP->Timelines.Num();

	// Event dispatchers.
	for (UEdGraph* SigGraph : BP->DelegateSignatureGraphs)
	{
		if (!SigGraph) continue;
		FString Name = SigGraph->GetName();
		static const FString DelegateSuffix = TEXT("__DelegateSignature");
		if (Name.EndsWith(DelegateSuffix)) Name = Name.LeftChop(DelegateSuffix.Len());
		OutSummary.EventDispatchers.Add(Name);
	}

	// Interfaces.
	for (const FBPInterfaceDescription& Iface : BP->ImplementedInterfaces)
	{
		if (Iface.Interface) OutSummary.Interfaces.Add(Iface.Interface->GetName());
	}

	// Walk every node across every graph once.
	TMap<FString, int32> ClassCallFreq;
	TMap<FString, int32> AssetRefFreq;
	for (const FAllGraphs& Entry : CollectAllGraphs(BP))
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (!Node) continue;
			OutSummary.TotalNodeCount += 1;

			if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (UFunction* Func = CallNode->GetTargetFunction())
				{
					if (UClass* Own = Func->GetOwnerClass())
					{
						// Exclude self-class calls — agent cares about "what other systems does this BP talk to".
						if (Own != GenClass)
						{
							ClassCallFreq.FindOrAdd(Own->GetName()) += 1;
						}
					}
				}
			}
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				const FString Ref = PinAssetReference(Pin);
				if (!Ref.IsEmpty()) AssetRefFreq.FindOrAdd(Ref) += 1;
			}
		}
	}
	// Component class refs are "references" too.
	if (USimpleConstructionScript* SCS = BP->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->ComponentClass)
			{
				AssetRefFreq.FindOrAdd(Node->ComponentClass->GetPathName()) += 1;
			}
		}
	}

	OutSummary.KeyReferencedClasses = TopN(ClassCallFreq, 10);
	OutSummary.KeyReferencedAssets  = TopN(AssetRefFreq, 10);

	return true;
}

// ─── GetFunctionSummary ─────────────────────────────────────────

namespace TetherBPSummaryImpl
{
	/** Cap on emitted outline lines; prevents blow-up on 500-node event graphs. */
	constexpr int32 MaxOutlineLines = 200;
	constexpr int32 MaxOutlineDepth = 8;

	/** Short human-readable describe of a node for one outline line, no indent. */
	static FString DescribeNodeOneLine(const UEdGraphNode* Node)
	{
		if (const UK2Node_IfThenElse* /*Branch*/ _ = Cast<UK2Node_IfThenElse>(Node))
		{
			return TEXT("Branch");
		}
		if (const UK2Node_ExecutionSequence* Seq = Cast<UK2Node_ExecutionSequence>(Node))
		{
			int32 Count = 0;
			for (const UEdGraphPin* P : Seq->Pins)
			{
				if (P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) ++Count;
			}
			return FString::Printf(TEXT("Sequence (%d outputs)"), Count);
		}
		if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
		{
			UFunction* Func = Call->GetTargetFunction();
			FString FuncName = Func ? Func->GetName() : Call->GetFunctionName().ToString();
			FString ClassName = (Func && Func->GetOwnerClass()) ? Func->GetOwnerClass()->GetName() : TEXT("?");
			return FString::Printf(TEXT("Call %s.%s"), *ClassName, *FuncName);
		}
		if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
		{
			return FString::Printf(TEXT("Set %s"), *Set->GetVarNameString());
		}
		if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
		{
			return FString::Printf(TEXT("Get %s"), *Get->GetVarNameString());
		}
		if (const UK2Node_DynamicCast* CastN = Cast<UK2Node_DynamicCast>(Node))
		{
			FString T = CastN->TargetType ? CastN->TargetType->GetName() : TEXT("?");
			return FString::Printf(TEXT("Cast to %s"), *T);
		}
		if (const UK2Node_MacroInstance* Mac = Cast<UK2Node_MacroInstance>(Node))
		{
			UEdGraph* MG = Mac->GetMacroGraph();
			return FString::Printf(TEXT("Macro %s"), MG ? *MG->GetName() : TEXT("?"));
		}
		if (const UK2Node_SpawnActorFromClass* Spawn = Cast<UK2Node_SpawnActorFromClass>(Node))
		{
			if (const UEdGraphPin* ClassPin = Spawn->GetClassPin())
			{
				FString C = ClassPin->DefaultObject ? ClassPin->DefaultObject->GetName() : TEXT("?");
				return FString::Printf(TEXT("Spawn %s"), *C);
			}
			return TEXT("Spawn ?");
		}
		if (const UK2Node_CallDelegate* CD = Cast<UK2Node_CallDelegate>(Node))
		{
			return FString::Printf(TEXT("Fire dispatcher %s"), *CD->GetPropertyName().ToString());
		}
		if (const UK2Node_AddDelegate* AD = Cast<UK2Node_AddDelegate>(Node))
		{
			return FString::Printf(TEXT("Bind dispatcher %s"), *AD->GetPropertyName().ToString());
		}
		if (const UK2Node_RemoveDelegate* RD = Cast<UK2Node_RemoveDelegate>(Node))
		{
			return FString::Printf(TEXT("Unbind dispatcher %s"), *RD->GetPropertyName().ToString());
		}
		if (const UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(Node))
		{
			return FString::Printf(TEXT("Event %s (custom)"), *CE->CustomFunctionName.ToString());
		}
		if (const UK2Node_Event* Ev = Cast<UK2Node_Event>(Node))
		{
			return FString::Printf(TEXT("Event %s"), *Ev->GetFunctionName().ToString());
		}
		if (Cast<UK2Node_FunctionEntry>(Node))
		{
			return TEXT("Entry");
		}
		if (Cast<UK2Node_FunctionResult>(Node))
		{
			return TEXT("Return");
		}
		if (Cast<UK2Node_Message>(Node))
		{
			return FString::Printf(TEXT("Interface msg %s"),
				*Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		}
		// Skip reroute knots in outline.
		if (Cast<UK2Node_Knot>(Node)) return FString();
		return Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	}

	/** Detect loop macros by name match. */
	static bool IsLoopMacro(const UK2Node_MacroInstance* Mac)
	{
		if (!Mac) return false;
		UEdGraph* G = Mac->GetMacroGraph();
		if (!G) return false;
		const FString N = G->GetName();
		return N.Contains(TEXT("ForEachLoop")) || N.Contains(TEXT("ForLoop"))
			|| N.Contains(TEXT("WhileLoop")) || N.Contains(TEXT("ReverseForEachLoop"));
	}

	/** Exec-output pins of a node, in declaration order, labelled with pin name. */
	struct FExecOut { FName PinName; UEdGraphNode* Target = nullptr; };
	static TArray<FExecOut> GetExecOutputs(const UEdGraphNode* Node)
	{
		TArray<FExecOut> Out;
		if (!Node) return Out;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction != EGPD_Output) continue;
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked)
				{
					// Follow through knot (reroute) nodes transparently.
					UEdGraphNode* T = Linked->GetOwningNode();
					while (const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(T))
					{
						const UEdGraphPin* Next = Knot->GetOutputPin();
						if (!Next || Next->LinkedTo.Num() == 0) { T = nullptr; break; }
						T = Next->LinkedTo[0]->GetOwningNode();
					}
					Out.Add({ Pin->PinName, T });
				}
			}
		}
		return Out;
	}

	/** Recursive outline builder. */
	static void BuildOutline(
		const UEdGraphNode* Node,
		int32 Depth,
		TSet<const UEdGraphNode*>& Visited,
		TArray<FString>& OutLines)
	{
		if (!Node) return;
		if (OutLines.Num() >= MaxOutlineLines) return;
		if (Depth > MaxOutlineDepth) return;
		if (Visited.Contains(Node)) return;
		Visited.Add(Node);

		const FString Line = DescribeNodeOneLine(Node);
		if (!Line.IsEmpty())
		{
			FString Indent;
			for (int32 i = 0; i < Depth; ++i) Indent += TEXT("  ");
			OutLines.Add(Indent + Line);
		}

		// Stop recursion at Return — nothing follows.
		if (Cast<UK2Node_FunctionResult>(Node)) return;

		const TArray<FExecOut> Outputs = GetExecOutputs(Node);
		if (Outputs.Num() == 0) return;
		// Single linear path: recurse without extra indent.
		if (Outputs.Num() == 1)
		{
			BuildOutline(Outputs[0].Target, Depth, Visited, OutLines);
			return;
		}
		// Multiple outputs: label each and recurse at +1 indent.
		for (const FExecOut& Out : Outputs)
		{
			if (OutLines.Num() >= MaxOutlineLines) return;
			FString Label = Out.PinName.ToString();
			// Prettify common pin names.
			if (Label.Equals(TEXT("then"), ESearchCase::IgnoreCase))         Label = TEXT("True");
			else if (Label.Equals(TEXT("else"), ESearchCase::IgnoreCase))    Label = TEXT("False");
			FString Indent;
			for (int32 i = 0; i <= Depth; ++i) Indent += TEXT("  ");
			OutLines.Add(Indent + Label + TEXT(" →"));
			BuildOutline(Out.Target, Depth + 2, Visited, OutLines);
		}
	}

	/** Find the entry node in a graph for exec outline purposes. */
	static UEdGraphNode* FindEntry(const UEdGraph* Graph, const FString& FunctionName)
	{
		if (!Graph) return nullptr;
		// Prefer FunctionEntry (present in user function graphs).
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (Cast<UK2Node_FunctionEntry>(N)) return N;
		}
		// For event graphs, look for a specific event by name; else first event.
		UEdGraphNode* FirstEvent = nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_Event* E = Cast<UK2Node_Event>(N))
			{
				if (!FirstEvent) FirstEvent = E;
				if (E->GetFunctionName().ToString() == FunctionName) return E;
			}
			else if (UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(N))
			{
				if (CE->CustomFunctionName.ToString() == FunctionName) return CE;
			}
		}
		return FirstEvent;
	}
}

bool UTetherBlueprintLibrary::GetFunctionSummary(
	const FString& BlueprintPath, const FString& FunctionName,
	FTetherFunctionSemantics& OutSemantics)
{
	using namespace TetherBPSummaryImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return false;

	OutSemantics.Name = FunctionName;

	const UClass* GenClass = BP->GeneratedClass;
	const UClass* Super = GenClass->GetSuperClass();

	// Try to find the UFunction first (normal function or event).
	const UFunction* UFunc = GenClass->FindFunctionByName(FName(*FunctionName));
	if (UFunc)
	{
		// Kind + flags.
		if (UFunc->HasAnyFunctionFlags(FUNC_Event | FUNC_BlueprintEvent))
		{
			OutSemantics.Kind = (Super && Super->FindFunctionByName(UFunc->GetFName()))
				? TEXT("Override") : TEXT("Event");
			OutSemantics.bIsOverride = (Super && Super->FindFunctionByName(UFunc->GetFName()) != nullptr);
		}
		else
		{
			OutSemantics.Kind = TEXT("Function");
		}
		OutSemantics.bIsPure = UFunc->HasAnyFunctionFlags(FUNC_BlueprintPure);
		OutSemantics.Access  = UFunc->HasAnyFunctionFlags(FUNC_Public)    ? TEXT("Public")
							  : UFunc->HasAnyFunctionFlags(FUNC_Protected) ? TEXT("Protected")
							  : UFunc->HasAnyFunctionFlags(FUNC_Private)   ? TEXT("Private")
							  : TEXT("Public");
		// Tooltip / description.
		OutSemantics.Description = UFunc->GetMetaData(TEXT("ToolTip"));

		// Params.
		for (TFieldIterator<FProperty> PIt(UFunc); PIt; ++PIt)
		{
			const FProperty* Param = *PIt;
			if (!Param->HasAnyPropertyFlags(CPF_Parm)) continue;
			FTetherFunctionParam P;
			P.Name = Param->GetName();
			P.Type = PropertyTypeToString(Param);
			P.bIsOutput = Param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm);
			OutSemantics.Params.Add(P);
		}
	}

	// Locate graph (function / event / macro / ubergraph entry).
	TArray<UEdGraph*> Graphs = FindGraphs(BP, FunctionName);
	// If FindGraphs returned nothing but the name matches an event, fall back
	// to searching UbergraphPages for an event node by that name.
	if (Graphs.Num() == 0)
	{
		for (UEdGraph* G : BP->UbergraphPages)
		{
			if (!G) continue;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_Event* E = Cast<UK2Node_Event>(N))
				{
					if (E->GetFunctionName().ToString() == FunctionName)
					{
						Graphs.AddUnique(G);
					}
				}
			}
		}
	}
	if (Graphs.Num() == 0)
	{
		// Macro?
		for (UEdGraph* G : BP->MacroGraphs)
		{
			if (G && G->GetName() == FunctionName) { Graphs.Add(G); OutSemantics.Kind = TEXT("Macro"); break; }
		}
	}
	if (Graphs.Num() == 0) return false;

	if (OutSemantics.Kind.IsEmpty())
	{
		OutSemantics.Kind = ClassifyGraph(BP, Graphs[0]);
	}

	// Aggregates + outline across the matching graph(s).
	TSet<FString> ReadsSet, WritesSet, CallsSet, FiresSet, SpawnsSet;
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			OutSemantics.NodeCount += 1;

			if (const UK2Node_VariableGet* VG = Cast<UK2Node_VariableGet>(Node)) ReadsSet.Add(VG->GetVarNameString());
			else if (const UK2Node_VariableSet* VS = Cast<UK2Node_VariableSet>(Node)) WritesSet.Add(VS->GetVarNameString());
			else if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				UFunction* TF = Call->GetTargetFunction();
				const FString Name = TF ? TF->GetName() : Call->GetFunctionName().ToString();
				const FString Own  = (TF && TF->GetOwnerClass()) ? TF->GetOwnerClass()->GetName() : TEXT("?");
				CallsSet.Add(FString::Printf(TEXT("%s.%s"), *Own, *Name));
			}
			else if (const UK2Node_CallDelegate* CD = Cast<UK2Node_CallDelegate>(Node)) FiresSet.Add(CD->GetPropertyName().ToString());
			else if (const UK2Node_SpawnActorFromClass* SP = Cast<UK2Node_SpawnActorFromClass>(Node))
			{
				if (const UEdGraphPin* CP = SP->GetClassPin())
				{
					if (CP->DefaultObject) SpawnsSet.Add(CP->DefaultObject->GetName());
				}
			}
			else if (const UK2Node_IfThenElse* /*Branch*/ _ = Cast<UK2Node_IfThenElse>(Node))
			{
				OutSemantics.bHasBranches = true;
			}
			else if (const UK2Node_MacroInstance* Mac = Cast<UK2Node_MacroInstance>(Node))
			{
				if (IsLoopMacro(Mac)) OutSemantics.bHasLoops = true;
			}
			else if (const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
			{
				if (!Comment->NodeComment.IsEmpty())
					OutSemantics.CommentBlocks.Add(Comment->NodeComment);
			}
		}
	}
	OutSemantics.ReadsVariables    = ReadsSet.Array();  OutSemantics.ReadsVariables.Sort();
	OutSemantics.WritesVariables   = WritesSet.Array(); OutSemantics.WritesVariables.Sort();
	OutSemantics.CallsFunctions    = CallsSet.Array();  OutSemantics.CallsFunctions.Sort();
	OutSemantics.FiresDispatchers  = FiresSet.Array();  OutSemantics.FiresDispatchers.Sort();
	OutSemantics.SpawnsClasses     = SpawnsSet.Array(); OutSemantics.SpawnsClasses.Sort();

	// Outline: start from the entry node of the first matching graph.
	UEdGraphNode* Entry = FindEntry(Graphs[0], FunctionName);
	if (Entry)
	{
		TSet<const UEdGraphNode*> Visited;
		BuildOutline(Entry, 0, Visited, OutSemantics.ExecOutline);
	}

	return true;
}

// ─── Find* cross-reference queries ──────────────────────────────

namespace TetherBPSummaryImpl
{
	static FTetherReference MakeRefFromNode(
		const UEdGraph* Graph, const FString& GraphType,
		const UEdGraphNode* Node, const FString& Kind)
	{
		FTetherReference Ref;
		Ref.GraphName = Graph ? Graph->GetName() : FString();
		Ref.GraphType = GraphType;
		Ref.NodeGuid  = Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
		Ref.NodeTitle = Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString();
		Ref.Kind      = Kind;
		return Ref;
	}
}

TArray<FTetherReference> UTetherBlueprintLibrary::FindVariableReferences(
	const FString& BlueprintPath, const FString& VariableName)
{
	using namespace TetherBPSummaryImpl;
	TArray<FTetherReference> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || VariableName.IsEmpty()) return Result;

	for (const FAllGraphs& Entry : CollectAllGraphs(BP))
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (!Node) continue;
			if (const UK2Node_VariableGet* VG = Cast<UK2Node_VariableGet>(Node))
			{
				if (VG->GetVarNameString() == VariableName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("read")));
				}
			}
			else if (const UK2Node_VariableSet* VS = Cast<UK2Node_VariableSet>(Node))
			{
				if (VS->GetVarNameString() == VariableName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("write")));
				}
			}
		}
	}
	return Result;
}

TArray<FTetherReference> UTetherBlueprintLibrary::FindFunctionCallSites(
	const FString& BlueprintPath, const FString& FunctionName)
{
	using namespace TetherBPSummaryImpl;
	TArray<FTetherReference> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || FunctionName.IsEmpty()) return Result;

	for (const FAllGraphs& Entry : CollectAllGraphs(BP))
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				UFunction* TF = Call->GetTargetFunction();
				const FString Name = TF ? TF->GetName() : Call->GetFunctionName().ToString();
				if (Name == FunctionName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call")));
				}
			}
			else if (const UK2Node_Message* Msg = Cast<UK2Node_Message>(Node))
			{
				// Interface messages: match by message function name too.
				if (Msg->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(FunctionName))
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call")));
				}
			}
			else if (const UK2Node_MacroInstance* Mac = Cast<UK2Node_MacroInstance>(Node))
			{
				UEdGraph* MG = Mac->GetMacroGraph();
				if (MG && MG->GetName() == FunctionName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call")));
				}
			}
		}
	}
	return Result;
}

// ─── Cross-Blueprint call-site query (#7) ──────────────────────

namespace TetherBPInvokeImpl
{
	/** Compare an owning-class name against a user filter that may be either
	 *  the short name ("KismetSystemLibrary") or the C++ prefixed form
	 *  ("UKismetSystemLibrary"). */
	static bool OwnerMatches(const UClass* OwnerClass, const FString& Filter)
	{
		if (Filter.IsEmpty()) return true;
		if (!OwnerClass)     return false;
		const FString Short = OwnerClass->GetName();
		const FString CppName = OwnerClass->GetPrefixCPP() + Short;
		return Short == Filter || CppName == Filter;
	}
}

TArray<FTetherGlobalReference> UTetherBlueprintLibrary::FindFunctionCallSitesGlobal(
	const FString& FunctionName,
	const FString& OwningClassFilter,
	const FString& PackagePath,
	int32 MaxResults)
{
	using namespace TetherBPSummaryImpl;
	using namespace TetherBPInvokeImpl;

	TArray<FTetherGlobalReference> Result;
	if (FunctionName.IsEmpty()) return Result;

	const int32 EffectiveMax = (MaxResults > 0) ? MaxResults : 1000;
	FString Root = PackagePath.IsEmpty() ? FString(TEXT("/Game")) : PackagePath;
	if (!Root.StartsWith(TEXT("/"))) Root = TEXT("/") + Root;

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(*Root));
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> BpAssets;
	Registry.GetAssets(Filter, BpAssets);

	for (const FAssetData& Data : BpAssets)
	{
		if (Result.Num() >= EffectiveMax) break;

		const FString ObjectPath = Data.GetSoftObjectPath().ToString();
		UBlueprint* BP = LoadBP(ObjectPath);
		if (!BP) continue;

		for (const FAllGraphs& Entry : CollectAllGraphs(BP))
		{
			if (Result.Num() >= EffectiveMax) break;
			for (UEdGraphNode* Node : Entry.Graph->Nodes)
			{
				if (Result.Num() >= EffectiveMax) break;
				bool bHit = false;
				if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
				{
					UFunction* TF = Call->GetTargetFunction();
					const FString Name = TF ? TF->GetName() : Call->GetFunctionName().ToString();
					if (Name == FunctionName)
					{
						const UClass* Owner = TF ? TF->GetOuterUClass() : nullptr;
						if (OwnerMatches(Owner, OwningClassFilter))
						{
							bHit = true;
						}
					}
				}
				else if (const UK2Node_Message* Msg = Cast<UK2Node_Message>(Node))
				{
					// Interface-message dispatch: match by title substring
					// (same heuristic as the single-BP variant).
					if (OwningClassFilter.IsEmpty() &&
						Msg->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(FunctionName))
					{
						bHit = true;
					}
				}

				if (bHit)
				{
					const FTetherReference Ref = MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call"));
					FTetherGlobalReference G;
					G.BlueprintPath = ObjectPath;
					G.GraphName     = Ref.GraphName;
					G.GraphType     = Ref.GraphType;
					G.NodeGuid      = Ref.NodeGuid;
					G.NodeTitle     = Ref.NodeTitle;
					G.Kind          = Ref.Kind;
					Result.Add(MoveTemp(G));
				}
			}
		}
	}
	return Result;
}

// ─── find_blueprint_debug_prints ───────────────────────────────

TArray<FTetherDebugPrintSite> UTetherBlueprintLibrary::FindBlueprintDebugPrints(
	const FString& PackagePath,
	int32 MaxResults)
{
	using namespace TetherBPSummaryImpl;

	TArray<FTetherDebugPrintSite> Result;

	const int32 EffectiveMax = (MaxResults > 0) ? MaxResults : 1000;
	FString Root = PackagePath.IsEmpty() ? FString(TEXT("/Game")) : PackagePath;
	if (!Root.StartsWith(TEXT("/"))) Root = TEXT("/") + Root;

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(*Root));
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> BpAssets;
	Registry.GetAssets(Filter, BpAssets);

	for (const FAssetData& Data : BpAssets)
	{
		if (Result.Num() >= EffectiveMax) break;

		const FString ObjectPath = Data.GetSoftObjectPath().ToString();
		UBlueprint* BP = LoadBP(ObjectPath);
		if (!BP) continue;

		for (const FAllGraphs& Entry : CollectAllGraphs(BP))
		{
			if (Result.Num() >= EffectiveMax) break;
			for (UEdGraphNode* Node : Entry.Graph->Nodes)
			{
				if (Result.Num() >= EffectiveMax) break;

				const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
				if (!Call) continue;

				UFunction* TF = Call->GetTargetFunction();
				if (!TF) continue;

				const UClass* Owner = TF->GetOuterUClass();
				if (!Owner) continue;
				const FString OwnerName = Owner->GetName();
				// UKismetSystemLibrary — accept the "U"-prefixed real name.
				if (OwnerName != TEXT("KismetSystemLibrary")) continue;

				const FString FuncName = TF->GetName();
				const bool bIsPrintString  = (FuncName == TEXT("PrintString"));
				const bool bIsPrintText    = (FuncName == TEXT("PrintText"));
				const bool bIsPrintWarning = (FuncName == TEXT("PrintWarning"));
				if (!bIsPrintString && !bIsPrintText && !bIsPrintWarning) continue;

				FTetherDebugPrintSite Site;
				const FTetherReference Ref = MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call"));
				Site.BlueprintPath   = ObjectPath;
				Site.GraphName       = Ref.GraphName;
				Site.GraphType       = Ref.GraphType;
				Site.NodeGuid        = Ref.NodeGuid;
				Site.NodeTitle       = Ref.NodeTitle;
				Site.FunctionName    = FuncName;

				// Find the input pin: "InString" for PrintString/PrintWarning,
				// "InText" for PrintText.
				const TCHAR* PinName = bIsPrintText ? TEXT("InText") : TEXT("InString");
				const UEdGraphPin* InputPin = nullptr;
				for (const UEdGraphPin* P : Node->Pins)
				{
					if (P && P->Direction == EGPD_Input && P->PinName.ToString() == PinName)
					{
						InputPin = P;
						break;
					}
				}

				if (InputPin)
				{
					Site.bHasConnectedInput = InputPin->LinkedTo.Num() > 0;
					if (!Site.bHasConnectedInput)
					{
						// Prefer DefaultTextValue for FText pins, DefaultValue for FString.
						if (!InputPin->DefaultTextValue.IsEmpty())
						{
							Site.StringLiteral = InputPin->DefaultTextValue.ToString();
						}
						else
						{
							Site.StringLiteral = InputPin->DefaultValue;
						}
					}
				}

				Result.Add(MoveTemp(Site));
			}
		}
	}
	return Result;
}

// ─── invoke_blueprint_function (#5) ────────────────────────────

namespace TetherBPInvokeImpl
{
	/** Return true iff the function is safe to invoke from the tether. */
	static bool IsInvokable(UFunction* Func, UBlueprint* BP, FString& OutError)
	{
		if (!Func)
		{
			OutError = TEXT("function not found on generated class");
			return false;
		}

		// Reject latent functions — they need a tick loop to resolve.
		for (TFieldIterator<FProperty> It(Func); It; ++It)
		{
			const FProperty* Prop = *It;
			if (Prop->GetCPPType().Contains(TEXT("FLatentActionInfo")))
			{
				OutError = TEXT("function is latent (FLatentActionInfo param); cannot invoke synchronously");
				return false;
			}
		}

		// Accept if marked BlueprintCallable/BlueprintPure, OR if defined on
		// this BP (user functions on a BP don't always carry FUNC_BlueprintCallable
		// via the usual path — check the UBlueprint's FunctionGraphs instead).
		if (Func->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
		{
			return true;
		}
		if (BP)
		{
			for (UEdGraph* G : BP->FunctionGraphs)
			{
				if (G && G->GetFName() == Func->GetFName()) return true;
			}
		}
		OutError = TEXT("function is not BlueprintCallable/BlueprintPure; refusing to invoke engine lifecycle events");
		return false;
	}
}

bool UTetherBlueprintLibrary::InvokeBlueprintFunction(
	const FString& BlueprintPath,
	const FString& FunctionName,
	const FString& ArgsJson,
	FString& OutResultJson,
	FString& OutError)
{
	using namespace TetherBPInvokeImpl;

	OutResultJson = TEXT("{}");
	OutError.Reset();

	// Always return true so Python callers (which strip out-params when a
	// UFUNCTION bool returns false) can read OutResultJson. The JSON object
	// carries either the out/return params on success, or {"error": "..."}
	// on a handled failure. The bool return is reserved for catastrophic
	// C++ failures that can't even produce a JSON payload (currently none).
	auto Fail = [&OutResultJson, &OutError](const FString& Msg) -> bool
	{
		OutError = Msg;
		const FString Escaped = Msg.ReplaceCharWithEscapedChar();
		OutResultJson = FString::Printf(TEXT("{\"error\":\"%s\"}"), *Escaped);
		return true;
	};

	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass)
	{
		return Fail(FString::Printf(TEXT("blueprint not found or not compiled: %s"), *BlueprintPath));
	}

	UClass* GenClass = BP->GeneratedClass;
	UFunction* Func = GenClass->FindFunctionByName(FName(*FunctionName));
	{
		FString Reason;
		if (!IsInvokable(Func, BP, Reason))
		{
			return Fail(Reason);
		}
	}

	// Parse args JSON (empty = no args).
	TSharedPtr<FJsonObject> ArgsObj;
	if (!ArgsJson.IsEmpty())
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ArgsJson);
		if (!FJsonSerializer::Deserialize(Reader, ArgsObj) || !ArgsObj.IsValid())
		{
			return Fail(TEXT("failed to parse ArgsJson"));
		}
	}

	// Build the target instance.
	UObject* Instance = nullptr;
	AActor*  SpawnedActor = nullptr;
	if (GenClass->IsChildOf(AActor::StaticClass()))
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			return Fail(TEXT("no editor world to spawn transient actor"));
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bNoFail = true;
		SpawnedActor = World->SpawnActor<AActor>(GenClass, FTransform::Identity, Params);
		Instance = SpawnedActor;
	}
	else
	{
		Instance = NewObject<UObject>(GetTransientPackage(), GenClass, NAME_None, RF_Transient);
	}
	if (!Instance)
	{
		return Fail(TEXT("failed to create transient instance of generated class"));
	}

	// Allocate + zero-init the parameter buffer.
	TArray<uint8> ParamBuffer;
	ParamBuffer.SetNumZeroed(Func->ParmsSize);
	uint8* ParamBuf = ParamBuffer.GetData();

	// Initialize values (constructs TArray/TMap/TSet/FString/structs properly).
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(ParamBuf);
	}

	// Import caller-supplied args into input params. Treating UFunction as a
	// UStruct lets JsonAttributesToUStruct walk property names and convert
	// each JSON field with the engine's full type coverage (structs, arrays,
	// enums, object refs). We still need a pre-pass to skip pure-output
	// params so junk in ArgsJson for an out-only param doesn't pre-populate.
	if (ArgsObj.IsValid() && ArgsObj->Values.Num() > 0)
	{
		TMap<FTetherJsonAttrsKey, TSharedPtr<FJsonValue>> InputsOnly;
		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* Prop = *It;
			const bool bIsReturn = Prop->HasAnyPropertyFlags(CPF_ReturnParm);
			const bool bIsOut    = Prop->HasAnyPropertyFlags(CPF_OutParm);
			const bool bIsRef    = Prop->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (bIsReturn) continue;
			if (bIsOut && !bIsRef) continue;
			TSharedPtr<FJsonValue> Val = ArgsObj->TryGetField(Prop->GetName());
			if (Val.IsValid()) InputsOnly.Add(FTetherJsonAttrsKey(*Prop->GetName()), Val);
		}
		FText FailReason;
		if (InputsOnly.Num() > 0 &&
			!FJsonObjectConverter::JsonAttributesToUStruct(InputsOnly, Func, ParamBuf, 0, 0, false, &FailReason))
		{
			for (TFieldIterator<FProperty> D(Func); D && D->HasAnyPropertyFlags(CPF_Parm); ++D)
			{
				D->DestroyValue_InContainer(ParamBuf);
			}
			if (SpawnedActor) SpawnedActor->Destroy();
			return Fail(FString::Printf(TEXT("failed to import args: %s"), *FailReason.ToString()));
		}
	}

	// Execute.
	Instance->ProcessEvent(Func, ParamBuf);

	// Serialize return + out params.
	TSharedRef<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;
		const bool bIsReturn = Prop->HasAnyPropertyFlags(CPF_ReturnParm);
		const bool bIsOut    = Prop->HasAnyPropertyFlags(CPF_OutParm);
		if (!bIsReturn && !bIsOut) continue;

		const void* Addr = Prop->ContainerPtrToValuePtr<void>(ParamBuf);
		TSharedPtr<FJsonValue> Val = FJsonObjectConverter::UPropertyToJsonValue(Prop, Addr, 0, 0);
		if (!Val.IsValid()) continue;
		const FString Key = bIsReturn ? FString(TEXT("_return")) : Prop->GetName();
		ResultObj->SetField(Key, Val);
	}

	// Destroy param buffer contents.
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(ParamBuf);
	}

	// Cleanup instance.
	if (SpawnedActor) SpawnedActor->Destroy();

	// Emit the result JSON.
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(ResultObj, Writer);
	OutResultJson = Out;
	return true;
}

// ─── Pin introspection helpers ─────────────────────────────────

namespace TetherBPSummaryImpl
{
	/** Locate a node inside a graph by its digits-form guid. */
	UEdGraphNode* FindNodeInGraphByGuid(UEdGraph* Graph, const FString& NodeGuid)
	{
		if (!Graph) return nullptr;
		FGuid Target;
		if (!FGuid::ParseExact(NodeGuid, EGuidFormats::Digits, Target)) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->NodeGuid == Target) return N;
		}
		return nullptr;
	}

	/** Search all of a BP's graphs (ubergraph + functions + macros) by name. */
	UEdGraph* FindSingleGraphByName(UBlueprint* BP, const FString& Name)
	{
		if (!BP) return nullptr;
		auto Probe = [&Name](const TArray<UEdGraph*>& Arr) -> UEdGraph*
		{
			for (UEdGraph* G : Arr) if (G && G->GetName() == Name) return G;
			return nullptr;
		};
		if (UEdGraph* G = Probe(BP->FunctionGraphs)) return G;
		if (UEdGraph* G = Probe(BP->UbergraphPages)) return G;
		if (UEdGraph* G = Probe(BP->MacroGraphs))    return G;
		for (const FBPInterfaceDescription& Interface : BP->ImplementedInterfaces)
		{
			if (UEdGraph* G = Probe(Interface.Graphs)) return G;
		}
		return nullptr;
	}

	/** Short human-readable type for a pin from its FEdGraphPinType. */
	FString PinTypeToHuman(const FEdGraphPinType& PT)
	{
		auto BaseType = [&]() -> FString
		{
			const FName Cat = PT.PinCategory;
			if (Cat == UEdGraphSchema_K2::PC_Exec)      return TEXT("Exec");
			if (Cat == UEdGraphSchema_K2::PC_Boolean)   return TEXT("Bool");
			if (Cat == UEdGraphSchema_K2::PC_Byte)
			{
				if (UEnum* E = Cast<UEnum>(PT.PinSubCategoryObject.Get()))
				{
					return FString::Printf(TEXT("Enum<%s>"), *E->GetName());
				}
				return TEXT("Byte");
			}
			if (Cat == UEdGraphSchema_K2::PC_Int)       return TEXT("Int");
			if (Cat == UEdGraphSchema_K2::PC_Int64)     return TEXT("Int64");
			if (Cat == UEdGraphSchema_K2::PC_Float)     return TEXT("Float");
			if (Cat == UEdGraphSchema_K2::PC_Double)    return TEXT("Double");
			if (Cat == UEdGraphSchema_K2::PC_Real)
			{
				if (PT.PinSubCategory == UEdGraphSchema_K2::PC_Float) return TEXT("Float");
				if (PT.PinSubCategory == UEdGraphSchema_K2::PC_Double) return TEXT("Double");
				return TEXT("Real");
			}
			if (Cat == UEdGraphSchema_K2::PC_String)    return TEXT("String");
			if (Cat == UEdGraphSchema_K2::PC_Name)      return TEXT("Name");
			if (Cat == UEdGraphSchema_K2::PC_Text)      return TEXT("Text");
			if (Cat == UEdGraphSchema_K2::PC_Object ||
				Cat == UEdGraphSchema_K2::PC_Interface)
			{
				if (UClass* C = Cast<UClass>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("%s"), *C->GetName());
				return TEXT("Object");
			}
			if (Cat == UEdGraphSchema_K2::PC_SoftObject)
			{
				if (UClass* C = Cast<UClass>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("SoftObject<%s>"), *C->GetName());
				return TEXT("SoftObject");
			}
			if (Cat == UEdGraphSchema_K2::PC_Class)
			{
				if (UClass* C = Cast<UClass>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("Class<%s>"), *C->GetName());
				return TEXT("Class");
			}
			if (Cat == UEdGraphSchema_K2::PC_SoftClass)
			{
				if (UClass* C = Cast<UClass>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("SoftClass<%s>"), *C->GetName());
				return TEXT("SoftClass");
			}
			if (Cat == UEdGraphSchema_K2::PC_Struct)
			{
				if (UScriptStruct* S = Cast<UScriptStruct>(PT.PinSubCategoryObject.Get()))
					return S->GetName();
				return TEXT("Struct");
			}
			if (Cat == UEdGraphSchema_K2::PC_Enum)
			{
				if (UEnum* E = Cast<UEnum>(PT.PinSubCategoryObject.Get()))
					return FString::Printf(TEXT("Enum<%s>"), *E->GetName());
				return TEXT("Enum");
			}
			if (Cat == UEdGraphSchema_K2::PC_Delegate)  return TEXT("Delegate");
			if (Cat == UEdGraphSchema_K2::PC_MCDelegate) return TEXT("MulticastDelegate");
			if (Cat == UEdGraphSchema_K2::PC_Wildcard)  return TEXT("Wildcard");
			return Cat.ToString();
		}();
		// Containers.
		if (PT.ContainerType == EPinContainerType::Array) return FString::Printf(TEXT("Array of %s"), *BaseType);
		if (PT.ContainerType == EPinContainerType::Set)   return FString::Printf(TEXT("Set of %s"),   *BaseType);
		if (PT.ContainerType == EPinContainerType::Map)
		{
			// Recurse on a non-container proxy of the value-side terminal so
			// the V-side runs through the full BaseType matrix (handles
			// PC_Real → Float/Double, PC_Struct, PC_Enum, PC_Class, etc.)
			// instead of a hand-maintained whitelist.
			FEdGraphPinType ValueProxy;
			ValueProxy.PinCategory          = PT.PinValueType.TerminalCategory;
			ValueProxy.PinSubCategory       = PT.PinValueType.TerminalSubCategory;
			ValueProxy.PinSubCategoryObject = PT.PinValueType.TerminalSubCategoryObject;
			ValueProxy.ContainerType        = EPinContainerType::None;
			return FString::Printf(TEXT("Map<%s, %s>"), *BaseType, *PinTypeToHuman(ValueProxy));
		}
		return BaseType;
	}

	/** Extract a pin's effective default as a string. */
	FString GetPinDefaultString(const UEdGraphPin* Pin)
	{
		if (!Pin) return FString();
		if (Pin->DefaultObject) return Pin->DefaultObject->GetPathName();
		if (!Pin->DefaultTextValue.IsEmpty()) return Pin->DefaultTextValue.ToString();
		return Pin->DefaultValue;
	}
}

FString UTetherBlueprintLibrary::GetPinDefaultValue(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& PinName)
{
	using namespace TetherBPSummaryImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindSingleGraphByName(BP, GraphName);
	if (!Graph) return FString();
	UEdGraphNode* Node = FindNodeInGraphByGuid(Graph, NodeGuid);
	if (!Node) return FString();
	UEdGraphPin* Pin = Node->FindPin(PinName);
	if (!Pin) return FString();
	return GetPinDefaultString(Pin);
}

// ─── Node layout ───────────────────────────────────────────────

namespace TetherBPDescribeImpl
{
	using namespace TetherBPSummaryImpl;

	/** Fill an FTetherPinInfo from a UEdGraphPin. Optionally populates the
	 *  LinkedTo array with "<node_guid>:<pin_name>" strings for each link. */
	static void FillPinInfo(UEdGraphPin* Pin, FTetherPinInfo& Info, bool bFillLinkedTo)
	{
		const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();

		Info.Name        = Pin->PinName.ToString();
		Info.DisplayName = Pin->GetDisplayName().ToString();
		Info.Type        = PinTypeToHuman(Pin->PinType);
		Info.Direction   = (Pin->Direction == EGPD_Input) ? TEXT("input") : TEXT("output");
		Info.Category    = Pin->PinType.PinCategory.ToString();
		Info.SubCategory = Pin->PinType.PinSubCategory.ToString();
		if (UObject* SubObj = Pin->PinType.PinSubCategoryObject.Get())
		{
			Info.SubCategoryObjectPath = SubObj->GetPathName();
		}
		Info.DefaultValue      = GetPinDefaultString(Pin);
		Info.bHasDefaultObject = (Pin->DefaultObject != nullptr);
		Info.bIsExec           = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
		Info.bIsConnected      = (Pin->LinkedTo.Num() > 0);
		Info.LinkCount         = Pin->LinkedTo.Num();
		Info.bIsArray          = (Pin->PinType.ContainerType == EPinContainerType::Array);
		Info.bIsSet            = (Pin->PinType.ContainerType == EPinContainerType::Set);
		Info.bIsMap            = (Pin->PinType.ContainerType == EPinContainerType::Map);
		Info.bIsReference      = Pin->PinType.bIsReference;
		Info.bIsConst          = Pin->PinType.bIsConst;
		Info.bIsHidden         = Pin->bHidden;
		Info.bIsSelfPin        = (K2 && K2->IsSelfPin(*Pin));

		switch (Pin->PinType.ContainerType)
		{
			case EPinContainerType::Array: Info.ContainerKind = TEXT("Array"); break;
			case EPinContainerType::Set:   Info.ContainerKind = TEXT("Set");   break;
			case EPinContainerType::Map:   Info.ContainerKind = TEXT("Map");   break;
			default:                        Info.ContainerKind = TEXT("None");  break;
		}

		if (Pin->PinType.ContainerType == EPinContainerType::Map)
		{
			FEdGraphPinType ValueProxy;
			ValueProxy.PinCategory          = Pin->PinType.PinValueType.TerminalCategory;
			ValueProxy.PinSubCategory       = Pin->PinType.PinValueType.TerminalSubCategory;
			ValueProxy.PinSubCategoryObject = Pin->PinType.PinValueType.TerminalSubCategoryObject;
			ValueProxy.ContainerType        = EPinContainerType::None;
			Info.MapValueType        = PinTypeToHuman(ValueProxy);
			Info.MapValueCategory    = ValueProxy.PinCategory.ToString();
			Info.MapValueSubCategory = ValueProxy.PinSubCategory.ToString();
			if (UObject* VObj = ValueProxy.PinSubCategoryObject.Get())
			{
				Info.MapValueSubCategoryObjectPath = VObj->GetPathName();
			}
		}

		if (bFillLinkedTo)
		{
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (!Linked) continue;
				const UEdGraphNode* Owner = Linked->GetOwningNode();
				if (!Owner) continue;
				Info.LinkedTo.Add(FString::Printf(TEXT("%s:%s"),
					*Owner->NodeGuid.ToString(EGuidFormats::Digits),
					*Linked->PinName.ToString()));
			}
		}
	}
}

TArray<FTetherPinInfo> UTetherBlueprintLibrary::GetNodePins(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	using namespace TetherBPSummaryImpl;
	TArray<FTetherPinInfo> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Result;
	UEdGraph* Graph = FindSingleGraphByName(BP, GraphName);
	if (!Graph) return Result;
	UEdGraphNode* Node = FindNodeInGraphByGuid(Graph, NodeGuid);
	if (!Node) return Result;

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;
		FTetherPinInfo Info;
		TetherBPDescribeImpl::FillPinInfo(Pin, Info, /*bFillLinkedTo=*/false);
		Result.Add(Info);
	}
	return Result;
}

FTetherNodeDescription UTetherBlueprintLibrary::DescribeNode(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	using namespace TetherBPSummaryImpl;
	FTetherNodeDescription Out;

	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = FindSingleGraphByName(BP, GraphName);
	if (!Graph) return Out;
	UEdGraphNode* Node = FindNodeInGraphByGuid(Graph, NodeGuid);
	if (!Node) return Out;

	Out.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
	Out.Title    = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	Out.NodeType = ClassifyNode(Node);
	Out.NodeClass = Node->GetClass()->GetName();
	Out.PosX = Node->NodePosX;
	Out.PosY = Node->NodePosY;
	// Stored size if Slate has touched it; else estimate.
	{
		int32 EstW = 0, EstH = 0;
		EstimateNodeSize(Node, EstW, EstH);
		Out.Width  = (Node->NodeWidth  > 0) ? Node->NodeWidth  : EstW;
		Out.Height = (Node->NodeHeight > 0) ? Node->NodeHeight : EstH;
	}
	Out.Comment = Node->NodeComment;
	switch (Node->GetDesiredEnabledState())
	{
		case ENodeEnabledState::Disabled:         Out.EnabledState = TEXT("Disabled"); break;
		case ENodeEnabledState::DevelopmentOnly:  Out.EnabledState = TEXT("DevelopmentOnly"); break;
		default:                                  Out.EnabledState = TEXT("Enabled"); break;
	}

	// ── Subclass-specific fields ────────────────────────────
	if (const UK2Node_CallFunction* CF = Cast<UK2Node_CallFunction>(Node))
	{
		if (UFunction* Func = CF->GetTargetFunction())
		{
			Out.TargetName = Func->GetName();
			Out.bIsConst   = Func->HasAnyFunctionFlags(FUNC_Const);
			if (UClass* OwnerClass = Func->GetOwnerClass())
			{
				Out.TargetClass = OwnerClass->GetName();
			}
		}
		else
		{
			Out.TargetName  = CF->FunctionReference.GetMemberName().ToString();
			if (UClass* MC = CF->FunctionReference.GetMemberParentClass(nullptr))
			{
				Out.TargetClass = MC->GetName();
			}
		}
		Out.bIsPure = CF->IsNodePure();
	}
	else if (const UK2Node_DynamicCast* DC = Cast<UK2Node_DynamicCast>(Node))
	{
		if (DC->TargetType)
		{
			Out.TargetClass = DC->TargetType->GetName();
		}
		Out.bIsPure = DC->IsNodePure();
	}
	else if (const UK2Node_VariableGet* VG = Cast<UK2Node_VariableGet>(Node))
	{
		Out.VariableName = VG->GetVarNameString();
		// UE exposes a "Validated Get" variant of K2Node_VariableGet that
		// has exec pins (execute / then / else) and runs a null check —
		// those are NOT pure. Use IsNodePure() instead of assuming true.
		Out.bIsPure = VG->IsNodePure();
		if (FProperty* Prop = VG->GetPropertyForVariable())
		{
			Out.VariableType = ::PropertyTypeToString(Prop);
			Out.VariableScope = VG->VariableReference.IsSelfContext() ? TEXT("member")
				: (VG->VariableReference.IsLocalScope() ? TEXT("local") : TEXT("external"));
		}
	}
	else if (const UK2Node_VariableSet* VS = Cast<UK2Node_VariableSet>(Node))
	{
		Out.VariableName = VS->GetVarNameString();
		if (FProperty* Prop = VS->GetPropertyForVariable())
		{
			Out.VariableType = ::PropertyTypeToString(Prop);
			Out.VariableScope = VS->VariableReference.IsSelfContext() ? TEXT("member")
				: (VS->VariableReference.IsLocalScope() ? TEXT("local") : TEXT("external"));
		}
	}
	else if (const UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(Node))
	{
		Out.TargetName = CE->CustomFunctionName.ToString();
	}
	else if (const UK2Node_Event* EN = Cast<UK2Node_Event>(Node))
	{
		Out.TargetName = EN->GetFunctionName().ToString();
		if (UClass* OwnerClass = EN->EventReference.GetMemberParentClass(nullptr))
		{
			Out.TargetClass = OwnerClass->GetName();
		}
	}
	else if (const UK2Node_MacroInstance* MI = Cast<UK2Node_MacroInstance>(Node))
	{
		if (UEdGraph* MG = MI->GetMacroGraph())
		{
			Out.MacroGraph = MG->GetName();
		}
	}
	else if (const UK2Node_MakeStruct* MS = Cast<UK2Node_MakeStruct>(Node))
	{
		if (MS->StructType) Out.StructType = MS->StructType->GetPathName();
	}
	else if (const UK2Node_BreakStruct* BS = Cast<UK2Node_BreakStruct>(Node))
	{
		if (BS->StructType) Out.StructType = BS->StructType->GetPathName();
	}
	else if (const UK2Node_AddDelegate* AD = Cast<UK2Node_AddDelegate>(Node))
	{
		Out.DelegateName = AD->GetPropertyName().ToString();
	}
	else if (const UK2Node_RemoveDelegate* RD = Cast<UK2Node_RemoveDelegate>(Node))
	{
		Out.DelegateName = RD->GetPropertyName().ToString();
	}
	else if (const UK2Node_CallDelegate* CD = Cast<UK2Node_CallDelegate>(Node))
	{
		Out.DelegateName = CD->GetPropertyName().ToString();
	}

	// ExecOutCount — count wired-OR-unwired exec-output pins (surface count,
	// not connection count). Useful for fanout detection.
	{
		int32 NExecOut = 0;
		for (UEdGraphPin* P : Node->Pins)
		{
			if (!P || P->bHidden) continue;
			if (P->Direction != EGPD_Output) continue;
			if (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) ++NExecOut;
		}
		Out.ExecOutCount = NExecOut;
	}

	// LiteralValue for literal K2 nodes (MakeLiteralInt/Bool/Float/etc.):
	// grab the "Value" output pin's default if present.
	{
		const FString ClassName = Node->GetClass()->GetName();
		if (ClassName.StartsWith(TEXT("K2Node_MakeLiteral"))
			|| ClassName == TEXT("K2Node_EnumLiteral"))
		{
			for (UEdGraphPin* P : Node->Pins)
			{
				if (!P || P->Direction != EGPD_Output) continue;
				if (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
				Out.LiteralValue = GetPinDefaultString(P);
				if (!Out.LiteralValue.IsEmpty()) break;
			}
		}
	}

	// Pins (with LinkedTo populated).
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;
		FTetherPinInfo Info;
		TetherBPDescribeImpl::FillPinInfo(Pin, Info, /*bFillLinkedTo=*/true);
		Out.Pins.Add(Info);
	}

	return Out;
}

FTetherFunctionSignature UTetherBlueprintLibrary::GetFunctionSignature(
	const FString& ClassPath, const FString& FunctionName)
{
	FTetherFunctionSignature Out;
	Out.FunctionName = FunctionName;

	UClass* Cls = nullptr;
	if (ClassPath.Contains(TEXT(".")) || ClassPath.StartsWith(TEXT("/")))
	{
		Cls = LoadObject<UClass>(nullptr, *ClassPath);
		// BP path without the `_C` suffix resolves to the UBlueprint; auto-pivot
		// to its GeneratedClass so callers can pass either shape interchangeably.
		if (!Cls)
		{
			if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ClassPath))
			{
				Cls = BP->GeneratedClass;
			}
		}
	}
	if (!Cls)
	{
		Cls = FindFirstObject<UClass>(*ClassPath, EFindFirstObjectOptions::None, ELogVerbosity::NoLogging);
	}
	if (!Cls) return Out;

	UFunction* Func = Cls->FindFunctionByName(FName(*FunctionName));
	if (!Func) return Out;

	Out.bFound           = true;
	Out.FunctionName     = Func->GetName();
	Out.OwningClass      = Cls->GetName();
	Out.OwningClassPath  = Cls->GetPathName();
	Out.bIsConst         = Func->HasAnyFunctionFlags(FUNC_Const);
	Out.bIsStatic        = Func->HasAnyFunctionFlags(FUNC_Static);
	Out.bIsNative        = Func->HasAnyFunctionFlags(FUNC_Native);
	Out.bIsBlueprintCallable = Func->HasAnyFunctionFlags(FUNC_BlueprintCallable);
	Out.bIsBlueprintPure     = Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
	Out.bIsPure          = Out.bIsBlueprintPure;
	Out.bIsLatent        = Func->HasMetaData(TEXT("Latent"));
	Out.Category         = Func->HasMetaData(TEXT("Category")) ? Func->GetMetaData(TEXT("Category")) : FString();
	Out.Tooltip          = Func->HasMetaData(TEXT("ToolTip")) ? Func->GetMetaData(TEXT("ToolTip")) : FString();

	// Walk parameters. Include return/out params so the caller sees the
	// output contract explicitly. Iterate all children then filter (safer
	// than relying on locals coming after params in UFunction layout).
	for (TFieldIterator<FProperty> It(Func); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_Parm)) continue;
		FTetherFunctionParam Param;
		Param.Name          = Prop->GetName();
		Param.Type          = ::PropertyTypeToString(Prop);
		Param.bIsOutput     = Prop->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm);
		Param.bIsReference  = Prop->HasAnyPropertyFlags(CPF_ReferenceParm);
		Param.bIsConst      = Prop->HasAnyPropertyFlags(CPF_ConstParm);
		// Declared default — UE stores BP-exposed defaults as metadata
		// "CPP_Default_<ParamName>" on the UFunction.
		const FString DefaultKey = FString::Printf(TEXT("CPP_Default_%s"), *Prop->GetName());
		if (Func->HasMetaData(*DefaultKey))
		{
			Param.DefaultValue = Func->GetMetaData(*DefaultKey);
		}
		Out.Parameters.Add(Param);
	}

	return Out;
}

TArray<FTetherReference> UTetherBlueprintLibrary::FindEventHandlerSites(
	const FString& BlueprintPath, const FString& EventName)
{
	using namespace TetherBPSummaryImpl;
	TArray<FTetherReference> Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || EventName.IsEmpty()) return Result;

	for (const FAllGraphs& Entry : CollectAllGraphs(BP))
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			if (const UK2Node_Event* E = Cast<UK2Node_Event>(Node))
			{
				if (E->GetFunctionName().ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("event")));
				}
			}
			else if (const UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(Node))
			{
				if (CE->CustomFunctionName.ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("event")));
				}
			}
			else if (const UK2Node_CallDelegate* CD = Cast<UK2Node_CallDelegate>(Node))
			{
				if (CD->GetPropertyName().ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("call")));
				}
			}
			else if (const UK2Node_AddDelegate* AD = Cast<UK2Node_AddDelegate>(Node))
			{
				if (AD->GetPropertyName().ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("bind")));
				}
			}
			else if (const UK2Node_RemoveDelegate* RD = Cast<UK2Node_RemoveDelegate>(Node))
			{
				if (RD->GetPropertyName().ToString() == EventName)
				{
					Result.Add(MakeRefFromNode(Entry.Graph, Entry.Type, Node, TEXT("unbind")));
				}
			}
		}
	}
	return Result;
}

// ─── Universal node spawner (FBlueprintActionDatabase) ─────────

// ─── Fine-grained pin link control ─────────────────────────────

// ─── Lint / quality review ─────────────────────────────────────

// ─── Collapse to function ──────────────────────────────────────

// ─── Straighten exec rail ──────────────────────────────────────

// ─── Comment-box color + node tint ─────────────────────────────

// ─── Auto-insert reroute knots for crossing wires ──────────────

// ═══════════════════════════════════════════════════════════════════
//   Extended BP editing: typed pins, struct split, promote, signature
//   edit, collapse-to-macro, async-action, class-name node, editor
//   focus state, cross-BP rename, type-change reporter.
// ═══════════════════════════════════════════════════════════════════

// ─── #11 Set FDataTableRowHandle pin default ────────────────────

// ─── #12 Split / recombine struct pin ───────────────────────────

// ─── #9 Promote pin to variable ─────────────────────────────────

// ─── #14 Remove / reorder function parameter ────────────────────

// ─── #10 Collapse to macro ──────────────────────────────────────

// ─── #13 Async action node ──────────────────────────────────────

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

// ─── #19 Add K2Node by class name ───────────────────────────────

// ─── #17 Editor focus state ─────────────────────────────────────

// ─── #16 Cross-BP rename helpers ────────────────────────────────

// ─── #15 Variable-type change with ref report ───────────────────

// ═══════════════════════════════════════════════════════════════════
//   Graph fingerprint / snapshot / diff (#3)
// ═══════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════
//   Entry-friction fix (#4): EnsureFunctionExecWired +
//   GetFunctionSignature BP-path auto-resolution
// ═══════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════
//   Refactor primitives (#2): InsertNodeOnWire +
//   ReplaceNodePreservingConnections
// ═══════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════
//   Batch graph ops (#1)
// ═══════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════
//   CDO override query (#5)
// ═══════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════
//   PIE node coverage + breakpoint-hit snapshot (verification loop)
// ═══════════════════════════════════════════════════════════════════

bool UTetherBlueprintLibrary::ChangeVariableTypeWithReport(
	const FString& BlueprintPath, const FString& VariableName,
	const FString& NewTypeString, TArray<FString>& OutBrokenNodeGuids)
{
	OutBrokenNodeGuids.Reset();
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName VarName(*VariableName);
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, VarName) == INDEX_NONE) return false;

	FEdGraphPinType NewType;
	if (!TetherTypeParseImpl::ParseTypeString(NewTypeString, NewType)) return false;

	// FBlueprintEditorUtils::ChangeMemberVariableType pops a suppressible
	// modal ("this could break connections — continue?") whenever the
	// variable has active Get/Set nodes in any graph. Programmatic callers
	// can't answer a modal — temporarily flip the suppression ini and
	// restore afterwards so the dialog auto-confirms but we don't leak
	// that choice into the user's persistent settings.
	const TCHAR* DlgSection = TEXT("SuppressableDialogs");
	const TCHAR* DlgKey     = TEXT("ChangeVariableType_Warning");
	bool bPrev = false;
	const bool bHadPrev = GConfig->GetBool(DlgSection, DlgKey, bPrev, GEditorPerProjectIni);
	GConfig->SetBool(DlgSection, DlgKey, true, GEditorPerProjectIni);

	FBlueprintEditorUtils::ChangeMemberVariableType(BP, VarName, NewType);

	if (bHadPrev)
	{
		GConfig->SetBool(DlgSection, DlgKey, bPrev, GEditorPerProjectIni);
	}
	else
	{
		GConfig->RemoveKey(DlgSection, DlgKey, GEditorPerProjectIni);
	}

	FKismetEditorUtilities::CompileBlueprint(BP);

	// Walk every Get/Set node for the variable; flag any whose value pin type
	// doesn't match the new variable type (indicates a broken reconform).
	for (const TetherBPSummaryImpl::FAllGraphs& Entry : TetherBPSummaryImpl::CollectAllGraphs(BP))
	{
		for (UEdGraphNode* Node : Entry.Graph->Nodes)
		{
			UK2Node_Variable* V = Cast<UK2Node_Variable>(Node);
			if (!V) continue;
			if (V->GetVarNameString() != VariableName) continue;
			UEdGraphPin* ValuePin = V->FindPin(VarName);
			if (!ValuePin) { OutBrokenNodeGuids.Add(Node->NodeGuid.ToString(EGuidFormats::Digits)); continue; }
			if (ValuePin->PinType != NewType)
			{
				OutBrokenNodeGuids.Add(Node->NodeGuid.ToString(EGuidFormats::Digits));
			}
		}
	}
	return true;
}

// ─── Enhanced Input — graph-node factories (B1) ─────────────────

// ─── B2/B3/B4/B5 Legacy K2Node factories ────────────────────────────

// ─── C4 BeginPlay → AddMappingContext graph ─────────────────────────

