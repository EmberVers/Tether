// Shared internal helpers for the split TetherBlueprintLibrary translation
// units (CRUD / Graph / Layout / Debug). Everything here was previously
// file-local to the single 11865-line TetherBlueprintLibrary.cpp; the class
// and its UFUNCTION surface are unchanged — only the TU boundaries moved.
//
// Rules of the split:
//   * LoadBP is used by every TU → inline here.
//   * Namespaces used by more than one TU are declared here with internal
//     linkage bodies emitted in exactly one TU (see bottom-of-file map).
//   * Namespaces with a single consumer TU stay in that TU's .cpp.
#pragma once

#include "TetherBlueprintLibrary.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

// ─── LoadBP (every TU) ─────────────────────────────────────────

inline UBlueprint* LoadBP(const FString& Path)
{
	return LoadObject<UBlueprint>(nullptr, *Path);
}

// ─── TetherBlueprintGraphWriteImpl (Graph/Layout/Debug + main) ─
// Body lives in TetherBlueprintLibrary_Graph.cpp.

namespace TetherBlueprintGraphWriteImpl
{
	UEdGraph* FindGraphByName(UBlueprint* BP, const FString& GraphName);
	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& GuidStr);
}

// ─── TetherBPCollapseImpl::FindNodeByGuid (Graph TU + main) ────
// Body lives in TetherBlueprintLibrary_Graph.cpp.

namespace TetherBPCollapseImpl
{
	UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FString& GuidStr);
}

// ─── TetherBPSummaryImpl, block 5169 (lookup + pin typing) ─────
// Bodies live in TetherBlueprintLibrary.cpp (the main TU).

namespace TetherBPSummaryImpl
{
	UEdGraphNode* FindNodeInGraphByGuid(UEdGraph* Graph, const FString& NodeGuid);
	UEdGraph* FindSingleGraphByName(UBlueprint* BP, const FString& Name);
	FString PinTypeToHuman(const FEdGraphPinType& PT);
	FString GetPinDefaultString(const UEdGraphPin* Pin);
}

// ─── TetherBPSummaryImpl, all-graphs iteration (Graph + main) ──
// Bodies live in TetherBlueprintLibrary.cpp (the main TU).

namespace TetherBPSummaryImpl
{
	struct FAllGraphs { UEdGraph* Graph; FString Type; };
	TArray<FAllGraphs> CollectAllGraphs(UBlueprint* BP);
}

// ─── PropertyTypeToString (Graph + main) ───────────────────────
// Body lives in TetherBlueprintLibrary.cpp (the main TU).

FString PropertyTypeToString(const FProperty* Prop);

// ─── TetherBPSummaryImpl, block 5321 (size estimation) ─────────
// Bodies live in TetherBlueprintLibrary_Layout.cpp.

namespace TetherBPSummaryImpl
{
	// Rough UE K2 node layout constants — match typical rendered sizes
	// within ±20 px, enough for relative adjacency placement.
	constexpr int32 HeaderHeight  = 40;  // title bar above pins
	constexpr int32 PinRowHeight  = 22;
	constexpr int32 FooterHeight  = 12;
	constexpr int32 MinNodeWidth  = 180;
	constexpr int32 MinNodeHeight = 60;
	constexpr int32 MaxNodeWidth  = 520;
	constexpr int32 PinInsetX     = 10;

	int32 ComputePinLocalY(const UEdGraphNode* Node,
		EEdGraphPinDirection Dir, int32 DirIdx);
	void EstimateNodeSize(const UEdGraphNode* Node, int32& W, int32& H);
}
