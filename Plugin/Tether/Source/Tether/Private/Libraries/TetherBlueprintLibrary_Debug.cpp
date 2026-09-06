#include "TetherBlueprintLibrary.h"
#include "TetherBlueprintImpl_Shared.h"
#include "Misc/EngineVersionComparison.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/UnrealType.h"
#include "UObject/Stack.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/Breakpoint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "K2Node.h"

bool UTetherBlueprintLibrary::AddBreakpoint(
	const FString& BlueprintPath, const FString& GraphName,
	const FString& NodeGuid, bool bEnabled)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;
	FKismetDebugUtilities::CreateBreakpoint(BP, Node, bEnabled);
	FKismetDebugUtilities::SetBreakpointEnabled(Node, BP, bEnabled);
	return true;
}
bool UTetherBlueprintLibrary::RemoveBreakpoint(
	const FString& BlueprintPath, const FString& GraphName, const FString& NodeGuid)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	UEdGraph* Graph = TetherBlueprintGraphWriteImpl::FindGraphByName(BP, GraphName); if (!Graph) return false;
	UEdGraphNode* Node = TetherBlueprintGraphWriteImpl::FindNodeByGuid(Graph, NodeGuid);
	if (!Node) return false;
	if (!FKismetDebugUtilities::FindBreakpointForNode(Node, BP)) return false;
	FKismetDebugUtilities::RemoveBreakpointFromNode(Node, BP);
	return true;
}

int32 UTetherBlueprintLibrary::ClearAllBreakpoints(const FString& BlueprintPath)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return 0;
	int32 Count = 0;
	FKismetDebugUtilities::ForeachBreakpoint(BP, [&Count](FBlueprintBreakpoint&) { ++Count; });
	if (Count > 0)
	{
		FKismetDebugUtilities::ClearBreakpoints(BP);
	}
	return Count;
}

TArray<FTetherBreakpointInfo> UTetherBlueprintLibrary::GetBreakpoints(const FString& BlueprintPath)
{
	TArray<FTetherBreakpointInfo> Out;
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return Out;
	FKismetDebugUtilities::ForeachBreakpoint(BP, [&Out](FBlueprintBreakpoint& BP_BP)
	{
		FTetherBreakpointInfo Info;
		UEdGraphNode* Node = BP_BP.GetLocation();
		if (Node)
		{
			Info.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
			Info.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			if (UEdGraph* OwningGraph = Node->GetGraph())
			{
				Info.GraphName = OwningGraph->GetName();
			}
		}
		Info.bEnabled = BP_BP.IsEnabledByUser();
		Out.Add(Info);
	});
	return Out;
}
namespace TetherDebugState
{
	/** Keyed by BP's GetPathName() — module-lifetime storage. */
	static TMap<FString, FTetherBreakpointHit> LastHits;
	static FDelegateHandle ScriptExceptionHandle;

	/** Resolve the UBlueprint that owns a running UFunction, when possible. */
	static UBlueprint* BPFromFunction(const UFunction* Func)
	{
		if (!Func) return nullptr;
		UClass* OwnerClass = Func->GetOuterUClass();
		if (!OwnerClass) return nullptr;
		if (UBlueprint* BP = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy))
		{
			return BP;
		}
		return nullptr;
	}

	/** Name of the graph that owns the UFunction (UbergraphPages / FunctionGraphs). */
	static FString ResolveGraphNameForFunction(UBlueprint* BP, const UFunction* Func)
	{
		if (!BP || !Func) return FString();
		const FName FnName = Func->GetFName();
		for (UEdGraph* G : BP->FunctionGraphs) { if (G && G->GetFName() == FnName) return G->GetName(); }
		// Ubergraph functions are the flattened form of EventGraph events.
		for (UEdGraph* G : BP->UbergraphPages) { if (G) return G->GetName(); }
		return FString();
	}

	static void HandleScriptException(const UObject* ActiveObject,
		const FFrame& StackFrame, const FBlueprintExceptionInfo& Info)
	{
		if (Info.GetType() != EBlueprintExceptionType::Breakpoint) return;

		UFunction* Func = StackFrame.Node;
		UBlueprint* BP = BPFromFunction(Func);
		if (!BP) return;

		// Resolve the node that triggered the break.
		const int32 Offset = static_cast<int32>(StackFrame.Code - Func->Script.GetData()) - 1;
		UEdGraphNode* Node = FKismetDebugUtilities::FindSourceNodeForCodeLocation(
			ActiveObject, Func, Offset, /*bAllowImpreciseHit*/ true);

		FTetherBreakpointHit Hit;
		Hit.bHasHit      = true;
		Hit.BlueprintPath = BP->GetPathName();
		Hit.FunctionName = Func->GetName();
		Hit.GraphName    = ResolveGraphNameForFunction(BP, Func);
		Hit.NodeGuid     = Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
		Hit.NodeTitle    = Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString();
		Hit.SelfPath     = ActiveObject ? ActiveObject->GetPathName() : FString();
		Hit.HitTime      = FPlatformTime::Seconds();

		constexpr int32 MaxLen = 512;
		auto CapValue = [](FString& S)
		{
			if (S.Len() > MaxLen) S = S.Left(MaxLen) + TEXT("…");
		};

		// Walk UFunction properties: params + locals share the Locals buffer.
		uint8* Locals = StackFrame.Locals;
		if (Locals && Func)
		{
			for (TFieldIterator<FProperty> It(Func); It; ++It)
			{
				FProperty* Prop = *It;
				if (!Prop) continue;
				FTetherBreakpointHitValue V;
				V.Name = Prop->GetName();
				V.Type = Prop->GetCPPType();
				if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))    V.Kind = TEXT("return");
				else if (Prop->HasAnyPropertyFlags(CPF_Parm))     V.Kind = TEXT("param");
				else                                              V.Kind = TEXT("local");

				const void* Addr = Prop->ContainerPtrToValuePtr<void>(Locals);
				Prop->ExportTextItem_Direct(V.Value, Addr, nullptr, const_cast<UObject*>(ActiveObject), PPF_None);
				CapValue(V.Value);
				Hit.Values.Add(MoveTemp(V));
			}
		}

		// Walk instance UPROPERTYs of the executing object — but only those
		// declared on a Blueprint-generated class. Including native parents
		// would dump 50+ Engine UPROPERTYs (Actor / Pawn / Character internals)
		// per hit, drowning the BP-authored variables the user actually wants.
		if (ActiveObject)
		{
			UClass* InstanceClass = ActiveObject->GetClass();
			for (TFieldIterator<FProperty> It(InstanceClass); It; ++It)
			{
				FProperty* Prop = *It;
				if (!Prop) continue;
				UClass* OwnerClass = Prop->GetOwnerClass();
				if (!OwnerClass || !OwnerClass->IsChildOf<UBlueprintGeneratedClass>()) continue;

				FTetherBreakpointHitValue V;
				V.Name = Prop->GetName();
				V.Type = Prop->GetCPPType();
				V.Kind = TEXT("instance");
				V.OwnerClass = OwnerClass->GetPathName();

				const void* Addr = Prop->ContainerPtrToValuePtr<void>(ActiveObject);
				Prop->ExportTextItem_Direct(V.Value, Addr, nullptr, const_cast<UObject*>(ActiveObject), PPF_None);
				CapValue(V.Value);
				Hit.Values.Add(MoveTemp(V));
			}
		}

		LastHits.Add(Hit.BlueprintPath, MoveTemp(Hit));
	}

	void Register()
	{
		if (ScriptExceptionHandle.IsValid()) return;
		ScriptExceptionHandle =
			FBlueprintCoreDelegates::OnScriptException.AddStatic(&HandleScriptException);
	}

	void Unregister()
	{
		if (!ScriptExceptionHandle.IsValid()) return;
		FBlueprintCoreDelegates::OnScriptException.Remove(ScriptExceptionHandle);
		ScriptExceptionHandle.Reset();
		LastHits.Empty();
	}
}

TArray<FTetherNodeCoverageEntry> UTetherBlueprintLibrary::GetPIENodeCoverage(
	const FString& BlueprintPath)
{
	TArray<FTetherNodeCoverageEntry> Out;
	UBlueprint* BP = LoadBP(BlueprintPath);
	if (!BP || !BP->GeneratedClass) return Out;
	UClass* GenClass = BP->GeneratedClass;
	UClass* SkelClass = BP->SkeletonGeneratedClass;

	// Aggregate samples from UE's script-trace ring buffer that belong to
	// functions on the target BP's generated or skeleton class.
	const TSimpleRingBuffer<FKismetTraceSample>& Ring = FKismetDebugUtilities::GetTraceStack();

	struct FAgg { int32 Count = 0; double Last = 0.0; FString Title; FString Graph; };
	TMap<FString, FAgg> ByGuid;

	for (int32 i = 0; i < Ring.Num(); ++i)
	{
		const FKismetTraceSample& S = Ring(i);
		const UFunction* Func = S.Function.Get();
		if (!Func) continue;
		UClass* Owner = Func->GetOuterUClass();
		if (!Owner) continue;
		const bool bMatch =
			(GenClass  && Owner == GenClass)  ||
			(SkelClass && Owner == SkelClass) ||
			(GenClass  && Owner->IsChildOf(GenClass));
		if (!bMatch) continue;

		UObject* Ctx = S.Context.Get();
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
		UEdGraphNode* Node = FKismetDebugUtilities::FindSourceNodeForCodeLocation(
			Ctx, Func, S.Offset, /*bAllowImpreciseHit*/ true);
#else
		// 5.4: 2nd arg expects UFunction* (non-const).
		UEdGraphNode* Node = FKismetDebugUtilities::FindSourceNodeForCodeLocation(
			Ctx, const_cast<UFunction*>(Func), S.Offset, /*bAllowImpreciseHit*/ true);
#endif
		if (!Node) continue;

		const FString Guid = Node->NodeGuid.ToString(EGuidFormats::Digits);
		FAgg& Row = ByGuid.FindOrAdd(Guid);
		Row.Count += 1;
		if (S.ObservationTime > Row.Last) Row.Last = S.ObservationTime;
		if (Row.Title.IsEmpty())
		{
			Row.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			if (UEdGraph* G = Node->GetGraph()) Row.Graph = G->GetName();
		}
	}

	Out.Reserve(ByGuid.Num());
	for (const auto& It : ByGuid)
	{
		FTetherNodeCoverageEntry E;
		E.NodeGuid    = It.Key;
		E.NodeTitle   = It.Value.Title;
		E.GraphName   = It.Value.Graph;
		E.HitCount    = It.Value.Count;
		E.LastHitTime = It.Value.Last;
		Out.Add(MoveTemp(E));
	}
	// Sort by hit count descending — most-run nodes first.
	Out.Sort([](const FTetherNodeCoverageEntry& A, const FTetherNodeCoverageEntry& B)
	{
		return A.HitCount > B.HitCount;
	});
	return Out;
}

bool UTetherBlueprintLibrary::SetBlueprintDebugObject(
	const FString& BlueprintPath, const FString& ActorName)
{
	UBlueprint* BP = LoadBP(BlueprintPath); if (!BP) return false;
	if (ActorName.IsEmpty())
	{
		BP->SetObjectBeingDebugged(nullptr);
		return true;
	}
	// Walk every world UE currently knows about (editor + PIE copies) for an
	// actor with the requested label so the caller doesn't have to care which
	// world owns it.
	AActor* Found = nullptr;
	for (TObjectIterator<UWorld> WorldIt; WorldIt && !Found; ++WorldIt)
	{
		UWorld* W = *WorldIt;
		if (!W || (W->WorldType != EWorldType::Editor && W->WorldType != EWorldType::PIE))
			continue;
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (It->GetActorLabel() == ActorName ||
			    It->GetName()       == ActorName)
			{
				Found = *It; break;
			}
		}
	}
	if (!Found) return false;
	BP->SetObjectBeingDebugged(Found);
	return true;
}

FTetherBreakpointHit UTetherBlueprintLibrary::GetLastBreakpointHit(
	const FString& BlueprintPath)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	const FString Key = BP ? BP->GetPathName() : BlueprintPath;
	if (const FTetherBreakpointHit* Found = TetherDebugState::LastHits.Find(Key))
	{
		return *Found;
	}
	return FTetherBreakpointHit();
}

void UTetherBlueprintLibrary::ClearLastBreakpointHit(const FString& BlueprintPath)
{
	UBlueprint* BP = LoadBP(BlueprintPath);
	const FString Key = BP ? BP->GetPathName() : BlueprintPath;
	TetherDebugState::LastHits.Remove(Key);
}

void UTetherBlueprintLibrary::ResumeScriptExecution()
{
	FKismetDebugUtilities::RequestAbortingExecution();
}

int32 UTetherBlueprintLibrary::ClearProjectBreakpoints(const FString& PackagePath)
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

	int32 Total = 0;
	for (const FAssetData& Data : BpAssets)
	{
		UBlueprint* BP = LoadBP(Data.GetSoftObjectPath().ToString());
		if (!BP) continue;
		int32 Before = 0;
		FKismetDebugUtilities::ForeachBreakpoint(BP,
			[&Before](FBlueprintBreakpoint&) { ++Before; });
		if (Before == 0) continue;
		FKismetDebugUtilities::ClearBreakpoints(BP);
		Total += Before;
	}
	return Total;
}
