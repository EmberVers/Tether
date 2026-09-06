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
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Self.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_Message.h"
#include "K2Node_Timeline.h"
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

namespace TetherBPLocalVarImpl
{
	/** Find a function graph by name. Restricted to FunctionGraphs — local
	 *  variables don't apply to ubergraph pages or macros. */
	static UEdGraph* FindFunctionGraph(UBlueprint* BP, const FString& FunctionName)
	{
		if (!BP) return nullptr;
		for (UEdGraph* G : BP->FunctionGraphs)
		{
			if (G && G->GetName() == FunctionName) return G;
		}
		return nullptr;
	}

	/** The function-entry node owns the LocalVariables array. */
	static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
	{
		if (!Graph) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N))
			{
				return E;
			}
		}
		return nullptr;
	}

	/** Resolve the UStruct scope used by FBlueprintEditorUtils' local-var
	 *  helpers. For a function graph this is the UFunction on the skeleton
	 *  class (which exists even before a successful compile of the body). */
	static UStruct* FindFunctionScope(UBlueprint* BP, UEdGraph* Graph)
	{
		if (!BP || !Graph) return nullptr;
		UClass* Skeleton = BP->SkeletonGeneratedClass ? BP->SkeletonGeneratedClass : BP->GeneratedClass;
		if (!Skeleton) return nullptr;
		if (UFunction* Fn = Skeleton->FindFunctionByName(Graph->GetFName()))
		{
			return Fn;
		}
		return nullptr;
	}

	/** Build FTetherVariableInfo from a local FBPVariableDescription. Looks up
	 *  the skeleton UFunction's corresponding FProperty (if any) for a more
	 *  precise type string; falls back to pin-category string. */
	static FTetherVariableInfo MakeLocalVarInfo(
		const FBPVariableDescription& Var, const UStruct* Scope)
	{
		FTetherVariableInfo Info;
		Info.Name = Var.VarName.ToString();

		const FProperty* Prop = Scope ? Scope->FindPropertyByName(Var.VarName) : nullptr;
		Info.Type = Prop ? PropertyTypeToString(Prop) : Var.VarType.PinCategory.ToString();
		Info.Category = Var.Category.ToString();
		if (Var.HasMetaData(TEXT("tooltip")))
		{
			Info.Description = Var.GetMetaData(TEXT("tooltip"));
		}
		Info.DefaultValue = Var.DefaultValue;
		Info.bInstanceEditable  = (Var.PropertyFlags & CPF_Edit) != 0;
		Info.bBlueprintReadOnly = (Var.PropertyFlags & CPF_BlueprintReadOnly) != 0;
		Info.ReplicationCondition = TEXT("None");
		return Info;
	}
}
FString UTetherBlueprintLibrary::AddFunctionLocalVariableNode(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& VariableName, bool bIsSet, int32 NodePosX, int32 NodePosY)
{
	using namespace TetherBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return FString();
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return FString();

	const FName VarFName(*VariableName);

	// Resolve via LocalVariables first (actual local var). Fall back to
	// UserDefinedPins on the FunctionEntry (function input parameters) —
	// they compile to FProperty fields on the generated UFunction too, so
	// a K2Node_VariableGet with SetLocalMember resolves against them the
	// same way the editor's drag-from-MyBlueprint path does. This lets
	// callers avoid authoring a redundant `ParamL = Param` SET chain just
	// to use a Get node near a consumer.
	FGuid VarGuid;
	bool bFound = false;
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == VarFName) { VarGuid = V.VarGuid; bFound = true; break; }
	}
	if (!bFound)
	{
		for (const TSharedPtr<FUserPinInfo>& UDP : Entry->UserDefinedPins)
		{
			if (UDP.IsValid() && UDP->PinName == VarFName)
			{
				// Function parameters carry no GUID in FUserPinInfo —
				// leave VarGuid empty. VariableReference resolution at
				// compile time falls back to name-based lookup against
				// the generated UFunction's parameter properties, which
				// is what makes the editor's drag-from-MyBlueprint path
				// work for parameters too (see EdGraphSchema_K2::
				// ConfigureVarNode's local-var branch).
				bFound = true;
				break;
			}
		}
	}
	if (!bFound) return FString();

	Graph->Modify();
	BP->Modify();

	UK2Node_Variable* Node = bIsSet
		? (UK2Node_Variable*)NewObject<UK2Node_VariableSet>(Graph)
		: (UK2Node_Variable*)NewObject<UK2Node_VariableGet>(Graph);
	Node->CreateNewGuid();
	// Local-member scope is the top-level function graph's *name* (string),
	// with the variable's declared FGuid — see K2Node_LocalVariable.cpp.
	Node->VariableReference.SetLocalMember(VarFName, Graph->GetName(), VarGuid);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
namespace TetherBpInterfaceOps
{
	static UClass* ResolveInterfaceClass(const FString& InterfacePath)
	{
		if (InterfacePath.IsEmpty()) return nullptr;

		// Try as a loaded / native class first.
		if (UClass* Cls = FindObject<UClass>(nullptr, *InterfacePath))
		{
			return Cls;
		}

		// Try as a Blueprint interface asset path → GeneratedClass.
		if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *InterfacePath))
		{
			return BP->GeneratedClass;
		}

		// Try LoadObject<UClass> for TopLevelAssetPath-style strings.
		if (UClass* Cls = LoadObject<UClass>(nullptr, *InterfacePath))
		{
			return Cls;
		}

		// Try appending "_C" for Blueprint class paths.
		const FString WithC = InterfacePath + TEXT("_C");
		if (UClass* Cls = LoadObject<UClass>(nullptr, *WithC))
		{
			return Cls;
		}
		return nullptr;
	}
}
namespace TetherBlueprintGraphWriteImpl
{
	UEdGraph* FindGraphByName(UBlueprint* BP, const FString& GraphName)
	{
		if (!BP || GraphName.IsEmpty()) return nullptr;

		// Standard first-tier search (top-level graphs on the Blueprint).
		for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetName() == GraphName) return G; }
		for (UEdGraph* G : BP->UbergraphPages) { if (G && G->GetName() == GraphName) return G; }
		for (UEdGraph* G : BP->MacroGraphs)    { if (G && G->GetName() == GraphName) return G; }
		for (UEdGraph* G : BP->DelegateSignatureGraphs) { if (G && G->GetName() == GraphName) return G; }
		for (const FBPInterfaceDescription& Interface : BP->ImplementedInterfaces)
		{
			for (UEdGraph* G : Interface.Graphs)
			{
				if (G && G->GetName() == GraphName) return G;
			}
		}

		// Deep walk: covers AnimBlueprint interiors (state-machine graphs,
		// state BoundGraphs, transition rule graphs) and nested K2 SubGraphs
		// (collapsed-function graphs, macro expansions). Clients writing into
		// transition rule graphs or an anim state's inner graph used to get
		// "graph not found" silently — this catches them.
		TArray<UEdGraph*> Stack;
		Stack.Append(BP->FunctionGraphs);
		Stack.Append(BP->UbergraphPages);
		Stack.Append(BP->MacroGraphs);
		Stack.Append(BP->DelegateSignatureGraphs);
		for (const FBPInterfaceDescription& Interface : BP->ImplementedInterfaces)
		{
			Stack.Append(Interface.Graphs);
		}

		TSet<UEdGraph*> Visited;
		while (Stack.Num() > 0)
		{
			UEdGraph* G = Stack.Pop(EAllowShrinking::No);
			if (!G || Visited.Contains(G)) continue;
			Visited.Add(G);

			// Nested SubGraphs on a graph (collapsed nodes, composite graphs).
			Stack.Append(G->SubGraphs);

			// Nodes may own sub-graphs. Three families we care about:
			//  - UAnimGraphNode_StateMachineBase::EditorStateMachineGraph
			//  - UAnimStateNodeBase::BoundGraph (covers state + conduit + transition)
			//  - UK2Node_Composite / UK2Node_MacroInstance::BoundGraph
			for (UEdGraphNode* N : G->Nodes)
			{
				if (!N) continue;
				// Use reflection-free property access via duck-typed virtual
				// UEdGraphNode::GetSubGraphs().
				TArray<UEdGraph*> Subs = N->GetSubGraphs();
				for (UEdGraph* Sub : Subs)
				{
					if (Sub && Sub->GetName() == GraphName) return Sub;
					if (Sub) Stack.Add(Sub);
				}
			}
		}
		return nullptr;
	}

	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& GuidStr)
	{
		if (!Graph) return nullptr;
		FGuid Guid;
		if (!FGuid::Parse(GuidStr, Guid)) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->NodeGuid == Guid) return N;
		}
		return nullptr;
	}

	UClass* ResolveTargetClass(UBlueprint* BP, const FString& TargetClassPath)
	{
		if (TargetClassPath.IsEmpty())
		{
			return BP->GeneratedClass ? BP->GeneratedClass : BP->ParentClass;
		}
		if (UClass* C = FindObject<UClass>(nullptr, *TargetClassPath))
		{
			return C;
		}
		if (UClass* C = LoadObject<UClass>(nullptr, *TargetClassPath))
		{
			return C;
		}
		// Try BP asset path (auto-append _C).
		const FString WithC = TargetClassPath.EndsWith(TEXT("_C"))
			? TargetClassPath : TargetClassPath + TEXT("_C");
		if (UClass* C = LoadObject<UClass>(nullptr, *WithC))
		{
			return C;
		}
		if (UBlueprint* Other = LoadObject<UBlueprint>(nullptr, *TargetClassPath))
		{
			return Other->GeneratedClass;
		}
		return nullptr;
	}
}
FString UTetherBlueprintLibrary::AddCallFunctionNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& TargetClassPath, const FString& FunctionName,
	int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();

	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	UClass* TargetClass = TetherBlueprintGraphWriteImpl::ResolveTargetClass(BP, TargetClassPath);
	if (!TargetClass) return FString();

	UFunction* Fn = TargetClass->FindFunctionByName(FName(*FunctionName));
	if (!Fn) return FString();

	Graph->Modify();
	BP->Modify();

	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	Node->CreateNewGuid();
	Node->SetFromFunction(Fn);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddVariableNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& VariableName, bool bIsSet,
	int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();

	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	const FName VarFName(*VariableName);

	// Resolve against either this BP's declared vars or an inherited property.
	bool bSelfDeclared = false;
	for (const FBPVariableDescription& V : BP->NewVariables)
	{
		if (V.VarName == VarFName) { bSelfDeclared = true; break; }
	}
	UClass* SearchClass = BP->GeneratedClass ? BP->GeneratedClass : BP->ParentClass;
	FProperty* Prop = SearchClass ? FindFProperty<FProperty>(SearchClass, VarFName) : nullptr;
	if (!bSelfDeclared && !Prop)
	{
		return FString();
	}

	Graph->Modify();
	BP->Modify();

	UK2Node_Variable* Node = bIsSet
		? (UK2Node_Variable*)NewObject<UK2Node_VariableSet>(Graph)
		: (UK2Node_Variable*)NewObject<UK2Node_VariableGet>(Graph);
	Node->CreateNewGuid();
	Node->VariableReference.SetSelfMember(VarFName);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddExternalVariableNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& OwnerClassPath, const FString& VariableName, bool bIsSet,
	int32 NodePosX, int32 NodePosY)
{
	const TCHAR* Operation = bIsSet ? TEXT("Set") : TEXT("Get");
	const auto Fail = [&](const TCHAR* Reason) -> FString
	{
		UE_LOG(LogTetherBlueprintGraph, Warning,
			TEXT("AddExternalVariableNode failed: Reason='%s' Blueprint='%s' Graph='%s' OwnerClass='%s' Property='%s' Operation='%s'"),
			Reason, *BlueprintPath, *GraphName, *OwnerClassPath, *VariableName, Operation);
		return FString();
	};

	if (OwnerClassPath.TrimStartAndEnd().IsEmpty())
	{
		return Fail(TEXT("OwnerClassPath must be explicitly provided"));
	}
	if (VariableName.TrimStartAndEnd().IsEmpty())
	{
		return Fail(TEXT("VariableName must not be empty"));
	}

	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP)
	{
		return Fail(TEXT("Blueprint could not be loaded"));
	}

	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph)
	{
		return Fail(TEXT("Graph was not found"));
	}
	if (!Cast<UEdGraphSchema_K2>(Graph->GetSchema()))
	{
		return Fail(TEXT("Graph does not use a K2 schema"));
	}

	UClass* RequestedOwnerClass = TetherBlueprintGraphWriteImpl::ResolveTargetClass(BP, OwnerClassPath);
	if (!RequestedOwnerClass)
	{
		return Fail(TEXT("Owner class could not be resolved"));
	}

	const FName VarFName(*VariableName);
	FProperty* Property = FindFProperty<FProperty>(RequestedOwnerClass, VarFName);
	if (!Property)
	{
		return Fail(TEXT("Property was not found on the owner class"));
	}
	// 使用引擎的权威访问检查，让 consumer Blueprint 的身份参与 private/read-only/visible 判定。
	// Use the engine's canonical access checks so the consumer Blueprint identity participates in private/read-only/visible decisions.
	if (bIsSet)
	{
		const FBlueprintEditorUtils::EPropertyWritableState WritableState =
			FBlueprintEditorUtils::IsPropertyWritableInBlueprint(BP, Property);
		if (WritableState != FBlueprintEditorUtils::EPropertyWritableState::Writable)
		{
			return Fail(TEXT("Property is not writable from the consumer Blueprint"));
		}
	}
	else
	{
		const FBlueprintEditorUtils::EPropertyReadableState ReadableState =
			FBlueprintEditorUtils::IsPropertyReadableInBlueprint(BP, Property);
		if (ReadableState != FBlueprintEditorUtils::EPropertyReadableState::Readable)
		{
			return Fail(TEXT("Property is not readable from the consumer Blueprint"));
		}
	}

	UClass* DeclaringOwnerClass = Property->GetOwner<UClass>();
	if (!DeclaringOwnerClass)
	{
		return Fail(TEXT("Property does not have a declaring UClass"));
	}

	// 先用未加入图的候选节点验证 K2 引脚契约，确保所有失败都发生在事务和图修改之前。
	// Validate the K2 pin contract on an unregistered candidate so every failure precedes the transaction and graph mutation.
	UK2Node_Variable* CandidateNode = bIsSet
		? static_cast<UK2Node_Variable*>(NewObject<UK2Node_VariableSet>(Graph, NAME_None, RF_Transient))
		: static_cast<UK2Node_Variable*>(NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transient));
	ON_SCOPE_EXIT
	{
		CandidateNode->MarkAsGarbage();
	};
	CandidateNode->VariableReference.SetExternalMember(VarFName, DeclaringOwnerClass);
	CandidateNode->AllocateDefaultPins();

	UEdGraphPin* TargetPin = CandidateNode->FindPin(TEXT("self"));
	UEdGraphPin* ValuePin = CandidateNode->FindPin(VarFName);
	const EEdGraphPinDirection ExpectedValueDirection = bIsSet ? EGPD_Input : EGPD_Output;
	if (!TargetPin || TargetPin->Direction != EGPD_Input)
	{
		return Fail(TEXT("Variable node did not allocate the required 'self' input pin"));
	}
	if (!ValuePin || ValuePin->Direction != ExpectedValueDirection)
	{
		return Fail(TEXT("Variable node did not allocate a value pin with the expected direction"));
	}

	const FScopedTransaction Transaction(NSLOCTEXT(
		"Tether", "AddExternalVariableNode", "Tether: Add External Variable Node"));
	BP->Modify();
	Graph->Modify();

	UK2Node_Variable* Node = bIsSet
		? static_cast<UK2Node_Variable*>(NewObject<UK2Node_VariableSet>(Graph))
		: static_cast<UK2Node_Variable*>(NewObject<UK2Node_VariableGet>(Graph));
	Node->SetFlags(RF_Transactional);
	Node->Modify();
	Node->CreateNewGuid();
	Node->VariableReference.SetExternalMember(VarFName, DeclaringOwnerClass);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::ConnectGraphPins(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& SourceNodeGuid, const FString& SourcePinName,
	const FString& TargetNodeGuid, const FString& TargetPinName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* SrcNode = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, SourceNodeGuid);
	UEdGraphNode* DstNode = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, TargetNodeGuid);
	if (!SrcNode || !DstNode) return false;

	UEdGraphPin* SrcPin = SrcNode->FindPin(SourcePinName);
	UEdGraphPin* DstPin = DstNode->FindPin(TargetPinName);
	if (!SrcPin || !DstPin) return false;

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!Schema) return false;

	Graph->Modify();
	BP->Modify();

	const bool bConnected = Schema->TryCreateConnection(SrcPin, DstPin);
	if (bConnected)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	return bConnected;
}
bool UTetherBlueprintLibrary::RemoveGraphNode(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* Node = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	Graph->Modify();
	BP->Modify();
	Node->BreakAllNodeLinks();
	Graph->RemoveNode(Node);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
FString UTetherBlueprintLibrary::AddEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& ParentClassPath, const FString& EventName,
	int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();

	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	UClass* ParentClass = ParentClassPath.IsEmpty()
		? static_cast<UClass*>(BP->ParentClass)
		: TetherBlueprintGraphWriteImpl::ResolveTargetClass(BP, ParentClassPath);
	if (!ParentClass) return FString();

	UFunction* Fn = ParentClass->FindFunctionByName(FName(*EventName));
	if (!Fn) return FString();

	const FName EventFName = Fn->GetFName();

	// Reuse existing event (e.g. the default ghost ReceiveTick) instead of creating a duplicate.
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (UK2Node_Event* Existing = Cast<UK2Node_Event>(N))
		{
			if (Existing->EventReference.GetMemberName() == EventFName)
			{
				Existing->Modify();
				Existing->bOverrideFunction = true;
				Existing->NodePosX = NodePosX;
				Existing->NodePosY = NodePosY;
				FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
				return Existing->NodeGuid.ToString(EGuidFormats::Digits);
			}
		}
	}

	Graph->Modify();
	BP->Modify();

	UK2Node_Event* Node = NewObject<UK2Node_Event>(Graph);
	Node->CreateNewGuid();
	Node->EventReference.SetExternalMember(EventFName, ParentClass);
	Node->bOverrideFunction = true;
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::SetPinDefaultValue(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& PinName, const FString& NewDefaultValue)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* Node = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	UEdGraphPin* Pin = Node->FindPin(PinName);
	if (!Pin) return false;

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!Schema) return false;

	Node->Modify();
	Schema->TrySetDefaultValue(*Pin, NewDefaultValue);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
namespace TetherBpP0Impl
{
	// Close any open SGraphEditor tab that the BP editor is currently showing
	// for `Graph`. MUST be called before FBlueprintEditorUtils::RemoveGraph —
	// RemoveGraph does not dismiss editor tabs, so a subsequent CompileBlueprint
	// walks the zombie nodes Slate is still holding references to and hits
	// `FindBlueprintForNodeChecked` → fatal assert → editor crash.
	inline void CloseOpenGraphTabs(UBlueprint* BP, UEdGraph* Graph)
	{
		if (!BP || !Graph) return;
		UAssetEditorSubsystem* Sub = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
		if (!Sub) return;
		IAssetEditorInstance* Inst = Sub->FindEditorForAsset(BP, /*bFocusIfOpen*/false);
		if (!Inst) return;
		FBlueprintEditor* BPEd = static_cast<FBlueprintEditor*>(Inst);
		BPEd->CloseDocumentTab(Graph);
	}

	// Shared helper: finalize a newly-constructed K2Node and add it to Graph.
	//
	// Mirrors FGraphNodeCreator::Finalize: only call AllocateDefaultPins ourselves
	// when PostPlacedNewNode did not already populate them. Most K2 nodes leave
	// Pins empty after PostPlacedNewNode, but a few — notably UK2Node_FunctionResult,
	// whose PostPlacedNewNode runs SyncWithEntryNode → ReconstructNode → allocate —
	// ship with a full pin set already. Calling AllocateDefaultPins again there
	// duplicates the default `execute` exec pin.
	template<typename TNode>
	TNode* FinalizeNewNode(UEdGraph* Graph, TNode* Node, int32 X, int32 Y)
	{
		Node->CreateNewGuid();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
		Node->PostPlacedNewNode();
		if (Node->Pins.Num() == 0)
		{
			Node->AllocateDefaultPins();
		}
		return Node;
	}

	// Parse "public"/"protected"/"private" to UE access flags applied on FunctionEntry.
	// Returns true if AccessSpec is non-empty and understood.
	bool ApplyAccessSpecifier(UK2Node_FunctionEntry* Entry, const FString& AccessSpec)
	{
		if (!Entry || AccessSpec.IsEmpty()) return false;
		int32 Flags = Entry->GetExtraFlags();
		Flags &= ~(FUNC_Public | FUNC_Protected | FUNC_Private);
		if (AccessSpec.Equals(TEXT("public"), ESearchCase::IgnoreCase))    Flags |= FUNC_Public;
		else if (AccessSpec.Equals(TEXT("protected"), ESearchCase::IgnoreCase)) Flags |= FUNC_Protected;
		else if (AccessSpec.Equals(TEXT("private"), ESearchCase::IgnoreCase))   Flags |= FUNC_Private;
		else return false;
		Entry->SetExtraFlags(Flags);
		return true;
	}

	UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
	{
		if (!Graph) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N)) return E;
		}
		return nullptr;
	}

	UK2Node_FunctionResult* FindOrCreateFunctionResult(UEdGraph* Graph, UBlueprint* BP)
	{
		if (!Graph) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_FunctionResult* R = Cast<UK2Node_FunctionResult>(N)) return R;
		}
		// Create a result node, positioned right of the entry.
		UK2Node_FunctionResult* Result = NewObject<UK2Node_FunctionResult>(Graph);
		int32 X = 600, Y = 0;
		if (UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph))
		{
			X = Entry->NodePosX + 600;
			Y = Entry->NodePosY;
		}
		return FinalizeNewNode(Graph, Result, X, Y);
	}

	// Resolve a member variable's FMulticastDelegateProperty on the BP's generated class.
	FMulticastDelegateProperty* FindDispatcherProp(UBlueprint* BP, const FString& DispatcherName)
	{
		if (!BP || !BP->SkeletonGeneratedClass) return nullptr;
		return FindFProperty<FMulticastDelegateProperty>(BP->SkeletonGeneratedClass, FName(*DispatcherName));
	}
}
FString UTetherBlueprintLibrary::AddBranchNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_IfThenElse* Node = TetherBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_IfThenElse>(Graph), X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddSequenceNode(
	const FString& BlueprintPath, const FString& GraphName, int32 PinCount, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	const int32 Want = FMath::Clamp(PinCount, 2, 16);
	Graph->Modify(); BP->Modify();
	UK2Node_ExecutionSequence* Node = TetherBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_ExecutionSequence>(Graph), X, Y);
	// AllocateDefaultPins already creates "Then 0" + "Then 1".
	for (int32 i = 2; i < Want; ++i)
	{
		Node->AddInputPin();
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddCastNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& TargetClassPath, bool bPure, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UClass* Target = TetherBlueprintGraphWriteImpl::ResolveTargetClass(BP, TargetClassPath);
	if (!Target) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_DynamicCast* Node = NewObject<UK2Node_DynamicCast>(Graph);
	Node->TargetType = Target;
	Node->SetPurity(bPure);
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddSelfNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_Self* Node = TetherBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_Self>(Graph), X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddCustomEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& EventName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	if (EventName.IsEmpty()) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_CustomEvent* Node = NewObject<UK2Node_CustomEvent>(Graph);
	Node->CustomFunctionName = FBlueprintEditorUtils::FindUniqueKismetName(BP, EventName);
	Node->bIsEditable = true;
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::CreateFunctionGraph(
	const FString& BlueprintPath, const FString& FunctionName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName FnName(*FunctionName);
	if (FnName.IsNone()) return false;

	// Reject if already present.
	for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) return false; }

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, FnName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, NewGraph, /*bIsUserCreated*/true, /*SignatureFromObject*/nullptr);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::RemoveFunctionGraph(
	const FString& BlueprintPath, const FString& FunctionName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName FnName(*FunctionName);
	for (UEdGraph* G : BP->FunctionGraphs)
	{
		if (G && G->GetFName() == FnName)
		{
			TetherBpP0Impl::CloseOpenGraphTabs(BP, G);
			FBlueprintEditorUtils::RemoveGraph(BP, G, EGraphRemoveFlags::Recompile);
			return true;
		}
	}
	return false;
}
bool UTetherBlueprintLibrary::RenameFunctionGraph(
	const FString& BlueprintPath, const FString& OldName, const FString& NewName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName Old(*OldName), New(*NewName);
	if (Old == New || New.IsNone()) return false;
	for (UEdGraph* G : BP->FunctionGraphs)
	{
		if (G && G->GetFName() == Old)
		{
			FBlueprintEditorUtils::RenameGraph(G, NewName);
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
			FKismetEditorUtilities::CompileBlueprint(BP);
			return true;
		}
	}
	return false;
}
bool UTetherBlueprintLibrary::AddFunctionParameter(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& ParamName, const FString& TypeString, bool bIsReturn)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = nullptr;
	const FName FnName(*FunctionName);
	for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) { Graph = G; break; } }
	if (!Graph) return false;

	FEdGraphPinType PinType;
	if (!TetherTypeParseImpl::ParseTypeString(TypeString, PinType)) return false;

	UK2Node_EditablePinBase* Target = bIsReturn
		? static_cast<UK2Node_EditablePinBase*>(TetherBpP0Impl::FindOrCreateFunctionResult(Graph, BP))
		: static_cast<UK2Node_EditablePinBase*>(TetherBpP0Impl::FindFunctionEntry(Graph));
	if (!Target) return false;

	Target->Modify();
	UEdGraphPin* NewPin = Target->CreateUserDefinedPin(FName(*ParamName), PinType,
		bIsReturn ? EGPD_Input : EGPD_Output);
	if (!NewPin) return false;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::SetFunctionMetadata(
	const FString& BlueprintPath, const FString& FunctionName,
	bool bPure, bool bConst, const FString& Category, const FString& AccessSpecifier)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = nullptr;
	const FName FnName(*FunctionName);
	for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) { Graph = G; break; } }
	if (!Graph) return false;

	UK2Node_FunctionEntry* Entry = TetherBpP0Impl::FindFunctionEntry(Graph);
	if (!Entry) return false;

	Entry->Modify();
	int32 Flags = Entry->GetExtraFlags();
	if (bPure)  Flags |= FUNC_BlueprintPure; else Flags &= ~FUNC_BlueprintPure;
	if (bConst) Flags |= FUNC_Const;         else Flags &= ~FUNC_Const;
	Entry->SetExtraFlags(Flags);

	if (!Category.IsEmpty())
	{
		Entry->MetaData.Category = FText::FromString(Category);
	}
	TetherBpP0Impl::ApplyAccessSpecifier(Entry, AccessSpecifier);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::AddEventDispatcher(
	const FString& BlueprintPath, const FString& DispatcherName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName Name(*DispatcherName);
	if (Name.IsNone()) return false;
	// Reject duplicate against any existing dispatcher signature graph or other kismet name.
	for (UEdGraph* G : BP->DelegateSignatureGraphs) { if (G && G->GetFName() == Name) return false; }
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Name) != INDEX_NONE) return false;

	BP->Modify();

	// Step 1: add the matching member variable of MCDelegate type.
	FEdGraphPinType DelegateType;
	DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
	if (!FBlueprintEditorUtils::AddMemberVariable(BP, Name, DelegateType))
	{
		return false;
	}

	// Step 2: create the signature graph.
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!NewGraph)
	{
		FBlueprintEditorUtils::RemoveMemberVariable(BP, Name);
		return false;
	}
	NewGraph->bEditable = false;

	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	K2->CreateDefaultNodesForGraph(*NewGraph);
	K2->CreateFunctionGraphTerminators(*NewGraph, static_cast<UClass*>(nullptr));
	K2->AddExtraFunctionFlags(NewGraph, (FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public));
	K2->MarkFunctionEntryAsEditable(NewGraph, true);

	BP->DelegateSignatureGraphs.Add(NewGraph);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::RemoveEventDispatcher(
	const FString& BlueprintPath, const FString& DispatcherName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName Name(*DispatcherName);

	UEdGraph* SigGraph = nullptr;
	for (UEdGraph* G : BP->DelegateSignatureGraphs)
	{
		if (G && G->GetFName() == Name) { SigGraph = G; break; }
	}
	const bool bHasVar = FBlueprintEditorUtils::FindNewVariableIndex(BP, Name) != INDEX_NONE;
	if (!SigGraph && !bHasVar) return false;

	BP->Modify();
	if (SigGraph)
	{
		TetherBpP0Impl::CloseOpenGraphTabs(BP, SigGraph);
		BP->DelegateSignatureGraphs.Remove(SigGraph);
		FBlueprintEditorUtils::RemoveGraph(BP, SigGraph, EGraphRemoveFlags::Recompile);
	}
	if (bHasVar)
	{
		FBlueprintEditorUtils::RemoveMemberVariable(BP, Name);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::RenameEventDispatcher(
	const FString& BlueprintPath, const FString& OldName, const FString& NewName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName Old(*OldName), New(*NewName);
	if (Old == New || New.IsNone()) return false;
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, New) != INDEX_NONE) return false;

	UEdGraph* SigGraph = nullptr;
	for (UEdGraph* G : BP->DelegateSignatureGraphs)
	{
		if (G && G->GetFName() == Old) { SigGraph = G; break; }
	}
	if (!SigGraph) return false;
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Old) == INDEX_NONE) return false;

	BP->Modify();
	FBlueprintEditorUtils::RenameMemberVariable(BP, Old, New);
	FBlueprintEditorUtils::RenameGraph(SigGraph, NewName);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}
FString UTetherBlueprintLibrary::AddDispatcherCallNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& DispatcherName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	FMulticastDelegateProperty* Prop = TetherBpP0Impl::FindDispatcherProp(BP, DispatcherName);
	if (!Prop) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_CallDelegate* Node = NewObject<UK2Node_CallDelegate>(Graph);
	Node->SetFromProperty(Prop, /*bSelfContext*/true, Prop->GetOwnerClass());
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddDispatcherBindNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& DispatcherName, bool bUnbind, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	FMulticastDelegateProperty* Prop = TetherBpP0Impl::FindDispatcherProp(BP, DispatcherName);
	if (!Prop) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_BaseMCDelegate* Node = bUnbind
		? static_cast<UK2Node_BaseMCDelegate*>(NewObject<UK2Node_RemoveDelegate>(Graph))
		: static_cast<UK2Node_BaseMCDelegate*>(NewObject<UK2Node_AddDelegate>(Graph));
	Node->SetFromProperty(Prop, /*bSelfContext*/true, Prop->GetOwnerClass());
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::ImplementInterfaceFunction(
	const FString& BlueprintPath, const FString& InterfacePath, const FString& FunctionName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UClass* IFace = TetherBpInterfaceOps::ResolveInterfaceClass(InterfacePath);
	if (!IFace) return false;

	FBPInterfaceDescription* Desc = nullptr;
	for (FBPInterfaceDescription& D : BP->ImplementedInterfaces)
	{
		if (D.Interface == IFace) { Desc = &D; break; }
	}
	if (!Desc) return false;

	UFunction* Fn = IFace->FindFunctionByName(FName(*FunctionName));
	if (!Fn) return false;

	// Event-type interface members (BlueprintImplementableEvent with no return, no out params)
	// don't use a dedicated function graph — caller should use AddEventNode on the EventGraph.
	if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Fn)) return false;

	// Already implemented?
	for (UEdGraph* G : Desc->Graphs) { if (G && G->GetFName() == Fn->GetFName()) return true; }

	BP->Modify();
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, Fn->GetFName(), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!NewGraph) return false;
	NewGraph->bAllowDeletion = false;
	NewGraph->InterfaceGuid = FBlueprintEditorUtils::FindInterfaceFunctionGuid(Fn, IFace);
	Desc->Graphs.Add(NewGraph);
	FBlueprintEditorUtils::AddInterfaceGraph(BP, NewGraph, IFace);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}
FString UTetherBlueprintLibrary::AddInterfaceMessageNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& InterfacePath, const FString& FunctionName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UClass* IFace = TetherBpInterfaceOps::ResolveInterfaceClass(InterfacePath);
	if (!IFace || !IFace->HasAnyClassFlags(CLASS_Interface)) return FString();
	UFunction* Fn = IFace->FindFunctionByName(FName(*FunctionName));
	if (!Fn) return FString();

	Graph->Modify(); BP->Modify();
	UK2Node_Message* Node = NewObject<UK2Node_Message>(Graph);
	Node->FunctionReference.SetExternalMember(Fn->GetFName(), IFace);
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::SetVariableMetadata(
	const FString& BlueprintPath, const FString& VariableName,
	bool bInstanceEditable, bool bBlueprintReadOnly, bool bExposeOnSpawn, bool bPrivate,
	const FString& Category, const FString& Tooltip, const FString& ReplicationMode)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName VarName(*VariableName);
	const int32 Idx = FBlueprintEditorUtils::FindNewVariableIndex(BP, VarName);
	if (Idx == INDEX_NONE) return false;

	FBPVariableDescription& Var = BP->NewVariables[Idx];

	auto SetBit = [&](uint64 Bit, bool bOn)
	{
		if (bOn)  Var.PropertyFlags |= Bit;
		else      Var.PropertyFlags &= ~Bit;
	};
	// InstanceEditable = !DisableEditOnInstance
	SetBit(CPF_DisableEditOnInstance, !bInstanceEditable);
	SetBit(CPF_BlueprintReadOnly,      bBlueprintReadOnly);
	SetBit(CPF_ExposeOnSpawn,          bExposeOnSpawn);
	// Private = DisableEditOnInstance && ~BlueprintVisible via metadata? The common flag is
	// CPF_Protected via metadata "BlueprintPrivate". Use meta key.
	if (bPrivate) Var.SetMetaData(TEXT("BlueprintPrivate"), TEXT("true"));
	else          Var.RemoveMetaData(TEXT("BlueprintPrivate"));

	if (!Category.IsEmpty())
	{
		FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, VarName, nullptr,
			Category.TrimStartAndEnd().IsEmpty() ? FText::GetEmpty() : FText::FromString(Category));
	}
	if (!Tooltip.IsEmpty())
	{
		Var.SetMetaData(TEXT("tooltip"), Tooltip.TrimStartAndEnd().IsEmpty() ? TEXT("") : *Tooltip);
	}
	if (!ReplicationMode.IsEmpty())
	{
		Var.PropertyFlags &= ~(CPF_Net | CPF_RepNotify);
		Var.ReplicationCondition = ELifetimeCondition::COND_None;
		if (ReplicationMode.Equals(TEXT("Replicated"), ESearchCase::IgnoreCase))
		{
			Var.PropertyFlags |= CPF_Net;
		}
		else if (ReplicationMode.Equals(TEXT("RepNotify"), ESearchCase::IgnoreCase))
		{
			Var.PropertyFlags |= (CPF_Net | CPF_RepNotify);
			// Caller is expected to set RepNotifyFunc via a separate call (not in P0 scope).
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::SetVariableType(
	const FString& BlueprintPath, const FString& VariableName, const FString& NewTypeString)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName VarName(*VariableName);
	if (FBlueprintEditorUtils::FindNewVariableIndex(BP, VarName) == INDEX_NONE) return false;

	FEdGraphPinType NewType;
	if (!TetherTypeParseImpl::ParseTypeString(NewTypeString, NewType)) return false;

	FBlueprintEditorUtils::ChangeMemberVariableType(BP, VarName, NewType);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
namespace TetherBpP1Impl
{
	UEdGraph* FindStandardMacro(const TCHAR* MacroName)
	{
		UBlueprint* MacrosBP = LoadObject<UBlueprint>(nullptr,
			TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
		if (!MacrosBP) return nullptr;
		const FName Target(MacroName);
		for (UEdGraph* G : MacrosBP->MacroGraphs)
		{
			if (G && G->GetFName() == Target) return G;
		}
		return nullptr;
	}

	FString AddMacroNodeByName(UBlueprint* BP, UEdGraph* Graph,
		const TCHAR* MacroName, int32 X, int32 Y)
	{
		UEdGraph* Macro = FindStandardMacro(MacroName);
		if (!Macro) return FString();
		Graph->Modify(); BP->Modify();
		UK2Node_MacroInstance* Node = NewObject<UK2Node_MacroInstance>(Graph);
		Node->SetMacroGraph(Macro);
		TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		return Node->NodeGuid.ToString(EGuidFormats::Digits);
	}

	// Resolve {component name} → SCS_Node in the BP's SimpleConstructionScript.
	// Walks across parent BPs too (inherited components live there).
	USCS_Node* FindSCSNodeInHierarchy(UBlueprint* BP, const FName& CompName, UBlueprint*& OutOwnerBP)
	{
		OutOwnerBP = nullptr;
		for (UBlueprint* Cur = BP; Cur; )
		{
			if (Cur->SimpleConstructionScript)
			{
				if (USCS_Node* N = Cur->SimpleConstructionScript->FindSCSNode(CompName))
				{
					OutOwnerBP = Cur;
					return N;
				}
			}
			UClass* PC = Cur->ParentClass;
			Cur = (PC && PC->ClassGeneratedBy) ? Cast<UBlueprint>(PC->ClassGeneratedBy) : nullptr;
		}
		return nullptr;
	}

	// Return parent SCS_Node (same SCS) that currently owns InNode in its ChildNodes, or nullptr if root.
	USCS_Node* FindSCSParent(USimpleConstructionScript* SCS, USCS_Node* InNode)
	{
		if (!SCS || !InNode) return nullptr;
		TArray<USCS_Node*> All = SCS->GetAllNodes();
		for (USCS_Node* N : All)
		{
			if (N && N != InNode && N->GetChildNodes().Contains(InNode)) return N;
		}
		return nullptr;
	}
}
FString UTetherBlueprintLibrary::AddForeachNode(
	const FString& BlueprintPath, const FString& GraphName, bool bWithBreak, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	return TetherBpP1Impl::AddMacroNodeByName(BP, Graph,
		bWithBreak ? TEXT("ForEachLoopWithBreak") : TEXT("ForEachLoop"), X, Y);
}
FString UTetherBlueprintLibrary::AddForLoopNode(
	const FString& BlueprintPath, const FString& GraphName, bool bWithBreak, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	return TetherBpP1Impl::AddMacroNodeByName(BP, Graph,
		bWithBreak ? TEXT("ForLoopWithBreak") : TEXT("ForLoop"), X, Y);
}
FString UTetherBlueprintLibrary::AddWhileLoopNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	return TetherBpP1Impl::AddMacroNodeByName(BP, Graph, TEXT("WhileLoop"), X, Y);
}
FString UTetherBlueprintLibrary::AddSelectNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_Select* Node = TetherBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_Select>(Graph), X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddMakeLiteralNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& TypeString, const FString& Value, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();

	FString FnName;
	const FString T = TypeString.TrimStartAndEnd();
	if      (T.Equals(TEXT("Int"),    ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralInt");
	else if (T.Equals(TEXT("Int64"),  ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralInt64");
	else if (T.Equals(TEXT("Float"),  ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralDouble"); // UE5 float=double
	else if (T.Equals(TEXT("Double"), ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralDouble");
	else if (T.Equals(TEXT("Bool"),   ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralBool");
	else if (T.Equals(TEXT("Byte"),   ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralByte");
	else if (T.Equals(TEXT("Name"),   ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralName");
	else if (T.Equals(TEXT("String"), ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralString");
	else if (T.Equals(TEXT("Text"),   ESearchCase::IgnoreCase)) FnName = TEXT("MakeLiteralText");
	else return FString();

	UFunction* Fn = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(*FnName));
	if (!Fn) return FString();

	Graph->Modify(); BP->Modify();
	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	Node->FunctionReference.SetExternalMember(Fn->GetFName(), UKismetSystemLibrary::StaticClass());
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);

	// Set the Value pin default if provided.
	if (!Value.IsEmpty())
	{
		if (UEdGraphPin* ValuePin = Node->FindPin(TEXT("Value")))
		{
			GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*ValuePin, Value);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
namespace TetherBPCommentImpl
{
	/** Resolve the best-known rendered size for a node. Priority:
	 *   1. Live SGraphNode widget's GetDesiredSize (pixel-accurate; only
	 *      available when the graph has been opened + ticked in an editor).
	 *   2. Node->NodeWidth / NodeHeight (non-zero only for comment boxes
	 *      in normal UE — regular nodes don't populate these).
	 *   3. Flat 200×80 fallback (used only when the graph isn't rendered;
	 *      comment boxes will under-wrap wide Custom-Event / Break-Struct
	 *      nodes in that case — always call open_function_graph_for_render
	 *      first for an accurate frame).
	 *  Used by FitCommentToNodes. */
	static void BestNodeSize(UEdGraphNode* N, int32& OutW, int32& OutH)
	{
		OutW = 0; OutH = 0;
		if (!N) return;

		// 1. Try live Slate widget via the owning BP editor's graph panel.
		UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(N);
		if (BP && GEditor)
		{
			if (UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				if (IAssetEditorInstance* Inst = Sub->FindEditorForAsset(BP, false))
				{
					FBlueprintEditor* BPEd = static_cast<FBlueprintEditor*>(Inst);
					// OpenGraphAndBringToFront is idempotent when the graph
					// is already the focused tab; grabs the SGraphEditor
					// either way.
					if (TSharedPtr<SGraphEditor> GEd = BPEd->OpenGraphAndBringToFront(N->GetGraph()))
					{
						if (SGraphPanel* Panel = GEd->GetGraphPanel())
						{
							TSharedPtr<SGraphNode> NW = Panel->GetNodeWidgetFromGuid(N->NodeGuid);
							if (NW.IsValid())
							{
								const FVector2D Desired = FVector2D(NW->GetDesiredSize());
								if (Desired.X > 0) OutW = int32(Desired.X);
								if (Desired.Y > 0) OutH = int32(Desired.Y);
							}
						}
					}
				}
			}
		}
		// 2. Fall back to stored (comment boxes, resizable nodes).
		if (OutW <= 0 && N->NodeWidth  > 0) OutW = N->NodeWidth;
		if (OutH <= 0 && N->NodeHeight > 0) OutH = N->NodeHeight;
	}

	/** Compute the bounding box of the given nodes and size the comment to
	 *  enclose them with a standard padding + 32 px title strip on top.
	 *  Returns true if any guid resolved to a node. Uses BestNodeSize so
	 *  nodes with larger-than-fallback actual widths (Custom Events,
	 *  Break Structs, long labels) are framed without clipping. */
	static bool FitCommentToNodes(UEdGraphNode_Comment* Comment,
		const TArray<FString>& NodeGuids, UEdGraph* Graph)
	{
		if (!Comment || !Graph) return false;
		int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
		bool bHit = false;
		for (const FString& G : NodeGuids)
		{
			UEdGraphNode* N = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, G);
			if (!N) continue;
			int32 W = 0, H = 0;
			BestNodeSize(N, W, H);
			if (W <= 0) W = 200;
			if (H <= 0) H = 80;
			MinX = FMath::Min(MinX, N->NodePosX);
			MinY = FMath::Min(MinY, N->NodePosY);
			MaxX = FMath::Max(MaxX, N->NodePosX + W);
			MaxY = FMath::Max(MaxY, N->NodePosY + H);
			bHit = true;
		}
		if (!bHit) return false;
		const int32 Pad = 32;
		Comment->NodePosX  = MinX - Pad;
		Comment->NodePosY  = MinY - Pad - 32;
		Comment->NodeWidth  = (MaxX - MinX) + Pad * 2;
		Comment->NodeHeight = (MaxY - MinY) + Pad * 2 + 32;
		return true;
	}
}
FString UTetherBlueprintLibrary::AddCommentBox(
	const FString& BlueprintPath, const FString& GraphName,
	const TArray<FString>& NodeGuids, const FString& Text,
	int32 X, int32 Y, int32 Width, int32 Height)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();

	Graph->Modify(); BP->Modify();
	UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(Graph);
	Comment->CreateNewGuid();

	if (NodeGuids.Num() == 0 || !TetherBPCommentImpl::FitCommentToNodes(Comment, NodeGuids, Graph))
	{
		// No nodes given (or none resolved): use manual X/Y/W/H placement.
		Comment->NodePosX = X; Comment->NodePosY = Y;
		Comment->NodeWidth  = Width  > 0 ? Width  : 400;
		Comment->NodeHeight = Height > 0 ? Height : 200;
	}
	Graph->AddNode(Comment, /*bFromUI*/false, /*bSelectNewNode*/false);
	Comment->PostPlacedNewNode();
	// NodeComment must be assigned AFTER PostPlacedNewNode — that call resets it
	// to the localized default "Comment" string, clobbering the caller's text.
	Comment->NodeComment = Text;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Comment->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::WrapNodesInCommentBox(
	const FString& BlueprintPath, const FString& GraphName,
	const TArray<FString>& NodeGuids, const FString& Text)
{
	if (NodeGuids.Num() == 0) return FString();
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();

	Graph->Modify(); BP->Modify();
	UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(Graph);
	Comment->CreateNewGuid();
	if (!TetherBPCommentImpl::FitCommentToNodes(Comment, NodeGuids, Graph))
	{
		// None of the guids resolved — no box worth creating.
		Comment->MarkAsGarbage();
		return FString();
	}
	Graph->AddNode(Comment, /*bFromUI*/false, /*bSelectNewNode*/false);
	Comment->PostPlacedNewNode();
	Comment->NodeComment = Text;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Comment->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::UpdateCommentBox(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& CommentGuid, const TArray<FString>& NodeGuids,
	const FString& Text)
{
	if (NodeGuids.Num() == 0 && Text.IsEmpty()) return false;

	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, CommentGuid); if (!Node) return false;
	UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node); if (!Comment) return false;

	Graph->Modify(); BP->Modify(); Comment->Modify();
	bool bChanged = false;
	if (NodeGuids.Num() > 0)
	{
		if (TetherBPCommentImpl::FitCommentToNodes(Comment, NodeGuids, Graph)) bChanged = true;
	}
	if (!Text.IsEmpty())
	{
		Comment->NodeComment = Text;
		bChanged = true;
	}
	if (bChanged) FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return bChanged;
}
FString UTetherBlueprintLibrary::AddRerouteNode(
	const FString& BlueprintPath, const FString& GraphName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_Knot* Node = TetherBpP0Impl::FinalizeNewNode(Graph, NewObject<UK2Node_Knot>(Graph), X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::SetNodeEnabled(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& EnabledState)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid); if (!Node) return false;

	const FString S = EnabledState.TrimStartAndEnd();
	ENodeEnabledState NewState;
	if      (S.Equals(TEXT("Enabled"),         ESearchCase::IgnoreCase)) NewState = ENodeEnabledState::Enabled;
	else if (S.Equals(TEXT("Disabled"),        ESearchCase::IgnoreCase)) NewState = ENodeEnabledState::Disabled;
	else if (S.Equals(TEXT("DevelopmentOnly"), ESearchCase::IgnoreCase)) NewState = ENodeEnabledState::DevelopmentOnly;
	else return false;

	Node->Modify();
	Node->SetEnabledState(NewState, /*bUserAction*/true);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::ReparentBlueprint(
	const FString& BlueprintPath, const FString& NewParentPath)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UClass* NewParent = TetherBlueprintGraphWriteImpl::ResolveTargetClass(BP, NewParentPath);
	if (!NewParent) return false;
	if (NewParent == BP->ParentClass) return true;
	if (NewParent == BP->GeneratedClass) return false; // prevent self-parent

	BP->Modify();
	BP->ParentClass = NewParent;
	if (BP->SimpleConstructionScript) BP->SimpleConstructionScript->ValidateSceneRootNodes();
	FBlueprintEditorUtils::RefreshAllNodes(BP);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::SetBlueprintMetadata(
	const FString& BlueprintPath,
	const FString& DisplayName, const FString& Description,
	const FString& Category, const FString& Namespace)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	BP->Modify();
	if (!DisplayName.IsEmpty()) BP->BlueprintDisplayName = DisplayName;
	if (!Description.IsEmpty()) BP->BlueprintDescription = Description;
	if (!Category.IsEmpty())    BP->BlueprintCategory    = Category;
	if (!Namespace.IsEmpty())   BP->BlueprintNamespace   = Namespace;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
FString UTetherBlueprintLibrary::AddDispatcherEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& DispatcherName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	FMulticastDelegateProperty* Prop = TetherBpP0Impl::FindDispatcherProp(BP, DispatcherName);
	if (!Prop || !Prop->SignatureFunction) return FString();
	UFunction* Sig = Prop->SignatureFunction;

	Graph->Modify(); BP->Modify();
	UK2Node_CustomEvent* Node = NewObject<UK2Node_CustomEvent>(Graph);
	Node->CustomFunctionName = FBlueprintEditorUtils::FindUniqueKismetName(
		BP, FString::Printf(TEXT("On%s"), *DispatcherName));
	Node->bIsEditable = true;
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);

	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	for (TFieldIterator<FProperty> It(Sig); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
		FEdGraphPinType PinType;
		if (K2->ConvertPropertyToPinType(*It, PinType))
		{
			Node->CreateUserDefinedPin(It->GetFName(), PinType, EGPD_Output, /*bUseUniqueName*/false);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
namespace TetherBpP2Impl
{
	UScriptStruct* ResolveStruct(const FString& StructPath)
	{
		if (StructPath.IsEmpty()) return nullptr;
		if (UScriptStruct* S = FindObject<UScriptStruct>(nullptr, *StructPath)) return S;
		if (UScriptStruct* S = LoadObject<UScriptStruct>(nullptr, *StructPath)) return S;
		if (UScriptStruct* S = FindFirstObject<UScriptStruct>(*StructPath, EFindFirstObjectOptions::NativeFirst))
		{
			return S;
		}
		return nullptr;
	}

	UK2Node_CallFunction* AddKSLCall(UBlueprint* BP, UEdGraph* Graph, const TCHAR* FnName, int32 X, int32 Y)
	{
		UFunction* Fn = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(FnName));
		if (!Fn) return nullptr;
		UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
		Node->FunctionReference.SetExternalMember(Fn->GetFName(), UKismetSystemLibrary::StaticClass());
		TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
		return Node;
	}
}
FString UTetherBlueprintLibrary::AddDelayNode(
	const FString& BlueprintPath, const FString& GraphName,
	float DurationSeconds, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_CallFunction* Node = TetherBpP2Impl::AddKSLCall(BP, Graph, TEXT("Delay"), X, Y);
	if (!Node) return FString();
	if (UEdGraphPin* P = Node->FindPin(TEXT("Duration")))
	{
		GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*P, FString::SanitizeFloat(DurationSeconds));
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddSetTimerByFunctionNameNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& FunctionName, float TimeSeconds, bool bLooping, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_CallFunction* Node = TetherBpP2Impl::AddKSLCall(BP, Graph, TEXT("K2_SetTimer"), X, Y);
	if (!Node) return FString();
	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	if (UEdGraphPin* P = Node->FindPin(TEXT("FunctionName"))) K2->TrySetDefaultValue(*P, FunctionName);
	if (UEdGraphPin* P = Node->FindPin(TEXT("Time")))         K2->TrySetDefaultValue(*P, FString::SanitizeFloat(TimeSeconds));
	if (UEdGraphPin* P = Node->FindPin(TEXT("bLooping")))     K2->TrySetDefaultValue(*P, bLooping ? TEXT("true") : TEXT("false"));
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddSpawnActorFromClassNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& ActorClassPath, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UClass* SpawnClass = TetherBlueprintGraphWriteImpl::ResolveTargetClass(BP, ActorClassPath);
	if (!SpawnClass || !SpawnClass->IsChildOf(AActor::StaticClass())) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_SpawnActorFromClass* Node = NewObject<UK2Node_SpawnActorFromClass>(Graph);
	// SpawnActorFromClass::PostPlacedNewNode requires pins to exist (uses FindPinChecked),
	// so allocate pins first, add to graph, then post-place.
	Node->CreateNewGuid();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
	Node->AllocateDefaultPins();
	Node->PostPlacedNewNode();
	if (UEdGraphPin* ClassPin = Node->GetClassPin())
	{
		ClassPin->DefaultObject = SpawnClass;
		ClassPin->DefaultValue.Empty();
		// Trigger pin regeneration (exposed spawn vars) without a full reconstruct.
		Node->PinDefaultValueChanged(ClassPin);
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddMakeStructNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& StructPath, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UScriptStruct* S = TetherBpP2Impl::ResolveStruct(StructPath);
	// Allow native-make structs too by passing bForInternalUse=true (matches "advanced" UI path).
	if (!S || !UK2Node_MakeStruct::CanBeMade(S, /*bForInternalUse*/true)) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_MakeStruct* Node = NewObject<UK2Node_MakeStruct>(Graph);
	Node->StructType = S;
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddBreakStructNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& StructPath, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UScriptStruct* S = TetherBpP2Impl::ResolveStruct(StructPath);
	if (!S) return FString();
	// Note: UK2Node_BreakStruct::CanBeBroken is not DLL-exported; rely on compile-time validation instead.
	Graph->Modify(); BP->Modify();
	UK2Node_BreakStruct* Node = NewObject<UK2Node_BreakStruct>(Graph);
	Node->StructType = S;
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::CreateMacroGraph(
	const FString& BlueprintPath, const FString& MacroName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName MName(*MacroName);
	if (MName.IsNone()) return false;
	for (UEdGraph* G : BP->MacroGraphs) { if (G && G->GetFName() == MName) return false; }

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, MName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddMacroGraph(BP, NewGraph, /*bIsUserCreated*/true, /*SignatureFromClass*/nullptr);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
FString UTetherBlueprintLibrary::AddTimelineNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& TimelineTemplateName, int32 X, int32 Y)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	if (!FBlueprintEditorUtils::DoesSupportTimelines(BP)) return FString();

	FName TLName;
	if (TimelineTemplateName.IsEmpty())
	{
		TLName = FBlueprintEditorUtils::FindUniqueTimelineName(BP);
	}
	else
	{
		TLName = FName(*TimelineTemplateName);
	}

	Graph->Modify(); BP->Modify();

	UTimelineTemplate* Template = nullptr;
	const int32 ExistingIdx = FBlueprintEditorUtils::FindTimelineIndex(BP, TLName);
	if (ExistingIdx != INDEX_NONE)
	{
		Template = BP->Timelines[ExistingIdx];
	}
	else
	{
		Template = FBlueprintEditorUtils::AddNewTimeline(BP, TLName);
	}
	if (!Template) return FString();

	UK2Node_Timeline* Node = NewObject<UK2Node_Timeline>(Graph);
	Node->TimelineName = TLName;
	Node->TimelineGuid = Template->TimelineGuid;
	Node->bAutoPlay = Template->bAutoPlay;
	Node->bLoop = Template->bLoop;
	Node->bReplicated = Template->bReplicated;
	Node->bIgnoreTimeDilation = Template->bIgnoreTimeDilation;
	TetherBpP0Impl::FinalizeNewNode(Graph, Node, X, Y);

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::SetTimelineProperties(
	const FString& BlueprintPath, const FString& TimelineName,
	float Length, bool bAutoPlay, bool bLoop, bool bReplicated, bool bIgnoreTimeDilation)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	if (TimelineName.IsEmpty()) return false;
	const FName TLName(*TimelineName);
	const int32 Idx = FBlueprintEditorUtils::FindTimelineIndex(BP, TLName);
	if (Idx == INDEX_NONE) return false;
	UTimelineTemplate* Template = BP->Timelines[Idx];
	if (!Template) return false;

	BP->Modify();
	Template->Modify();
	if (Length >= 0.0f)
	{
		Template->TimelineLength = Length;
	}
	Template->bAutoPlay = bAutoPlay;
	Template->bLoop = bLoop;
	Template->bReplicated = bReplicated;
	Template->bIgnoreTimeDilation = bIgnoreTimeDilation;

	// Mirror into the K2Node_Timeline instance so the graph node reflects the new settings.
	if (UK2Node_Timeline* TLNode = FBlueprintEditorUtils::FindNodeForTimeline(BP, Template))
	{
		TLNode->Modify();
		TLNode->bAutoPlay = bAutoPlay;
		TLNode->bLoop = bLoop;
		TLNode->bReplicated = bReplicated;
		TLNode->bIgnoreTimeDilation = bIgnoreTimeDilation;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::RemoveMacroGraph(
	const FString& BlueprintPath, const FString& MacroName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	const FName MName(*MacroName);
	if (MName.IsNone()) return false;
	UEdGraph* Target = nullptr;
	for (UEdGraph* G : BP->MacroGraphs)
	{
		if (G && G->GetFName() == MName) { Target = G; break; }
	}
	if (!Target) return false;
	BP->Modify();
	TetherBpP0Impl::CloseOpenGraphTabs(BP, Target);
	FBlueprintEditorUtils::RemoveGraph(BP, Target, EGraphRemoveFlags::Recompile);
	return true;
}
bool UTetherBlueprintLibrary::SetNodeComment(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& Comment, bool bCommentBubbleVisible)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	Node->Modify();
	Node->NodeComment = Comment;
	Node->bCommentBubbleVisible = bCommentBubbleVisible && !Comment.IsEmpty();
	Node->bCommentBubblePinned = bCommentBubbleVisible && !Comment.IsEmpty();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
FString UTetherBlueprintLibrary::DuplicateGraphNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();
	UEdGraphNode* Src = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Src) return FString();

	Graph->Modify();
	BP->Modify();

	UEdGraphNode* Dup = DuplicateObject<UEdGraphNode>(Src, Graph);
	if (!Dup) return FString();
	Dup->CreateNewGuid();
	Dup->NodePosX = NodePosX;
	Dup->NodePosY = NodePosY;
	// Break any links that the duplicate inherited from Src.
	for (UEdGraphPin* Pin : Dup->Pins)
	{
		if (Pin)
		{
			Pin->BreakAllPinLinks();
		}
	}
	Graph->AddNode(Dup, /*bFromUI*/false, /*bSelectNewNode*/false);
	Dup->PostPlacedNewNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Dup->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::DisconnectGraphPin(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& PinName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;
	UEdGraphPin* Pin = Node->FindPin(PinName);
	if (!Pin) return false;
	if (Pin->LinkedTo.Num() == 0) return false;

	Graph->Modify();
	BP->Modify();
	Pin->BreakAllPinLinks();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
FString UTetherBlueprintLibrary::AddMakeArrayNode(
	const FString& BlueprintPath, const FString& GraphName,
	int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();

	Graph->Modify();
	BP->Modify();

	UK2Node_MakeArray* Node = NewObject<UK2Node_MakeArray>(Graph);
	Node->CreateNewGuid();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddEnumLiteralNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& EnumPath, const FString& ValueName, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return FString();

	UEnum* EnumObj = FindObject<UEnum>(nullptr, *EnumPath);
	if (!EnumObj) EnumObj = LoadObject<UEnum>(nullptr, *EnumPath);
	if (!EnumObj) return FString();

	// Resolve the chosen entry. Enum entries are stored as "EnumName::EntryName" — match on short name.
	FString ResolvedEntry;
	const int32 NumEntries = EnumObj->NumEnums();
	if (NumEntries <= 1) return FString(); // only the hidden _MAX sentinel
	if (ValueName.IsEmpty())
	{
		ResolvedEntry = EnumObj->GetNameStringByIndex(0);
	}
	else
	{
		for (int32 i = 0; i < NumEntries; ++i)
		{
			const FString ShortName = EnumObj->GetNameStringByIndex(i);
			if (ShortName == ValueName || EnumObj->GetNameByIndex(i).ToString() == ValueName)
			{
				ResolvedEntry = ShortName;
				break;
			}
		}
	}
	if (ResolvedEntry.IsEmpty()) return FString();

	Graph->Modify();
	BP->Modify();

	UK2Node_EnumLiteral* Node = NewObject<UK2Node_EnumLiteral>(Graph);
	Node->Enum = EnumObj;
	Node->CreateNewGuid();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, false, false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	// Default the Enum input pin to the chosen entry.
	if (UEdGraphPin* EnumPin = Node->FindPin(UK2Node_EnumLiteral::GetEnumInputPinName()))
	{
		EnumPin->DefaultValue = ResolvedEntry;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
namespace TetherBPActionDBImpl
{
	/** Resolve the spawner's owning class/asset path for filtering + reporting.
	 *  Function spawners report the function's owning class; variable spawners
	 *  the variable's owner; for everything else we use the action-DB key
	 *  (typically the K2Node class itself or the asset that registered it). */
	static void ResolveOwner(const UBlueprintNodeSpawner* Spawner, UObject* RegistryKey,
		FString& OutOwnerName, FString& OutOwnerPath)
	{
		if (const UBlueprintFunctionNodeSpawner* FS = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
		{
			if (const UFunction* Fn = FS->GetFunction())
			{
				if (UClass* OwnerCls = Fn->GetOwnerClass())
				{
					OutOwnerName = OwnerCls->GetName();
					OutOwnerPath = OwnerCls->GetPathName();
					return;
				}
			}
		}
		if (RegistryKey)
		{
			OutOwnerName = RegistryKey->GetName();
			OutOwnerPath = RegistryKey->GetPathName();
		}
	}

	/** Build a UI signature for a spawner without touching the template-node
	 *  cache. PrimeDefaultUiSpec() is unsafe to call in a loop because it
	 *  spawns a transient template node and asserts when the spawner's
	 *  NodeClass has no schema-compatible cached graph (e.g. AnimGraph,
	 *  SoundCue, Niagara nodes registered alongside K2 ones).
	 *
	 *  Strategy:
	 *    1. Use DefaultMenuSignature if a registrar already populated it.
	 *    2. Otherwise synthesise from the spawner subtype:
	 *       - Function spawners → reflect on the UFunction (DisplayName,
	 *         Category, Tooltip, Keywords metadata).
	 *       - Anything else → use NodeClass display name as the title and
	 *         leave the rest blank. Caller can still spawn by Key.
	 */
	static FBlueprintActionUiSpec BuildUiSpecSafe(const UBlueprintNodeSpawner* Spawner)
	{
		FBlueprintActionUiSpec Out = Spawner->DefaultMenuSignature;
		if (!Out.MenuName.IsEmpty())
		{
			return Out;
		}

		if (const UBlueprintFunctionNodeSpawner* FS = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
		{
			if (const UFunction* Fn = FS->GetFunction())
			{
				const FString DisplayName = Fn->HasMetaData(TEXT("DisplayName"))
					? Fn->GetMetaData(TEXT("DisplayName")) : Fn->GetName();
				Out.MenuName = FText::FromString(DisplayName);
				Out.Category = FText::FromString(Fn->GetMetaData(TEXT("Category")));
				Out.Tooltip  = Fn->GetToolTipText();
				Out.Keywords = FText::FromString(Fn->GetMetaData(TEXT("Keywords")));
				return Out;
			}
		}

		if (Spawner->NodeClass)
		{
			Out.MenuName = Spawner->NodeClass->GetDisplayNameText();
		}
		return Out;
	}
}
TArray<FTetherSpawnableAction> UTetherBlueprintLibrary::ListSpawnableActions(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& Keyword, const FString& CategoryContains,
	const FString& OwningClassPath, const FString& NodeType,
	int32 MaxResults)
{
	TArray<FTetherSpawnableAction> Out;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return Out;

	const int32 Cap = FMath::Max(1, MaxResults);

	FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
	// Make sure asset-defined actions for this BP are present (custom events,
	// user functions etc.). Cheap if already cached.
	Database.RefreshAssetActions(BP);
	const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();

	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Pair : Registry)
	{
		UObject* RegistryKey = Pair.Key.ResolveObjectPtr();

		// Per-key OwningClassPath fast-path: skip the whole bucket when the
		// filter is set and the bucket's key doesn't match. (Function spawners
		// override the owner with the function's owning class, so we only
		// short-circuit here when both the bucket key and any function-owner
		// would also miss — to stay correct, just compare on RegistryKey.)
		if (!OwningClassPath.IsEmpty() && RegistryKey
			&& RegistryKey->GetPathName() != OwningClassPath)
		{
			// Function spawners may still match via their function owner;
			// fall through and let the per-spawner check decide.
		}

		for (UBlueprintNodeSpawner* Spawner : Pair.Value)
		{
			if (!Spawner || !Spawner->NodeClass) continue;

			// Skip non-K2 spawners (anim/niagara/sound graph nodes etc.).
			// They share the database but their template-node lookup asserts
			// when probed against a K2 graph; keeping them out also prevents
			// the resulting Spawn from ever succeeding on a BP graph.
			if (!Spawner->NodeClass->IsChildOf(UK2Node::StaticClass())) continue;

			// NodeType filter
			if (!NodeType.IsEmpty() && Spawner->NodeClass->GetName() != NodeType) continue;

			// Owner resolution + OwningClassPath filter
			FString OwnerName, OwnerPath;
			TetherBPActionDBImpl::ResolveOwner(Spawner, RegistryKey, OwnerName, OwnerPath);
			if (!OwningClassPath.IsEmpty() && OwnerPath != OwningClassPath) continue;

			// Pull the UI signature without touching the template-node cache.
			const FBlueprintActionUiSpec UiSpec = TetherBPActionDBImpl::BuildUiSpecSafe(Spawner);

			const FString TitleStr    = UiSpec.MenuName.ToString();
			const FString CategoryStr = UiSpec.Category.ToString();
			const FString TooltipStr  = UiSpec.Tooltip.ToString();
			const FString KeywordsStr = UiSpec.Keywords.ToString();

			if (!CategoryContains.IsEmpty()
				&& !CategoryStr.Contains(CategoryContains, ESearchCase::IgnoreCase))
			{
				continue;
			}

			if (!Keyword.IsEmpty())
			{
				const bool bHit =
					   TitleStr.Contains(Keyword,    ESearchCase::IgnoreCase)
					|| TooltipStr.Contains(Keyword,  ESearchCase::IgnoreCase)
					|| KeywordsStr.Contains(Keyword, ESearchCase::IgnoreCase)
					|| CategoryStr.Contains(Keyword, ESearchCase::IgnoreCase);
				if (!bHit) continue;
			}

			FTetherSpawnableAction Action;
			Action.Key             = Spawner->GetSpawnerSignature().ToString();
			Action.Title           = TitleStr;
			Action.Category        = CategoryStr;
			Action.Tooltip         = TooltipStr;
			Action.Keywords        = KeywordsStr;
			Action.NodeType        = Spawner->NodeClass ? Spawner->NodeClass->GetName() : FString();
			Action.OwningClass     = OwnerName;
			Action.OwningClassPath = OwnerPath;
			Out.Add(MoveTemp(Action));

			if (Out.Num() >= Cap) return Out;
		}
	}
	return Out;
}
FString UTetherBlueprintLibrary::SpawnNodeByActionKey(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& ActionKey, int32 NodePosX, int32 NodePosY)
{
	if (ActionKey.IsEmpty()) return FString();
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
	Database.RefreshAssetActions(BP);
	const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();

	UBlueprintNodeSpawner* Found = nullptr;
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Pair : Registry)
	{
		for (UBlueprintNodeSpawner* Spawner : Pair.Value)
		{
			if (!Spawner || !Spawner->NodeClass) continue;
			if (!Spawner->NodeClass->IsChildOf(UK2Node::StaticClass())) continue;
			if (Spawner->GetSpawnerSignature().ToString() == ActionKey)
			{
				Found = Spawner;
				break;
			}
		}
		if (Found) break;
	}
	if (!Found) return FString();

	Graph->Modify();
	BP->Modify();

	IBlueprintNodeBinder::FBindingSet Bindings;
	UEdGraphNode* NewNode = Found->Invoke(Graph, Bindings,
		FVector2D(static_cast<float>(NodePosX), static_cast<float>(NodePosY)));
	if (!NewNode) return FString();

	// Spawner::Invoke usually adds + allocates pins itself, but a few spawners
	// rely on the schema's PostPlacedNewNode hook — call it defensively.
	NewNode->NodePosX = NodePosX;
	NewNode->NodePosY = NodePosY;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return NewNode->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::DisconnectPinLink(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& SourceNodeGuid, const FString& SourcePinName,
	const FString& TargetNodeGuid, const FString& TargetPinName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* SrcNode = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, SourceNodeGuid);
	UEdGraphNode* DstNode = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, TargetNodeGuid);
	if (!SrcNode || !DstNode) return false;

	UEdGraphPin* SrcPin = SrcNode->FindPin(SourcePinName);
	UEdGraphPin* DstPin = DstNode->FindPin(TargetPinName);
	if (!SrcPin || !DstPin) return false;

	if (!SrcPin->LinkedTo.Contains(DstPin)) return false;

	Graph->Modify();
	BP->Modify();

	// BreakLinkTo is symmetric — removes the edge from both pins' LinkedTo lists.
	SrcPin->BreakLinkTo(DstPin);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
namespace TetherBPLintImpl
{
	struct FLintCtx
	{
		UBlueprint* BP = nullptr;
		TArray<FTetherLintIssue>* Out = nullptr;
		int32 OversizedFnThreshold = 20;
		int32 LongExecChainThreshold = 15;
		int32 LargeGraphThreshold = 10;
	};

	static void Emit(FLintCtx& C, const FString& Severity, const FString& Code,
		const FString& Message, const FString& GraphName,
		const FString& NodeGuid, const FString& VarName, const FString& FnName)
	{
		FTetherLintIssue I;
		I.Severity = Severity;
		I.Code = Code;
		I.Message = Message;
		I.GraphName = GraphName;
		I.NodeGuid = NodeGuid;
		I.VariableName = VarName;
		I.FunctionName = FnName;
		C.Out->Add(I);
	}

	/** True if every pin on the node has zero links and zero default pin-object. */
	static bool IsFullyOrphan(const UEdGraphNode* Node)
	{
		if (!Node) return false;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			if (Pin->LinkedTo.Num() > 0) return false;
		}
		return true;
	}

	/** Heuristic: name matches default UE naming for new custom events. */
	static bool LooksDefaultCustomEventName(const FString& Name)
	{
		// e.g. "CustomEvent", "CustomEvent_0", "Event", "Event_0"
		if (Name.Equals(TEXT("CustomEvent"), ESearchCase::IgnoreCase)) return true;
		if (Name.Equals(TEXT("Event"),       ESearchCase::IgnoreCase)) return true;
		for (const TCHAR* Prefix : { TEXT("CustomEvent_"), TEXT("Event_") })
		{
			if (Name.StartsWith(Prefix))
			{
				const FString Tail = Name.RightChop(FCString::Strlen(Prefix));
				if (!Tail.IsEmpty() && Tail.IsNumeric()) return true;
			}
		}
		return false;
	}

	static bool LooksDefaultFunctionName(const FString& Name)
	{
		if (Name.Equals(TEXT("NewFunction"), ESearchCase::IgnoreCase)) return true;
		if (Name.StartsWith(TEXT("NewFunction_")))
		{
			const FString Tail = Name.RightChop(12);
			if (!Tail.IsEmpty() && Tail.IsNumeric()) return true;
		}
		return false;
	}

	/** Count non-comment, non-ghost nodes. */
	static int32 CountRealNodes(const UEdGraph* Graph)
	{
		int32 Count = 0;
		if (!Graph) return 0;
		for (const UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			if (N->IsA<UEdGraphNode_Comment>()) continue;
			Count += 1;
		}
		return Count;
	}

	static int32 CountCommentBoxes(const UEdGraph* Graph)
	{
		int32 Count = 0;
		if (!Graph) return 0;
		for (const UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->IsA<UEdGraphNode_Comment>()) Count += 1;
		}
		return Count;
	}

	/** Find the longest linear exec chain starting from any node. "Linear"
	 *  means each successor has exactly 1 exec-input link AND the upstream
	 *  emits exactly 1 exec-output link — i.e. no branching, no merging. */
	static int32 LongestLinearExecChain(UEdGraph* Graph)
	{
		if (!Graph) return 0;
		int32 Best = 0;
		TMap<UEdGraphNode*, int32> Memo;
		TFunction<int32(UEdGraphNode*)> Walk = [&](UEdGraphNode* N) -> int32
		{
			if (!N) return 0;
			if (int32* M = Memo.Find(N)) return *M;
			Memo.Add(N, 1);  // break cycles by reserving

			int32 MyBest = 1;
			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin) continue;
				if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				if (Pin->Direction != EGPD_Output) continue;
				if (Pin->LinkedTo.Num() != 1) continue;  // branching → not linear
				UEdGraphPin* DownIn = Pin->LinkedTo[0];
				if (!DownIn || DownIn->LinkedTo.Num() != 1) continue;  // merging
				UEdGraphNode* Next = DownIn->GetOwningNode();
				if (!Next || Next == N) continue;
				MyBest = FMath::Max(MyBest, 1 + Walk(Next));
			}
			Memo.Add(N, MyBest);  // overwrite placeholder
			return MyBest;
		};
		for (UEdGraphNode* N : Graph->Nodes)
		{
			Best = FMath::Max(Best, Walk(N));
		}
		return Best;
	}

	// ─── Individual checks ────────────────────────────────

	static void CheckOrphanNodes(FLintCtx& C)
	{
		TArray<UEdGraph*> Graphs;
		for (UEdGraph* G : C.BP->UbergraphPages)  Graphs.Add(G);
		for (UEdGraph* G : C.BP->FunctionGraphs)  Graphs.Add(G);
		for (UEdGraph* G : C.BP->MacroGraphs)     Graphs.Add(G);

		for (UEdGraph* G : Graphs)
		{
			if (!G) continue;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (!N) continue;
				if (N->IsA<UEdGraphNode_Comment>()) continue;
				if (N->IsA<UK2Node_FunctionEntry>()) continue;
				if (N->IsA<UK2Node_FunctionResult>()) continue;

				// "Tunnel" nodes on macro graphs are entry/exit — always orphan-ish.
				if (N->GetClass()->GetName().Contains(TEXT("Tunnel"))) continue;

				if (!IsFullyOrphan(N)) continue;

				// Events with no "then" connection ARE dead code and worth flagging.
				const FString Msg = FString::Printf(
					TEXT("Node '%s' (%s) has no connections — dead code or left-over spawn"),
					*N->GetNodeTitle(ENodeTitleType::ListView).ToString(),
					*N->GetClass()->GetName());
				Emit(C, TEXT("warning"), TEXT("OrphanNode"), Msg,
					G->GetName(), N->NodeGuid.ToString(EGuidFormats::Digits),
					FString(), FString());
			}
		}
	}

	static void CheckOversizedFunctions(FLintCtx& C)
	{
		for (UEdGraph* G : C.BP->FunctionGraphs)
		{
			if (!G) continue;
			const int32 N = CountRealNodes(G);
			if (N <= C.OversizedFnThreshold) continue;
			const FString Msg = FString::Printf(
				TEXT("Function '%s' has %d nodes (threshold %d) — consider extracting sub-functions"),
				*G->GetName(), N, C.OversizedFnThreshold);
			Emit(C, TEXT("warning"), TEXT("OversizedFunction"), Msg,
				G->GetName(), FString(), FString(), G->GetName());
		}
	}

	static void CheckUnnamedCustomEvents(FLintCtx& C)
	{
		for (UEdGraph* G : C.BP->UbergraphPages)
		{
			if (!G) continue;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(N))
				{
					if (LooksDefaultCustomEventName(CE->CustomFunctionName.ToString()))
					{
						const FString Msg = FString::Printf(
							TEXT("CustomEvent '%s' still uses a default name — rename to describe intent"),
							*CE->CustomFunctionName.ToString());
						Emit(C, TEXT("warning"), TEXT("UnnamedCustomEvent"), Msg,
							G->GetName(), CE->NodeGuid.ToString(EGuidFormats::Digits),
							FString(), FString());
					}
				}
			}
		}
	}

	static void CheckUnnamedFunctions(FLintCtx& C)
	{
		for (UEdGraph* G : C.BP->FunctionGraphs)
		{
			if (!G) continue;
			if (LooksDefaultFunctionName(G->GetName()))
			{
				const FString Msg = FString::Printf(
					TEXT("Function graph '%s' still uses the default name — rename to describe intent"),
					*G->GetName());
				Emit(C, TEXT("warning"), TEXT("UnnamedFunction"), Msg,
					G->GetName(), FString(), FString(), G->GetName());
			}
		}
	}

	static void CheckVariableMetadata(FLintCtx& C)
	{
		for (const FBPVariableDescription& V : C.BP->NewVariables)
		{
			const bool bEditable = (V.PropertyFlags & CPF_Edit) != 0;
			if (!bEditable) continue;

			const FString Cat = V.Category.ToString();
			const bool bDefaultCat = Cat.IsEmpty()
			                       || Cat.Equals(TEXT("Default"), ESearchCase::IgnoreCase)
			                       || Cat.Equals(V.VarName.ToString(), ESearchCase::IgnoreCase);
			if (bDefaultCat)
			{
				const FString Msg = FString::Printf(
					TEXT("Editable variable '%s' lacks a custom Category — set one to group it in Details"),
					*V.VarName.ToString());
				Emit(C, TEXT("info"), TEXT("InstanceEditableNoCategory"), Msg,
					FString(), FString(), V.VarName.ToString(), FString());
			}
			const FString Tip = V.HasMetaData(TEXT("tooltip"))
				? V.GetMetaData(TEXT("tooltip")) : FString();
			if (Tip.IsEmpty())
			{
				const FString Msg = FString::Printf(
					TEXT("Editable variable '%s' has no tooltip — designers won't know what it does"),
					*V.VarName.ToString());
				Emit(C, TEXT("info"), TEXT("InstanceEditableNoTooltip"), Msg,
					FString(), FString(), V.VarName.ToString(), FString());
			}
		}
	}

	/** Walk every graph, count K2Node_Variable references to VarName. */
	static int32 CountVariableRefs(UBlueprint* BP, const FName& VarName, UEdGraph* RestrictToGraph = nullptr)
	{
		int32 Count = 0;
		TArray<UEdGraph*> Graphs;
		if (RestrictToGraph) { Graphs.Add(RestrictToGraph); }
		else
		{
			for (UEdGraph* G : BP->UbergraphPages) Graphs.Add(G);
			for (UEdGraph* G : BP->FunctionGraphs) Graphs.Add(G);
			for (UEdGraph* G : BP->MacroGraphs)    Graphs.Add(G);
		}
		for (UEdGraph* G : Graphs)
		{
			if (!G) continue;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_Variable* V = Cast<UK2Node_Variable>(N))
				{
					if (V->VariableReference.GetMemberName() == VarName) Count += 1;
				}
			}
		}
		return Count;
	}

	static void CheckUnusedClassVariables(FLintCtx& C)
	{
		for (const FBPVariableDescription& V : C.BP->NewVariables)
		{
			const int32 Refs = CountVariableRefs(C.BP, V.VarName);
			if (Refs > 0) continue;
			// Skip editable vars — they're a legitimate external API even with 0 internal refs.
			if ((V.PropertyFlags & CPF_Edit) != 0) continue;
			const FString Msg = FString::Printf(
				TEXT("Variable '%s' is never read or written inside this Blueprint"),
				*V.VarName.ToString());
			Emit(C, TEXT("info"), TEXT("UnusedVariable"), Msg,
				FString(), FString(), V.VarName.ToString(), FString());
		}
	}

	static void CheckUnusedLocalVariables(FLintCtx& C)
	{
		for (UEdGraph* G : C.BP->FunctionGraphs)
		{
			if (!G) continue;
			UK2Node_FunctionEntry* Entry = nullptr;
			for (UEdGraphNode* N : G->Nodes)
			{
				if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N)) { Entry = E; break; }
			}
			if (!Entry) continue;
			for (const FBPVariableDescription& V : Entry->LocalVariables)
			{
				const int32 Refs = CountVariableRefs(C.BP, V.VarName, G);
				if (Refs > 0) continue;
				const FString Msg = FString::Printf(
					TEXT("Local variable '%s' in function '%s' is never read or written"),
					*V.VarName.ToString(), *G->GetName());
				Emit(C, TEXT("info"), TEXT("UnusedLocalVariable"), Msg,
					G->GetName(), FString(), V.VarName.ToString(), G->GetName());
			}
		}
	}

	static void CheckLargeUncommentedGraphs(FLintCtx& C)
	{
		auto Check = [&](UEdGraph* G)
		{
			if (!G) return;
			const int32 NReal = CountRealNodes(G);
			if (NReal < C.LargeGraphThreshold) return;
			if (CountCommentBoxes(G) > 0) return;
			const FString Msg = FString::Printf(
				TEXT("Graph '%s' has %d nodes but no comment boxes — add section labels to orient readers"),
				*G->GetName(), NReal);
			Emit(C, TEXT("info"), TEXT("LargeUncommentedGraph"), Msg,
				G->GetName(), FString(), FString(), FString());
		};
		for (UEdGraph* G : C.BP->UbergraphPages) Check(G);
		for (UEdGraph* G : C.BP->FunctionGraphs) Check(G);
	}

	static void CheckLongExecChains(FLintCtx& C)
	{
		auto Check = [&](UEdGraph* G)
		{
			if (!G) return;
			const int32 Len = LongestLinearExecChain(G);
			if (Len < C.LongExecChainThreshold) return;
			const FString Msg = FString::Printf(
				TEXT("Graph '%s' has a linear exec chain of %d nodes — consider extracting a function"),
				*G->GetName(), Len);
			Emit(C, TEXT("warning"), TEXT("LongExecChain"), Msg,
				G->GetName(), FString(), FString(), FString());
		};
		for (UEdGraph* G : C.BP->UbergraphPages) Check(G);
		for (UEdGraph* G : C.BP->FunctionGraphs) Check(G);
	}
}
TArray<FTetherLintIssue> UTetherBlueprintLibrary::LintBlueprint(
	const FString& BlueprintPath, const FString& SeverityFilter,
	int32 OversizedFunctionThreshold, int32 LongExecChainThreshold,
	int32 LargeGraphThreshold)
{
	using namespace TetherBPLintImpl;
	TArray<FTetherLintIssue> Issues;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Issues;

	FLintCtx C;
	C.BP = BP;
	C.Out = &Issues;
	C.OversizedFnThreshold   = OversizedFunctionThreshold > 0 ? OversizedFunctionThreshold : 20;
	C.LongExecChainThreshold = LongExecChainThreshold   > 0 ? LongExecChainThreshold   : 15;
	C.LargeGraphThreshold    = LargeGraphThreshold      > 0 ? LargeGraphThreshold      : 10;

	CheckOrphanNodes(C);
	CheckOversizedFunctions(C);
	CheckUnnamedCustomEvents(C);
	CheckUnnamedFunctions(C);
	CheckVariableMetadata(C);
	CheckUnusedClassVariables(C);
	CheckUnusedLocalVariables(C);
	CheckLargeUncommentedGraphs(C);
	CheckLongExecChains(C);

	if (!SeverityFilter.IsEmpty())
	{
		Issues.RemoveAll([&](const FTetherLintIssue& I)
		{
			return !I.Severity.Equals(SeverityFilter, ESearchCase::IgnoreCase);
		});
	}
	return Issues;
}
namespace TetherBPCollapseImpl
{
	/** Resolve a node GUID in a graph. */
	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& GuidStr)
	{
		if (!Graph) return nullptr;
		FGuid Guid;
		if (!FGuid::Parse(GuidStr, Guid)) return nullptr;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (N && N->NodeGuid == Guid) return N;
		}
		return nullptr;
	}

	/** Sort pins for stable user-facing order: inputs first by Y, outputs by Y. */
	static void SortPinsByPosition(TArray<UEdGraphPin*>& Pins)
	{
		Pins.Sort([](const UEdGraphPin& A, const UEdGraphPin& B)
		{
			const UEdGraphNode* NA = A.GetOwningNode();
			const UEdGraphNode* NB = B.GetOwningNode();
			if (A.Direction != B.Direction) return A.Direction == EGPD_Input;
			if (!NA || !NB) return false;
			if (NA->NodePosY != NB->NodePosY) return NA->NodePosY < NB->NodePosY;
			return NA->NodePosX < NB->NodePosX;
		});
	}
}
FString UTetherBlueprintLibrary::CollapseNodesToFunction(
	const FString& BlueprintPath, const FString& SourceGraphName,
	const TArray<FString>& NodeGuids, const FString& NewFunctionName,
	FString& OutNewGraphName)
{
	using namespace TetherBPCollapseImpl;
	using namespace TetherBlueprintGraphWriteImpl;

	OutNewGraphName.Empty();
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();

	UEdGraph* SourceGraph = FindGraphByName(BP, SourceGraphName);
	if (!SourceGraph) return FString();

	// Resolve selection; reject entry/result/tunnel/orphaned-event scaffolding.
	TSet<UEdGraphNode*> Selection;
	int32 MinX = INT32_MAX, MinY = INT32_MAX, MaxX = INT32_MIN, MaxY = INT32_MIN;
	int32 SumX = 0, SumY = 0;
	for (const FString& GuidStr : NodeGuids)
	{
		UEdGraphNode* N = TetherBPCollapseImpl::FindNodeByGuid(SourceGraph, GuidStr);
		if (!N) return FString();
		if (N->IsA<UK2Node_FunctionEntry>() || N->IsA<UK2Node_FunctionResult>()) return FString();
		if (N->GetClass()->GetName().Contains(TEXT("Tunnel"))) return FString();
		Selection.Add(N);
		SumX += N->NodePosX; SumY += N->NodePosY;
		MinX = FMath::Min(MinX, N->NodePosX); MinY = FMath::Min(MinY, N->NodePosY);
		MaxX = FMath::Max(MaxX, N->NodePosX); MaxY = FMath::Max(MaxY, N->NodePosY);
	}
	if (Selection.Num() == 0) return FString();

	// Create the new function graph.
	const FName BaseName = FBlueprintEditorUtils::FindUniqueKismetName(
		BP, NewFunctionName.IsEmpty() ? TEXT("ExtractedFunction") : NewFunctionName);
	UEdGraph* DestGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, BaseName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!DestGraph) return FString();
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, DestGraph, /*bIsUserCreated*/true, nullptr);

	// Fetch the auto-created entry; result is created on demand below.
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* N : DestGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N)) { EntryNode = E; break; }
	}
	if (!EntryNode) return FString();

	FGraphNodeCreator<UK2Node_FunctionResult> ResultNodeCreator(*DestGraph);
	UK2Node_FunctionResult* ResultNode = ResultNodeCreator.CreateNode();
	ResultNode->NodePosX = EntryNode->NodePosX + 300;
	ResultNode->NodePosY = EntryNode->NodePosY;
	ResultNodeCreator.Finalize();

	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();

	// Spawn the gateway CallFunction node in the source graph. We set it up
	// against the skeleton's new UFunction after moving nodes (the function
	// shape depends on the boundary pins we discover below).
	UK2Node_CallFunction* Gateway = NewObject<UK2Node_CallFunction>(SourceGraph);
	Gateway->CreateNewGuid();
	const int32 CenterX = Selection.Num() > 0 ? (SumX / Selection.Num()) : 0;
	const int32 CenterY = Selection.Num() > 0 ? (SumY / Selection.Num()) : 0;
	Gateway->NodePosX = CenterX;
	Gateway->NodePosY = CenterY;
	SourceGraph->AddNode(Gateway, false, false);

	// Point the gateway at the skeleton's freshly-created function so it can
	// materialise default exec pins. AddFunctionGraph registers the skeleton
	// signature immediately — FindFunctionByName should succeed.
	UClass* SkelClass = BP->SkeletonGeneratedClass ? BP->SkeletonGeneratedClass : BP->GeneratedClass;
	UFunction* SkelFn = SkelClass ? SkelClass->FindFunctionByName(BaseName) : nullptr;
	if (SkelFn)
	{
		Gateway->SetFromFunction(SkelFn);
	}
	Gateway->PostPlacedNewNode();
	Gateway->AllocateDefaultPins();

	// Move the nodes and collect boundary pins in the process.
	TArray<UEdGraphPin*> GatewayPins;  // pins on the *moved* nodes that cross the boundary
	SourceGraph->Modify();
	DestGraph->Modify();
	BP->Modify();

	for (UEdGraphNode* N : Selection)
	{
		N->Modify();
		SourceGraph->Nodes.Remove(N);
		DestGraph->Nodes.Add(N);
		N->Rename(nullptr, DestGraph);

		for (UEdGraphPin* P : N->Pins)
		{
			if (!P || P->LinkedTo.Num() == 0) continue;
			bool bCrosses = false;
			for (UEdGraphPin* Linked : P->LinkedTo)
			{
				if (!Linked) continue;
				if (!Selection.Contains(Linked->GetOwningNode())) { bCrosses = true; break; }
			}
			if (bCrosses) GatewayPins.Add(P);
		}
	}
	SortPinsByPosition(GatewayPins);

	bool bDiscardResult = true;

	// Thunk each boundary pin through entry/result + gateway.
	for (UEdGraphPin* LocalPin : GatewayPins)
	{
		UK2Node_EditablePinBase* LocalPort = (LocalPin->Direction == EGPD_Input) ? (UK2Node_EditablePinBase*)EntryNode : (UK2Node_EditablePinBase*)ResultNode;
		UEdGraphPin* LocalPortPin = nullptr;
		UEdGraphPin* RemotePortPin = nullptr;

		const bool bIsExec = (LocalPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
		if (bIsExec)
		{
			// Functions have fixed exec in/out. For an input exec on a moved
			// node (consumed from the outside), we want Entry.exec-out on the
			// inside and Gateway.exec-in on the outside — directions mirror.
			LocalPortPin = K2->FindExecutionPin(*LocalPort,
				(LocalPin->Direction == EGPD_Input) ? EGPD_Output : EGPD_Input);
			RemotePortPin = K2->FindExecutionPin(*Gateway, LocalPin->Direction);
		}
		else
		{
			const FName UniquePortName = Gateway->CreateUniquePinName(LocalPin->PinName);
			FEdGraphPinType PinType = LocalPin->PinType;
			if (PinType.bIsWeakPointer && !PinType.IsContainer()) PinType.bIsWeakPointer = false;
			RemotePortPin = Gateway->CreatePin(LocalPin->Direction, PinType, UniquePortName);
			LocalPortPin  = LocalPort->CreateUserDefinedPin(UniquePortName, PinType,
				(LocalPin->Direction == EGPD_Input) ? EGPD_Output : EGPD_Input);
			if (LocalPin->Direction == EGPD_Output) bDiscardResult = false;
		}
		if (!LocalPortPin || !RemotePortPin) continue;

		LocalPin->Modify();
		// Re-route each external link: external↔Gateway, internal↔Entry/Result.
		for (int32 i = LocalPin->LinkedTo.Num() - 1; i >= 0; --i)
		{
			UEdGraphPin* RemotePin = LocalPin->LinkedTo[i];
			if (!RemotePin) continue;
			UEdGraphNode* RemoteOwner = RemotePin->GetOwningNode();
			if (!RemoteOwner) continue;
			if (Selection.Contains(RemoteOwner)) continue;  // purely internal — leave it
			if (RemoteOwner == EntryNode || RemoteOwner == ResultNode) continue;

			RemotePin->Modify();
			RemotePin->LinkedTo.Remove(LocalPin);
			if (RemotePin->GetOwningNode()->GetOuter() == RemotePortPin->GetOwningNode()->GetOuter())
			{
				RemotePin->MakeLinkTo(RemotePortPin);
			}
			if (LocalPort == EntryNode) LocalPortPin->BreakAllPinLinks();
			LocalPin->LinkedTo.Remove(RemotePin);
			LocalPin->MakeLinkTo(LocalPortPin);
		}
	}

	// Ensure the new function has a walkable exec path even if the selection
	// had no exec boundary pins (pure-data extraction).
	if (UEdGraphPin* ResultExecIn = K2->FindExecutionPin(*ResultNode, EGPD_Input))
	{
		if (ResultExecIn->LinkedTo.Num() == 0)
		{
			if (UEdGraphPin* EntryExecOut = K2->FindExecutionPin(*EntryNode, EGPD_Output))
			{
				if (EntryExecOut->LinkedTo.Num() == 0) EntryExecOut->MakeLinkTo(ResultExecIn);
			}
		}
	}

	// Reposition entry / result around the moved nodes.
	if (Selection.Num() > 0)
	{
		EntryNode->NodePosX = MinX - 260;
		EntryNode->NodePosY = CenterY;
		ResultNode->NodePosX = MaxX + 300;
		ResultNode->NodePosY = CenterY;
	}
	if (bDiscardResult && ResultNode->UserDefinedPins.Num() == 0)
	{
		ResultNode->DestroyNode();
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	Gateway->ReconstructNode();
	FKismetEditorUtilities::CompileBlueprint(BP);

	OutNewGraphName = DestGraph->GetName();
	return Gateway->NodeGuid.ToString(EGuidFormats::Digits);
}
int32 UTetherBlueprintLibrary::StraightenExecChain(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& StartNodeGuid, const FString& StartExecPinName)
{
	using namespace TetherBPSummaryImpl;
	using namespace TetherBlueprintGraphWriteImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return 0;
	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph) return 0;
	UEdGraphNode* Start = TetherBPCollapseImpl::FindNodeByGuid(Graph, StartNodeGuid);
	if (!Start) return 0;

	// Find the starting exec-output pin.
	UEdGraphPin* StartPin = nullptr;
	if (!StartExecPinName.IsEmpty())
	{
		StartPin = Start->FindPin(StartExecPinName);
		if (StartPin && (StartPin->Direction != EGPD_Output || StartPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec))
		{
			StartPin = nullptr;
		}
	}
	if (!StartPin)
	{
		for (UEdGraphPin* P : Start->Pins)
		{
			if (P && P->Direction == EGPD_Output && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && P->LinkedTo.Num() == 1)
			{
				StartPin = P; break;
			}
		}
	}
	if (!StartPin || StartPin->LinkedTo.Num() != 1) return 0;

	Graph->Modify();
	BP->Modify();

	int32 Adjusted = 0;
	int32 IterCap = 512;
	UEdGraphNode* Current = Start;
	UEdGraphPin* CurrentOutPin = StartPin;

	while (Current && IterCap-- > 0)
	{
		if (!CurrentOutPin || CurrentOutPin->LinkedTo.Num() != 1) break;
		UEdGraphPin* NextInPin = CurrentOutPin->LinkedTo[0];
		if (!NextInPin || NextInPin->LinkedTo.Num() != 1) break;  // merge — stop
		UEdGraphNode* Next = NextInPin->GetOwningNode();
		if (!Next || Next == Current) break;

		// Compute Y offset: align Next's exec-input pin Y to Current's exec-output pin Y.
		// Pin Y = NodePosY + HeaderHeight + (direction-index * PinRowHeight).
		auto PinRowIndex = [](const UEdGraphNode* N, const UEdGraphPin* P) -> int32
		{
			int32 Idx = 0;
			for (const UEdGraphPin* Q : N->Pins)
			{
				if (!Q || Q->bHidden) continue;
				if (Q->Direction != P->Direction) continue;
				if (Q == P) return Idx;
				Idx += 1;
			}
			return 0;
		};
		const int32 OutIdx = PinRowIndex(Current, CurrentOutPin);
		const int32 InIdx  = PinRowIndex(Next, NextInPin);
		const int32 OutPinY = Current->NodePosY + ComputePinLocalY(Current, EGPD_Output, OutIdx);
		const int32 DesiredNextY = OutPinY - ComputePinLocalY(Next, EGPD_Input, InIdx);
		if (Next->NodePosY != DesiredNextY)
		{
			Next->Modify();
			Next->NodePosY = DesiredNextY;
			Adjusted += 1;
		}

		// Pick Next's single exec-output (if any, unambiguous) and continue.
		UEdGraphPin* NextOut = nullptr;
		int32 ExecOutCount = 0;
		for (UEdGraphPin* P : Next->Pins)
		{
			if (!P || P->bHidden) continue;
			if (P->Direction != EGPD_Output) continue;
			if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
			if (P->LinkedTo.Num() == 1) { NextOut = P; ExecOutCount += 1; }
			else if (P->LinkedTo.Num() > 1) { ExecOutCount += 2; break; }
		}
		if (ExecOutCount != 1) break;  // branch — stop

		Current = Next;
		CurrentOutPin = NextOut;
	}

	if (Adjusted > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	return Adjusted;
}
namespace TetherBPColorImpl
{
	static bool ParseColorOrPreset(const FString& In, FLinearColor& Out)
	{
		if (In.IsEmpty()) return false;
		// Presets first.
		const FString Lower = In.ToLower();
		struct FPreset { const TCHAR* Name; FLinearColor C; };
		static const FPreset Presets[] = {
			{ TEXT("section"),    FLinearColor(0.35f, 0.35f, 0.35f, 1.f) },
			{ TEXT("validation"), FLinearColor(0.90f, 0.75f, 0.10f, 1.f) },
			{ TEXT("danger"),     FLinearColor(0.85f, 0.18f, 0.18f, 1.f) },
			{ TEXT("network"),    FLinearColor(0.52f, 0.25f, 0.80f, 1.f) },
			{ TEXT("ui"),         FLinearColor(0.18f, 0.68f, 0.70f, 1.f) },
			{ TEXT("debug"),      FLinearColor(0.28f, 0.72f, 0.28f, 1.f) },
			{ TEXT("setup"),      FLinearColor(0.22f, 0.48f, 0.82f, 1.f) },
		};
		for (const FPreset& P : Presets)
		{
			if (Lower.Equals(P.Name)) { Out = P.C; return true; }
		}
		// Hex string: #RRGGBB or #RRGGBBAA, with or without leading #.
		FString Hex = In;
		if (Hex.StartsWith(TEXT("#"))) Hex = Hex.RightChop(1);
		if (Hex.Len() != 6 && Hex.Len() != 8) return false;
		auto HexByte = [](TCHAR A, TCHAR B, uint8& Out) -> bool
		{
			auto Nib = [](TCHAR C, uint8& N) -> bool {
				if (C >= '0' && C <= '9') { N = uint8(C - '0'); return true; }
				if (C >= 'a' && C <= 'f') { N = uint8(C - 'a' + 10); return true; }
				if (C >= 'A' && C <= 'F') { N = uint8(C - 'A' + 10); return true; }
				return false;
			};
			uint8 Hi = 0, Lo = 0;
			if (!Nib(A, Hi) || !Nib(B, Lo)) return false;
			Out = (Hi << 4) | Lo;
			return true;
		};
		uint8 R=0, G=0, B=0, A=255;
		if (!HexByte(Hex[0], Hex[1], R)) return false;
		if (!HexByte(Hex[2], Hex[3], G)) return false;
		if (!HexByte(Hex[4], Hex[5], B)) return false;
		if (Hex.Len() == 8 && !HexByte(Hex[6], Hex[7], A)) return false;
		Out = FLinearColor(FColor(R, G, B, A));
		return true;
	}
}
bool UTetherBlueprintLibrary::SetCommentBoxColor(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& ColorOrPreset)
{
	using namespace TetherBlueprintGraphWriteImpl;
	using namespace TetherBPColorImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = TetherBPCollapseImpl::FindNodeByGuid(Graph, NodeGuid);
	UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
	if (!Comment) return false;

	FLinearColor C;
	if (!ParseColorOrPreset(ColorOrPreset, C)) return false;

	Comment->Modify();
	Comment->CommentColor = C;
	Comment->bColorCommentBubble = true;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::SetNodeColor(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& ColorOrPreset)
{
	using namespace TetherBlueprintGraphWriteImpl;
	using namespace TetherBPColorImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = TetherBPCollapseImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	Node->Modify();
	if (ColorOrPreset.IsEmpty())
	{
		// Clear: UE 5.x uses NodeColorMetaData on K2Nodes. Using node-title-color
		// instead is hacky; easiest path is to clear bHasCustomNodeColor via meta.
		Node->SetEnabledState(Node->GetDesiredEnabledState());  // no-op; just Modify + mark
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		return true;
	}
	FLinearColor C;
	if (!ParseColorOrPreset(ColorOrPreset, C)) return false;

	// K2Node titles draw using GetNodeTitleColor; individual override isn't
	// exposed as a property on UEdGraphNode. The cleanest user-visible hook is
	// setting a node comment with that color via bColorCommentBubble.
	Node->NodeComment = ColorOrPreset.StartsWith(TEXT("#")) ? ColorOrPreset : ColorOrPreset;
	Node->bCommentBubbleVisible = true;
	Node->bCommentBubblePinned  = true;
	// Fall through: without engine-level tint, emit a coloured comment bubble
	// on the node so agents still have a visible "this is UI/Network/..." cue.
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
namespace TetherBPRerouteImpl
{
	using namespace TetherBPSummaryImpl;

	struct FNodeBox
	{
		UEdGraphNode* Node = nullptr;
		int32 X0 = 0, Y0 = 0, X1 = 0, Y1 = 0;
	};

	static FNodeBox BoxOf(UEdGraphNode* N)
	{
		FNodeBox B;
		B.Node = N;
		int32 W = 0, H = 0;
		EstimateNodeSize(N, W, H);
		if (N->NodeWidth  > 0) W = N->NodeWidth;
		if (N->NodeHeight > 0) H = N->NodeHeight;
		B.X0 = N->NodePosX;
		B.Y0 = N->NodePosY;
		B.X1 = N->NodePosX + W;
		B.Y1 = N->NodePosY + H;
		return B;
	}

	/** Segment (x0,y0)→(x1,y1) intersects rect? Simple AABB clip test. */
	static bool SegmentHitsBox(int32 x0, int32 y0, int32 x1, int32 y1, const FNodeBox& B)
	{
		// Broad reject.
		const int32 SMinX = FMath::Min(x0, x1);
		const int32 SMaxX = FMath::Max(x0, x1);
		const int32 SMinY = FMath::Min(y0, y1);
		const int32 SMaxY = FMath::Max(y0, y1);
		if (SMaxX < B.X0 || SMinX > B.X1) return false;
		if (SMaxY < B.Y0 || SMinY > B.Y1) return false;

		// Liang-Barsky-ish: test full line against box.
		const float dx = float(x1 - x0);
		const float dy = float(y1 - y0);
		float t0 = 0.f, t1 = 1.f;
		auto Clip = [&](float p, float q) -> bool
		{
			if (FMath::IsNearlyZero(p))
			{
				return q >= 0.f;  // parallel — inside if non-negative
			}
			const float t = q / p;
			if (p < 0.f)      { if (t > t1) return false; if (t > t0) t0 = t; }
			else              { if (t < t0) return false; if (t < t1) t1 = t; }
			return true;
		};
		if (!Clip(-dx, float(x0 - B.X0))) return false;
		if (!Clip( dx, float(B.X1 - x0))) return false;
		if (!Clip(-dy, float(y0 - B.Y0))) return false;
		if (!Clip( dy, float(B.Y1 - y0))) return false;
		return t0 < t1;
	}
}
int32 UTetherBlueprintLibrary::AutoInsertReroutes(
	const FString& BlueprintPath, const FString& GraphName)
{
	using namespace TetherBlueprintGraphWriteImpl;
	using namespace TetherBPRerouteImpl;
	using namespace TetherBPSummaryImpl;

	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return 0;
	UEdGraph* Graph = FindGraphByName(BP, GraphName);
	if (!Graph) return 0;

	// Snapshot boxes first (we'll mutate the graph).
	TArray<FNodeBox> Boxes;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (!N) continue;
		if (N->IsA<UEdGraphNode_Comment>()) continue;
		if (N->IsA<UK2Node_Knot>()) continue;
		Boxes.Add(BoxOf(N));
	}

	// Snapshot wires before mutating. Each wire = (src-node, src-pin-name,
	// dst-node, dst-pin-name) so we can resolve after mutation.
	struct FWire { UEdGraphNode* SrcNode; FName SrcPin; UEdGraphNode* DstNode; FName DstPin; };
	TArray<FWire> Wires;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (!N || N->IsA<UK2Node_Knot>()) continue;
		for (UEdGraphPin* P : N->Pins)
		{
			if (!P || P->Direction != EGPD_Output) continue;
			for (UEdGraphPin* Linked : P->LinkedTo)
			{
				if (!Linked) continue;
				UEdGraphNode* DstNode = Linked->GetOwningNode();
				if (!DstNode || DstNode->IsA<UK2Node_Knot>()) continue;
				FWire W; W.SrcNode = N; W.SrcPin = P->PinName; W.DstNode = DstNode; W.DstPin = Linked->PinName;
				Wires.Add(W);
			}
		}
	}

	Graph->Modify();
	BP->Modify();

	int32 KnotsInserted = 0;
	for (const FWire& W : Wires)
	{
		UEdGraphPin* SrcPin = W.SrcNode->FindPin(W.SrcPin);
		UEdGraphPin* DstPin = W.DstNode->FindPin(W.DstPin);
		if (!SrcPin || !DstPin) continue;

		// Current endpoint positions (pin Y = node Y + compact-aware local Y).
		auto PinLocalY = [](const UEdGraphNode* N, const UEdGraphPin* P) -> int32
		{
			int32 Idx = 0;
			for (const UEdGraphPin* Q : N->Pins)
			{
				if (!Q || Q->bHidden) continue;
				if (Q->Direction != P->Direction) continue;
				if (Q == P) break;
				Idx += 1;
			}
			return ComputePinLocalY(N, P->Direction, Idx);
		};
		int32 SrcW = 0, SrcH = 0, DstW = 0, DstH = 0;
		EstimateNodeSize(W.SrcNode, SrcW, SrcH);
		EstimateNodeSize(W.DstNode, DstW, DstH);
		if (W.SrcNode->NodeWidth  > 0) SrcW = W.SrcNode->NodeWidth;
		if (W.DstNode->NodeWidth  > 0) DstW = W.DstNode->NodeWidth;

		const int32 SX = W.SrcNode->NodePosX + SrcW;
		const int32 SY = W.SrcNode->NodePosY + PinLocalY(W.SrcNode, SrcPin);
		const int32 DX = W.DstNode->NodePosX;
		const int32 DY = W.DstNode->NodePosY + PinLocalY(W.DstNode, DstPin);

		// Is any non-endpoint node's box intersected by this line?
		const FNodeBox* Blocker = nullptr;
		for (const FNodeBox& Box : Boxes)
		{
			if (Box.Node == W.SrcNode || Box.Node == W.DstNode) continue;
			if (SegmentHitsBox(SX, SY, DX, DY, Box))
			{
				Blocker = &Box; break;
			}
		}
		if (!Blocker) continue;

		// Insert a knot at X just past the blocker's right edge, Y a bit above
		// the blocker's top (keeps the wire clear). Break original link, route
		// src→knot→dst.
		const int32 KnotX = (Blocker->X1 + 40);
		const int32 KnotY = Blocker->Y0 - 40;

		UK2Node_Knot* Knot = NewObject<UK2Node_Knot>(Graph);
		Knot->CreateNewGuid();
		Knot->NodePosX = KnotX;
		Knot->NodePosY = KnotY;
		Graph->AddNode(Knot, false, false);
		Knot->PostPlacedNewNode();
		Knot->AllocateDefaultPins();

		UEdGraphPin* KnotIn  = Knot->GetInputPin();
		UEdGraphPin* KnotOut = Knot->GetOutputPin();
		if (!KnotIn || !KnotOut) { Knot->DestroyNode(); continue; }

		SrcPin->BreakLinkTo(DstPin);
		SrcPin->MakeLinkTo(KnotIn);
		KnotOut->MakeLinkTo(DstPin);
		KnotsInserted += 1;
	}

	if (KnotsInserted > 0) FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return KnotsInserted;
}
namespace TetherBPExtImpl
{
	/** Resolve a UEdGraphPin on a specific node by name. */
	static UEdGraphPin* ResolvePin(UBlueprint* BP, const FString& GraphName,
		const FString& NodeGuidStr, const FString& PinName)
	{
		if (!BP) return nullptr;
		UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
		if (!Graph) return nullptr;
		UEdGraphNode* Node = TetherBPCollapseImpl::FindNodeByGuid(Graph, NodeGuidStr);
		if (!Node) return nullptr;
		return Node->FindPin(FName(*PinName));
	}

	/** Locate a UClass by either `/Script/Module.Class`, `/Game/.../BP_C`, or a
	 *  short class name via FindFirstObject. */
	static UClass* ResolveClass(const FString& PathOrShortName)
	{
		if (PathOrShortName.IsEmpty()) return nullptr;
		if (PathOrShortName.StartsWith(TEXT("/")))
		{
			if (UClass* C = LoadObject<UClass>(nullptr, *PathOrShortName)) return C;
			// Fallback — maybe caller passed a BP path without the _C suffix.
			if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *PathOrShortName))
			{
				return BP->GeneratedClass;
			}
			return nullptr;
		}
		return FindFirstObject<UClass>(*PathOrShortName, EFindFirstObjectOptions::NativeFirst);
	}
}
bool UTetherBlueprintLibrary::SetDataTableRowHandlePin(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& PinName,
	const FString& DataTablePath, const FString& RowName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraphPin* Pin = TetherBPExtImpl::ResolvePin(BP, GraphName, NodeGuid, PinName);
	if (!Pin) return false;

	// Must be a struct pin of type FDataTableRowHandle.
	if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct) return false;
	const UScriptStruct* StructType = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
	if (!StructType || StructType != FDataTableRowHandle::StaticStruct()) return false;

	// Load the DataTable to validate (optional asset ref is allowed — just
	// emit the path). Quote values that contain non-identifier chars.
	FString Exported = FString::Printf(
		TEXT("(DataTable=\"%s\",RowName=\"%s\")"),
		*DataTablePath, *RowName);

	const UEdGraphSchema* Schema = Pin->GetSchema();
	if (!Schema) return false;
	Schema->TrySetDefaultValue(*Pin, Exported);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::SplitStructPin(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& PinName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraphPin* Pin = TetherBPExtImpl::ResolvePin(BP, GraphName, NodeGuid, PinName);
	if (!Pin || Pin->bHidden) return false;
	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	if (!K2) return false;
	if (!K2->CanSplitStructPin(*Pin)) return false;
	K2->SplitPin(Pin, /*bNotify*/ true);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::RecombineStructPin(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& SubPinName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraphPin* Pin = TetherBPExtImpl::ResolvePin(BP, GraphName, NodeGuid, SubPinName);
	if (!Pin) return false;
	// Find the split-root (sub-pins carry a ParentPin reference).
	UEdGraphPin* Parent = Pin->ParentPin ? Pin->ParentPin : Pin;
	// Walk up to the outermost parent in case of nested structs.
	while (Parent->ParentPin) Parent = Parent->ParentPin;
	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	if (!K2) return false;
	K2->RecombinePin(Parent->SubPins.Num() > 0 ? Parent->SubPins[0] : Parent);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::PromotePinToVariable(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, const FString& PinName,
	const FString& VariableName, bool bToMemberVariable,
	FString& OutNewVariableName, FString& OutNewNodeGuid)
{
	OutNewVariableName.Reset();
	OutNewNodeGuid.Reset();

	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;
	UEdGraphNode* Node = TetherBPCollapseImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;
	UEdGraphPin* Pin = Node->FindPin(FName(*PinName));
	if (!Pin || Pin->bHidden) return false;

	// Pin type copy — variables use the same PinType layout.
	FEdGraphPinType VarType = Pin->PinType;
	if (VarType.PinCategory == UEdGraphSchema_K2::PC_Exec) return false;
	if (VarType.PinCategory == UEdGraphSchema_K2::PC_Wildcard) return false;

	// Uniquify the requested name against existing BP member / local vars.
	const FName Desired = FBlueprintEditorUtils::FindUniqueKismetName(BP,
		VariableName.IsEmpty() ? TEXT("NewVar") : VariableName);

	bool bCreated = false;
	if (bToMemberVariable)
	{
		bCreated = FBlueprintEditorUtils::AddMemberVariable(BP, Desired, VarType);
	}
	else
	{
		// Local variable requires the function's UFunction scope.
		UFunction* Scope = BP->SkeletonGeneratedClass
			? BP->SkeletonGeneratedClass->FindFunctionByName(Graph->GetFName())
			: nullptr;
		if (!Scope) return false;
		bCreated = FBlueprintEditorUtils::AddLocalVariable(BP, Graph, Desired, VarType, FString());
	}
	if (!bCreated) return false;

	// Spawn a Get or Set node near the pin and wire it.
	const bool bWantSet = (Pin->Direction == EGPD_Output);
	UK2Node_Variable* VarNode = nullptr;
	if (bWantSet)
	{
		UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(Graph);
		SetNode->CreateNewGuid();
		SetNode->NodePosX = Node->NodePosX + 260;
		SetNode->NodePosY = Node->NodePosY;
		if (bToMemberVariable)
		{
			SetNode->VariableReference.SetSelfMember(Desired);
		}
		else
		{
			UFunction* Scope = BP->SkeletonGeneratedClass
				? BP->SkeletonGeneratedClass->FindFunctionByName(Graph->GetFName())
				: nullptr;
			SetNode->VariableReference.SetLocalMember(Desired, Scope, FGuid());
		}
		Graph->AddNode(SetNode, /*bFromUI*/ false, /*bSelectNewNode*/ false);
		SetNode->PostPlacedNewNode();
		SetNode->AllocateDefaultPins();
		VarNode = SetNode;
	}
	else
	{
		UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
		GetNode->CreateNewGuid();
		GetNode->NodePosX = Node->NodePosX - 260;
		GetNode->NodePosY = Node->NodePosY;
		if (bToMemberVariable)
		{
			GetNode->VariableReference.SetSelfMember(Desired);
		}
		else
		{
			UFunction* Scope = BP->SkeletonGeneratedClass
				? BP->SkeletonGeneratedClass->FindFunctionByName(Graph->GetFName())
				: nullptr;
			GetNode->VariableReference.SetLocalMember(Desired, Scope, FGuid());
		}
		Graph->AddNode(GetNode, /*bFromUI*/ false, /*bSelectNewNode*/ false);
		GetNode->PostPlacedNewNode();
		GetNode->AllocateDefaultPins();
		VarNode = GetNode;
	}
	if (!VarNode) return false;

	// Wire the variable node's data pin to the user's pin.
	UEdGraphPin* VarValuePin = VarNode->FindPin(Desired);
	if (!VarValuePin)
	{
		// Fallback — any non-exec pin with matching direction.
		for (UEdGraphPin* P : VarNode->Pins)
		{
			if (!P || P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
			if (bWantSet ? P->Direction == EGPD_Input : P->Direction == EGPD_Output) { VarValuePin = P; break; }
		}
	}
	if (VarValuePin)
	{
		Pin->MakeLinkTo(VarValuePin);
	}

	OutNewVariableName = Desired.ToString();
	OutNewNodeGuid = VarNode->NodeGuid.ToString(EGuidFormats::Digits);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
namespace TetherBPExtImpl
{
	/** Find the editable-pin-base node that owns the user-defined pin of
	 *  the given name in a function graph (FunctionEntry for inputs,
	 *  FunctionResult for outputs). */
	static UK2Node_EditablePinBase* FindParamOwner(UEdGraph* Graph, const FName& ParamName, int32& OutIndex)
	{
		OutIndex = INDEX_NONE;
		if (!Graph) return nullptr;
		auto Scan = [&](UK2Node_EditablePinBase* N) -> UK2Node_EditablePinBase*
		{
			if (!N) return nullptr;
			for (int32 i = 0; i < N->UserDefinedPins.Num(); ++i)
			{
				if (N->UserDefinedPins[i]->PinName == ParamName)
				{
					OutIndex = i;
					return N;
				}
			}
			return nullptr;
		};
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N))
			{
				if (UK2Node_EditablePinBase* R = Scan(E)) return R;
			}
			if (UK2Node_FunctionResult* R = Cast<UK2Node_FunctionResult>(N))
			{
				if (UK2Node_EditablePinBase* Out = Scan(R)) return Out;
			}
		}
		return nullptr;
	}
}
bool UTetherBlueprintLibrary::RemoveFunctionParameter(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& ParamName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = nullptr;
	const FName FnName(*FunctionName);
	for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) { Graph = G; break; } }
	if (!Graph) return false;

	int32 Idx = INDEX_NONE;
	UK2Node_EditablePinBase* Owner = TetherBPExtImpl::FindParamOwner(Graph, FName(*ParamName), Idx);
	if (!Owner) return false;

	Owner->Modify();
	Owner->RemoveUserDefinedPinByName(FName(*ParamName));
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::ReorderFunctionParameter(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& ParamName, int32 NewIndex)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = nullptr;
	const FName FnName(*FunctionName);
	for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) { Graph = G; break; } }
	if (!Graph) return false;

	int32 OldIdx = INDEX_NONE;
	UK2Node_EditablePinBase* Owner = TetherBPExtImpl::FindParamOwner(Graph, FName(*ParamName), OldIdx);
	if (!Owner || OldIdx == INDEX_NONE) return false;

	const int32 Count = Owner->UserDefinedPins.Num();
	int32 Target = NewIndex;
	if (Target < 0 || Target >= Count) Target = Count - 1;
	if (Target == OldIdx) return true;

	Owner->Modify();
	TSharedPtr<FUserPinInfo> Moving = Owner->UserDefinedPins[OldIdx];
	Owner->UserDefinedPins.RemoveAt(OldIdx);
	Owner->UserDefinedPins.Insert(Moving, Target);
	Owner->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
FString UTetherBlueprintLibrary::CollapseNodesToMacro(
	const FString& BlueprintPath, const FString& SourceGraphName,
	const TArray<FString>& NodeGuids, const FString& NewMacroName,
	FString& OutNewGraphName)
{
	using namespace TetherBPCollapseImpl;
	using namespace TetherBlueprintGraphWriteImpl;

	OutNewGraphName.Empty();
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return FString();
	UEdGraph* SourceGraph = FindGraphByName(BP, SourceGraphName);
	if (!SourceGraph) return FString();

	TSet<UEdGraphNode*> Selection;
	int32 MinX = INT32_MAX, MinY = INT32_MAX, MaxX = INT32_MIN, MaxY = INT32_MIN;
	int32 SumX = 0, SumY = 0;
	for (const FString& GuidStr : NodeGuids)
	{
		UEdGraphNode* N = TetherBPCollapseImpl::FindNodeByGuid(SourceGraph, GuidStr);
		if (!N) return FString();
		if (N->IsA<UK2Node_FunctionEntry>() || N->IsA<UK2Node_FunctionResult>()) return FString();
		if (N->GetClass()->GetName().Contains(TEXT("Tunnel"))) return FString();
		Selection.Add(N);
		SumX += N->NodePosX; SumY += N->NodePosY;
		MinX = FMath::Min(MinX, N->NodePosX); MinY = FMath::Min(MinY, N->NodePosY);
		MaxX = FMath::Max(MaxX, N->NodePosX); MaxY = FMath::Max(MaxY, N->NodePosY);
	}
	if (Selection.Num() == 0) return FString();

	// Create the macro graph (with Tunnel/Tunnel scaffolding).
	const FName BaseName = FBlueprintEditorUtils::FindUniqueKismetName(
		BP, NewMacroName.IsEmpty() ? TEXT("ExtractedMacro") : NewMacroName);
	UEdGraph* DestGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, BaseName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!DestGraph) return FString();
	FBlueprintEditorUtils::AddMacroGraph(BP, DestGraph, /*bIsUserCreated*/ true, nullptr);

	// Find the auto-created Entry / Exit tunnels.
	UK2Node_Tunnel* Entry = nullptr;
	UK2Node_Tunnel* Exit  = nullptr;
	for (UEdGraphNode* N : DestGraph->Nodes)
	{
		if (UK2Node_Tunnel* T = Cast<UK2Node_Tunnel>(N))
		{
			if (T->bCanHaveInputs && !T->bCanHaveOutputs) { Exit = T; continue; }
			if (T->bCanHaveOutputs && !T->bCanHaveInputs) { Entry = T; continue; }
		}
	}
	if (!Entry || !Exit) return FString();

	const int32 CenterX = SumX / Selection.Num();
	const int32 CenterY = SumY / Selection.Num();

	// Spawn the gateway MacroInstance in the source graph.
	UK2Node_MacroInstance* Gateway = NewObject<UK2Node_MacroInstance>(SourceGraph);
	Gateway->CreateNewGuid();
	Gateway->NodePosX = CenterX;
	Gateway->NodePosY = CenterY;
	SourceGraph->AddNode(Gateway, false, false);
	Gateway->SetMacroGraph(DestGraph);
	Gateway->PostPlacedNewNode();
	Gateway->AllocateDefaultPins();

	// Move selected nodes + collect boundary pins.
	TArray<UEdGraphPin*> GatewayPins;
	SourceGraph->Modify();
	DestGraph->Modify();
	BP->Modify();

	for (UEdGraphNode* N : Selection)
	{
		N->Modify();
		SourceGraph->Nodes.Remove(N);
		DestGraph->Nodes.Add(N);
		N->Rename(nullptr, DestGraph);
		for (UEdGraphPin* P : N->Pins)
		{
			if (!P || P->LinkedTo.Num() == 0) continue;
			bool bCrosses = false;
			for (UEdGraphPin* Linked : P->LinkedTo)
			{
				if (!Linked) continue;
				if (!Selection.Contains(Linked->GetOwningNode())) { bCrosses = true; break; }
			}
			if (bCrosses) GatewayPins.Add(P);
		}
	}
	TetherBPCollapseImpl::SortPinsByPosition(GatewayPins);

	// Thunk each boundary pin through Entry / Exit tunnels.
	for (UEdGraphPin* LocalPin : GatewayPins)
	{
		const bool bIsExec = (LocalPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
		UK2Node_EditablePinBase* Port = (LocalPin->Direction == EGPD_Input) ? (UK2Node_EditablePinBase*)Entry : (UK2Node_EditablePinBase*)Exit;

		const FName UniquePortName = Gateway->CreateUniquePinName(LocalPin->PinName);
		FEdGraphPinType PinType = LocalPin->PinType;
		if (PinType.bIsWeakPointer && !PinType.IsContainer()) PinType.bIsWeakPointer = false;

		UEdGraphPin* RemotePortPin = Gateway->CreatePin(LocalPin->Direction, PinType, UniquePortName);
		UEdGraphPin* LocalPortPin  = Port->CreateUserDefinedPin(UniquePortName, PinType,
			(LocalPin->Direction == EGPD_Input) ? EGPD_Output : EGPD_Input);
		(void)bIsExec;
		if (!RemotePortPin || !LocalPortPin) continue;

		LocalPin->Modify();
		for (int32 i = LocalPin->LinkedTo.Num() - 1; i >= 0; --i)
		{
			UEdGraphPin* RemotePin = LocalPin->LinkedTo[i];
			if (!RemotePin) continue;
			UEdGraphNode* RemoteOwner = RemotePin->GetOwningNode();
			if (!RemoteOwner) continue;
			if (Selection.Contains(RemoteOwner)) continue;

			RemotePin->Modify();
			RemotePin->LinkedTo.Remove(LocalPin);
			RemotePin->MakeLinkTo(RemotePortPin);
			LocalPin->LinkedTo.Remove(RemotePin);
			LocalPin->MakeLinkTo(LocalPortPin);
		}
	}

	// Reposition Entry / Exit around the moved nodes.
	Entry->NodePosX = MinX - 260;
	Entry->NodePosY = CenterY;
	Exit->NodePosX  = MaxX + 300;
	Exit->NodePosY  = CenterY;

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	Gateway->ReconstructNode();
	FKismetEditorUtilities::CompileBlueprint(BP);

	OutNewGraphName = DestGraph->GetName();
	return Gateway->NodeGuid.ToString(EGuidFormats::Digits);
}
// ─── #13 Async action node ──────────────────────────────────────

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
FString UTetherBlueprintLibrary::AddAsyncActionNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& FactoryClassPath, const FString& FactoryFunctionName,
	int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();
	UClass* FactoryClass = TetherBPExtImpl::ResolveClass(FactoryClassPath);
	if (!FactoryClass) return FString();
	UFunction* FactoryFn = FactoryClass->FindFunctionByName(FName(*FactoryFunctionName));
	if (!FactoryFn) return FString();

	UK2Node_AsyncAction* Node = NewObject<UK2Node_AsyncAction>(Graph);
	Node->CreateNewGuid();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
	Node->InitializeProxyFromFunction(FactoryFn);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	Node->ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

// ─── #19 Add K2Node by class name ───────────────────────────────

FString UTetherBlueprintLibrary::AddNodeByClassName(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeClassPath, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();
	UClass* Cls = TetherBPExtImpl::ResolveClass(NodeClassPath);
	if (!Cls) return FString();
	if (!Cls->IsChildOf(UK2Node::StaticClass())) return FString();
	if (Cls->HasAnyClassFlags(CLASS_Abstract)) return FString();

	UK2Node* Node = NewObject<UK2Node>(Graph, Cls);
	Node->CreateNewGuid();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FTetherEditorFocusState UTetherBlueprintLibrary::GetEditorFocusState()
{
	FTetherEditorFocusState Out;
	UAssetEditorSubsystem* Sub = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!Sub) return Out;

	const TArray<UObject*> EditedAssets = Sub->GetAllEditedAssets();
	for (UObject* Asset : EditedAssets)
	{
		if (UBlueprint* BP = Cast<UBlueprint>(Asset))
		{
			Out.OpenBlueprintPaths.Add(BP->GetPathName());
		}
	}

	// Find the BP editor currently in focus. AssetEditorSubsystem doesn't
	// expose "active" directly — use the last-focused BP editor. We scan
	// the open BP editors and pick the one whose FocusedGraph is non-null
	// (means it has a graph tab active).
	UBlueprint* FocusedBP = nullptr;
	FBlueprintEditor* FocusedEditor = nullptr;
	for (UObject* Asset : EditedAssets)
	{
		UBlueprint* BP = Cast<UBlueprint>(Asset);
		if (!BP) continue;
		IAssetEditorInstance* Inst = Sub->FindEditorForAsset(BP, /*bFocusIfOpen*/ false);
		if (!Inst) continue;
		FBlueprintEditor* BPEd = static_cast<FBlueprintEditor*>(Inst);
		if (BPEd->GetFocusedGraph() != nullptr)
		{
			FocusedBP = BP;
			FocusedEditor = BPEd;
			break;
		}
	}

	if (FocusedBP && FocusedEditor)
	{
		Out.BlueprintPath = FocusedBP->GetPathName();
		if (UEdGraph* G = FocusedEditor->GetFocusedGraph())
		{
			Out.FocusedGraphName = G->GetName();
			if (FocusedBP->UbergraphPages.Contains(G))      Out.FocusedGraphType = TEXT("EventGraph");
			else if (FocusedBP->FunctionGraphs.Contains(G)) Out.FocusedGraphType = TEXT("Function");
			else if (FocusedBP->MacroGraphs.Contains(G))    Out.FocusedGraphType = TEXT("Macro");
		}

		// Selected nodes in the focused graph panel.
		const FGraphPanelSelectionSet Selected = FocusedEditor->GetSelectedNodes();
		for (UObject* N : Selected)
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(N))
			{
				FTetherSelectedNode S;
				S.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
				S.NodeClass = Node->GetClass()->GetName();
				S.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
				Out.SelectedNodes.Add(MoveTemp(S));
			}
		}
	}
	return Out;
}
namespace TetherBPExtImpl
{
	/** Enumerate all BPs under PackagePath via AssetRegistry. */
	static void EnumerateBlueprints(const FString& PackagePath, TArray<UBlueprint*>& Out)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FString Root = PackagePath.IsEmpty() ? FString(TEXT("/Game")) : PackagePath;
		if (!Root.StartsWith(TEXT("/"))) Root = TEXT("/") + Root;

		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(FName(*Root));
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> BpAssets;
		Registry.GetAssets(Filter, BpAssets);
		for (const FAssetData& Data : BpAssets)
		{
			if (UBlueprint* BP = Cast<UBlueprint>(Data.GetAsset()))
			{
				Out.Add(BP);
			}
		}
	}

	/** Check that the owner-class on a variable reference matches the defining
	 *  BP (generated class or one of its parents). */
	static bool ReferencesVariableOnBP(const FMemberReference& Ref, UClass* DefiningClass)
	{
		if (!DefiningClass) return false;
		const UClass* MemberScope = Ref.GetMemberParentClass();
		if (!MemberScope) return false;
		// Covers the defining class AND subclasses that inherit the variable.
		return DefiningClass->IsChildOf(MemberScope) || MemberScope->IsChildOf(DefiningClass);
	}
}
FTetherRenameReport UTetherBlueprintLibrary::RenameMemberVariableGlobal(
	const FString& DefiningBlueprintPath,
	const FString& OldName, const FString& NewName,
	const FString& PackagePath)
{
	FTetherRenameReport Report;
	UBlueprint* DefBP = LoadBP(DefiningBlueprintPath);
	if (!DefBP || !DefBP->GeneratedClass)
	{
		Report.Message = TEXT("defining blueprint not found or not compiled");
		return Report;
	}
	const FName Old(*OldName), New(*NewName);
	if (Old.IsNone() || New.IsNone() || Old == New)
	{
		Report.Message = TEXT("invalid rename — old/new names must be distinct + non-empty");
		return Report;
	}
	if (FBlueprintEditorUtils::FindNewVariableIndex(DefBP, Old) == INDEX_NONE)
	{
		Report.Message = TEXT("variable not found on defining blueprint");
		return Report;
	}

	// Rename on the defining BP (this also rewrites its own call sites).
	FBlueprintEditorUtils::RenameMemberVariable(DefBP, Old, New);
	Report.UpdatedBlueprints.Add(DefBP->GetPathName());

	// Scan every other BP under PackagePath for references and rewrite.
	UClass* DefClass = DefBP->GeneratedClass;
	TArray<UBlueprint*> All;
	TetherBPExtImpl::EnumerateBlueprints(PackagePath, All);
	for (UBlueprint* BP : All)
	{
		if (!BP || BP == DefBP) continue;
		bool bChanged = false;
		for (const TetherBPSummaryImpl::FAllGraphs& Entry : TetherBPSummaryImpl::CollectAllGraphs(BP))
		{
			for (UEdGraphNode* Node : Entry.Graph->Nodes)
			{
				if (UK2Node_Variable* V = Cast<UK2Node_Variable>(Node))
				{
					if (V->VariableReference.GetMemberName() == Old &&
						TetherBPExtImpl::ReferencesVariableOnBP(V->VariableReference, DefClass))
					{
						V->Modify();
						V->VariableReference.SetSelfMember(New);
						V->ReconstructNode();
						Report.UpdatedNodeCount += 1;
						bChanged = true;
					}
				}
			}
		}
		if (bChanged)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
			FKismetEditorUtilities::CompileBlueprint(BP);
			Report.UpdatedBlueprints.Add(BP->GetPathName());
		}
	}

	FKismetEditorUtilities::CompileBlueprint(DefBP);
	Report.bSuccess = true;
	return Report;
}
FTetherRenameReport UTetherBlueprintLibrary::RenameFunctionGlobal(
	const FString& DefiningBlueprintPath,
	const FString& OldName, const FString& NewName,
	const FString& PackagePath)
{
	FTetherRenameReport Report;
	UBlueprint* DefBP = LoadBP(DefiningBlueprintPath);
	if (!DefBP || !DefBP->GeneratedClass)
	{
		Report.Message = TEXT("defining blueprint not found or not compiled");
		return Report;
	}
	const FName Old(*OldName), New(*NewName);
	if (Old.IsNone() || New.IsNone() || Old == New)
	{
		Report.Message = TEXT("invalid rename — old/new names must be distinct + non-empty");
		return Report;
	}

	UEdGraph* FnGraph = nullptr;
	for (UEdGraph* G : DefBP->FunctionGraphs)
	{
		if (G && G->GetFName() == Old) { FnGraph = G; break; }
	}
	if (!FnGraph)
	{
		Report.Message = TEXT("function graph not found on defining blueprint");
		return Report;
	}
	// Uniqueness check on the defining BP.
	if (FBlueprintEditorUtils::FindUniqueKismetName(DefBP, NewName) != New)
	{
		Report.Message = TEXT("new name collides with an existing kismet name");
		return Report;
	}

	FBlueprintEditorUtils::RenameGraph(FnGraph, NewName);
	Report.UpdatedBlueprints.Add(DefBP->GetPathName());

	UClass* DefClass = DefBP->GeneratedClass;
	TArray<UBlueprint*> All;
	TetherBPExtImpl::EnumerateBlueprints(PackagePath, All);
	for (UBlueprint* BP : All)
	{
		if (!BP || BP == DefBP) continue;
		bool bChanged = false;
		for (const TetherBPSummaryImpl::FAllGraphs& Entry : TetherBPSummaryImpl::CollectAllGraphs(BP))
		{
			for (UEdGraphNode* Node : Entry.Graph->Nodes)
			{
				UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
				if (!Call) continue;
				if (Call->FunctionReference.GetMemberName() != Old) continue;
				const UClass* Scope = Call->FunctionReference.GetMemberParentClass();
				if (!Scope) continue;
				if (!(DefClass->IsChildOf(Scope) || Scope->IsChildOf(DefClass))) continue;
				Call->Modify();
				Call->FunctionReference.SetExternalMember(New, DefClass);
				Call->ReconstructNode();
				Report.UpdatedNodeCount += 1;
				bChanged = true;
			}
		}
		if (bChanged)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
			FKismetEditorUtilities::CompileBlueprint(BP);
			Report.UpdatedBlueprints.Add(BP->GetPathName());
		}
	}

	FKismetEditorUtilities::CompileBlueprint(DefBP);
	Report.bSuccess = true;
	return Report;
}
namespace TetherBPSnapshotImpl
{
	/** Sort two guid strings case-insensitively for deterministic output. */
	struct FGuidLess
	{
		bool operator()(const FString& A, const FString& B) const
		{
			return A.Compare(B, ESearchCase::IgnoreCase) < 0;
		}
	};

	/** Build a canonical JSON snapshot for a graph. */
	static FString BuildSnapshotJson(UEdGraph* Graph)
	{
		if (!Graph) return FString();

		// Sort nodes by guid for determinism.
		TArray<UEdGraphNode*> Nodes;
		Nodes.Reserve(Graph->Nodes.Num());
		for (UEdGraphNode* N : Graph->Nodes) if (N) Nodes.Add(N);
		Nodes.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
		{
			return A.NodeGuid.ToString(EGuidFormats::Digits) <
			       B.NodeGuid.ToString(EGuidFormats::Digits);
		});

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

		TArray<TSharedPtr<FJsonValue>> NodeArr;
		NodeArr.Reserve(Nodes.Num());
		TArray<TTuple<FString, FString, FString, FString>> Wires;

		for (UEdGraphNode* N : Nodes)
		{
			const FString Guid = N->NodeGuid.ToString(EGuidFormats::Digits);
			TSharedRef<FJsonObject> NObj = MakeShared<FJsonObject>();
			NObj->SetStringField(TEXT("guid"),  Guid);
			NObj->SetStringField(TEXT("class"), N->GetClass()->GetName());
			NObj->SetStringField(TEXT("title"),
				N->GetNodeTitle(ENodeTitleType::ListView).ToString());
			NObj->SetNumberField(TEXT("x"), N->NodePosX);
			NObj->SetNumberField(TEXT("y"), N->NodePosY);
			NodeArr.Add(MakeShared<FJsonValueObject>(NObj));

			// Collect wires from output pins only so each wire is emitted once.
			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked || !Linked->GetOwningNode()) continue;
					const FString DstGuid =
						Linked->GetOwningNode()->NodeGuid.ToString(EGuidFormats::Digits);
					Wires.Emplace(Guid, Pin->PinName.ToString(),
					              DstGuid, Linked->PinName.ToString());
				}
			}
		}
		Root->SetArrayField(TEXT("nodes"), NodeArr);

		// Sort wires for determinism.
		Wires.Sort([](const TTuple<FString, FString, FString, FString>& A,
		              const TTuple<FString, FString, FString, FString>& B)
		{
			if (A.Get<0>() != B.Get<0>()) return A.Get<0>() < B.Get<0>();
			if (A.Get<1>() != B.Get<1>()) return A.Get<1>() < B.Get<1>();
			if (A.Get<2>() != B.Get<2>()) return A.Get<2>() < B.Get<2>();
			return A.Get<3>() < B.Get<3>();
		});

		TArray<TSharedPtr<FJsonValue>> WireArr;
		WireArr.Reserve(Wires.Num());
		for (const auto& W : Wires)
		{
			TSharedRef<FJsonObject> WObj = MakeShared<FJsonObject>();
			WObj->SetStringField(TEXT("src"),     W.Get<0>());
			WObj->SetStringField(TEXT("src_pin"), W.Get<1>());
			WObj->SetStringField(TEXT("dst"),     W.Get<2>());
			WObj->SetStringField(TEXT("dst_pin"), W.Get<3>());
			WireArr.Add(MakeShared<FJsonValueObject>(WObj));
		}
		Root->SetArrayField(TEXT("wires"), WireArr);

		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}
}
FString UTetherBlueprintLibrary::GetGraphFingerprint(
	const FString& BlueprintPath, const FString& GraphName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();
	const FString Json = TetherBPSnapshotImpl::BuildSnapshotJson(Graph);
	if (Json.IsEmpty()) return FString();

	FSHA1 Hasher;
	const FTCHARToUTF8 Utf8(*Json);
	Hasher.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	Hasher.Final();
	uint8 Digest[20];
	Hasher.GetHash(Digest);

	FString Hex;
	Hex.Reserve(40);
	for (int32 i = 0; i < 20; ++i)
	{
		Hex.Appendf(TEXT("%02x"), Digest[i]);
	}
	return Hex;
}
FString UTetherBlueprintLibrary::SnapshotGraphJson(
	const FString& BlueprintPath, const FString& GraphName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();
	return TetherBPSnapshotImpl::BuildSnapshotJson(Graph);
}
FTetherGraphDiff UTetherBlueprintLibrary::DiffGraphSnapshots(
	const FString& BeforeJson, const FString& AfterJson)
{
	FTetherGraphDiff Out;

	auto Parse = [](const FString& S, TSharedPtr<FJsonObject>& Obj) -> bool
	{
		if (S.IsEmpty()) return false;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(S);
		return FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid();
	};

	TSharedPtr<FJsonObject> A, B;
	if (!Parse(BeforeJson, A) || !Parse(AfterJson, B)) return Out;

	auto CollectNodes = [](const TSharedPtr<FJsonObject>& J,
		TMap<FString, FTetherGraphDiffNode>& OutMap)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!J->TryGetArrayField(TEXT("nodes"), Arr) || !Arr) return;
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj) continue;
			FTetherGraphDiffNode N;
			N.NodeGuid  = (*Obj)->GetStringField(TEXT("guid"));
			N.NodeClass = (*Obj)->GetStringField(TEXT("class"));
			N.Title     = (*Obj)->GetStringField(TEXT("title"));
			OutMap.Add(N.NodeGuid, MoveTemp(N));
		}
	};

	auto CollectWires = [](const TSharedPtr<FJsonObject>& J,
		TMap<FString, FTetherGraphDiffWire>& OutMap)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!J->TryGetArrayField(TEXT("wires"), Arr) || !Arr) return;
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj) continue;
			FTetherGraphDiffWire W;
			W.SrcNodeGuid = (*Obj)->GetStringField(TEXT("src"));
			W.SrcPinName  = (*Obj)->GetStringField(TEXT("src_pin"));
			W.DstNodeGuid = (*Obj)->GetStringField(TEXT("dst"));
			W.DstPinName  = (*Obj)->GetStringField(TEXT("dst_pin"));
			const FString Key = W.SrcNodeGuid + TEXT("|") + W.SrcPinName +
			                    TEXT("|") + W.DstNodeGuid + TEXT("|") + W.DstPinName;
			OutMap.Add(Key, MoveTemp(W));
		}
	};

	TMap<FString, FTetherGraphDiffNode> NA, NB;
	TMap<FString, FTetherGraphDiffWire> WA, WB;
	CollectNodes(A, NA); CollectNodes(B, NB);
	CollectWires(A, WA); CollectWires(B, WB);

	for (const auto& It : NB)  { if (!NA.Contains(It.Key)) Out.AddedNodes.Add(It.Value); }
	for (const auto& It : NA)  { if (!NB.Contains(It.Key)) Out.RemovedNodes.Add(It.Value); }
	for (const auto& It : WB)  { if (!WA.Contains(It.Key)) Out.AddedWires.Add(It.Value); }
	for (const auto& It : WA)  { if (!WB.Contains(It.Key)) Out.RemovedWires.Add(It.Value); }
	return Out;
}
bool UTetherBlueprintLibrary::EnsureFunctionExecWired(
	const FString& BlueprintPath, const FString& FunctionName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = nullptr;
	const FName FnName(*FunctionName);
	for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) { Graph = G; break; } }
	if (!Graph) return false;

	UK2Node_FunctionEntry* Entry = nullptr;
	UK2Node_FunctionResult* Result = nullptr;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (!Entry)  Entry  = Cast<UK2Node_FunctionEntry>(N);
		if (!Result) Result = Cast<UK2Node_FunctionResult>(N);
	}
	if (!Entry || !Result) return false;

	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* EntryExec = K2 ? K2->FindExecutionPin(*Entry,  EGPD_Output) : nullptr;
	UEdGraphPin* ResultExec = K2 ? K2->FindExecutionPin(*Result, EGPD_Input)  : nullptr;
	if (!EntryExec || !ResultExec) return false;
	// Already wired to something — don't touch.
	if (EntryExec->LinkedTo.Num() > 0 || ResultExec->LinkedTo.Num() > 0) return false;

	EntryExec->MakeLinkTo(ResultExec);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::InsertNodeOnWire(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& SrcNodeGuid, const FString& SrcPinName,
	const FString& DstNodeGuid, const FString& DstPinName,
	const FString& InsertNodeGuid,
	const FString& InsertInPinName, const FString& InsertOutPinName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* SrcNode    = TetherBPCollapseImpl::FindNodeByGuid(Graph, SrcNodeGuid);
	UEdGraphNode* DstNode    = TetherBPCollapseImpl::FindNodeByGuid(Graph, DstNodeGuid);
	UEdGraphNode* InsertNode = TetherBPCollapseImpl::FindNodeByGuid(Graph, InsertNodeGuid);
	if (!SrcNode || !DstNode || !InsertNode) return false;

	UEdGraphPin* SrcPin    = SrcNode->FindPin(FName(*SrcPinName));
	UEdGraphPin* DstPin    = DstNode->FindPin(FName(*DstPinName));
	UEdGraphPin* InsertIn  = InsertNode->FindPin(FName(*InsertInPinName));
	UEdGraphPin* InsertOut = InsertNode->FindPin(FName(*InsertOutPinName));
	if (!SrcPin || !DstPin || !InsertIn || !InsertOut) return false;

	// Confirm the original wire actually exists.
	if (!SrcPin->LinkedTo.Contains(DstPin)) return false;

	Graph->Modify();
	SrcPin->BreakLinkTo(DstPin);
	SrcPin->MakeLinkTo(InsertIn);
	InsertOut->MakeLinkTo(DstPin);
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
FTetherReplaceNodeReport UTetherBlueprintLibrary::ReplaceNodePreservingConnections(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& OldNodeGuid, const FString& NewNodeClassPath)
{
	FTetherReplaceNodeReport Rep;
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP)
	{
		Rep.Message = TEXT("blueprint not found"); return Rep;
	}
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) { Rep.Message = TEXT("graph not found"); return Rep; }
	UEdGraphNode* Old = TetherBPCollapseImpl::FindNodeByGuid(Graph, OldNodeGuid);
	if (!Old) { Rep.Message = TEXT("old node not found"); return Rep; }

	UClass* NewCls = TetherBPExtImpl::ResolveClass(NewNodeClassPath);
	if (!NewCls || !NewCls->IsChildOf(UK2Node::StaticClass()) ||
	    NewCls->HasAnyClassFlags(CLASS_Abstract))
	{
		Rep.Message = TEXT("new class not resolvable / not a UK2Node");
		return Rep;
	}

	const int32 OldX = Old->NodePosX;
	const int32 OldY = Old->NodePosY;

	// Cache old pin links before we start breaking things.
	struct FOldPinRef { FName Name; EEdGraphPinDirection Dir; FEdGraphPinType Type; TArray<UEdGraphPin*> Links; };
	TArray<FOldPinRef> OldPins;
	for (UEdGraphPin* P : Old->Pins)
	{
		if (!P || P->bHidden) continue;
		FOldPinRef R;
		R.Name  = P->PinName;
		R.Dir   = P->Direction;
		R.Type  = P->PinType;
		R.Links = P->LinkedTo;
		OldPins.Add(MoveTemp(R));
	}

	UK2Node* NewNode = NewObject<UK2Node>(Graph, NewCls);
	NewNode->CreateNewGuid();
	NewNode->NodePosX = OldX;
	NewNode->NodePosY = OldY;
	Graph->AddNode(NewNode, /*bFromUI*/ false, /*bSelectNewNode*/ false);
	NewNode->PostPlacedNewNode();
	NewNode->AllocateDefaultPins();

	const UEdGraphSchema* Schema = Graph->GetSchema();

	Graph->Modify();
	for (const FOldPinRef& R : OldPins)
	{
		UEdGraphPin* NewPin = NewNode->FindPin(R.Name, R.Dir);
		if (!NewPin)
		{
			if (R.Links.Num() > 0) Rep.DroppedPins.Add(R.Name.ToString());
			continue;
		}
		// Compatible types only.
		bool bCompatible = (NewPin->PinType == R.Type);
		if (!bCompatible && Schema)
		{
			// Check schema compatibility for data pins (exec pins match by
			// category alone; this covers the common case).
			bCompatible = (NewPin->PinType.PinCategory == R.Type.PinCategory);
		}
		if (!bCompatible)
		{
			if (R.Links.Num() > 0) Rep.DroppedPins.Add(R.Name.ToString());
			continue;
		}
		int32 Rewired = 0;
		for (UEdGraphPin* Linked : R.Links)
		{
			if (!Linked) continue;
			Linked->MakeLinkTo(NewPin);
			Rewired += 1;
		}
		if (Rewired > 0) Rep.ReconnectedPins.Add(R.Name.ToString());
	}

	Old->DestroyNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

	Rep.bSuccess = true;
	Rep.NewNodeGuid = NewNode->NodeGuid.ToString(EGuidFormats::Digits);
	return Rep;
}
namespace TetherBPBatchImpl
{
	/** Resolve a guid field that may be "$N" back-reference to an earlier op. */
	static FString ResolveGuid(const FString& Token,
		const TArray<FTetherGraphOpResult>& PriorResults)
	{
		if (Token.Len() < 2 || Token[0] != TCHAR('$')) return Token;
		const FString NumStr = Token.Mid(1);
		if (!NumStr.IsNumeric()) return Token;
		const int32 Idx = FCString::Atoi(*NumStr);
		if (!PriorResults.IsValidIndex(Idx)) return FString();
		return PriorResults[Idx].NewNodeGuid;
	}

	static FString GetString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FString& Fallback = FString())
	{
		FString V;
		if (Obj->TryGetStringField(Key, V)) return V;
		return Fallback;
	}

	static int32 GetInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, int32 Fallback = 0)
	{
		int32 V;
		if (Obj->TryGetNumberField(Key, V)) return V;
		double D;
		if (Obj->TryGetNumberField(Key, D)) return static_cast<int32>(D);
		return Fallback;
	}

	static bool GetBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, bool Fallback = false)
	{
		bool V;
		if (Obj->TryGetBoolField(Key, V)) return V;
		return Fallback;
	}
}
TArray<FTetherGraphOpResult> UTetherBlueprintLibrary::ApplyGraphOps(
	const FString& BlueprintPath, const FString& OpsJson)
{
	using namespace TetherBPBatchImpl;

	TArray<FTetherGraphOpResult> Results;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Results;

	TArray<TSharedPtr<FJsonValue>> OpArr;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(OpsJson);
	if (!FJsonSerializer::Deserialize(Reader, OpArr)) return Results;

	bool bAnyMutation = false;
	for (int32 i = 0; i < OpArr.Num(); ++i)
	{
		FTetherGraphOpResult R;
		R.Index = i;

		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!OpArr[i].IsValid() || !OpArr[i]->TryGetObject(ObjPtr) || !ObjPtr)
		{
			R.Message = TEXT("op is not a JSON object");
			Results.Add(R); continue;
		}
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;
		R.Op = GetString(Obj, TEXT("op"));

		if (R.Op == TEXT("add_call_function"))
		{
			const FString G = GetString(Obj, TEXT("graph"));
			const FString TC = GetString(Obj, TEXT("target_class"));
			const FString FN = GetString(Obj, TEXT("function_name"));
			const int32 X = GetInt(Obj, TEXT("x")); const int32 Y = GetInt(Obj, TEXT("y"));
			R.NewNodeGuid = AddCallFunctionNode(BlueprintPath, G, TC, FN, X, Y);
			R.bSuccess = !R.NewNodeGuid.IsEmpty();
			if (!R.bSuccess) R.Message = TEXT("add_call_function failed");
		}
		else if (R.Op == TEXT("add_variable_node"))
		{
			const FString G = GetString(Obj, TEXT("graph"));
			const FString VN = GetString(Obj, TEXT("variable"));
			const bool bSet = GetBool(Obj, TEXT("is_set"));
			const int32 X = GetInt(Obj, TEXT("x")); const int32 Y = GetInt(Obj, TEXT("y"));
			R.NewNodeGuid = AddVariableNode(BlueprintPath, G, VN, bSet, X, Y);
			R.bSuccess = !R.NewNodeGuid.IsEmpty();
			if (!R.bSuccess) R.Message = TEXT("add_variable_node failed");
		}
		else if (R.Op == TEXT("add_node_by_class"))
		{
			const FString G = GetString(Obj, TEXT("graph"));
			const FString CP = GetString(Obj, TEXT("class"));
			const int32 X = GetInt(Obj, TEXT("x")); const int32 Y = GetInt(Obj, TEXT("y"));
			R.NewNodeGuid = AddNodeByClassName(BlueprintPath, G, CP, X, Y);
			R.bSuccess = !R.NewNodeGuid.IsEmpty();
			if (!R.bSuccess) R.Message = TEXT("add_node_by_class failed");
		}
		else if (R.Op == TEXT("connect_pins"))
		{
			const FString G  = GetString(Obj, TEXT("graph"));
			const FString SN = ResolveGuid(GetString(Obj, TEXT("src_node")), Results);
			const FString SP = GetString(Obj, TEXT("src_pin"));
			const FString DN = ResolveGuid(GetString(Obj, TEXT("dst_node")), Results);
			const FString DP = GetString(Obj, TEXT("dst_pin"));
			R.bSuccess = ConnectGraphPins(BlueprintPath, G, SN, SP, DN, DP);
			if (!R.bSuccess) R.Message = TEXT("connect_pins failed");
		}
		else if (R.Op == TEXT("set_pin_default"))
		{
			const FString G = GetString(Obj, TEXT("graph"));
			const FString N = ResolveGuid(GetString(Obj, TEXT("node")), Results);
			const FString P = GetString(Obj, TEXT("pin"));
			const FString V = GetString(Obj, TEXT("value"));
			R.bSuccess = SetPinDefaultValue(BlueprintPath, G, N, P, V);
			if (!R.bSuccess) R.Message = TEXT("set_pin_default failed");
		}
		else if (R.Op == TEXT("remove_node"))
		{
			const FString G = GetString(Obj, TEXT("graph"));
			const FString N = ResolveGuid(GetString(Obj, TEXT("node")), Results);
			R.bSuccess = RemoveGraphNode(BlueprintPath, G, N);
			if (!R.bSuccess) R.Message = TEXT("remove_node failed");
		}
		else
		{
			R.Message = FString::Printf(TEXT("unknown op '%s'"), *R.Op);
		}
		if (R.bSuccess) bAnyMutation = true;
		Results.Add(R);
	}

	// Compile once at the end if anything mutated.
	if (bAnyMutation)
	{
		FKismetEditorUtilities::CompileBlueprint(BP);
	}
	return Results;
}
TArray<FTetherCdoOverride> UTetherBlueprintLibrary::FindCdoVariableOverrides(
	const FString& DefiningBlueprintPath,
	const FString& VariableName,
	const FString& PackagePath)
{
	TArray<FTetherCdoOverride> Out;
	UBlueprint* DefBP = LoadBP(DefiningBlueprintPath);
	if (!DefBP || !DefBP->GeneratedClass) return Out;
	UClass* DefClass = DefBP->GeneratedClass;
	UObject* DefCDO  = DefClass->GetDefaultObject(false);
	if (!DefCDO) return Out;

	FProperty* Prop = FindFProperty<FProperty>(DefClass, FName(*VariableName));
	if (!Prop) return Out;

	FString ParentVal;
	Prop->ExportText_InContainer(0, ParentVal, DefCDO, DefCDO, DefCDO, PPF_None);

	// Enumerate all BP assets under PackagePath; keep the ones whose generated
	// class is a subclass of DefClass and whose CDO's value differs.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FString Root = PackagePath.IsEmpty() ? FString(TEXT("/Game")) : PackagePath;
	if (!Root.StartsWith(TEXT("/"))) Root = TEXT("/") + Root;

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(*Root));
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> BpAssets;
	Registry.GetAssets(Filter, BpAssets);

	for (const FAssetData& Data : BpAssets)
	{
		const FString Path = Data.GetSoftObjectPath().ToString();
		UBlueprint* BP = LoadBP(Path);
		if (!BP || !BP->GeneratedClass || BP == DefBP) continue;
		if (!BP->GeneratedClass->IsChildOf(DefClass)) continue;
		UObject* CDO = BP->GeneratedClass->GetDefaultObject(false);
		if (!CDO) continue;
		FProperty* ChildProp = FindFProperty<FProperty>(BP->GeneratedClass, FName(*VariableName));
		if (!ChildProp) continue;

		FString ChildVal;
		ChildProp->ExportText_InContainer(0, ChildVal, CDO, CDO, CDO, PPF_None);
		if (ChildVal == ParentVal) continue;

		FTetherCdoOverride Row;
		Row.BlueprintPath = Path;
		Row.VariableName  = VariableName;
		Row.ParentValue   = ParentVal;
		Row.ChildValue    = ChildVal;
		Out.Add(MoveTemp(Row));
	}
	return Out;
}
FString UTetherBlueprintLibrary::AddEnhancedInputActionEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& InputActionPath, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) return FString();

	// Reuse existing event node bound to the same IA — matches the behavior
	// of UInputActionEventNodeSpawner::FindExistingNode (one IA → one event
	// node per graph; second invocation just repositions the existing one).
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (UK2Node_EnhancedInputAction* Existing = Cast<UK2Node_EnhancedInputAction>(N))
		{
			if (Existing->InputAction == IA)
			{
				Existing->Modify();
				Existing->NodePosX = NodePosX;
				Existing->NodePosY = NodePosY;
				FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
				return Existing->NodeGuid.ToString(EGuidFormats::Digits);
			}
		}
	}

	Graph->Modify();
	BP->Modify();

	UK2Node_EnhancedInputAction* Node = NewObject<UK2Node_EnhancedInputAction>(Graph);
	// MUST set InputAction BEFORE AllocateDefaultPins — the ActionValue pin's
	// type is derived from IA->ValueType inside AllocateDefaultPins via
	// UK2Node_GetInputActionValue::GetValueCategory(InputAction).
	Node->InputAction = IA;
	Node->CreateNewGuid();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddGetInputActionValueNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& InputActionPath, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();

	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) return FString();

	Graph->Modify();
	BP->Modify();

	UK2Node_GetInputActionValue* Node = NewObject<UK2Node_GetInputActionValue>(Graph);
	// Same constraint as the event node: InputAction must be set BEFORE
	// AllocateDefaultPins so the output value pin is typed correctly via
	// GetValueCategory/SubCategory/SubCategoryObject(InputAction).
	Node->InputAction = IA;
	Node->CreateNewGuid();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Graph->AddNode(Node, /*bFromUI*/false, /*bSelectNewNode*/false);
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return Node->NodeGuid.ToString(EGuidFormats::Digits);
}
FTetherWireIAResult UTetherBlueprintLibrary::WireEnhancedInputActionToFunction(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& InputActionPath, const FString& TriggerEventPin,
	const FString& TargetClassPath, const FString& TargetFunctionName,
	int32 EventNodeX, int32 EventNodeY,
	int32 CallNodeX,  int32 CallNodeY,
	bool bAutoWireActionValue)
{
	FTetherWireIAResult Out;

	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) { Out.FailureReason = TEXT("blueprint not found"); return Out; }
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) { Out.FailureReason = TEXT("graph not found"); return Out; }

	UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IA) { Out.FailureReason = TEXT("input action asset not found"); return Out; }

	// Resolve target class — empty path means self (this BP's generated class).
	UClass* TargetClass = TargetClassPath.IsEmpty()
		? (UClass*)(BP->GeneratedClass ? BP->GeneratedClass : BP->ParentClass)
		: TetherBlueprintGraphWriteImpl::ResolveTargetClass(BP, TargetClassPath);
	if (!TargetClass) { Out.FailureReason = TEXT("target class not found"); return Out; }

	UFunction* Fn = TargetClass->FindFunctionByName(FName(*TargetFunctionName));
	if (!Fn) { Out.FailureReason = TEXT("target function not found on class"); return Out; }

	Graph->Modify();
	BP->Modify();

	// (1) Event node — reuse existing if same IA already on graph.
	UK2Node_EnhancedInputAction* EventNode = nullptr;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (UK2Node_EnhancedInputAction* Existing = Cast<UK2Node_EnhancedInputAction>(N))
		{
			if (Existing->InputAction == IA) { EventNode = Existing; break; }
		}
	}
	if (!EventNode)
	{
		EventNode = NewObject<UK2Node_EnhancedInputAction>(Graph);
		EventNode->InputAction = IA;
		EventNode->CreateNewGuid();
		EventNode->NodePosX = EventNodeX;
		EventNode->NodePosY = EventNodeY;
		Graph->AddNode(EventNode, false, false);
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
	}
	else
	{
		EventNode->Modify();
		EventNode->NodePosX = EventNodeX;
		EventNode->NodePosY = EventNodeY;
	}
	Out.EventNodeGuid = EventNode->NodeGuid.ToString(EGuidFormats::Digits);

	// (2) CallFunction node.
	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
	CallNode->CreateNewGuid();
	CallNode->SetFromFunction(Fn);
	CallNode->NodePosX = CallNodeX;
	CallNode->NodePosY = CallNodeY;
	Graph->AddNode(CallNode, false, false);
	CallNode->PostPlacedNewNode();
	CallNode->AllocateDefaultPins();
	Out.CallNodeGuid = CallNode->NodeGuid.ToString(EGuidFormats::Digits);

	// (3) Wire trigger exec → call exec_in.
	UEdGraphPin* TriggerPin = EventNode->FindPin(FName(*TriggerEventPin), EGPD_Output);
	if (!TriggerPin || TriggerPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
	{
		Out.FailureReason = FString::Printf(
			TEXT("trigger event pin '%s' not found on event node (or not an exec pin)"),
			*TriggerEventPin);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		return Out;
	}

	// CallFunction exec input is named "execute" (UEdGraphSchema_K2::PN_Execute).
	UEdGraphPin* CallExecIn = CallNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	if (!CallExecIn)
	{
		// Pure functions have no exec_in pin — that's a real misconfig for this helper.
		Out.FailureReason = TEXT("call function has no exec_in pin (pure function?)");
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		return Out;
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!Schema || !Schema->TryCreateConnection(TriggerPin, CallExecIn))
	{
		Out.FailureReason = TEXT("schema rejected exec connection");
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		return Out;
	}

	// (4) Optional: auto-wire the event's ActionValue → first compatible
	//     input data pin on the CallFunction. Compatible means same struct
	//     subcategory object (Vector2D ↔ Vector2D), same primitive (bool ↔
	//     bool, double ↔ double), or InputActionValue ↔ InputActionValue.
	//     Schema->TryCreateConnection will reject incompatible types so a
	//     conservative attempt is safe.
	if (bAutoWireActionValue)
	{
		if (UEdGraphPin* ActionValuePin = EventNode->FindPin(TEXT("ActionValue"), EGPD_Output))
		{
			for (UEdGraphPin* CallPin : CallNode->Pins)
			{
				if (CallPin->Direction != EGPD_Input) continue;
				if (CallPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
				if (CallPin->PinName == UEdGraphSchema_K2::PN_Self) continue;
				// Try; the schema gates on type compat. Stop at the first that takes.
				if (Schema->TryCreateConnection(ActionValuePin, CallPin))
				{
					break;
				}
			}
		}
	}

	Out.bWired = true;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return Out;
}
namespace TetherLegacyInputImpl
{
	template<typename TNode>
	static TNode* PlaceNode(UEdGraph* Graph, int32 X, int32 Y)
	{
		TNode* N = NewObject<TNode>(Graph);
		N->CreateNewGuid();
		N->NodePosX = X;
		N->NodePosY = Y;
		Graph->AddNode(N, false, false);
		N->PostPlacedNewNode();
		return N;
	}
}
FString UTetherBlueprintLibrary::AddLegacyInputActionEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& ActionName, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph || ActionName.IsEmpty()) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_InputAction* N = TetherLegacyInputImpl::PlaceNode<UK2Node_InputAction>(Graph, NodePosX, NodePosY);
	N->InputActionName = FName(*ActionName);
	N->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return N->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddLegacyInputAxisEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& AxisName, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph || AxisName.IsEmpty()) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_InputAxisEvent* N = TetherLegacyInputImpl::PlaceNode<UK2Node_InputAxisEvent>(Graph, NodePosX, NodePosY);
	// UK2Node_InputAxisEvent::Initialize() sets InputAxisName + EventReference for K2Node_Event base.
	N->Initialize(FName(*AxisName));
	N->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return N->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddInputKeyEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& KeyName, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();
	FKey K(*KeyName);
	if (!K.IsValid()) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_InputKey* N = TetherLegacyInputImpl::PlaceNode<UK2Node_InputKey>(Graph, NodePosX, NodePosY);
	N->InputKey = K;
	N->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return N->NodeGuid.ToString(EGuidFormats::Digits);
}
FString UTetherBlueprintLibrary::AddInputAxisKeyEventNode(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& AxisKeyName, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return FString();
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return FString();
	FKey K(*AxisKeyName);
	if (!K.IsValid() || !K.IsAxis1D()) return FString();
	Graph->Modify(); BP->Modify();
	UK2Node_InputAxisKeyEvent* N = TetherLegacyInputImpl::PlaceNode<UK2Node_InputAxisKeyEvent>(Graph, NodePosX, NodePosY);
	N->AxisKey = K;
	N->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return N->NodeGuid.ToString(EGuidFormats::Digits);
}
bool UTetherBlueprintLibrary::AddPawnInputBeginPlaySetup(
	const FString& BlueprintPath, const FString& IMCPath, int32 Priority,
	int32 OriginX, int32 OriginY)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, TEXT("EventGraph"));
	if (!Graph) return false;
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *IMCPath);
	if (!IMC) return false;

	// Resolve all UFUNCTIONs we need up front.
	UFunction* GetPCFn = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("GetPlayerController"));
	if (!GetPCFn) return false;
	UFunction* AddMCFn = UEnhancedInputLocalPlayerSubsystem::StaticClass()->FindFunctionByName(TEXT("AddMappingContext"));
	if (!AddMCFn) return false;

	Graph->Modify(); BP->Modify();
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!Schema) return false;

	// (1) Reuse-or-add Event ReceiveBeginPlay.
	UClass* ParentClass = (UClass*)(BP->ParentClass);
	UFunction* BeginPlayFn = ParentClass ? ParentClass->FindFunctionByName(TEXT("ReceiveBeginPlay")) : nullptr;
	if (!BeginPlayFn) return false;
	UK2Node_Event* BeginPlayNode = nullptr;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (UK2Node_Event* Ev = Cast<UK2Node_Event>(N))
		{
			if (Ev->EventReference.GetMemberName() == TEXT("ReceiveBeginPlay"))
			{
				BeginPlayNode = Ev; break;
			}
		}
	}
	if (!BeginPlayNode)
	{
		BeginPlayNode = NewObject<UK2Node_Event>(Graph);
		BeginPlayNode->CreateNewGuid();
		BeginPlayNode->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"), ParentClass);
		BeginPlayNode->bOverrideFunction = true;
		BeginPlayNode->NodePosX = OriginX;
		BeginPlayNode->NodePosY = OriginY;
		Graph->AddNode(BeginPlayNode, false, false);
		BeginPlayNode->PostPlacedNewNode();
		BeginPlayNode->AllocateDefaultPins();
	}
	else
	{
		BeginPlayNode->Modify();
		BeginPlayNode->bOverrideFunction = true;
	}

	// (2) GetPlayerController(Self, 0)
	UK2Node_CallFunction* GetPCNode = NewObject<UK2Node_CallFunction>(Graph);
	GetPCNode->CreateNewGuid();
	GetPCNode->SetFromFunction(GetPCFn);
	GetPCNode->NodePosX = OriginX + 320; GetPCNode->NodePosY = OriginY + 130;
	Graph->AddNode(GetPCNode, false, false);
	GetPCNode->PostPlacedNewNode();
	GetPCNode->AllocateDefaultPins();
	if (UEdGraphPin* IndexPin = GetPCNode->FindPin(TEXT("PlayerIndex"), EGPD_Input))
	{
		Schema->TrySetDefaultValue(*IndexPin, TEXT("0"));
	}

	// (3) GetSubsystemFromPC<UEnhancedInputLocalPlayerSubsystem>
	// UK2Node_GetSubsystemFromPC has UCLASS() (not MinimalAPI) — its
	// GetPrivateStaticClass symbol isn't exported from the BlueprintGraph DLL,
	// so NewObject<UK2Node_GetSubsystemFromPC> fails to link on 5.4-5.6 (and
	// likely 5.7 in stricter build configs). Resolve the UClass dynamically
	// via FindObject + construct via the parent type, which IS MinimalAPI'd.
	UClass* GetSubFromPCCls = FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.K2Node_GetSubsystemFromPC"));
	if (!GetSubFromPCCls)
	{
		UE_LOG(LogTemp, Error, TEXT("Tether: K2Node_GetSubsystemFromPC class not found in BlueprintGraph"));
		return false;
	}
	UK2Node_GetSubsystem* GetSubNode = NewObject<UK2Node_GetSubsystem>(Graph, GetSubFromPCCls);
	GetSubNode->Initialize(UEnhancedInputLocalPlayerSubsystem::StaticClass());
	GetSubNode->CreateNewGuid();
	GetSubNode->NodePosX = OriginX + 640; GetSubNode->NodePosY = OriginY + 130;
	Graph->AddNode(GetSubNode, false, false);
	GetSubNode->PostPlacedNewNode();
	GetSubNode->AllocateDefaultPins();

	// (4) AddMappingContext(IMC, Priority)
	UK2Node_CallFunction* AddMCNode = NewObject<UK2Node_CallFunction>(Graph);
	AddMCNode->CreateNewGuid();
	AddMCNode->SetFromFunction(AddMCFn);
	AddMCNode->NodePosX = OriginX + 960; AddMCNode->NodePosY = OriginY;
	Graph->AddNode(AddMCNode, false, false);
	AddMCNode->PostPlacedNewNode();
	AddMCNode->AllocateDefaultPins();
	if (UEdGraphPin* MCPin = AddMCNode->FindPin(TEXT("MappingContext"), EGPD_Input))
	{
		// For object/asset pins, set DefaultObject only — leave DefaultValue
		// empty. The K2 schema treats DefaultValue on object pins as a
		// fallback string parse and rejects "IMC_Sandbox" because that's
		// not a valid asset path; symptom is a compile error
		// 'String NewDefaultValue 'X' specified on object pin'.
		MCPin->DefaultObject = IMC;
		MCPin->DefaultValue = FString();
	}
	if (UEdGraphPin* PrioPin = AddMCNode->FindPin(TEXT("Priority"), EGPD_Input))
	{
		Schema->TrySetDefaultValue(*PrioPin, FString::FromInt(Priority));
	}

	// Wires:
	//   BeginPlay.then -> AddMC.exec_in
	//   GetPC.return  -> GetSub.PlayerController
	//   GetSub.return -> AddMC.self
	UEdGraphPin* BeginThen = BeginPlayNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
	UEdGraphPin* AddMCExec = AddMCNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	UEdGraphPin* GetPCRet = GetPCNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output);
	UEdGraphPin* GetSubPCIn = GetSubNode->FindPin(TEXT("PlayerController"), EGPD_Input);
	UEdGraphPin* GetSubRet = GetSubNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output);
	UEdGraphPin* AddMCSelf = AddMCNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);

	bool bAllOk = true;
	if (BeginThen && AddMCExec) bAllOk &= Schema->TryCreateConnection(BeginThen, AddMCExec);
	if (GetPCRet && GetSubPCIn) bAllOk &= Schema->TryCreateConnection(GetPCRet, GetSubPCIn);
	if (GetSubRet && AddMCSelf) bAllOk &= Schema->TryCreateConnection(GetSubRet, AddMCSelf);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return bAllOk;
}

TArray<FTetherVariableInfo> UTetherBlueprintLibrary::GetFunctionLocalVariables(
	const FString& BlueprintPath, const FString& FunctionName)
{
	using namespace TetherBPLocalVarImpl;
	TArray<FTetherVariableInfo> Out;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return Out;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return Out;

	const UStruct* Scope = FindFunctionScope(BP, Graph);
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		Out.Add(MakeLocalVarInfo(V, Scope));
	}
	return Out;
}
bool UTetherBlueprintLibrary::AddFunctionLocalVariable(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& VariableName, const FString& TypeString, const FString& DefaultValue)
{
	using namespace TetherBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return false;

	const FName VarFName(*VariableName);
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == VarFName) return false;
	}

	FEdGraphPinType PinType;
	if (!TetherTypeParseImpl::ParseTypeString(TypeString, PinType)) return false;

	// FBlueprintEditorUtils::AddLocalVariable handles the full propagation
	// (entry-node modify, MarkBlueprintAsModified, variable visibility).
	FBlueprintEditorUtils::AddLocalVariable(BP, Graph, VarFName, PinType, DefaultValue);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::RemoveFunctionLocalVariable(
	const FString& BlueprintPath, const FString& FunctionName, const FString& VariableName)
{
	using namespace TetherBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return false;

	const FName VarFName(*VariableName);
	const UStruct* Scope = FindFunctionScope(BP, Graph);

	bool bFound = false;
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == VarFName) { bFound = true; break; }
	}
	if (!bFound) return false;

	if (Scope)
	{
		FBlueprintEditorUtils::RemoveLocalVariable(BP, Scope, VarFName);
	}
	else
	{
		// Fallback: scope resolution failed (skeleton not compiled yet) — drop
		// the entry directly. Safe because LocalVariables is the source of truth.
		Entry->Modify();
		Entry->LocalVariables.RemoveAll([&](const FBPVariableDescription& V)
		{
			return V.VarName == VarFName;
		});
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::RenameFunctionLocalVariable(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& OldName, const FString& NewName)
{
	using namespace TetherBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return false;

	const FName OldFName(*OldName);
	const FName NewFName(*NewName);
	if (OldFName == NewFName || NewFName.IsNone()) return false;

	bool bFoundOld = false;
	for (const FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == OldFName) { bFoundOld = true; }
		if (V.VarName == NewFName) { return false; }
	}
	if (!bFoundOld) return false;

	const UStruct* Scope = FindFunctionScope(BP, Graph);
	if (Scope)
	{
		FBlueprintEditorUtils::RenameLocalVariable(BP, Scope, OldFName, NewFName);
	}
	else
	{
		Entry->Modify();
		for (FBPVariableDescription& V : Entry->LocalVariables)
		{
			if (V.VarName == OldFName) { V.VarName = NewFName; break; }
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::SetFunctionLocalVariableDefault(
	const FString& BlueprintPath, const FString& FunctionName,
	const FString& VariableName, const FString& Value)
{
	using namespace TetherBPLocalVarImpl;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = FindFunctionGraph(BP, FunctionName);
	if (!Graph) return false;
	UK2Node_FunctionEntry* Entry = FindFunctionEntry(Graph);
	if (!Entry) return false;

	const FName VarFName(*VariableName);
	bool bFound = false;
	Entry->Modify();
	for (FBPVariableDescription& V : Entry->LocalVariables)
	{
		if (V.VarName == VarFName)
		{
			V.DefaultValue = Value;
			bFound = true;
			break;
		}
	}
	if (!bFound) return false;

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::AddBlueprintInterface(
	const FString& BlueprintPath, const FString& InterfacePath)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UClass* InterfaceClass = TetherBpInterfaceOps::ResolveInterfaceClass(InterfacePath);
	if (!InterfaceClass || !InterfaceClass->HasAnyClassFlags(CLASS_Interface))
	{
		return false;
	}

	const FString InterfaceClassName = InterfaceClass->GetPathName();
	if (!FBlueprintEditorUtils::ImplementNewInterface(BP, FTopLevelAssetPath(InterfaceClassName)))
	{
		return false;
	}
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::RemoveBlueprintInterface(
	const FString& BlueprintPath, const FString& InterfaceNameOrPath)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UClass* InterfaceClass = TetherBpInterfaceOps::ResolveInterfaceClass(InterfaceNameOrPath);

	// Fallback: match by short name against implemented interfaces.
	if (!InterfaceClass)
	{
		for (const FBPInterfaceDescription& Impl : BP->ImplementedInterfaces)
		{
			if (Impl.Interface && Impl.Interface->GetName() == InterfaceNameOrPath)
			{
				InterfaceClass = Impl.Interface;
				break;
			}
		}
	}
	if (!InterfaceClass) return false;

	const bool bWasImplemented = BP->ImplementedInterfaces.ContainsByPredicate(
		[InterfaceClass](const FBPInterfaceDescription& D) { return D.Interface == InterfaceClass; });
	if (!bWasImplemented) return false;

	FBlueprintEditorUtils::RemoveInterface(BP, FTopLevelAssetPath(InterfaceClass->GetPathName()), /*bPreserveFunctions*/ false);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return true;
}
bool UTetherBlueprintLibrary::ReparentComponent(
	const FString& BlueprintPath, const FString& ComponentName, const FString& NewParentName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	if (!BP->SimpleConstructionScript) return false;

	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	USCS_Node* Node = SCS->FindSCSNode(FName(*ComponentName)); if (!Node) return false;

	// Detach from current parent (or root).
	USCS_Node* CurParent = TetherBpP1Impl::FindSCSParent(SCS, Node);
	BP->Modify();
	if (CurParent) { CurParent->Modify(); CurParent->RemoveChildNode(Node, /*bRemoveFromAllNodes*/false); }
	else           { SCS->Modify();        SCS->RemoveNode(Node, /*bValidateSceneRoot*/false); }

	// Attach to new parent, or promote to root.
	if (NewParentName.IsEmpty())
	{
		SCS->AddNode(Node);
	}
	else
	{
		USCS_Node* NewParent = SCS->FindSCSNode(FName(*NewParentName));
		if (!NewParent)
		{
			// Roll back: re-add as root so we don't orphan.
			SCS->AddNode(Node);
			return false;
		}
		NewParent->Modify();
		NewParent->AddChildNode(Node, /*bAddToAllNodes*/false);
	}
	SCS->ValidateSceneRootNodes();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::ReorderComponent(
	const FString& BlueprintPath, const FString& ComponentName, int32 NewIndex)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	if (!BP->SimpleConstructionScript) return false;

	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	USCS_Node* Node = SCS->FindSCSNode(FName(*ComponentName)); if (!Node) return false;

	USCS_Node* Parent = TetherBpP1Impl::FindSCSParent(SCS, Node);
	BP->Modify();

	// Acquire the current sibling list reference.
	const TArray<USCS_Node*>& Siblings = Parent ? Parent->GetChildNodes() : SCS->GetRootNodes();
	const int32 Count = Siblings.Num();
	if (Count == 0) return false;
	const int32 Clamped = FMath::Clamp(NewIndex, 0, Count - 1);

	if (Parent)
	{
		Parent->Modify();
		Parent->RemoveChildNode(Node, /*bRemoveFromAllNodes*/false);
		// Re-insert at new position.
		Parent->AddChildNode(Node, /*bAddToAllNodes*/false);
		// AddChildNode appends — pull to desired index.
		// Access mutable via const_cast since API doesn't expose mutator directly.
		TArray<USCS_Node*>& MChildren = const_cast<TArray<USCS_Node*>&>(Parent->GetChildNodes());
		MChildren.Remove(Node);
		MChildren.Insert(Node, Clamped);
	}
	else
	{
		SCS->Modify();
		SCS->RemoveNode(Node, /*bValidateSceneRoot*/false);
		SCS->AddNode(Node);
		TArray<USCS_Node*>& MRoots = const_cast<TArray<USCS_Node*>&>(SCS->GetRootNodes());
		MRoots.Remove(Node);
		MRoots.Insert(Node, Clamped);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::RemoveComponent(
	const FString& BlueprintPath, const FString& ComponentName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	if (!BP->SimpleConstructionScript) return false;
	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
	USCS_Node* Node = SCS->FindSCSNode(FName(*ComponentName)); if (!Node) return false;

	BP->Modify(); SCS->Modify();
	USCS_Node* Parent = TetherBpP1Impl::FindSCSParent(SCS, Node);
	if (Parent) { Parent->Modify(); Parent->RemoveChildNode(Node, /*bRemoveFromAllNodes*/true); }
	else        { SCS->RemoveNode(Node); }
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
}
