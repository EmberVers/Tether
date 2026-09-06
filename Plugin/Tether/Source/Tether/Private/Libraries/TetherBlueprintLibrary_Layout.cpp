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
bool UTetherBlueprintLibrary::SetGraphNodePosition(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, int32 NodePosX, int32 NodePosY)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;

	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;

	UEdGraphNode* Node = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;

	Node->Modify();
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}
bool UTetherBlueprintLibrary::AlignNodes(
	const FString& BlueprintPath, const FString& GraphName,
	const TArray<FString>& NodeGuids, const FString& Axis)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	if (NodeGuids.Num() < 2) return false;

	TArray<UEdGraphNode*> Nodes;
	for (const FString& G : NodeGuids)
	{
		if (UEdGraphNode* N = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, G)) Nodes.Add(N);
	}
	if (Nodes.Num() < 2) return false;

	Graph->Modify();
	for (UEdGraphNode* N : Nodes) N->Modify();

	const FString A = Axis.TrimStartAndEnd();
	if (A.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
	{
		int32 Min = Nodes[0]->NodePosX;
		for (UEdGraphNode* N : Nodes) Min = FMath::Min(Min, N->NodePosX);
		for (UEdGraphNode* N : Nodes) N->NodePosX = Min;
	}
	else if (A.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
	{
		int32 Max = Nodes[0]->NodePosX;
		for (UEdGraphNode* N : Nodes) Max = FMath::Max(Max, N->NodePosX);
		for (UEdGraphNode* N : Nodes) N->NodePosX = Max;
	}
	else if (A.Equals(TEXT("Top"), ESearchCase::IgnoreCase))
	{
		int32 Min = Nodes[0]->NodePosY;
		for (UEdGraphNode* N : Nodes) Min = FMath::Min(Min, N->NodePosY);
		for (UEdGraphNode* N : Nodes) N->NodePosY = Min;
	}
	else if (A.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase))
	{
		int32 Max = Nodes[0]->NodePosY;
		for (UEdGraphNode* N : Nodes) Max = FMath::Max(Max, N->NodePosY);
		for (UEdGraphNode* N : Nodes) N->NodePosY = Max;
	}
	else if (A.Equals(TEXT("CenterHorizontal"), ESearchCase::IgnoreCase))
	{
		int64 Sum = 0; for (UEdGraphNode* N : Nodes) Sum += N->NodePosX;
		int32 Avg = (int32)(Sum / Nodes.Num());
		for (UEdGraphNode* N : Nodes) N->NodePosX = Avg;
	}
	else if (A.Equals(TEXT("CenterVertical"), ESearchCase::IgnoreCase))
	{
		int64 Sum = 0; for (UEdGraphNode* N : Nodes) Sum += N->NodePosY;
		int32 Avg = (int32)(Sum / Nodes.Num());
		for (UEdGraphNode* N : Nodes) N->NodePosY = Avg;
	}
	else if (A.Equals(TEXT("DistributeHorizontal"), ESearchCase::IgnoreCase))
	{
		Nodes.Sort([](const UEdGraphNode& L, const UEdGraphNode& R){ return L.NodePosX < R.NodePosX; });
		const int32 First = Nodes[0]->NodePosX;
		const int32 Last  = Nodes.Last()->NodePosX;
		const int32 N     = Nodes.Num();
		for (int32 i = 1; i < N - 1; ++i) Nodes[i]->NodePosX = First + (Last - First) * i / (N - 1);
	}
	else if (A.Equals(TEXT("DistributeVertical"), ESearchCase::IgnoreCase))
	{
		Nodes.Sort([](const UEdGraphNode& L, const UEdGraphNode& R){ return L.NodePosY < R.NodePosY; });
		const int32 First = Nodes[0]->NodePosY;
		const int32 Last  = Nodes.Last()->NodePosY;
		const int32 N     = Nodes.Num();
		for (int32 i = 1; i < N - 1; ++i) Nodes[i]->NodePosY = First + (Last - First) * i / (N - 1);
	}
	else
	{
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return true;
}

namespace TetherBPSummaryImpl
{
	// Size-estimation helpers (EstimateNodeSize / ComputePinLocalY) are declared in
	// TetherBlueprintImpl_Shared.h together with the layout constants; this TU is
	// their definition site. FVisiblePinTally / TallyVisiblePins / IsCompactK2Node
	// stay TU-local.
	struct FVisiblePinTally
	{
		int32 InputCount = 0;
		int32 OutputCount = 0;
		int32 LongestLabel = 0;
	};

	static FVisiblePinTally TallyVisiblePins(const UEdGraphNode* Node)
	{
		FVisiblePinTally Out;
		if (!Node) return Out;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->bHidden) continue;
			if (Pin->Direction == EGPD_Input)  Out.InputCount  += 1;
			else                                Out.OutputCount += 1;
			const int32 Len = Pin->GetDisplayName().ToString().Len();
			if (Len > Out.LongestLabel) Out.LongestLabel = Len;
		}
		return Out;
	}

	void EstimateNodeSize(const UEdGraphNode* Node, int32& W, int32& H)
	{
		if (!Node) { W = MinNodeWidth; H = MinNodeHeight; return; }
		const FVisiblePinTally Tally = TallyVisiblePins(Node);
		const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		// Char-width heuristic: ~7 px per char in default font. Title bar
		// + longest pin label on either side plus padding drives width.
		const int32 TitleWidth = Title.Len() * 7 + 40;
		const int32 PinWidth   = Tally.LongestLabel * 7 + 80;  // both sides get padding
		W = FMath::Clamp(FMath::Max(TitleWidth, PinWidth), MinNodeWidth, MaxNodeWidth);
		// Height: header + max-rows per side * row height + footer.
		const int32 RowCount = FMath::Max(Tally.InputCount, Tally.OutputCount);
		H = FMath::Max(MinNodeHeight, HeaderHeight + RowCount * PinRowHeight + FooterHeight);
	}

	static bool IsCompactK2Node(const UEdGraphNode* Node)
	{
		const UK2Node* K2 = Cast<const UK2Node>(Node);
		return K2 && K2->ShouldDrawCompact();
	}

	int32 ComputePinLocalY(const UEdGraphNode* Node,
		EEdGraphPinDirection Dir, int32 DirIdx)
	{
		int32 Y = HeaderHeight + DirIdx * PinRowHeight;
		if (IsCompactK2Node(Node))
		{
			const FVisiblePinTally Tally = TallyVisiblePins(Node);
			const int32 MyCount    = (Dir == EGPD_Input) ? Tally.InputCount : Tally.OutputCount;
			const int32 OtherCount = (Dir == EGPD_Input) ? Tally.OutputCount : Tally.InputCount;
			const int32 MaxCount   = FMath::Max(MyCount, OtherCount);
			if (MyCount > 0 && MyCount < MaxCount)
			{
				// Center this direction's pin column inside the taller one:
				// shift down by (MaxCount - MyCount) * PinRowHeight / 2.
				Y += (MaxCount - MyCount) * PinRowHeight / 2;
			}
		}
		return Y;
	}
}

FTetherNodeLayout UTetherBlueprintLibrary::GetNodeLayout(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	using namespace TetherBPSummaryImpl;
	FTetherNodeLayout Out;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = FindSingleGraphByName(BP, GraphName);
	if (!Graph) return Out;
	UEdGraphNode* Node = FindNodeInGraphByGuid(Graph, NodeGuid);
	if (!Node) return Out;

	Out.PosX         = Node->NodePosX;
	Out.PosY         = Node->NodePosY;
	Out.StoredWidth  = Node->NodeWidth;
	Out.StoredHeight = Node->NodeHeight;

	Out.bIsCommentBox = Node->IsA<UEdGraphNode_Comment>();

	int32 EstW = 0, EstH = 0;
	EstimateNodeSize(Node, EstW, EstH);
	Out.EstimatedWidth  = EstW;
	Out.EstimatedHeight = EstH;

	// Use stored if sane (>0). Comments always have authored dims.
	const bool bStoredValid = (Out.StoredWidth > 0 && Out.StoredHeight > 0);
	Out.bSizeIsAuthoritative = bStoredValid || Out.bIsCommentBox;
	Out.EffectiveWidth  = bStoredValid ? Out.StoredWidth  : Out.EstimatedWidth;
	Out.EffectiveHeight = bStoredValid ? Out.StoredHeight : Out.EstimatedHeight;

	const float X = static_cast<float>(Out.PosX);
	const float Y = static_cast<float>(Out.PosY);
	const float W = static_cast<float>(Out.EffectiveWidth);
	const float H = static_cast<float>(Out.EffectiveHeight);
	Out.TopLeft     = FVector2D(X,     Y);
	Out.TopRight    = FVector2D(X + W, Y);
	Out.BottomLeft  = FVector2D(X,     Y + H);
	Out.BottomRight = FVector2D(X + W, Y + H);
	Out.Center      = FVector2D(X + W * 0.5f, Y + H * 0.5f);
	return Out;
}

TArray<FTetherPinLayout> UTetherBlueprintLibrary::GetNodePinLayouts(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	using namespace TetherBPSummaryImpl;
	TArray<FTetherPinLayout> Out;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = FindSingleGraphByName(BP, GraphName);
	if (!Graph) return Out;
	UEdGraphNode* Node = FindNodeInGraphByGuid(Graph, NodeGuid);
	if (!Node) return Out;

	// Layout derived from EffectiveWidth for correct right-edge position.
	int32 EstW = 0, EstH = 0;
	EstimateNodeSize(Node, EstW, EstH);
	const int32 EffWidth = (Node->NodeWidth > 0) ? Node->NodeWidth : EstW;

	int32 InputIdx = 0;
	int32 OutputIdx = 0;
	const float Ox = static_cast<float>(Node->NodePosX);
	const float Oy = static_cast<float>(Node->NodePosY);

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;
		FTetherPinLayout L;
		L.Name      = Pin->PinName.ToString();
		L.Direction = (Pin->Direction == EGPD_Input) ? TEXT("input") : TEXT("output");
		L.bIsExec   = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
		L.bIsHidden = Pin->bHidden;
		L.bIsEstimated = true;

		// Hidden pins keep index -1 and local offset = (0, 0) — they don't
		// occupy a visible row. Still surfaced so callers see the full list.
		if (Pin->bHidden)
		{
			L.DirectionIndex = -1;
			L.LocalOffset    = FVector2D::ZeroVector;
			L.Position       = FVector2D(Ox, Oy);
			Out.Add(L);
			continue;
		}

		const int32 DirIdx = (Pin->Direction == EGPD_Input) ? InputIdx++ : OutputIdx++;
		L.DirectionIndex = DirIdx;
		const float Lx = (Pin->Direction == EGPD_Input)
			? static_cast<float>(PinInsetX)
			: static_cast<float>(EffWidth - PinInsetX);
		const float Ly = static_cast<float>(ComputePinLocalY(Node, Pin->Direction, DirIdx));
		L.LocalOffset = FVector2D(Lx, Ly);
		L.Position    = FVector2D(Ox + Lx, Oy + Ly);
		Out.Add(L);
	}
	return Out;
}

bool UTetherBlueprintLibrary::OpenFunctionGraphForRender(
	const FString& BlueprintPath, const FString& GraphName)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return false;
	// Open the Blueprint editor, then ask its BlueprintEditor instance to
	// open the specific graph tab. Regular BringKismetToFocusAttentionOnObject
	// opens the editor but can leave the tab stale; OpenGraphAndBringToFront
	// forces the graph's SGraphEditor widget to be constructed or focused.
	UAssetEditorSubsystem* Sub = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!Sub) return false;
	Sub->OpenEditorForAsset(BP);
	IAssetEditorInstance* Inst = Sub->FindEditorForAsset(BP, false);
	if (!Inst) return false;
	FBlueprintEditor* BPEd = static_cast<FBlueprintEditor*>(Inst);
	TSharedPtr<SGraphEditor> GraphEd = BPEd->OpenGraphAndBringToFront(Graph);
	return GraphEd.IsValid();
}

TArray<FTetherRenderedNode> UTetherBlueprintLibrary::GetRenderedNodeInfo(
	const FString& BlueprintPath, const FString& GraphName)
{
	TArray<FTetherRenderedNode> Out;

	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) return Out;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) return Out;

	UAssetEditorSubsystem* Sub = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!Sub) return Out;
	IAssetEditorInstance* Inst = Sub->FindEditorForAsset(BP, /*bFocusIfOpen=*/false);
	if (!Inst) return Out;
	FBlueprintEditor* BPEd = static_cast<FBlueprintEditor*>(Inst);

	// Resolve the SGraphEditor for the requested graph. If it happens to be
	// the currently-focused graph, use it directly; otherwise ask the
	// BlueprintEditor to open it — returns the same widget if already open.
	TSharedPtr<SGraphEditor> GraphEd;
	if (BPEd->GetFocusedGraph() == Graph)
	{
		// Access via editor's focused pointer is internal; simpler to just
		// call OpenGraphAndBringToFront which returns the live widget and is
		// idempotent for an already-open graph.
		GraphEd = BPEd->OpenGraphAndBringToFront(Graph);
	}
	else
	{
		GraphEd = BPEd->OpenGraphAndBringToFront(Graph);
	}
	if (!GraphEd.IsValid()) return Out;

	SGraphPanel* Panel = GraphEd->GetGraphPanel();
	if (!Panel) return Out;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;
		FTetherRenderedNode R;
		R.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
		R.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		R.GraphPosition = FVector2D(Node->NodePosX, Node->NodePosY);

		TSharedPtr<SGraphNode> NodeWidget = Panel->GetNodeWidgetFromGuid(Node->NodeGuid);
		if (NodeWidget.IsValid())
		{
			// Desired size = what Slate plans to render the node at; this is
			// the authoritative pre-layout geometry. Cached geometry (post-
			// layout) would be equivalent once the panel has ticked, but
			// desired size is always valid after widget construction.
			const FVector2D Desired = FVector2D(NodeWidget->GetDesiredSize());
			R.Size = Desired;
			R.bIsLive = true;

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;
				FTetherRenderedPin P;
				P.Name       = Pin->PinName.ToString();
				P.Direction  = (Pin->Direction == EGPD_Input) ? TEXT("input") : TEXT("output");
				P.bIsExec    = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
				P.bIsHidden  = Pin->bHidden;

				// Compute direction-index (visible pins only) to match the
				// layout helpers' convention.
				int32 DirIdx = 0;
				for (UEdGraphPin* Q : Node->Pins)
				{
					if (!Q || Q->bHidden) continue;
					if (Q->Direction != Pin->Direction) continue;
					if (Q == Pin) break;
					DirIdx += 1;
				}
				P.DirectionIndex = DirIdx;

				TSharedPtr<SGraphPin> PinWidget = NodeWidget->FindWidgetForPin(Pin);
				if (PinWidget.IsValid())
				{
					const FVector2D NodeOff = FVector2D(PinWidget->GetNodeOffset());
					P.NodeOffset    = NodeOff;
					P.GraphPosition = R.GraphPosition + NodeOff;
				}
				R.Pins.Add(P);
			}
		}
		Out.Add(R);
	}
	return Out;
}
// ─── Pre-spawn size prediction + graph auto-layout ─────────────

namespace TetherBPLayoutImpl
{
	using namespace TetherBPSummaryImpl;

	/** Apply the shared size formula once pin counts + title are known. */
	static void ApplySizeFormula(
		int32 InputCount, int32 OutputCount,
		int32 LongestLabelLen, const FString& Title,
		int32& OutW, int32& OutH)
	{
		const int32 TitleWidth = Title.Len() * 7 + 40;
		const int32 PinWidth   = LongestLabelLen * 7 + 80;
		OutW = FMath::Clamp(FMath::Max(TitleWidth, PinWidth), MinNodeWidth, MaxNodeWidth);
		const int32 RowCount = FMath::Max(InputCount, OutputCount);
		OutH = FMath::Max(MinNodeHeight, HeaderHeight + RowCount * PinRowHeight + FooterHeight);
	}

	/** Count parameters on a UFunction as they'd appear on a K2Node_CallFunction. */
	static void TallyFunctionParams(
		const UFunction* Fn, int32& InPins, int32& OutPins, int32& LongestLabel)
	{
		if (!Fn) return;
		const bool bPure = Fn->HasAnyFunctionFlags(FUNC_BlueprintPure);
		if (!bPure)
		{
			InPins  += 1;  // exec in
			OutPins += 1;  // exec out (then)
		}
		if (!Fn->HasAnyFunctionFlags(FUNC_Static))
		{
			InPins += 1;  // self
			if (4 > LongestLabel) LongestLabel = 4;
		}
		for (TFieldIterator<FProperty> It(Fn); It; ++It)
		{
			const FProperty* P = *It;
			if (!P || !P->HasAnyPropertyFlags(CPF_Parm)) continue;
			const bool bReturn = P->HasAnyPropertyFlags(CPF_ReturnParm);
			const bool bOut    = P->HasAnyPropertyFlags(CPF_OutParm)
			                  && !P->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (bReturn || bOut) OutPins += 1;
			else                 InPins  += 1;
			const int32 Len = P->GetName().Len();
			if (Len > LongestLabel) LongestLabel = Len;
		}
	}

	/** Resolve a variable's name length for a get/set node title. */
	static int32 VariableTitleLen(UBlueprint* BP, const FString& VarName)
	{
		// K2Node_VariableGet / Set titles are just the variable name.
		return VarName.Len();
	}

	/** Width-of-member labels heuristic for a struct by path. */
	static void TallyStructMembers(
		const FString& StructPath, int32& MemberCount, int32& LongestLabel)
	{
		if (StructPath.IsEmpty()) return;
		UScriptStruct* S = FindObject<UScriptStruct>(nullptr, *StructPath);
		if (!S) S = LoadObject<UScriptStruct>(nullptr, *StructPath);
		if (!S) return;
		for (TFieldIterator<FProperty> It(S); It; ++It)
		{
			const FProperty* P = *It;
			if (!P) continue;
			MemberCount += 1;
			const int32 Len = P->GetName().Len();
			if (Len > LongestLabel) LongestLabel = Len;
		}
	}
}

FTetherNodeSizeEstimate UTetherBlueprintLibrary::PredictNodeSize(
	const FString& Kind, const FString& ParamA,
	const FString& ParamB, int32 ParamInt)
{
	using namespace TetherBPLayoutImpl;
	using namespace TetherBPSummaryImpl;

	FTetherNodeSizeEstimate Out;
	Out.Kind = Kind;
	Out.Width = MinNodeWidth;
	Out.Height = MinNodeHeight;

	auto Finish = [&](int32 InCount, int32 OutCount, int32 LongestLabel, const FString& Title)
	{
		int32 W = MinNodeWidth, H = MinNodeHeight;
		ApplySizeFormula(InCount, OutCount, LongestLabel, Title, W, H);
		Out.Width = W;
		Out.Height = H;
		Out.InputPinCount = InCount;
		Out.OutputPinCount = OutCount;
		Out.bResolved = true;
	};

	const int32 ClampedInt = FMath::Max(0, ParamInt);
	const FString LowerKind = Kind.ToLower();

	if (LowerKind == TEXT("function_call") || LowerKind == TEXT("event"))
	{
		UClass* Target = nullptr;
		if (!ParamA.IsEmpty())
		{
			Target = FindObject<UClass>(nullptr, *ParamA);
			if (!Target) Target = LoadObject<UClass>(nullptr, *ParamA);
		}
		UFunction* Fn = Target ? Target->FindFunctionByName(FName(*ParamB)) : nullptr;
		if (!Fn)
		{
			Out.Notes = TEXT("function not found — fallback default");
			Out.Width = 220; Out.Height = 80;
			return Out;
		}
		int32 In = 0, OutP = 0, LongestLabel = ParamB.Len();
		TallyFunctionParams(Fn, In, OutP, LongestLabel);
		if (LowerKind == TEXT("event"))
		{
			// Event entry nodes: no exec-in, only exec-out + data outs for params.
			In = 0;
			OutP = 1;  // exec out
			LongestLabel = ParamB.Len();
			for (TFieldIterator<FProperty> It(Fn); It; ++It)
			{
				const FProperty* P = *It;
				if (!P || !P->HasAnyPropertyFlags(CPF_Parm)) continue;
				if (P->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
				OutP += 1;
				const int32 L = P->GetName().Len();
				if (L > LongestLabel) LongestLabel = L;
			}
		}
		Finish(In, OutP, LongestLabel, ParamB);
		return Out;
	}

	if (LowerKind == TEXT("variable_get"))
	{
		const int32 Len = VariableTitleLen(nullptr, ParamB);
		Finish(/*in*/0, /*out*/1, Len, ParamB);
		return Out;
	}
	if (LowerKind == TEXT("variable_set"))
	{
		const int32 Len = VariableTitleLen(nullptr, ParamB);
		Finish(/*in*/2, /*out*/1, Len, FString::Printf(TEXT("SET %s"), *ParamB));
		return Out;
	}
	if (LowerKind == TEXT("custom_event"))
	{
		Finish(/*in*/0, /*out*/1 + ClampedInt, 8, ParamA.IsEmpty() ? TEXT("CustomEvent") : ParamA);
		return Out;
	}
	if (LowerKind == TEXT("branch"))
	{
		Finish(2, 2, 5, TEXT("Branch"));
		return Out;
	}
	if (LowerKind == TEXT("sequence"))
	{
		const int32 ThenCount = FMath::Max(2, ClampedInt);
		Finish(1, ThenCount, 5, TEXT("Sequence"));
		return Out;
	}
	if (LowerKind == TEXT("cast"))
	{
		FString Title = TEXT("Cast To ");
		if (!ParamA.IsEmpty())
		{
			// Use last path segment for brevity.
			int32 Dot = INDEX_NONE;
			ParamA.FindLastChar(TEXT('.'), Dot);
			Title += (Dot != INDEX_NONE ? ParamA.RightChop(Dot + 1) : ParamA);
		}
		Finish(2, 3, 8, Title);
		return Out;
	}
	if (LowerKind == TEXT("self"))       { Finish(0, 1, 4, TEXT("Self")); return Out; }
	if (LowerKind == TEXT("reroute"))
	{
		// Reroute renders as a small diamond in the editor, but NodeWidth/Height
		// follow the shared estimate formula (min-size for a 1-in/1-out node).
		// Match what GetNodeLayout's estimated path returns so Predict+Actual agree.
		Finish(1, 1, 0, TEXT(""));
		return Out;
	}
	if (LowerKind == TEXT("delay"))      { Finish(2, 1, 10, TEXT("Delay")); return Out; }
	if (LowerKind == TEXT("foreach"))    { Finish(3, 4, 14, TEXT("ForEachLoop")); return Out; }
	if (LowerKind == TEXT("forloop"))    { Finish(3, 3, 10, TEXT("ForLoop")); return Out; }
	if (LowerKind == TEXT("whileloop"))  { Finish(2, 2, 10, TEXT("WhileLoop")); return Out; }
	if (LowerKind == TEXT("select"))
	{
		const int32 Opts = FMath::Max(2, ClampedInt);
		Finish(1 + Opts, 1, 8, TEXT("Select"));
		return Out;
	}
	if (LowerKind == TEXT("make_array"))
	{
		const int32 Elems = FMath::Max(1, ClampedInt);
		Finish(Elems, 1, 6, TEXT("Make Array"));
		return Out;
	}
	if (LowerKind == TEXT("make_struct") || LowerKind == TEXT("break_struct"))
	{
		int32 Members = 0, Longest = 0;
		TallyStructMembers(ParamA, Members, Longest);
		if (Members == 0) { Out.Notes = TEXT("struct not found — fallback"); Members = 3; Longest = 8; }
		FString TitlePrefix = (LowerKind == TEXT("make_struct")) ? TEXT("Make ") : TEXT("Break ");
		int32 Dot = INDEX_NONE;
		ParamA.FindLastChar(TEXT('.'), Dot);
		FString ShortName = (Dot != INDEX_NONE) ? ParamA.RightChop(Dot + 1) : ParamA;
		if (LowerKind == TEXT("make_struct"))  Finish(Members, 1, Longest, TitlePrefix + ShortName);
		else                                    Finish(1, Members, Longest, TitlePrefix + ShortName);
		return Out;
	}
	if (LowerKind == TEXT("enum_literal"))
	{
		int32 Dot = INDEX_NONE;
		ParamA.FindLastChar(TEXT('.'), Dot);
		FString Short = (Dot != INDEX_NONE) ? ParamA.RightChop(Dot + 1) : ParamA;
		Finish(0, 1, Short.Len(), Short);
		return Out;
	}
	if (LowerKind == TEXT("make_literal"))
	{
		Finish(0, 1, ParamA.Len(), FString::Printf(TEXT("Literal %s"), *ParamA));
		return Out;
	}
	if (LowerKind == TEXT("spawn_actor"))
	{
		int32 Dot = INDEX_NONE;
		ParamA.FindLastChar(TEXT('.'), Dot);
		FString Short = (Dot != INDEX_NONE) ? ParamA.RightChop(Dot + 1) : ParamA;
		// SpawnActorFromClass: exec in, class, transform, collision, owner; out exec + return.
		Finish(5, 2, 16, FString::Printf(TEXT("SpawnActor %s"), *Short));
		return Out;
	}
	if (LowerKind == TEXT("dispatcher_call"))
	{
		Finish(2, 1, ParamB.Len(), FString::Printf(TEXT("Call %s"), *ParamB));
		return Out;
	}
	if (LowerKind == TEXT("dispatcher_bind"))
	{
		Finish(3, 1, ParamB.Len(), FString::Printf(TEXT("Bind %s"), *ParamB));
		return Out;
	}
	if (LowerKind == TEXT("dispatcher_event"))
	{
		Finish(0, 1, ParamB.Len(), FString::Printf(TEXT("Event %s"), *ParamB));
		return Out;
	}

	Out.Notes = TEXT("unknown kind — fallback default");
	Out.Width = MinNodeWidth;
	Out.Height = MinNodeHeight;
	return Out;
}

// ─── AutoLayoutGraph (Sugiyama-lite) ─────────────────────────

namespace TetherBPAutoLayoutImpl
{
	using namespace TetherBPSummaryImpl;

	struct FLayoutNode
	{
		UEdGraphNode* Node = nullptr;
		int32 Width = MinNodeWidth;
		int32 Height = MinNodeHeight;
		int32 Layer = -1;
		int32 OrderInLayer = 0;
		TArray<int32> ExecSuccessors;   // indices into nodes array
		TArray<int32> ExecPredecessors;
		TArray<int32> DataSuccessors;   // for pure-node pull-in
		bool bHasExecPins = false;
	};

	static bool PinIsExec(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	/** Build layout graph. Exec edges drive layering; data edges used for
	 *  pulling pure nodes near their consumers. */
	static void BuildGraph(
		UEdGraph* Graph, TArray<FLayoutNode>& Out,
		TMap<UEdGraphNode*, int32>& IndexOf)
	{
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			FLayoutNode LN;
			LN.Node = N;
			int32 EstW = 0, EstH = 0;
			EstimateNodeSize(N, EstW, EstH);
			LN.Width  = (N->NodeWidth  > 0) ? N->NodeWidth  : EstW;
			LN.Height = (N->NodeHeight > 0) ? N->NodeHeight : EstH;
			// Comment boxes: keep their authored size; they're not laid out.
			IndexOf.Add(N, Out.Num());
			Out.Add(LN);
		}
		for (int32 i = 0; i < Out.Num(); ++i)
		{
			FLayoutNode& LN = Out[i];
			for (UEdGraphPin* Pin : LN.Node->Pins)
			{
				if (!Pin) continue;
				const bool bExec = PinIsExec(Pin);
				if (bExec) LN.bHasExecPins = true;
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked) continue;
					UEdGraphNode* Other = Linked->GetOwningNode();
					if (!Other) continue;
					const int32* J = IndexOf.Find(Other);
					if (!J) continue;
					if (bExec && Pin->Direction == EGPD_Output)
					{
						LN.ExecSuccessors.AddUnique(*J);
						Out[*J].ExecPredecessors.AddUnique(i);
					}
					else if (!bExec && Pin->Direction == EGPD_Input)
					{
						// This node consumes data from Other → Other is a producer;
						// we want Other placed at layer(this) − 1 (handled later).
						Out[*J].DataSuccessors.AddUnique(i);
					}
				}
			}
		}
	}

	/** Assign layers by longest-path from exec sources. Cycles are broken by
	 *  ignoring back-edges (reported as warnings). */
	static int32 AssignLayers(TArray<FLayoutNode>& Nodes, TArray<FString>& Warnings)
	{
		int32 MaxLayer = 0;
		// Kahn-style longest-path: iterate until stable, with a cap.
		const int32 Cap = Nodes.Num() * 8 + 16;
		int32 Iter = 0;
		bool bChanged = true;
		// Seed: nodes with no exec preds get layer 0.
		for (FLayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins && LN.ExecPredecessors.Num() == 0)
			{
				LN.Layer = 0;
			}
		}
		while (bChanged && Iter++ < Cap)
		{
			bChanged = false;
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				FLayoutNode& LN = Nodes[i];
				if (!LN.bHasExecPins) continue;
				int32 Best = LN.Layer;
				for (int32 P : LN.ExecPredecessors)
				{
					if (Nodes[P].Layer >= 0)
					{
						Best = FMath::Max(Best, Nodes[P].Layer + 1);
					}
				}
				if (Best > LN.Layer)
				{
					LN.Layer = Best;
					bChanged = true;
				}
			}
		}
		if (Iter >= Cap)
		{
			Warnings.Add(TEXT("layer assignment hit iteration cap — possible exec cycle"));
		}
		// Any exec node still at -1 is in a cycle island — stick it at layer 0.
		for (FLayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins && LN.Layer < 0) LN.Layer = 0;
		}
		// Pure / data-only nodes: layer = min(consumer.layer) − 1, default 0.
		for (FLayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins) continue;
			int32 MinConsumer = INT32_MAX;
			for (int32 C : LN.DataSuccessors)
			{
				if (Nodes[C].Layer >= 0)
				{
					MinConsumer = FMath::Min(MinConsumer, Nodes[C].Layer);
				}
			}
			LN.Layer = (MinConsumer == INT32_MAX) ? 0 : FMath::Max(0, MinConsumer - 1);
		}
		for (const FLayoutNode& LN : Nodes)
		{
			if (LN.Layer > MaxLayer) MaxLayer = LN.Layer;
		}
		return MaxLayer + 1;
	}

	/** Barycentric ordering within each layer: sort by average Y of the
	 *  predecessors' OrderInLayer. Two passes is usually enough. */
	static void BarycentricOrder(TArray<FLayoutNode>& Nodes, int32 LayerCount)
	{
		TArray<TArray<int32>> Layers;
		Layers.SetNum(LayerCount);
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const int32 L = FMath::Clamp(Nodes[i].Layer, 0, LayerCount - 1);
			Layers[L].Add(i);
		}
		// Seed: stable by original Y so untouched graphs don't scramble.
		for (TArray<int32>& Layer : Layers)
		{
			Layer.Sort([&](int32 A, int32 B) {
				return Nodes[A].Node->NodePosY < Nodes[B].Node->NodePosY;
			});
			for (int32 k = 0; k < Layer.Num(); ++k) Nodes[Layer[k]].OrderInLayer = k;
		}
		// Two sweeps: forward (use predecessors) then back (use successors).
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 L = 1; L < LayerCount; ++L)
			{
				Layers[L].Sort([&](int32 A, int32 B) {
					auto Bary = [&](int32 I)
					{
						const TArray<int32>& Preds = Nodes[I].ExecPredecessors.Num() > 0
							? Nodes[I].ExecPredecessors : Nodes[I].DataSuccessors;
						if (Preds.Num() == 0) return (float)Nodes[I].OrderInLayer;
						float Sum = 0.f;
						for (int32 P : Preds) Sum += Nodes[P].OrderInLayer;
						return Sum / Preds.Num();
					};
					return Bary(A) < Bary(B);
				});
				for (int32 k = 0; k < Layers[L].Num(); ++k) Nodes[Layers[L][k]].OrderInLayer = k;
			}
		}
	}
}

// ─── PinAligned strategy: pin-to-pin exec backbone + data pull ─────

namespace TetherBPPinAlignedImpl
{
	using namespace TetherBPSummaryImpl;

	/** Direction-index of a pin among its own side's visible pins. Mirrors the
	 *  convention used by StraightenExecChain / AutoInsertReroutes so pin Y
	 *  estimates stay consistent across tools. Returns -1 if not found. */
	static int32 PinDirIndex(const UEdGraphNode* N, const UEdGraphPin* P)
	{
		if (!N || !P) return -1;
		int32 Idx = 0;
		for (const UEdGraphPin* Q : N->Pins)
		{
			if (!Q || Q->bHidden) continue;
			if (Q->Direction != P->Direction) continue;
			if (Q == P) return Idx;
			Idx += 1;
		}
		return -1;
	}

	/** Ground-truth geometry for a single node, queried from the live Slate
	 *  widget. Populated by BuildRenderedCache when the graph is open in BP
	 *  editor and has ticked at least once. Width/Height come from
	 *  SGraphNode::GetDesiredSize; InputPinLocalYs[i] / OutputPinLocalYs[i]
	 *  come from SGraphPin::GetNodeOffset for the i-th visible input/output
	 *  pin. When the cache has no entry for a node, placement falls back to
	 *  the EstimateNodeSize + ComputePinLocalY formula. */
	struct FRenderedGeom
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<int32> InputPinLocalYs;
		TArray<int32> OutputPinLocalYs;
		TArray<int32> InputPinLocalXs;
		TArray<int32> OutputPinLocalXs;
		bool bValid = false;
	};

	/** Strip all reroute knots from the graph by short-circuiting every wire
	 *  that passes through one. Each knot's input pin has exactly one source
	 *  link; its output pin may have multiple destinations. Re-wire source →
	 *  each destination directly, then delete the knot. Call before layout
	 *  so the algorithm sees a canonical graph (no inherited reroutes from
	 *  previous layout runs). The layout's own L-route pass re-adds knots
	 *  for any wires that end up diagonal in the new positions. */
	static void StripRerouteKnots(UEdGraph* Graph)
	{
		if (!Graph) return;
		TArray<UK2Node_Knot*> Knots;
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (UK2Node_Knot* K = Cast<UK2Node_Knot>(N)) Knots.Add(K);
		}
		for (UK2Node_Knot* K : Knots)
		{
			UEdGraphPin* KIn  = K->GetInputPin();
			UEdGraphPin* KOut = K->GetOutputPin();
			if (!KIn || !KOut) continue;

			// Snapshot source(s) reaching KIn and destination(s) leaving KOut.
			TArray<UEdGraphPin*> Sources;
			for (UEdGraphPin* L : KIn->LinkedTo)
			{
				if (L) Sources.Add(L);
			}
			TArray<UEdGraphPin*> Dests;
			for (UEdGraphPin* L : KOut->LinkedTo)
			{
				if (L) Dests.Add(L);
			}

			// Break existing links (LinkedTo is mutated as we break).
			while (KIn->LinkedTo.Num() > 0)
			{
				KIn->BreakLinkTo(KIn->LinkedTo[0]);
			}
			while (KOut->LinkedTo.Num() > 0)
			{
				KOut->BreakLinkTo(KOut->LinkedTo[0]);
			}

			// Re-wire every source to every destination. If a source is itself
			// a knot (chained reroutes), this wire still lands on the knot for
			// now — the next iteration of this loop will collapse that knot.
			for (UEdGraphPin* S : Sources)
			{
				for (UEdGraphPin* D : Dests)
				{
					if (S && D) S->MakeLinkTo(D);
				}
			}

			K->DestroyNode();
		}
	}

	/** Query the live SGraphPanel for each node's desired size and each pin's
	 *  node-offset. Requires the graph to already be open in a Blueprint
	 *  editor AND to have been ticked by Slate at least once (this function
	 *  runs on the game thread, so call `open_function_graph_for_render`
	 *  in a previous exec and wait a moment before calling the layout). */
	static void BuildRenderedCache(UEdGraph* Graph, TMap<UEdGraphNode*, FRenderedGeom>& OutCache)
	{
		if (!Graph || !GEditor) return;
		UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (!Sub) return;
		UBlueprint* OwningBP = Cast<UBlueprint>(Graph->GetOutermostObject());
		// Walk up the outer chain to find the Blueprint (functions sit below UFunction).
		UObject* Outer = Graph->GetOuter();
		while (Outer && !OwningBP)
		{
			OwningBP = Cast<UBlueprint>(Outer);
			Outer = Outer->GetOuter();
		}
		if (!OwningBP) return;
		IAssetEditorInstance* Inst = Sub->FindEditorForAsset(OwningBP, false);
		if (!Inst) return;
		FBlueprintEditor* BPEd = static_cast<FBlueprintEditor*>(Inst);
		TSharedPtr<SGraphEditor> GraphEd = BPEd->OpenGraphAndBringToFront(Graph);
		if (!GraphEd.IsValid()) return;
		SGraphPanel* Panel = GraphEd->GetGraphPanel();
		if (!Panel) return;
		// Read pin positions by manually running each SGraphNode's
		// OnArrangeChildren cascade with a local root geometry. This bypasses
		// SGraphPanel's viewport culling (which skips OnArrangeChildren for
		// off-viewport nodes and leaves their SGraphPin::NodeOffset at 0).
		// The cascade arranges the node's internal widget tree — title bar,
		// pin boxes, individual SGraphPin widgets — and we read each pin's
		// arranged AbsolutePosition. Because our root starts at (0,0), a
		// child's arranged AbsolutePosition is exactly its local offset
		// within the node, which is what we want to store.
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			TSharedPtr<SGraphNode> NW = Panel->GetNodeWidgetFromGuid(N->NodeGuid);
			if (!NW.IsValid()) continue;

			// SlatePrepass first so DesiredSize is valid and pin widgets
			// have their own sizes resolved (needed for inner layout).
			NW->SlatePrepass();
			const FVector2D Desired = FVector2D(NW->GetDesiredSize());
			if (Desired.X <= 0 || Desired.Y <= 0) continue;

			// Build reverse lookup: SGraphPin widget pointer -> UEdGraphPin*,
			// so the recursive walk can identify pins it encounters.
			TMap<const SWidget*, UEdGraphPin*> PinLookup;
			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin || Pin->bHidden) continue;
				TSharedPtr<SGraphPin> PW = NW->FindWidgetForPin(Pin);
				if (PW.IsValid()) PinLookup.Add(&PW.ToSharedRef().Get(), Pin);
			}

			// Recursively arrange and collect local pin offsets.
			// EVisibility::All so intermediate widgets with HitTestInvisible
			// or SelfHitTestInvisible (common for layout containers like
			// SBorder, SOverlay) don't cut the walk short.
			TMap<UEdGraphPin*, FVector2D> PinOffsets;
			const FGeometry RootGeo = FGeometry::MakeRoot(Desired, FSlateLayoutTransform());
			TFunction<void(TSharedRef<SWidget>, const FGeometry&)> Walk;
			Walk = [&](TSharedRef<SWidget> W, const FGeometry& G2)
			{
				// EVisibility::All so intermediate SelfHitTestInvisible /
				// HitTestInvisible containers (common for layout-only
				// wrappers like SBorder) don't get filtered out.
				FArrangedChildren Kids(EVisibility::All);
				W->ArrangeChildren(G2, Kids);
				for (int32 ChildIdx = 0; ChildIdx < Kids.Num(); ++ChildIdx)
				{
					const FArrangedWidget& AC = Kids[ChildIdx];
					if (UEdGraphPin** FoundPin = PinLookup.Find(&AC.Widget.Get()))
					{
						// Match the convention of SGraphPin::GetNodeOffset()
						// from SGraphPin.cpp:933 —
						//   offset = AbsPos/Scale - NodeUnscaledPos
						//   offset.Y += Size.Y * 0.5
						// i.e. return the pin's vertical CENTER, not its
						// top. The rest of the layout code (KnotY,
						// PlaceExecBackbone pin-to-pin alignment) all
						// reads this center convention.
						const FVector2D TopLeft = FVector2D(AC.Geometry.GetAbsolutePosition());
						const FVector2D PinSize = FVector2D(AC.Geometry.GetLocalSize());
						PinOffsets.Add(*FoundPin, FVector2D(TopLeft.X, TopLeft.Y + PinSize.Y * 0.5f));
					}
					else
					{
						Walk(AC.Widget, AC.Geometry);
					}
				}
			};
			Walk(StaticCastSharedRef<SWidget>(NW.ToSharedRef()), RootGeo);

			// Populate cache. Pin iteration order must match PinDirIndex's,
			// so walk N->Pins again in declaration order.
			//
			// Value priority (most accurate first):
			//   1. SGraphPin::GetNodeOffset() — Slate's arranged-and-painted
			//      result, if non-zero. Only populated for nodes that have
			//      been in the panel's visible viewport at least once, but
			//      when available this is the authoritative render position
			//      (SGraphPin.cpp:933 — center of pin, not top).
			//   2. Walk's ArrangeChildren result — correct for many node
			//      subclasses but can mis-model title-bar height or
			//      conditional slot layout, leaving pins a few px off from
			//      real render on some node types.
			//   3. ComputePinLocalY formula (in PinLocalYFor's fall-through)
			//      — rough estimate (~±20 px) when both above fail.
			FRenderedGeom G;
			G.Width  = int32(Desired.X);
			G.Height = int32(Desired.Y);
			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin || Pin->bHidden) continue;
				FVector2D Off = FVector2D::ZeroVector;
				if (TSharedPtr<SGraphPin> PW = NW->FindWidgetForPin(Pin))
				{
					Off = FVector2D(PW->GetNodeOffset());
				}
				if (Off.Y <= 0.0f)
				{
					if (FVector2D* OffPtr = PinOffsets.Find(Pin))
					{
						Off = *OffPtr;
					}
				}
				if (Pin->Direction == EGPD_Input)
				{
					G.InputPinLocalXs.Add(int32(Off.X));
					G.InputPinLocalYs.Add(int32(Off.Y));
				}
				else
				{
					G.OutputPinLocalXs.Add(int32(Off.X));
					G.OutputPinLocalYs.Add(int32(Off.Y));
				}
			}
			G.bValid = true;
			OutCache.Add(N, G);
		}
	}

	/** Fetch the local-Y of a visible pin (direction-indexed).
	 *
	 *  Slate's `SGraphPin::NodeOffset` is only written when the parent
	 *  SGraphNode runs its OnArrangeChildren — which requires the node to
	 *  be inside the panel's visible viewport at the time. Nodes off-screen
	 *  report (0,0). A cached value of exactly 0 is therefore treated as
	 *  "unknown" and we fall back to the EstimateNodeSize formula, which
	 *  puts the pin in the right row even if slightly off (± a few px).
	 *  Without this check, off-screen nodes would park the pin at the very
	 *  top of their node — putting any knot wired to them flush with the
	 *  node's top edge instead of the exec pin. */
	static int32 PinLocalYFor(UEdGraphNode* Node, EEdGraphPinDirection Dir, int32 DirIdx,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (const FRenderedGeom* G = Cache.Find(Node))
		{
			if (G->bValid)
			{
				const TArray<int32>& Arr = (Dir == EGPD_Input) ? G->InputPinLocalYs : G->OutputPinLocalYs;
				if (DirIdx >= 0 && DirIdx < Arr.Num())
				{
					const int32 Cached = Arr[DirIdx];
					if (Cached > 0) return Cached;
					// Cached == 0 means Slate hasn't arranged this node yet
					// (viewport-culled). Fall through to the formula.
				}
			}
		}
		return ComputePinLocalY(Node, Dir, DirIdx);
	}

	/** Node width from live cache, else from EstimateNodeSize. */
	static int32 NodeWidthFor(UEdGraphNode* Node,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (const FRenderedGeom* G = Cache.Find(Node))
		{
			if (G->bValid && G->Width > 0) return G->Width;
		}
		int32 W = 0, H = 0;
		EstimateNodeSize(Node, W, H);
		return (Node->NodeWidth > 0) ? Node->NodeWidth : W;
	}

	/** Node height from live cache, else from EstimateNodeSize. */
	static int32 NodeHeightFor(UEdGraphNode* Node,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (const FRenderedGeom* G = Cache.Find(Node))
		{
			if (G->bValid && G->Height > 0) return G->Height;
		}
		int32 W = 0, H = 0;
		EstimateNodeSize(Node, W, H);
		return (Node->NodeHeight > 0) ? Node->NodeHeight : H;
	}

	struct FPALayoutNode
	{
		UEdGraphNode* Node = nullptr;
		int32 Width = MinNodeWidth;
		int32 Height = MinNodeHeight;
		int32 Layer = -1;         // exec layer; -1 until assigned
		int32 RowId = -1;         // row / swim-lane id; -1 until assigned
		int32 NewX = 0;
		int32 NewY = 0;
		bool bHasExecPins = false;
		bool bPlaced = false;

		// Primary exec predecessor (for pin-alignment propagation).
		int32 PrimaryExecPredIdx = INDEX_NONE;
		// Dir-index of the exec-OUT pin on the primary predecessor connecting to us.
		int32 PredExecOutDirIdx = -1;
		// Dir-index of our own exec-IN pin receiving from the primary pred.
		int32 MyExecInDirIdx = -1;

		// Primary exec SUCCESSOR (the .then / first-exec-out path). Used for
		// downstream-driven Y alignment: each node aligns its primary exec-out
		// pin Y to its successor's exec-in pin Y, so the primary flow reads
		// as a clean horizontal rail. The .else branch and other non-primary
		// successors become diagonal (knotted if |ΔY| > threshold).
		int32 PrimaryExecSuccIdx = INDEX_NONE;
		int32 MyExecOutDirIdxToSucc = -1;  // dir-index of my exec-out pin feeding the succ
		int32 SuccExecInDirIdx = -1;       // dir-index of succ's exec-in pin receiving

		// Data-slot assignment (for pure/data nodes only).
		int32 DataConsumerLayer = INT32_MAX;  // exec layer of nearest exec consumer
		int32 DataDepth = INT32_MAX;          // 1 = direct producer, 2 = two hops, ...
		// Primary consumer for Y alignment (any node — exec or pure — whose input
		// pin this node feeds; picked by closest DataConsumerLayer/Depth).
		int32 PrimaryConsumerIdx = INDEX_NONE;
		int32 PrimaryConsumerInDirIdx = -1;  // which of consumer's input pins we feed
		int32 MyOutDirIdxToPrimary = -1;     // which of our output pins feeds it

		TArray<int32> ExecPredecessors;
		TArray<int32> ExecSuccessors;

		// Data-flow: producers feeding this node's input pins.
		// (producer_idx, this_node's_input_pin_dir_idx)
		TArray<TPair<int32, int32>> DataProducers;
	};

	static void BuildGraph(UEdGraph* Graph, TArray<FPALayoutNode>& Out,
		TMap<UEdGraphNode*, int32>& IndexOf,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			// Comment boxes: snapshot but never place (kept at original XY).
			FPALayoutNode LN;
			LN.Node = N;
			LN.Width  = NodeWidthFor(N, Cache);
			LN.Height = NodeHeightFor(N, Cache);
			IndexOf.Add(N, Out.Num());
			Out.Add(LN);
		}

		for (int32 i = 0; i < Out.Num(); ++i)
		{
			FPALayoutNode& LN = Out[i];
			for (UEdGraphPin* Pin : LN.Node->Pins)
			{
				if (!Pin || Pin->bHidden) continue;
				const bool bExec = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
				if (bExec) LN.bHasExecPins = true;
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked) continue;
					UEdGraphNode* Other = Linked->GetOwningNode();
					if (!Other) continue;
					const int32* J = IndexOf.Find(Other);
					if (!J) continue;
					if (bExec)
					{
						// exec-out on us → that's a successor
						if (Pin->Direction == EGPD_Output)
						{
							LN.ExecSuccessors.AddUnique(*J);
							Out[*J].ExecPredecessors.AddUnique(i);
						}
					}
					else
					{
						// data-in on us → Other is a producer
						if (Pin->Direction == EGPD_Input)
						{
							const int32 MyInDirIdx = PinDirIndex(LN.Node, Pin);
							LN.DataProducers.Add(TPair<int32,int32>(*J, MyInDirIdx));
						}
					}
				}
			}
		}
	}

	/** Longest-path layering on exec-only subgraph. */
	static int32 AssignExecLayers(TArray<FPALayoutNode>& Nodes,
		TArray<FString>& Warnings)
	{
		for (FPALayoutNode& LN : Nodes) LN.Layer = -1;

		// Seed: exec sources (have exec pins, no exec predecessors) → layer 0.
		for (FPALayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins && LN.ExecPredecessors.Num() == 0) LN.Layer = 0;
		}

		const int32 Cap = Nodes.Num() * 8 + 16;
		int32 Iter = 0;
		bool bChanged = true;
		while (bChanged && Iter++ < Cap)
		{
			bChanged = false;
			for (FPALayoutNode& LN : Nodes)
			{
				if (!LN.bHasExecPins) continue;
				int32 Best = LN.Layer;
				for (int32 P : LN.ExecPredecessors)
				{
					if (Nodes[P].Layer >= 0) Best = FMath::Max(Best, Nodes[P].Layer + 1);
				}
				if (Best > LN.Layer) { LN.Layer = Best; bChanged = true; }
			}
		}
		if (Iter >= Cap)
		{
			Warnings.Add(TEXT("pin_aligned: exec layer assignment hit iter cap — exec cycle suspected"));
		}
		// Unlayered exec nodes (cycle island): park at 0.
		for (FPALayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins && LN.Layer < 0) LN.Layer = 0;
		}

		int32 MaxLayer = 0;
		for (const FPALayoutNode& LN : Nodes)
		{
			if (LN.Layer > MaxLayer) MaxLayer = LN.Layer;
		}
		return MaxLayer + 1;
	}

	/** BFS from each exec node outward via DataProducers. Assigns every pure
	 *  node a (ConsumerLayer, Depth) slot; shared pure nodes get the closest
	 *  slot (smallest layer, then smallest depth). Also records each pure
	 *  node's primary consumer and the specific pin pair used for Y alignment.
	 *  Nodes with no exec ancestor (orphan pure chains) keep the sentinels
	 *  and are parked later. */
	static void AssignDataSlots(TArray<FPALayoutNode>& Nodes)
	{
		struct FFrontier { int32 NodeIdx; int32 ConsumerIdx; int32 ConsumerInDirIdx; int32 Depth; };
		TArray<FFrontier> Queue;

		// Seed: every exec node emits one frontier entry per direct data producer.
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const FPALayoutNode& Exec = Nodes[i];
			if (!Exec.bHasExecPins) continue;
			if (Exec.Layer < 0) continue;
			for (const TPair<int32,int32>& Prod : Exec.DataProducers)
			{
				if (Nodes[Prod.Key].bHasExecPins) continue;  // exec→exec handled elsewhere
				Queue.Add({ Prod.Key, i, Prod.Value, 1 });
			}
		}

		int32 Head = 0;
		while (Head < Queue.Num())
		{
			const FFrontier F = Queue[Head++];
			FPALayoutNode& N = Nodes[F.NodeIdx];
			if (N.bHasExecPins) continue;

			const int32 OwningExecLayer = Nodes[F.ConsumerIdx].bHasExecPins
				? Nodes[F.ConsumerIdx].Layer
				: Nodes[F.ConsumerIdx].DataConsumerLayer;
			if (OwningExecLayer == INT32_MAX) continue;

			const bool bBetter = (OwningExecLayer < N.DataConsumerLayer) ||
				(OwningExecLayer == N.DataConsumerLayer && F.Depth < N.DataDepth);
			if (!bBetter && N.PrimaryConsumerIdx != INDEX_NONE) continue;

			N.DataConsumerLayer = OwningExecLayer;
			N.DataDepth         = F.Depth;
			N.PrimaryConsumerIdx        = F.ConsumerIdx;
			N.PrimaryConsumerInDirIdx   = F.ConsumerInDirIdx;

			// Resolve the actual output pin on us that feeds the consumer.
			const FPALayoutNode& Cons = Nodes[F.ConsumerIdx];
			UEdGraphPin* ConsInPin = nullptr;
			{
				int32 DirIdx = 0;
				for (UEdGraphPin* Q : Cons.Node->Pins)
				{
					if (!Q || Q->bHidden) continue;
					if (Q->Direction != EGPD_Input) continue;
					if (DirIdx == F.ConsumerInDirIdx) { ConsInPin = Q; break; }
					++DirIdx;
				}
			}
			N.MyOutDirIdxToPrimary = -1;
			if (ConsInPin)
			{
				for (UEdGraphPin* Linked : ConsInPin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNode() == N.Node)
					{
						N.MyOutDirIdxToPrimary = PinDirIndex(N.Node, Linked);
						break;
					}
				}
			}

			// Recurse into this node's own data producers (depth+1).
			for (const TPair<int32,int32>& Prod : N.DataProducers)
			{
				if (Nodes[Prod.Key].bHasExecPins) continue;
				Queue.Add({ Prod.Key, F.NodeIdx, Prod.Value, F.Depth + 1 });
			}
		}
	}

	/** Walk every data node's PrimaryConsumerIdx chain to the first exec
	 *  consumer and inherit that exec node's **current** RowId. Called
	 *  initially from AssignRows, and again after
	 *  ClusterDelegateBoundEvents has reassigned exec-subtree RowIds to
	 *  Bind's row — without the re-propagation the cluster's pure data
	 *  producers keep their stale RowId (their original event's now-empty
	 *  row), which confuses ReBandRowsAfterClustering into yanking them
	 *  into a spurious band far from their consumer. Cap the walk to
	 *  avoid pathological data-cycle stalls. */
	static void PropagateDataNodeRowIds(TArray<FPALayoutNode>& Nodes)
	{
		const int32 MaxHop = Nodes.Num() + 1;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Nodes[i].bHasExecPins) continue;
			int32 Cur = Nodes[i].PrimaryConsumerIdx;
			int32 Hop = 0;
			while (Cur != INDEX_NONE && Hop < MaxHop)
			{
				if (Nodes[Cur].bHasExecPins)
				{
					Nodes[i].RowId = Nodes[Cur].RowId;
					break;
				}
				Cur = Nodes[Cur].PrimaryConsumerIdx;
				++Hop;
			}
		}
	}

	/** Group exec nodes into rows (swim lanes) seeded at layer-0 exec roots.
	 *  Each row owns the exec subtree reachable from its root via
	 *  ExecSuccessors. Data nodes inherit the RowId of their primary
	 *  consumer (walking PrimaryConsumerIdx until an exec node is hit).
	 *
	 *  Used by the per-row column-width pass: each row computes its own
	 *  LayerMaxW so a wide node in row A doesn't inflate the X grid of
	 *  row B. Rows are ordered by original root Y so the final vertical
	 *  band order matches authored intent. Returns the row count. */
	static int32 AssignRows(TArray<FPALayoutNode>& Nodes)
	{
		// Collect exec roots (layer 0). Disconnected exec islands (cycles)
		// seed additional rows in a second pass.
		TArray<int32> Roots;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Nodes[i].bHasExecPins && Nodes[i].Layer == 0) Roots.Add(i);
		}
		Roots.Sort([&](int32 A, int32 B) {
			return Nodes[A].Node->NodePosY < Nodes[B].Node->NodePosY;
		});

		int32 NextRowId = 0;
		auto FloodRow = [&](int32 Seed)
		{
			if (Nodes[Seed].RowId != -1) return;
			TArray<int32> Queue; Queue.Add(Seed);
			Nodes[Seed].RowId = NextRowId;
			while (Queue.Num() > 0)
			{
				const int32 Cur = Queue.Pop();
				for (int32 Succ : Nodes[Cur].ExecSuccessors)
				{
					if (Nodes[Succ].RowId != -1) continue;
					Nodes[Succ].RowId = NextRowId;
					Queue.Add(Succ);
				}
			}
			++NextRowId;
		};

		for (int32 R : Roots) FloodRow(R);
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (!Nodes[i].bHasExecPins) continue;
			if (Nodes[i].RowId == -1) FloodRow(i);
		}

		PropagateDataNodeRowIds(Nodes);
		return NextRowId;
	}

	/** Compute per-slot max width and per-slot X position. Slot keys are
	 *  encoded as (ConsumerLayer * LargeStride + Depth) packed into int64
	 *  to stay in a single TMap. Returns the per-exec-layer cumulative data
	 *  budget (sum of slot widths + gaps) so AutoLayoutGraph can space the
	 *  exec columns apart. */
	static int64 MakeSlotKey(int32 ConsumerLayer, int32 Depth)
	{
		return (int64(ConsumerLayer) << 20) | int64(Depth & 0xFFFFF);
	}

	static void ComputeDataSlots(
		const TArray<FPALayoutNode>& Nodes,
		TMap<int64, int32>& OutSlotWidth,
		TMap<int32, int32>& OutLayerMaxDepth)
	{
		for (const FPALayoutNode& LN : Nodes)
		{
			if (LN.bHasExecPins) continue;
			if (LN.DataConsumerLayer == INT32_MAX) continue;
			if (LN.DataDepth == INT32_MAX) continue;
			const int64 Key = MakeSlotKey(LN.DataConsumerLayer, LN.DataDepth);
			int32& W = OutSlotWidth.FindOrAdd(Key, 0);
			if (LN.Width > W) W = LN.Width;
			int32& MaxD = OutLayerMaxDepth.FindOrAdd(LN.DataConsumerLayer, 0);
			if (LN.DataDepth > MaxD) MaxD = LN.DataDepth;
		}
	}

	/** Recursive chain width: own width + DataHSpace + max child chain width.
	 *  Returns the pixel extent this data node and all its own producers
	 *  need to the left of their consumer. Key difference from slot-sum
	 *  budgeting: siblings of a data node at the same depth-slot but
	 *  different Y don't contribute to OUR chain budget — only what's
	 *  upstream of US does. Used for per-layer budget = max over all
	 *  data producers feeding the layer of their chain width. */
	static int32 ComputeChainWidth(int32 NodeIdx, const TArray<FPALayoutNode>& Nodes,
		int32 DataHSpace, TMap<int32, int32>& Memo)
	{
		if (int32* Cached = Memo.Find(NodeIdx)) return *Cached;
		Memo.Add(NodeIdx, 0);  // cycle sentinel
		const FPALayoutNode& LN = Nodes[NodeIdx];
		if (LN.bHasExecPins) return 0;  // reached exec boundary, no chain here
		int32 MaxChild = 0;
		for (const TPair<int32, int32>& Prod : LN.DataProducers)
		{
			if (Nodes[Prod.Key].bHasExecPins) continue;  // exec producer has own X
			const int32 Child = ComputeChainWidth(Prod.Key, Nodes, DataHSpace, Memo);
			if (Child > MaxChild) MaxChild = Child;
		}
		const int32 Result = LN.Width + DataHSpace + MaxChild;
		Memo[NodeIdx] = Result;
		return Result;
	}

	/** Per-layer chain budget: for each exec layer L, the maximum chain
	 *  width of any data producer feeding an exec node at L. This is what
	 *  LayerX[L] needs to reserve left of it for data pipelines to fit. */
	static void ComputeLayerChainBudgets(
		const TArray<FPALayoutNode>& Nodes, int32 LayerCount, int32 DataHSpace,
		TArray<int32>& OutBudgets)
	{
		OutBudgets.Init(0, LayerCount);
		TMap<int32, int32> Memo;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const FPALayoutNode& LN = Nodes[i];
			if (!LN.bHasExecPins) continue;
			if (LN.Layer < 0 || LN.Layer >= LayerCount) continue;
			int32 MaxBudget = 0;
			for (const TPair<int32, int32>& Prod : LN.DataProducers)
			{
				if (Nodes[Prod.Key].bHasExecPins) continue;  // exec feed, separate
				const int32 W = ComputeChainWidth(Prod.Key, Nodes, DataHSpace, Memo);
				if (W > MaxBudget) MaxBudget = W;
			}
			if (MaxBudget > OutBudgets[LN.Layer]) OutBudgets[LN.Layer] = MaxBudget;
		}
	}

	/** Pick the primary exec predecessor for each exec node; record the exact
	 *  pins on both sides for Y alignment. Preference: leftmost layer; tie
	 *  break by lowest original Y (matches "main rail" intuition). */
	static void SelectPrimaryExecPreds(TArray<FPALayoutNode>& Nodes)
	{
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			FPALayoutNode& LN = Nodes[i];
			if (!LN.bHasExecPins || LN.ExecPredecessors.Num() == 0) continue;

			int32 Best = LN.ExecPredecessors[0];
			for (int32 P : LN.ExecPredecessors)
			{
				const FPALayoutNode& Cand = Nodes[P];
				const FPALayoutNode& Cur  = Nodes[Best];
				if (Cand.Layer < Cur.Layer ||
					(Cand.Layer == Cur.Layer && Cand.Node->NodePosY < Cur.Node->NodePosY))
				{
					Best = P;
				}
			}
			LN.PrimaryExecPredIdx = Best;

			// Find exec pin pair (pred.out → me.in).
			const FPALayoutNode& Pred = Nodes[Best];
			bool bResolved = false;
			for (UEdGraphPin* OutPin : Pred.Node->Pins)
			{
				if (!OutPin || OutPin->bHidden) continue;
				if (OutPin->Direction != EGPD_Output) continue;
				if (OutPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				for (UEdGraphPin* Linked : OutPin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNode() == LN.Node)
					{
						LN.PredExecOutDirIdx = PinDirIndex(Pred.Node, OutPin);
						LN.MyExecInDirIdx    = PinDirIndex(LN.Node, Linked);
						bResolved = true;
						break;
					}
				}
				if (bResolved) break;
			}
		}
	}

	/** Pick each exec node's primary SUCCESSOR: the node reached through the
	 *  lowest-dir-index exec-output pin (the .then of a Branch, the single
	 *  .then of a SET, the Entry's .then). Downstream Y alignment follows
	 *  this chain so the primary path reads as a horizontal rail. Non-primary
	 *  successors (e.g. Branch.else) stay visually distinct — their wires
	 *  pick up a single reroute knot if they end up diagonal. */
	static void SelectPrimaryExecSuccs(TArray<FPALayoutNode>& Nodes)
	{
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			FPALayoutNode& LN = Nodes[i];
			if (!LN.bHasExecPins) continue;
			// Walk exec-output pins in dir order (PinDirIndex ascending). First
			// one with a link = primary successor. LinkedTo[0] if multi-link.
			UEdGraphPin* OutPin = nullptr;
			int32 OutDirIdx = -1;
			int32 CurDirIdx = 0;
			for (UEdGraphPin* P : LN.Node->Pins)
			{
				if (!P || P->bHidden) continue;
				if (P->Direction != EGPD_Output) continue;
				if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) { continue; }
				if (P->LinkedTo.Num() > 0)
				{
					OutPin = P;
					OutDirIdx = CurDirIdx;
					break;
				}
				// Advance dir counter for exec outputs we've seen.
				// (Non-exec outputs have their own dir-counter; we only care
				// about exec outputs here.)
				++CurDirIdx;
			}
			// Actually we need dir-index across ALL visible output pins, not
			// just exec ones. Re-derive properly:
			if (OutPin)
			{
				OutDirIdx = PinDirIndex(LN.Node, OutPin);
			}
			if (!OutPin || OutDirIdx < 0) continue;

			UEdGraphPin* Linked = OutPin->LinkedTo[0];
			if (!Linked) continue;
			UEdGraphNode* SuccNode = Linked->GetOwningNode();
			if (!SuccNode) continue;

			// Resolve successor's index in our flat node array.
			for (int32 j = 0; j < Nodes.Num(); ++j)
			{
				if (Nodes[j].Node == SuccNode)
				{
					LN.PrimaryExecSuccIdx = j;
					LN.MyExecOutDirIdxToSucc = OutDirIdx;
					LN.SuccExecInDirIdx = PinDirIndex(SuccNode, Linked);
					break;
				}
			}
		}
	}

	/** Layer 0 exec nodes stacked vertically (original-Y order), then each
	 *  later layer placed pin-aligned to its primary predecessor. Within each
	 *  layer, nodes that share a tentative Y are pushed down to clear overlaps.
	 *
	 *  `RowLayerX[RowId][Layer]` is the row-local X grid — each row starts
	 *  at X=0 and uses its own per-layer column widths, so rows are
	 *  horizontally compact. Rows are banded vertically via the leaf-stacking
	 *  pass, which inserts a block-gap whenever RowId changes. */
	static void PlaceExecBackbone(TArray<FPALayoutNode>& Nodes, int32 LayerCount,
		const TArray<TArray<int32>>& RowLayerX, int32 VSpace,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache,
		const TMap<UEdGraphNode*, int32>& IndexOf)
	{
		auto LayerXFor = [&](int32 RowId, int32 Layer) -> int32
		{
			const int32 ClampedL = FMath::Clamp(Layer, 0, LayerCount - 1);
			if (RowId < 0 || RowId >= RowLayerX.Num()) return 0;
			const TArray<int32>& Row = RowLayerX[RowId];
			if (ClampedL >= Row.Num()) return 0;
			return Row[ClampedL];
		};
		// DOWNSTREAM-driven Y via DFS of the exec tree:
		//  1. DFS from the entry following exec-out pins in dir order (.then
		//     first, then .else). Leaves (Return / terminal nodes) are
		//     pushed to a flat list in the order we meet them — this is
		//     the natural top-to-bottom order humans draw BPs in. Each node
		//     also records its "lane chain": the fanout-output pins crossed
		//     on the path from root. Two fanout-outputs K, K+1 on the same
		//     fanout appear at the same chain position but with increasing
		//     PinDirIdx, so lex-ordering chains gives then_0 ≺ then_1 ≺ ….
		//  2. Sort leaves by lane chain (lex), tiebreak by DFS visit order.
		//     Stack top-to-bottom; add an extra block-gap between leaves that
		//     belong to different lanes. This is what makes each fanout pin's
		//     sub-tree a visually separate "code block" with no overlap.
		//  3. Non-leaves pull Y from their primary successor so the .then
		//     wire reads as a horizontal rail. Processed deepest-first so
		//     successors are placed before predecessors.
		//  4. Collision-resolve per-layer as a safety net.

		TArray<int32> DFSLeaves;
		TSet<int32> Visited;
		TArray<TArray<int64>> LaneChainOf; LaneChainOf.SetNum(Nodes.Num());
		TArray<int32> DFSOrderOf;          DFSOrderOf.Init(INT32_MAX, Nodes.Num());
		int32 DFSTick = 0;

		TFunction<void(int32, const TArray<int64>&)> DFS;
		DFS = [&](int32 Idx, const TArray<int64>& Chain)
		{
			if (Visited.Contains(Idx)) return;
			Visited.Add(Idx);
			LaneChainOf[Idx] = Chain;
			DFSOrderOf[Idx]  = DFSTick++;

			FPALayoutNode& LN = Nodes[Idx];
			if (!LN.bHasExecPins) return;
			if (LN.ExecSuccessors.Num() == 0)
			{
				DFSLeaves.Add(Idx);
				return;
			}

			// Count exec outputs — any node with ≥2 wired exec outputs acts as
			// a fanout whose output pins each open a new lane. Branch (then /
			// else) and Sequence (then_0 / then_1 / …) are the canonical cases.
			int32 WiredExecOuts = 0;
			for (UEdGraphPin* P : LN.Node->Pins)
			{
				if (!P || P->bHidden) continue;
				if (P->Direction != EGPD_Output) continue;
				if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				if (P->LinkedTo.Num() > 0) ++WiredExecOuts;
			}
			const bool bIsFanout = WiredExecOuts >= 2;

			// Visit exec-output pins in direction order; each LinkedTo in pin
			// order. For a Branch that's .then (dir 0) first, then .else.
			for (UEdGraphPin* P : LN.Node->Pins)
			{
				if (!P || P->bHidden) continue;
				if (P->Direction != EGPD_Output) continue;
				if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				TArray<int64> ChildChain = Chain;
				if (bIsFanout)
				{
					const int32 PinDir = PinDirIndex(LN.Node, P);
					// Encode (FanoutIdx, PinDir) as int64: high 32 = node idx,
					// low 32 = pin dir. Lex-compare gives fanout-grouped then
					// pin-order ordering.
					ChildChain.Add((static_cast<int64>(Idx) << 32) |
					               static_cast<int64>(static_cast<uint32>(PinDir)));
				}
				for (UEdGraphPin* Linked : P->LinkedTo)
				{
					if (!Linked) continue;
					UEdGraphNode* Succ = Linked->GetOwningNode();
					if (!Succ) continue;
					const int32* J = IndexOf.Find(Succ);
					if (J) DFS(*J, ChildChain);
				}
			}
		};

		// Seed DFS at root exec nodes (layer 0). Multiple roots in original-Y order.
		TArray<int32> Roots;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Nodes[i].bHasExecPins && Nodes[i].Layer == 0) Roots.Add(i);
		}
		Roots.Sort([&](int32 A, int32 B) {
			return Nodes[A].Node->NodePosY < Nodes[B].Node->NodePosY;
		});
		for (int32 R : Roots) DFS(R, TArray<int64>{});
		// Handle disconnected exec subgraphs (cycle islands, etc.).
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Nodes[i].bHasExecPins && !Visited.Contains(i)) DFS(i, TArray<int64>{});
		}

		// Step 2a: sort leaves by (RowId, lane chain, DFS order). Row is
		// primary so each row's leaves form a contiguous block (vertical
		// band); lane chain gives fanout pin-K before pin-(K+1) within a
		// row; DFS order is the final tiebreak.
		DFSLeaves.Sort([&](int32 A, int32 B) {
			const int32 RA = Nodes[A].RowId;
			const int32 RB = Nodes[B].RowId;
			if (RA != RB) return RA < RB;
			const TArray<int64>& CA = LaneChainOf[A];
			const TArray<int64>& CB = LaneChainOf[B];
			const int32 Min = FMath::Min(CA.Num(), CB.Num());
			for (int32 k = 0; k < Min; ++k)
			{
				if (CA[k] != CB[k]) return CA[k] < CB[k];
			}
			if (CA.Num() != CB.Num()) return CA.Num() < CB.Num();
			return DFSOrderOf[A] < DFSOrderOf[B];
		});

		// Step 2b: stack leaves top-to-bottom. Insert a block-gap when the
		// lane chain changes (fanout pin boundary) AND when the row changes
		// (row-to-row boundary) so each row is a visually separate band and
		// each fanout pin's sub-tree is a separate code block within a row.
		const int32 BlockGap = VSpace * 2;
		const int32 RowGap   = VSpace * 3;  // bigger than block-gap to sell the "separate event" boundary
		int32 Cursor = 0;
		bool bFirstLeaf = true;
		TArray<int64> PrevChain;
		int32 PrevRow = -2;
		for (int32 Idx : DFSLeaves)
		{
			FPALayoutNode& LN = Nodes[Idx];
			const int32 CurRow = LN.RowId;
			if (!bFirstLeaf)
			{
				if (CurRow != PrevRow)       Cursor += RowGap;
				else if (LaneChainOf[Idx] != PrevChain) Cursor += BlockGap;
			}
			LN.NewX = LayerXFor(CurRow, LN.Layer);
			LN.NewY = Cursor;
			LN.bPlaced = true;
			Cursor += LN.Height + VSpace;
			PrevChain = LaneChainOf[Idx];
			PrevRow   = CurRow;
			bFirstLeaf = false;
		}

		// Step 3: place non-leaves from deepest layer back to root, each
		// aligned to its primary exec successor's input pin Y so the .then
		// wire is horizontal.
		for (int32 L = LayerCount - 1; L >= 0; --L)
		{
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				FPALayoutNode& LN = Nodes[i];
				if (!LN.bHasExecPins) continue;
				if (LN.Layer != L) continue;
				if (LN.bPlaced) continue;
				LN.NewX = LayerXFor(LN.RowId, L);
				if (LN.PrimaryExecSuccIdx != INDEX_NONE &&
					Nodes[LN.PrimaryExecSuccIdx].bPlaced &&
					LN.MyExecOutDirIdxToSucc >= 0 && LN.SuccExecInDirIdx >= 0)
				{
					const FPALayoutNode& Succ = Nodes[LN.PrimaryExecSuccIdx];
					const int32 SuccPinY = Succ.NewY + PinLocalYFor(
						Succ.Node, EGPD_Input, LN.SuccExecInDirIdx, Cache);
					const int32 MyOutLocalY = PinLocalYFor(
						LN.Node, EGPD_Output, LN.MyExecOutDirIdxToSucc, Cache);
					LN.NewY = SuccPinY - MyOutLocalY;
				}
				else
				{
					LN.NewY = LN.Node->NodePosY;
				}
				LN.bPlaced = true;
			}
		}

		// Step 4: collision-resolve per (row, layer). Nodes that collapsed
		// onto the same Y (most often siblings of a shared successor — e.g.
		// Branch.then and Branch.else both pulling to a common Return) are
		// ordered by LaneChain lex — earlier lanes ABOVE later lanes — then
		// pushed down so each lane keeps its own band. Scoping collision to
		// a single row prevents cross-row X-disjoint nodes from pushing each
		// other around just because they share a Y after independent row
		// layout.
		const int32 RowCount = RowLayerX.Num();
		for (int32 R = 0; R < RowCount; ++R)
		{
			for (int32 L = 0; L < LayerCount; ++L)
			{
				TArray<int32> LayerNodes;
				for (int32 i = 0; i < Nodes.Num(); ++i)
				{
					if (!Nodes[i].bHasExecPins) continue;
					if (Nodes[i].RowId != R) continue;
					if (Nodes[i].Layer != L) continue;
					if (!Nodes[i].bPlaced) continue;
					LayerNodes.Add(i);
				}
				LayerNodes.Sort([&](int32 A, int32 B) {
					if (Nodes[A].NewY != Nodes[B].NewY) return Nodes[A].NewY < Nodes[B].NewY;
					const TArray<int64>& CA = LaneChainOf[A];
					const TArray<int64>& CB = LaneChainOf[B];
					const int32 Min = FMath::Min(CA.Num(), CB.Num());
					for (int32 k = 0; k < Min; ++k)
					{
						if (CA[k] != CB[k]) return CA[k] < CB[k];
					}
					if (CA.Num() != CB.Num()) return CA.Num() < CB.Num();
					return DFSOrderOf[A] < DFSOrderOf[B];
				});
				for (int32 k = 1; k < LayerNodes.Num(); ++k)
				{
					const FPALayoutNode& Prev = Nodes[LayerNodes[k-1]];
					FPALayoutNode& Curr = Nodes[LayerNodes[k]];
					const int32 MinY = Prev.NewY + Prev.Height + VSpace;
					if (Curr.NewY < MinY) Curr.NewY = MinY;
				}
			}
		}
	}

	/** Post-exec-placement: co-locate CustomEvent subtrees with their Bind
	 *  Event nodes. A K2Node_CustomEvent's delegate OUT-pin wired to a
	 *  K2Node_AddDelegate's Event IN-pin means the event is the Bind's
	 *  callback — humans author them as a visual cluster ("bind registers
	 *  it, event handles it"). The layering pass above can't express this
	 *  because CustomEvent is itself an exec root (Layer 0), so the algorithm
	 *  separates it from the Bind by the full graph width.
	 *
	 *  For each such pair (E → B), translate E's entire exec subtree so E
	 *  sits immediately below B at the same X, preserving subtree shape.
	 *  The delegate wire becomes a short vertical; the handler exec chain
	 *  continues rightward from E in its own swim lane.
	 *
	 *  Subtree membership uses the PrimaryExecPredIdx chain — node X belongs
	 *  to E's subtree iff walking primary-pred from X reaches E. This
	 *  naturally excludes shared join points (where PrimaryExecPredIdx
	 *  picked a different root), so we don't yank nodes belonging to
	 *  another event's primary flow. */
	static void ClusterDelegateBoundEvents(TArray<FPALayoutNode>& Nodes,
		const TMap<UEdGraphNode*, int32>& IndexOf,
		int32 VSpace,
		TArray<FString>& Warnings)
	{
		int32 ClusteredCount = 0;
		// Track already-used target slots so stacking multiple events near
		// the same Bind doesn't pile them at the same Y. Key = BindIdx,
		// Value = next available Y below that Bind.
		TMap<int32, int32> BindNextY;

		for (int32 Ei = 0; Ei < Nodes.Num(); ++Ei)
		{
			FPALayoutNode& E = Nodes[Ei];
			if (!E.Node || !E.bPlaced) continue;
			UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(E.Node);
			if (!CE) continue;

			// Find the (multicast) delegate OUT-pin. CustomEvents expose one.
			UEdGraphPin* DelegatePin = nullptr;
			for (UEdGraphPin* P : CE->Pins)
			{
				if (!P || P->bHidden) continue;
				if (P->Direction != EGPD_Output) continue;
				const FName Cat = P->PinType.PinCategory;
				if (Cat == UEdGraphSchema_K2::PC_Delegate ||
					Cat == UEdGraphSchema_K2::PC_MCDelegate)
				{
					DelegatePin = P;
					break;
				}
			}
			if (!DelegatePin || DelegatePin->LinkedTo.Num() == 0) continue;

			// Prefer an AddDelegate consumer. Fall back to any node holding
			// a delegate IN-pin — covers BaseMCDelegate variants used by
			// Bind to Anim Finish / Bind to Dispatcher etc.
			int32 Bi = INDEX_NONE;
			for (UEdGraphPin* Linked : DelegatePin->LinkedTo)
			{
				if (!Linked) continue;
				UEdGraphNode* Owner = Linked->GetOwningNode();
				if (!Owner) continue;
				const int32* BPtr = IndexOf.Find(Owner);
				if (!BPtr) continue;
				if (!Nodes[*BPtr].bPlaced) continue;
				if (Owner->IsA<UK2Node_BaseMCDelegate>())
				{
					Bi = *BPtr;
					break;
				}
			}
			if (Bi == INDEX_NONE) continue;

			// Build E's primary-pred-defined subtree. Walk pred chain; if
			// it reaches E, include. Skip nodes whose chain reaches another
			// exec root first (they belong to a different event's cluster).
			TSet<int32> Subtree;
			Subtree.Add(Ei);
			for (int32 k = 0; k < Nodes.Num(); ++k)
			{
				if (k == Ei) continue;
				if (!Nodes[k].bPlaced) continue;
				int32 Cur = k;
				int32 Iter = 0;
				const int32 MaxIter = Nodes.Num() + 1;
				while (Cur != INDEX_NONE && Iter < MaxIter)
				{
					if (Cur == Ei) { Subtree.Add(k); break; }
					Cur = Nodes[Cur].PrimaryExecPredIdx;
					++Iter;
				}
			}

			// Safety: if the Bind itself fell into the subtree (a cyclic
			// bind-then-invoke-back pattern), skip — translating would drag
			// the Bind too.
			if (Subtree.Contains(Bi)) continue;

			// Target: under the Bind at the same X. If another event was
			// already clustered under this Bind, stack this one below it.
			const FPALayoutNode& B = Nodes[Bi];
			const int32 BlockGap = VSpace * 2;
			int32* NextYPtr = BindNextY.Find(Bi);
			const int32 TargetY = NextYPtr ? *NextYPtr : (B.NewY + B.Height + BlockGap);
			const int32 TargetX = B.NewX;
			const int32 DeltaX = TargetX - E.NewX;
			const int32 DeltaY = TargetY - E.NewY;
			if (DeltaX == 0 && DeltaY == 0)
			{
				BindNextY.Add(Bi, TargetY + E.Height + BlockGap);
				continue;
			}

			// Apply delta to the whole subtree and reassign RowId to Bind's
			// row so the cluster becomes part of Bind's row for all later
			// banding passes. Track post-translation bottom to seed the
			// next sibling's Y and the push-down shift below.
			const int32 BindRow = B.RowId;
			int32 SubtreeMaxBottom = INT32_MIN;
			for (int32 Idx : Subtree)
			{
				FPALayoutNode& LN = Nodes[Idx];
				LN.NewX += DeltaX;
				LN.NewY += DeltaY;
				LN.RowId = BindRow;
				const int32 Bottom = LN.NewY + LN.Height;
				if (Bottom > SubtreeMaxBottom) SubtreeMaxBottom = Bottom;
			}
			BindNextY.Add(Bi, SubtreeMaxBottom + BlockGap);
			++ClusteredCount;

			// Push-down: shift every Bind-row exec node below Bind down by
			// whatever it takes to clear the cluster bottom. Previously
			// filtered by X-overlap with the cluster to save vertical
			// space, but that left non-overlapping exec nodes (and their
			// later-placed data producers) landing inside the cluster's
			// Y band. Unfiltered uniform shift is a bit more generous
			// vertically but keeps the cluster cleanly separated.
			const int32 BindBottom = B.NewY + B.Height;
			int32 TopmostDisp = INT32_MAX;
			for (int32 k = 0; k < Nodes.Num(); ++k)
			{
				if (k == Bi) continue;
				if (Subtree.Contains(k)) continue;
				const FPALayoutNode& LN = Nodes[k];
				if (!LN.bPlaced) continue;
				if (LN.RowId != BindRow) continue;
				if (LN.Node && LN.Node->IsA<UEdGraphNode_Comment>()) continue;
				if (LN.NewY <= BindBottom) continue;
				if (LN.NewY < TopmostDisp) TopmostDisp = LN.NewY;
			}
			if (TopmostDisp != INT32_MAX)
			{
				const int32 Shift = (SubtreeMaxBottom + BlockGap) - TopmostDisp;
				if (Shift > 0)
				{
					for (int32 k = 0; k < Nodes.Num(); ++k)
					{
						if (k == Bi) continue;
						if (Subtree.Contains(k)) continue;
						FPALayoutNode& LN = Nodes[k];
						if (!LN.bPlaced) continue;
						if (LN.RowId != BindRow) continue;
						if (LN.Node && LN.Node->IsA<UEdGraphNode_Comment>()) continue;
						if (LN.NewY <= BindBottom) continue;
						LN.NewY += Shift;
					}
				}
			}
		}
		if (ClusteredCount > 0)
		{
			Warnings.Add(FString::Printf(
				TEXT("pin_aligned: clustered %d delegate-bound CustomEvent subtree(s) with their Bind nodes"),
				ClusteredCount));
		}
	}

	/** Final pass: re-band rows vertically. Clustering and push-down may
	 *  have extended some rows past their originally-assigned Y band, so
	 *  row N's content can now sit on top of row N+1. Walk rows in order
	 *  of their current topmost node, cascading a downward shift whenever
	 *  row N's bottom + RowGap exceeds row N+1's top. Data producer and
	 *  knot positions are finalized before this runs, so the shift moves
	 *  the full visible content of each row including pure data chains.
	 *  Comment boxes stay at authored positions (same policy as the rest
	 *  of pin_aligned — moving a comment breaks its "encloses these
	 *  specific nodes" anchor).
	 *
	 *  Without this pass, a cluster insertion could push Bind-row content
	 *  down into the visual space that used to hold row N+1, producing
	 *  cross-row overlap even when each row's internal layout is clean. */
	static void ReBandRowsAfterClustering(TArray<FPALayoutNode>& Nodes,
		int32 RowCount, int32 VSpace)
	{
		if (RowCount <= 1) return;

		// Gather per-row [minY, maxY] from placed, non-comment nodes.
		TArray<int32> RowMinY; RowMinY.Init(INT32_MAX, RowCount);
		TArray<int32> RowMaxY; RowMaxY.Init(INT32_MIN, RowCount);
		for (const FPALayoutNode& LN : Nodes)
		{
			if (!LN.bPlaced) continue;
			if (LN.RowId < 0 || LN.RowId >= RowCount) continue;
			if (LN.Node && LN.Node->IsA<UEdGraphNode_Comment>()) continue;
			RowMinY[LN.RowId] = FMath::Min(RowMinY[LN.RowId], LN.NewY);
			RowMaxY[LN.RowId] = FMath::Max(RowMaxY[LN.RowId], LN.NewY + LN.Height);
		}

		// Order rows by current top. Empty rows (no members) are dropped.
		TArray<int32> RowOrder;
		for (int32 R = 0; R < RowCount; ++R)
		{
			if (RowMinY[R] != INT32_MAX) RowOrder.Add(R);
		}
		RowOrder.Sort([&](int32 A, int32 B) { return RowMinY[A] < RowMinY[B]; });

		const int32 RowGap = VSpace * 3;
		for (int32 i = 1; i < RowOrder.Num(); ++i)
		{
			const int32 PrevRow = RowOrder[i - 1];
			const int32 CurRow  = RowOrder[i];
			const int32 Required = RowMaxY[PrevRow] + RowGap;
			const int32 Shift    = Required - RowMinY[CurRow];
			if (Shift <= 0) continue;
			for (FPALayoutNode& LN : Nodes)
			{
				if (!LN.bPlaced) continue;
				if (LN.RowId != CurRow) continue;
				if (LN.Node && LN.Node->IsA<UEdGraphNode_Comment>()) continue;
				LN.NewY += Shift;
			}
			RowMinY[CurRow] += Shift;
			RowMaxY[CurRow] += Shift;
		}
	}

	/** Place pure nodes per-consumer: each producer sits `HSpace` px to the
	 *  left of its primary consumer, with its own width (not the slot's max
	 *  width). All nodes whose primary consumer shares an X naturally share
	 *  a right edge, so sibling output pins stay aligned. Narrow siblings of
	 *  wide ones no longer pay for the wide one's extra width on their input
	 *  side — wire lengths to deeper producers become HSpace + 2×PinInset
	 *  regardless of slot composition.
	 *
	 *  Y is pin-aligned to the primary consumer's input pin; collisions among
	 *  any pair of placed nodes with overlapping X-range are resolved by
	 *  downward bump. Shallowest depth first so deeper producers align to
	 *  already-placed intermediates. */
	static void PlaceDataProducersPerConsumer(TArray<FPALayoutNode>& Nodes,
		int32 HSpace, int32 VSpace,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		// Gather pure data nodes, bucket by DataDepth ascending so we process
		// shallowest (depth 1) first — their consumers are exec nodes already
		// at their final positions. Running collision resolution per-depth
		// means deeper-placed nodes align to the POST-collision Y of their
		// consumer, avoiding the classic "placed before consumer moved" bug.
		int32 MaxDepth = 0;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const FPALayoutNode& LN = Nodes[i];
			if (LN.bHasExecPins) continue;
			if (LN.DataConsumerLayer == INT32_MAX) continue;
			if (LN.DataDepth == INT32_MAX) continue;
			if (LN.PrimaryConsumerIdx == INDEX_NONE) continue;
			if (LN.DataDepth > MaxDepth) MaxDepth = LN.DataDepth;
		}

		auto PlaceOne = [&](int32 Idx)
		{
			FPALayoutNode& LN = Nodes[Idx];
			const FPALayoutNode& Cons = Nodes[LN.PrimaryConsumerIdx];
			if (!Cons.bPlaced)
			{
				LN.NewX = LN.Node->NodePosX;
				LN.NewY = LN.Node->NodePosY;
				LN.bPlaced = true;
				return;
			}
			LN.NewX = Cons.NewX - HSpace - LN.Width;
			if (LN.PrimaryConsumerInDirIdx >= 0 && LN.MyOutDirIdxToPrimary >= 0)
			{
				const int32 ConsPinY = Cons.NewY + PinLocalYFor(
					Cons.Node, EGPD_Input, LN.PrimaryConsumerInDirIdx, Cache);
				const int32 MyOutLocalY = PinLocalYFor(
					LN.Node, EGPD_Output, LN.MyOutDirIdxToPrimary, Cache);
				LN.NewY = ConsPinY - MyOutLocalY;
			}
			else
			{
				LN.NewY = LN.Node->NodePosY;
			}
			LN.bPlaced = true;
		};

		// Per-depth: place, then resolve collisions against ALL already-placed
		// nodes (exec + earlier data depths). X-range overlap + Y-range overlap
		// triggers a downward bump.
		TArray<int32> AllPlaced;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Nodes[i].bPlaced) AllPlaced.Add(i);  // exec backbone pre-placed
		}

		for (int32 d = 1; d <= MaxDepth; ++d)
		{
			// Collect nodes at this depth and place them (pin-aligned to their
			// consumers, which are by now at their final Y).
			TArray<int32> ThisDepth;
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				const FPALayoutNode& LN = Nodes[i];
				if (LN.bHasExecPins) continue;
				if (LN.DataDepth != d) continue;
				if (LN.PrimaryConsumerIdx == INDEX_NONE) continue;
				PlaceOne(i);
				ThisDepth.Add(i);
			}
			if (ThisDepth.Num() == 0) continue;

			// Collision-sort this depth's nodes (lowest Y first), then push
			// any that overlap an earlier-placed node horizontally AND
			// vertically. "Earlier" = placed in a prior iteration (exec +
			// shallower depths + already-processed siblings in this depth).
			ThisDepth.Sort([&](int32 A, int32 B) {
				if (Nodes[A].NewY != Nodes[B].NewY) return Nodes[A].NewY < Nodes[B].NewY;
				return Nodes[A].NewX < Nodes[B].NewX;
			});
			for (int32 Idx : ThisDepth)
			{
				FPALayoutNode& Curr = Nodes[Idx];
				int32 MinY = INT32_MIN;
				for (int32 Other : AllPlaced)
				{
					if (Other == Idx) continue;
					const FPALayoutNode& P = Nodes[Other];
					if (!P.bPlaced) continue;
					const bool bXOverlap =
						!(Curr.NewX + Curr.Width <= P.NewX ||
						  P.NewX + P.Width <= Curr.NewX);
					if (!bXOverlap) continue;
					// Only react when the actual bounding boxes overlap (with
					// a VSpace cushion). If Curr is already fully above or
					// fully below P, no adjustment needed — otherwise we'd
					// push a perfectly-placed node down for no reason when a
					// previously-placed node was collision-bumped higher Y
					// than Curr's natural position.
					const bool bAboveP = Curr.NewY + Curr.Height + VSpace <= P.NewY;
					const bool bBelowP = P.NewY + P.Height + VSpace <= Curr.NewY;
					if (bAboveP || bBelowP) continue;
					const int32 PrevBottomPlusGap = P.NewY + P.Height + VSpace;
					if (PrevBottomPlusGap > MinY) MinY = PrevBottomPlusGap;
				}
				if (MinY != INT32_MIN && Curr.NewY < MinY) Curr.NewY = MinY;
				AllPlaced.Add(Idx);
			}
		}
	}

	/** For each primary exec edge, if the Y offset between pred's output exec
	 *  pin and successor's input exec pin exceeds YThreshold (indicating that
	 *  pin alignment was broken by collision resolution), insert a pair of
	 *  reroute knots to convert the diagonal wire into a clean L-shape:
	 *    src ─┐   (knot1 at mid-X, src pin Y — horizontal segment)
	 *         │
	 *         └─ dst   (knot2 at mid-X + offset, dst pin Y)
	 *  Must be called AFTER final Node positions (NodePosX/Y) are applied —
	 *  reads those directly. Returns the number of L-route edges knotted. */
	static int32 InsertLRouteKnotsForDiagonalExec(
		UEdGraph* Graph, const TArray<FPALayoutNode>& Nodes, int32 YThreshold,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (!Graph) return 0;

		// Single-knot routing: place ONE knot near the destination at the
		// destination pin's Y so the final segment is horizontal and the
		// first segment is a natural diagonal Bezier. Matches the way UE's
		// auto-routed wires look — cleaner than stacking two knots in an
		// L-shape for every diagonal.
		struct FSpec { UEdGraphNode* SrcNode; FName SrcPin; UEdGraphNode* DstNode; FName DstPin;
			int32 KnotX, KnotY; };
		TArray<FSpec> Specs;

		constexpr int32 KnotWidth = 42;
		// UK2Node_Knot is 42×24 with pin center at NodeOffset.y = 9.5 (live
		// Slate measurement). Integer NodePosY rounds that to 10, giving a
		// 0.5 px residual — invisible. The previous value of 12 left a
		// visible 2.5 px offset between the knot's pin and the exec/data
		// pin it was trying to align with.
		constexpr int32 KnotPinCenterOffset = 10;

		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const FPALayoutNode& LN = Nodes[i];
			if (!LN.bPlaced || !LN.bHasExecPins) continue;
			if (LN.PrimaryExecPredIdx == INDEX_NONE) continue;
			if (LN.PredExecOutDirIdx < 0 || LN.MyExecInDirIdx < 0) continue;

			const FPALayoutNode& Pred = Nodes[LN.PrimaryExecPredIdx];
			if (!Pred.bPlaced) continue;

			const int32 PredPinY = Pred.Node->NodePosY + PinLocalYFor(
				Pred.Node, EGPD_Output, LN.PredExecOutDirIdx, Cache);
			const int32 MyPinY   = LN.Node->NodePosY + PinLocalYFor(
				LN.Node, EGPD_Input, LN.MyExecInDirIdx, Cache);
			if (FMath::Abs(PredPinY - MyPinY) < YThreshold) continue;

			UEdGraphPin* SrcPin = nullptr;
			{
				int32 VIdx = 0;
				for (UEdGraphPin* Q : Pred.Node->Pins)
				{
					if (!Q || Q->bHidden) continue;
					if (Q->Direction != EGPD_Output) continue;
					if (VIdx == LN.PredExecOutDirIdx) { SrcPin = Q; break; }
					++VIdx;
				}
			}
			UEdGraphPin* DstPin = nullptr;
			{
				int32 VIdx = 0;
				for (UEdGraphPin* Q : LN.Node->Pins)
				{
					if (!Q || Q->bHidden) continue;
					if (Q->Direction != EGPD_Input) continue;
					if (VIdx == LN.MyExecInDirIdx) { DstPin = Q; break; }
					++VIdx;
				}
			}
			if (!SrcPin || !DstPin) continue;
			if (!SrcPin->LinkedTo.Contains(DstPin)) continue;

			const int32 PredRight = Pred.Node->NodePosX + Pred.Width;
			const int32 DstLeft = LN.Node->NodePosX;
			// Needs room for the knot itself + a margin either side; otherwise
			// skip and let the bare diagonal render.
			if (DstLeft - PredRight < KnotWidth + 40) continue;

			// Place the knot near the destination, at the destination pin's
			// Y. The final leg (knot.out → dst.in) is then horizontal; the
			// first leg (src.out → knot.in) bends as a natural Bezier curve.
			// Bias X toward the destination (~70%) so the horizontal tail
			// stays short relative to the curved approach.
			const int32 GapRange = DstLeft - PredRight;
			const int32 KnotX = PredRight + (GapRange * 70) / 100 - KnotWidth / 2;
			const int32 KnotY = MyPinY - KnotPinCenterOffset;

			FSpec S;
			S.SrcNode = Pred.Node; S.SrcPin = SrcPin->PinName;
			S.DstNode = LN.Node;   S.DstPin = DstPin->PinName;
			S.KnotX = KnotX;
			S.KnotY = KnotY;
			Specs.Add(S);
		}

		int32 Inserted = 0;
		for (const FSpec& S : Specs)
		{
			UEdGraphPin* SrcPin = S.SrcNode->FindPin(S.SrcPin);
			UEdGraphPin* DstPin = S.DstNode->FindPin(S.DstPin);
			if (!SrcPin || !DstPin) continue;
			if (!SrcPin->LinkedTo.Contains(DstPin)) continue;

			UK2Node_Knot* K = NewObject<UK2Node_Knot>(Graph);
			K->CreateNewGuid();
			K->NodePosX = S.KnotX;
			K->NodePosY = S.KnotY;
			Graph->AddNode(K, false, false);
			K->PostPlacedNewNode();
			K->AllocateDefaultPins();

			UEdGraphPin* KIn  = K->GetInputPin();
			UEdGraphPin* KOut = K->GetOutputPin();
			if (!KIn || !KOut)
			{
				K->DestroyNode();
				continue;
			}

			SrcPin->BreakLinkTo(DstPin);
			SrcPin->MakeLinkTo(KIn);
			KOut->MakeLinkTo(DstPin);
			Inserted += 1;
		}
		return Inserted;
	}

	/** Iterate every non-exec wire in the graph. For each one whose source
	 *  pin Y doesn't match destination pin Y (|Δ| >= YThreshold), insert
	 *  two reroute knots to convert the diagonal into a src-horizontal /
	 *  vertical / horizontal-to-dst L. Skips wires that already pass
	 *  through a knot, wires that involve comments/knots, and wires whose
	 *  endpoints aren't placed. Handles primary AND fan-out edges — e.g.
	 *  a single output feeding two pins of the same consumer at different
	 *  Y, where one wire aligns pin-to-pin but the other needs a knot. */
	static int32 InsertLRouteKnotsForAllDataWires(
		UEdGraph* Graph, int32 YThreshold,
		const TMap<UEdGraphNode*, FRenderedGeom>& Cache)
	{
		if (!Graph) return 0;

		constexpr int32 KnotWidth = 42;
		// See InsertLRouteKnotsForDiagonalExec — measured pin offset is 9.5,
		// rounded to 10 for int32 NodePosY.
		constexpr int32 KnotPinCenterOffset = 10;

		auto DirIndex = [](const UEdGraphNode* N, const UEdGraphPin* P) -> int32
		{
			if (!N || !P) return -1;
			int32 Idx = 0;
			for (const UEdGraphPin* Q : N->Pins)
			{
				if (!Q || Q->bHidden) continue;
				if (Q->Direction != P->Direction) continue;
				if (Q == P) return Idx;
				Idx += 1;
			}
			return -1;
		};

		// Snapshot edges first — we'll mutate the graph and don't want
		// iteration to include the knots we just inserted.
		struct FEdge { UEdGraphNode* SrcNode; FName SrcPin; UEdGraphNode* DstNode; FName DstPin;
			FEdGraphPinType PinType;
			int32 SrcPinY; int32 DstPinY;
			int32 SrcNodeRight; int32 DstNodeLeft; };
		TArray<FEdge> Edges;

		for (UEdGraphNode* N : Graph->Nodes)
		{
			if (!N) continue;
			if (N->IsA<UEdGraphNode_Comment>()) continue;
			if (N->IsA<UK2Node_Knot>()) continue;
			for (UEdGraphPin* Pin : N->Pins)
			{
				if (!Pin || Pin->bHidden) continue;
				if (Pin->Direction != EGPD_Output) continue;
				// Data-only: skip exec category (exec uses its own L-route pass).
				if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
				const int32 SrcDirIdx = DirIndex(N, Pin);
				if (SrcDirIdx < 0) continue;
				const int32 SrcPinY = N->NodePosY + PinLocalYFor(N, EGPD_Output, SrcDirIdx, Cache);
				const int32 SrcRight = N->NodePosX + NodeWidthFor(N, Cache);
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked) continue;
					UEdGraphNode* Dst = Linked->GetOwningNode();
					if (!Dst) continue;
					if (Dst->IsA<UEdGraphNode_Comment>()) continue;
					if (Dst->IsA<UK2Node_Knot>()) continue;
					const int32 DstDirIdx = DirIndex(Dst, Linked);
					if (DstDirIdx < 0) continue;
					const int32 DstPinY = Dst->NodePosY + PinLocalYFor(Dst, EGPD_Input, DstDirIdx, Cache);
					if (FMath::Abs(SrcPinY - DstPinY) < YThreshold) continue;
					FEdge E;
					E.SrcNode = N; E.SrcPin = Pin->PinName;
					E.DstNode = Dst; E.DstPin = Linked->PinName;
					E.PinType = Pin->PinType;
					E.SrcPinY = SrcPinY; E.DstPinY = DstPinY;
					E.SrcNodeRight = SrcRight; E.DstNodeLeft = Dst->NodePosX;
					Edges.Add(E);
				}
			}
		}

		int32 Inserted = 0;
		for (const FEdge& E : Edges)
		{
			UEdGraphPin* SrcPin = E.SrcNode->FindPin(E.SrcPin);
			UEdGraphPin* DstPin = E.DstNode->FindPin(E.DstPin);
			if (!SrcPin || !DstPin) continue;
			if (!SrcPin->LinkedTo.Contains(DstPin)) continue;
			if (E.DstNodeLeft <= E.SrcNodeRight) continue;  // wire goes right-to-left, skip
			// Need at least 2 × KnotWidth of horizontal gap to fit both knots.
			// Without that, the second knot would overshoot the destination's
			// left edge and the outbound wire would curl backwards.
			if (E.DstNodeLeft - E.SrcNodeRight < KnotWidth + 40) continue;

			// Single knot near the destination at the destination pin's Y —
			// final leg horizontal, first leg a natural Bezier curve.
			const int32 GapRange = E.DstNodeLeft - E.SrcNodeRight;
			const int32 KnotX = E.SrcNodeRight + (GapRange * 70) / 100 - KnotWidth / 2;
			const int32 KnotY = E.DstPinY - KnotPinCenterOffset;

			UK2Node_Knot* K = NewObject<UK2Node_Knot>(Graph);
			K->CreateNewGuid();
			K->NodePosX = KnotX;
			K->NodePosY = KnotY;
			Graph->AddNode(K, false, false);
			K->PostPlacedNewNode();
			K->AllocateDefaultPins();

			UEdGraphPin* KIn  = K->GetInputPin();
			UEdGraphPin* KOut = K->GetOutputPin();
			if (!KIn || !KOut) { K->DestroyNode(); continue; }

			// Make the knot carry the edge's actual value type (default is
			// wildcard). Without this, compile errors may appear on save.
			KIn->PinType  = E.PinType;
			KOut->PinType = E.PinType;

			SrcPin->BreakLinkTo(DstPin);
			SrcPin->MakeLinkTo(KIn);
			KOut->MakeLinkTo(DstPin);
			Inserted += 1;
		}
		return Inserted;
	}

	/** Park any still-unplaced, non-comment nodes (disconnected islands, etc.)
	 *  in a spillover strip below the main layout so they don't get lost. */
	static void ParkUnplacedNodes(TArray<FPALayoutNode>& Nodes, int32 MinX, int32 MaxY,
		int32 VSpace)
	{
		int32 SpillX = MinX;
		int32 SpillY = MaxY + VSpace * 3;
		for (FPALayoutNode& LN : Nodes)
		{
			if (LN.bPlaced) continue;
			if (LN.Node && LN.Node->IsA<UEdGraphNode_Comment>()) continue;
			LN.NewX = SpillX;
			LN.NewY = SpillY;
			LN.bPlaced = true;
			SpillX += LN.Width + 40;
			if (SpillX > MinX + 2000)
			{
				SpillX = MinX;
				SpillY += LN.Height + VSpace;
			}
		}
	}
}

FTetherLayoutResult UTetherBlueprintLibrary::AutoLayoutGraph(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& Strategy, const FString& AnchorNodeGuid,
	int32 HorizontalSpacing, int32 VerticalSpacing)
{
	using namespace TetherBPSummaryImpl;

	FTetherLayoutResult Result;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP) { Result.Warnings.Add(TEXT("blueprint not found")); return Result; }
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName);
	if (!Graph) { Result.Warnings.Add(TEXT("graph not found")); return Result; }

	const FString LowerStrat = Strategy.IsEmpty() ? TEXT("exec_flow") : Strategy.ToLower();
	if (LowerStrat != TEXT("exec_flow") && LowerStrat != TEXT("pin_aligned"))
	{
		Result.Warnings.Add(FString::Printf(
			TEXT("unknown strategy '%s' — supported: 'exec_flow', 'pin_aligned'"), *Strategy));
		return Result;
	}

	const int32 HSpace = HorizontalSpacing > 0 ? HorizontalSpacing : 80;
	const int32 VSpace = VerticalSpacing   > 0 ? VerticalSpacing   : 40;

	// ─── pin_aligned strategy ─────────────────────────────────
	if (LowerStrat == TEXT("pin_aligned"))
	{
		using namespace TetherBPPinAlignedImpl;

		// Query live Slate widgets for accurate node sizes + pin offsets.
		// MUST run before StripRerouteKnots — stripping knots invalidates
		// the graph panel and leaves SGraphPin widgets in an untickable
		// state where GetNodeOffset() returns (0,0). Capturing the cache
		// first gets valid offsets from the stable, pre-modification
		// widget tree. Requires the graph to already be open + ticked;
		// if not, Cache is empty and all helpers fall back to
		// EstimateNodeSize/ComputePinLocalY.
		TMap<UEdGraphNode*, FRenderedGeom> Cache;
		BuildRenderedCache(Graph, Cache);

		// Collapse any reroute knots from prior layout runs so primary-succ
		// walking and data-chain BFS see the canonical topology. L-route
		// will re-insert knots for whatever diagonals the new layout leaves.
		Graph->Modify();
		StripRerouteKnots(Graph);
		if (Cache.Num() > 0)
		{
			Result.Warnings.Add(FString::Printf(
				TEXT("pin_aligned: using live Slate geometry for %d nodes"), Cache.Num()));
		}

		TArray<FPALayoutNode> Nodes;
		TMap<UEdGraphNode*, int32> IndexOf;
		BuildGraph(Graph, Nodes, IndexOf, Cache);
		if (Nodes.Num() == 0) { Result.bSucceeded = true; return Result; }

		const int32 LayerCount = AssignExecLayers(Nodes, Result.Warnings);
		SelectPrimaryExecPreds(Nodes);
		SelectPrimaryExecSuccs(Nodes);
		AssignDataSlots(Nodes);
		const int32 RowCount = AssignRows(Nodes);

		// Compact-pack horizontally. Hand-authored BPs pack data pipelines
		// and exec columns more densely than a round HSpace=100 suggests —
		// we treat the user's HSpace as a loose upper bound and derive two
		// tight inner values. Data columns (inside a single exec layer's
		// chain) get the tightest gap; exec-layer-to-exec-layer is a bit
		// roomier so branches and single-pin consumers read clearly.
		const int32 DataHSpace = FMath::Max(15, HSpace / 3);
		const int32 ExecGap    = FMath::Max(30, HSpace / 2);

		// Per-row, per-layer widest exec node. Each row derives its own
		// column widths so a wide node in row A doesn't inflate the X grid
		// of row B — "per-row local column widths" rather than the prior
		// global column widths. Rows are then vertically stacked so the
		// visual outcome is several compact swim lanes instead of one
		// globally-wide grid.
		TArray<TArray<int32>> RowLayerMaxW;
		RowLayerMaxW.SetNum(RowCount);
		for (TArray<int32>& A : RowLayerMaxW) A.SetNumZeroed(LayerCount);
		for (const FPALayoutNode& LN : Nodes)
		{
			if (!LN.bHasExecPins) continue;
			if (LN.RowId < 0 || LN.RowId >= RowCount) continue;
			if (LN.Layer < 0 || LN.Layer >= LayerCount) continue;
			if (LN.Width > RowLayerMaxW[LN.RowId][LN.Layer])
				RowLayerMaxW[LN.RowId][LN.Layer] = LN.Width;
		}

		// Per-row, per-layer data-chain budget. For each exec node, walk its
		// data producers' chain widths (memoised globally since chain width
		// is a pure property of the producer subtree), and take the max per
		// (row, layer) cell. This reserves horizontal space left of the
		// layer's exec column for the producer pipeline to fit.
		TArray<TArray<int32>> RowLayerDataBudget;
		RowLayerDataBudget.SetNum(RowCount);
		for (TArray<int32>& A : RowLayerDataBudget) A.SetNumZeroed(LayerCount);
		{
			TMap<int32, int32> ChainMemo;
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				const FPALayoutNode& LN = Nodes[i];
				if (!LN.bHasExecPins) continue;
				if (LN.RowId < 0 || LN.RowId >= RowCount) continue;
				if (LN.Layer < 0 || LN.Layer >= LayerCount) continue;
				int32 MaxB = 0;
				for (const TPair<int32, int32>& Prod : LN.DataProducers)
				{
					if (Nodes[Prod.Key].bHasExecPins) continue;
					const int32 W = ComputeChainWidth(Prod.Key, Nodes, DataHSpace, ChainMemo);
					if (W > MaxB) MaxB = W;
				}
				if (MaxB > RowLayerDataBudget[LN.RowId][LN.Layer])
					RowLayerDataBudget[LN.RowId][LN.Layer] = MaxB;
			}
		}

		// Per-row LayerX. Each row restarts at X=0 — rows are later
		// translated vertically (via leaf-stacking with row block-gap) and
		// stay X-overlapped at 0-origin. Result: each row is exactly as
		// wide as IT needs, no wasted column from a wider neighbour.
		TArray<TArray<int32>> RowLayerX;
		RowLayerX.SetNum(RowCount);
		for (int32 R = 0; R < RowCount; ++R)
		{
			RowLayerX[R].SetNumZeroed(LayerCount);
			int32 Cur = 0;
			for (int32 L = 0; L < LayerCount; ++L)
			{
				RowLayerX[R][L] = Cur;
				Cur += RowLayerMaxW[R][L] + ExecGap;
				if (L + 1 < LayerCount) Cur += RowLayerDataBudget[R][L + 1];
			}
		}

		PlaceExecBackbone(Nodes, LayerCount, RowLayerX, VSpace, Cache, IndexOf);
		ClusterDelegateBoundEvents(Nodes, IndexOf, VSpace, Result.Warnings);
		// Cluster may have reassigned exec-subtree RowIds. Refresh data-node
		// RowIds so their PrimaryConsumer's new row is propagated down the
		// data chain — otherwise pure data producers of a moved exec node
		// still hold the stale original-event row and get yanked far from
		// their consumer in ReBandRowsAfterClustering.
		PropagateDataNodeRowIds(Nodes);
		PlaceDataProducersPerConsumer(Nodes, DataHSpace, VSpace, Cache);
		ReBandRowsAfterClustering(Nodes, RowCount, VSpace);

		// Compute current-layout bounds, then park anything still unplaced.
		int32 MinNewX = INT32_MAX, MinNewY = INT32_MAX, MaxNewX = INT32_MIN, MaxNewY = INT32_MIN;
		for (const FPALayoutNode& LN : Nodes)
		{
			if (!LN.bPlaced) continue;
			if (LN.Node && LN.Node->IsA<UEdGraphNode_Comment>()) continue;
			MinNewX = FMath::Min(MinNewX, LN.NewX);
			MinNewY = FMath::Min(MinNewY, LN.NewY);
			MaxNewX = FMath::Max(MaxNewX, LN.NewX + LN.Width);
			MaxNewY = FMath::Max(MaxNewY, LN.NewY + LN.Height);
		}
		if (MinNewX == INT32_MAX) { MinNewX = MinNewY = MaxNewX = MaxNewY = 0; }
		ParkUnplacedNodes(Nodes, MinNewX, MaxNewY, VSpace);

		// Anchor origin: preserve anchor node's current graph position, else
		// reuse the graph's original bounding-box top-left.
		int32 OriginX = 0, OriginY = 0;
		const FPALayoutNode* Anchor = nullptr;
		if (!AnchorNodeGuid.IsEmpty())
		{
			for (const FPALayoutNode& LN : Nodes)
			{
				if (LN.Node && LN.Node->NodeGuid.ToString(EGuidFormats::Digits) == AnchorNodeGuid)
				{
					Anchor = &LN; break;
				}
			}
			if (!Anchor)
			{
				Result.Warnings.Add(FString::Printf(
					TEXT("anchor node %s not found"), *AnchorNodeGuid));
			}
			else
			{
				OriginX = Anchor->Node->NodePosX;
				OriginY = Anchor->Node->NodePosY;
			}
		}
		if (!Anchor)
		{
			int32 MinX = INT32_MAX, MinY = INT32_MAX;
			for (const FPALayoutNode& LN : Nodes)
			{
				if (!LN.Node) continue;
				if (LN.Node->IsA<UEdGraphNode_Comment>()) continue;
				MinX = FMath::Min(MinX, LN.Node->NodePosX);
				MinY = FMath::Min(MinY, LN.Node->NodePosY);
			}
			if (MinX != INT32_MAX) { OriginX = MinX; OriginY = MinY; }
		}

		int32 DeltaX, DeltaY;
		if (Anchor)
		{
			DeltaX = OriginX - Anchor->NewX;
			DeltaY = OriginY - Anchor->NewY;
		}
		else
		{
			DeltaX = OriginX - MinNewX;
			DeltaY = OriginY - MinNewY;
		}

		Graph->Modify();
		BP->Modify();

		int32 MaxXOut = INT32_MIN, MaxYOut = INT32_MIN;
		int32 MinXOut = INT32_MAX, MinYOut = INT32_MAX;
		for (FPALayoutNode& LN : Nodes)
		{
			if (!LN.bPlaced) continue;
			if (!LN.Node) continue;
			if (LN.Node->IsA<UEdGraphNode_Comment>()) continue;
			const int32 NX = LN.NewX + DeltaX;
			const int32 NY = LN.NewY + DeltaY;
			LN.Node->Modify();
			LN.Node->NodePosX = NX;
			LN.Node->NodePosY = NY;
			MinXOut = FMath::Min(MinXOut, NX);
			MinYOut = FMath::Min(MinYOut, NY);
			MaxXOut = FMath::Max(MaxXOut, NX + LN.Width);
			MaxYOut = FMath::Max(MaxYOut, NY + LN.Height);
			Result.NodesPositioned += 1;
		}

		// Post-placement: L-route diagonal EXEC edges only. Exec flow needs
		// rectilinear turns to read cleanly, and diagonal Bezier on an exec
		// wire reads as a misdirection. Data wires, by contrast, are
		// comfortable as gentle Beziers — hand-authored BPs regularly leave
		// data diagonals with no knots (see reference BPs); adding knots for
		// every small Y offset just creates visual clutter. Keep the data
		// layout pin-aligned at the node level (which PlaceDataProducersPerConsumer
		// already does) and let UE's bezier render the unavoidable diagonals.
		const int32 LKnotsExec = InsertLRouteKnotsForDiagonalExec(Graph, Nodes, 30, Cache);
		const int32 LKnotsData = 0;

		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

		Result.bSucceeded = true;
		Result.LayerCount = LayerCount;
		Result.BoundsWidth  = (MaxXOut == INT32_MIN) ? 0 : (MaxXOut - MinXOut);
		Result.BoundsHeight = (MaxYOut == INT32_MIN) ? 0 : (MaxYOut - MinYOut);
		if (LKnotsExec > 0)
		{
			Result.Warnings.Add(FString::Printf(
				TEXT("pin_aligned: inserted %d reroute knots for diagonal exec edges"), LKnotsExec));
		}
		if (LKnotsData > 0)
		{
			Result.Warnings.Add(FString::Printf(
				TEXT("pin_aligned: inserted %d reroute knots for diagonal data edges"), LKnotsData));
		}
		return Result;
	}

	// ─── exec_flow strategy (original) ────────────────────────
	using namespace TetherBPAutoLayoutImpl;

	TArray<FLayoutNode> Nodes;
	TMap<UEdGraphNode*, int32> IndexOf;
	BuildGraph(Graph, Nodes, IndexOf);
	if (Nodes.Num() == 0) { Result.bSucceeded = true; return Result; }

	// Skip comment boxes — they enclose other nodes; moving them breaks intent.
	TArray<int32> Layoutable;
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		if (Nodes[i].Node && !Nodes[i].Node->IsA<UEdGraphNode_Comment>())
		{
			Layoutable.Add(i);
		}
	}

	const int32 LayerCount = AssignLayers(Nodes, Result.Warnings);
	BarycentricOrder(Nodes, LayerCount);

	// Compute per-layer widest + per-layer cumulative X offset.
	TArray<int32> LayerWidth; LayerWidth.SetNumZeroed(LayerCount);
	TArray<TArray<int32>> NodesByLayer; NodesByLayer.SetNum(LayerCount);
	for (int32 Idx : Layoutable)
	{
		const FLayoutNode& LN = Nodes[Idx];
		const int32 L = FMath::Clamp(LN.Layer, 0, LayerCount - 1);
		if (LN.Width > LayerWidth[L]) LayerWidth[L] = LN.Width;
		NodesByLayer[L].Add(Idx);
	}

	// Order within each layer by OrderInLayer (already set).
	for (TArray<int32>& Layer : NodesByLayer)
	{
		Layer.Sort([&](int32 A, int32 B) {
			return Nodes[A].OrderInLayer < Nodes[B].OrderInLayer;
		});
	}

	// Compute anchor origin: use anchor node's current position if given, else
	// top-left of existing bounding box.
	int32 OriginX = 0, OriginY = 0;
	const FLayoutNode* Anchor = nullptr;
	if (!AnchorNodeGuid.IsEmpty())
	{
		for (const FLayoutNode& LN : Nodes)
		{
			if (LN.Node && LN.Node->NodeGuid.ToString(EGuidFormats::Digits) == AnchorNodeGuid)
			{
				Anchor = &LN; break;
			}
		}
		if (!Anchor)
		{
			Result.Warnings.Add(FString::Printf(TEXT("anchor node %s not found"), *AnchorNodeGuid));
		}
		else
		{
			OriginX = Anchor->Node->NodePosX;
			OriginY = Anchor->Node->NodePosY;
		}
	}
	if (!Anchor)
	{
		int32 MinX = INT32_MAX, MinY = INT32_MAX;
		for (int32 Idx : Layoutable)
		{
			MinX = FMath::Min(MinX, Nodes[Idx].Node->NodePosX);
			MinY = FMath::Min(MinY, Nodes[Idx].Node->NodePosY);
		}
		if (MinX != INT32_MAX) { OriginX = MinX; OriginY = MinY; }
	}

	// Walk layers: compute X offset per layer, then stack nodes vertically.
	TArray<int32> LayerX; LayerX.SetNum(LayerCount);
	int32 Cursor = 0;
	for (int32 L = 0; L < LayerCount; ++L)
	{
		LayerX[L] = Cursor;
		Cursor += LayerWidth[L] + HSpace;
	}

	// Vertically center each layer around the bounding box midpoint.
	int32 MaxLayerHeight = 0;
	TArray<int32> LayerHeight; LayerHeight.SetNumZeroed(LayerCount);
	for (int32 L = 0; L < LayerCount; ++L)
	{
		int32 H = 0;
		for (int32 Idx : NodesByLayer[L])
		{
			H += Nodes[Idx].Height + VSpace;
		}
		if (H > 0) H -= VSpace;  // no trailing spacing
		LayerHeight[L] = H;
		if (H > MaxLayerHeight) MaxLayerHeight = H;
	}

	// If anchor is set, preserve anchor's (NodePosX, NodePosY) exactly. Compute
	// the delta between anchor's would-be layout position and its current one,
	// then translate everyone by the delta.
	int32 DeltaX = OriginX;
	int32 DeltaY = OriginY;
	if (Anchor)
	{
		const int32 AnchorIdx = IndexOf[Anchor->Node];
		const int32 AL = FMath::Clamp(Nodes[AnchorIdx].Layer, 0, LayerCount - 1);
		const int32 AnchorLayoutX = LayerX[AL];
		int32 AnchorLayoutY = 0;
		for (int32 Idx : NodesByLayer[AL])
		{
			if (Idx == AnchorIdx) break;
			AnchorLayoutY += Nodes[Idx].Height + VSpace;
		}
		// Center layer vertically: layer y starts at -LayerHeight/2.
		AnchorLayoutY -= LayerHeight[AL] / 2;
		DeltaX = OriginX - AnchorLayoutX;
		DeltaY = OriginY - AnchorLayoutY;
	}

	Graph->Modify();
	BP->Modify();

	int32 MaxXOut = 0, MaxYOut = 0;
	for (int32 L = 0; L < LayerCount; ++L)
	{
		int32 Y = -LayerHeight[L] / 2;
		for (int32 Idx : NodesByLayer[L])
		{
			FLayoutNode& LN = Nodes[Idx];
			const int32 NewX = DeltaX + LayerX[L];
			const int32 NewY = DeltaY + Y;
			LN.Node->Modify();
			LN.Node->NodePosX = NewX;
			LN.Node->NodePosY = NewY;
			if (NewX + LN.Width  > MaxXOut) MaxXOut = NewX + LN.Width;
			if (NewY + LN.Height > MaxYOut) MaxYOut = NewY + LN.Height;
			Y += LN.Height + VSpace;
			Result.NodesPositioned += 1;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

	Result.bSucceeded = true;
	Result.LayerCount = LayerCount;
	Result.BoundsWidth  = MaxXOut - DeltaX;
	Result.BoundsHeight = MaxYOut - DeltaY;
	return Result;
}
