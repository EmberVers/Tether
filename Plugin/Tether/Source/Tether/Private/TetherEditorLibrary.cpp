#include "TetherEditorLibrary.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Selection.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserItemPath.h"
#include "FileHelpers.h"
#include "PackageTools.h"
#include "GameFramework/Actor.h"
#include "PlayInEditorDataTypes.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"
#include "Engine/Engine.h"
#include "Engine/Blueprint.h"
#include "Engine/ObjectLibrary.h"
#include "UObject/ObjectRedirector.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "LevelEditorViewport.h"
#include "IAssetViewport.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/EngineVersionComparison.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "HighResScreenshot.h"
#include "ShowFlags.h"
#include "Engine/EngineBaseTypes.h"
#include "Editor/UnrealEdTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SViewport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Interfaces/IMainFrameModule.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "EditorModeManager.h"
#include "UnrealWidget.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "PixelFormat.h"
#include "HitProxies.h"
#include "EngineUtils.h"
#if PLATFORM_WINDOWS
#include "ILiveCodingModule.h"
#endif
#include "TetherCallLog.h"
#include "TetherWorldSelection.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "UObject/UnrealType.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/WindowsWindow.h"
#endif

#define LOCTEXT_NAMESPACE "TetherEditor"

namespace TetherEditorImpl
{
	UWorld* GetEditorWorld()
	{
		if (GEditor)
		{
			return GEditor->GetEditorWorldContext().World();
		}
		return nullptr;
	}

	FLevelEditorViewportClient* GetActiveViewportClient()
	{
		if (!FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			return nullptr;
		}
		FLevelEditorModule& LE = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<SLevelViewport> Viewport = LE.GetFirstActiveLevelViewport();
		if (!Viewport.IsValid())
		{
			return nullptr;
		}
		return &Viewport->GetLevelViewportClient();
	}
}

// ─── State ──────────────────────────────────────────────────

FString UTetherEditorLibrary::GetEngineVersion()
{
	return FEngineVersion::Current().ToString();
}

bool UTetherEditorLibrary::IsInPIE()
{
	return GEditor && GEditor->PlayWorld != nullptr;
}

bool UTetherEditorLibrary::IsPlayInEditorPaused()
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return false;
	}
	return GEditor->PlayWorld->bDebugPauseExecution;
}

FTetherEditorState UTetherEditorLibrary::GetEditorState()
{
	FTetherEditorState S;
	S.EngineVersion = FEngineVersion::Current().ToString();
	S.ProjectName = FApp::GetProjectName();
	S.bIsPIE = IsInPIE();
	S.bIsPaused = IsPlayInEditorPaused();

	if (UWorld* World = TetherEditorImpl::GetEditorWorld())
	{
		if (UPackage* Pkg = World->GetOutermost())
		{
			S.CurrentLevelPath = Pkg->GetName();
		}
	}

	if (GEditor)
	{
		if (UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			S.NumOpenedAssets = AES->GetAllEditedAssets().Num();
		}
		S.NumSelectedActors = GEditor->GetSelectedActorCount();
	}

	if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> Sel;
		CBM.Get().GetSelectedAssets(Sel);
		S.NumContentBrowserSelection = Sel.Num();
	}
	return S;
}

TArray<FTetherOpenedAsset> UTetherEditorLibrary::GetOpenedAssets()
{
	TArray<FTetherOpenedAsset> Out;
	if (!GEditor)
	{
		return Out;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AES)
	{
		return Out;
	}
	for (UObject* O : AES->GetAllEditedAssets())
	{
		if (!O)
		{
			continue;
		}
		FTetherOpenedAsset A;
		A.Path = O->GetPathName();
		A.ClassName = O->GetClass()->GetName();
		if (UPackage* Pkg = O->GetOutermost())
		{
			A.bIsDirty = Pkg->IsDirty();
		}
		Out.Add(A);
	}
	return Out;
}

TArray<FString> UTetherEditorLibrary::GetContentBrowserSelection()
{
	TArray<FString> Out;
	if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		return Out;
	}
	FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> Sel;
	CBM.Get().GetSelectedAssets(Sel);
	for (const FAssetData& AD : Sel)
	{
		Out.Add(AD.GetObjectPathString());
	}
	return Out;
}

FString UTetherEditorLibrary::GetContentBrowserPath()
{
	if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		return FString();
	}
	FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
	return CBM.Get().GetCurrentPath().GetInternalPathString();
}

FTetherViewportCamera UTetherEditorLibrary::GetEditorViewportCamera()
{
	FTetherViewportCamera Cam;
	if (FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient())
	{
		Cam.Location = VC->GetViewLocation();
		Cam.Rotation = VC->GetViewRotation();
		Cam.FOV = VC->ViewFOV;
	}
	return Cam;
}

// ─── Asset control ──────────────────────────────────────────

namespace TetherEditorImpl
{
	UObject* ResolveInnerAssetIfPackage(UObject* Obj)
	{
		UPackage* Pkg = Cast<UPackage>(Obj);
		if (!Pkg)
		{
			return Obj;
		}
		const FString LeafName = FPackageName::GetShortName(Pkg->GetFName());
		UObject* Match = nullptr;
		UObject* Fallback = nullptr;
		ForEachObjectWithOuter(Pkg, [&](UObject* Inner)
		{
			if (Match || !Inner || Inner->HasAnyFlags(RF_Transient))
			{
				return;
			}
			if (Inner->GetName() == LeafName)
			{
				Match = Inner;
			}
			else if (!Fallback && Inner->IsAsset())
			{
				Fallback = Inner;
			}
		}, false);
		if (Match)
		{
			return Match;
		}
		return Fallback ? Fallback : Obj;
	}

	UObject* LoadAssetFromPath(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		// Normalize "/Foo/Bar" → "/Foo/Bar.Bar" so we load the inner asset, not the UPackage wrapper.
		FString Normalized = AssetPath;
		int32 DotIdx;
		if (!Normalized.FindChar(TEXT('.'), DotIdx))
		{
			int32 SlashIdx;
			if (Normalized.FindLastChar(TEXT('/'), SlashIdx) && SlashIdx + 1 < Normalized.Len())
			{
				const FString Leaf = Normalized.Mid(SlashIdx + 1);
				Normalized = Normalized + TEXT(".") + Leaf;
			}
		}
		UObject* Result = FindObject<UObject>(nullptr, *Normalized);
		if (!Result)
		{
			Result = LoadObject<UObject>(nullptr, *Normalized);
		}
		return ResolveInnerAssetIfPackage(Result);
	}
}

bool UTetherEditorLibrary::OpenAsset(const FString& AssetPath)
{
	if (!GEditor)
	{
		return false;
	}
	UObject* A = TetherEditorImpl::LoadAssetFromPath(AssetPath);
	if (!A)
	{
		UE_LOG(LogTemp, Warning, TEXT("Tether: OpenAsset could not load '%s'"), *AssetPath);
		return false;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	return AES && AES->OpenEditorForAsset(A);
}

bool UTetherEditorLibrary::CloseAllAssetEditors()
{
	if (!GEditor)
	{
		return false;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AES)
	{
		return false;
	}
	AES->CloseAllAssetEditors();
	return true;
}

bool UTetherEditorLibrary::SaveAsset(const FString& AssetPath)
{
	UObject* A = TetherEditorImpl::LoadAssetFromPath(AssetPath);
	if (!A)
	{
		UE_LOG(LogTemp, Warning, TEXT("Tether: SaveAsset could not load '%s'"), *AssetPath);
		return false;
	}
	UPackage* Pkg = A->GetOutermost();
	if (!Pkg)
	{
		return false;
	}
	return UEditorLoadingAndSavingUtils::SavePackages({ Pkg }, false);
}

bool UTetherEditorLibrary::SaveAllDirtyAssets(bool bIncludeMaps)
{
	return UEditorLoadingAndSavingUtils::SaveDirtyPackages(bIncludeMaps, true);
}

bool UTetherEditorLibrary::SaveCurrentLevel()
{
	return FEditorFileUtils::SaveCurrentLevel();
}

bool UTetherEditorLibrary::ReloadAsset(const FString& AssetPath)
{
	UObject* A = TetherEditorImpl::LoadAssetFromPath(AssetPath);
	if (!A)
	{
		return false;
	}
	UPackage* Pkg = A->GetOutermost();
	if (!Pkg)
	{
		return false;
	}
	return UPackageTools::ReloadPackages({ Pkg });
}

bool UTetherEditorLibrary::SetContentBrowserSelection(const TArray<FString>& AssetPaths)
{
	if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		return false;
	}
	TArray<FAssetData> ADs;
	for (const FString& P : AssetPaths)
	{
		UObject* O = TetherEditorImpl::LoadAssetFromPath(P);
		if (O)
		{
			ADs.Emplace(O);
		}
	}
	if (ADs.Num() == 0)
	{
		return false;
	}
	FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
	CBM.Get().SyncBrowserToAssets(ADs);
	return true;
}

bool UTetherEditorLibrary::SyncContentBrowserToAsset(const FString& AssetPath)
{
	TArray<FString> One;
	One.Add(AssetPath);
	return SetContentBrowserSelection(One);
}

bool UTetherEditorLibrary::FocusViewportOnSelection()
{
	if (!GEditor)
	{
		return false;
	}
	USelection* Sel = GEditor->GetSelectedActors();
	if (!Sel || Sel->Num() == 0)
	{
		return false;
	}
	AActor* First = nullptr;
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		if (AActor* A = Cast<AActor>(*It))
		{
			First = A;
			break;
		}
	}
	if (!First)
	{
		return false;
	}
	GEditor->MoveViewportCamerasToActor(*First, true);
	return true;
}

bool UTetherEditorLibrary::SetEditorViewportCamera(FVector Location, FRotator Rotation, float FOV)
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	VC->SetViewLocation(Location);
	VC->SetViewRotation(Rotation);
	if (FOV > 0.f)
	{
		VC->ViewFOV = FOV;
	}
	VC->Invalidate();
	return true;
}

// ─── PIE ────────────────────────────────────────────────────

bool UTetherEditorLibrary::StartPIE()
{
	if (!GEditor)
	{
		return false;
	}
	if (GEditor->PlayWorld)
	{
		// Already in PIE.
		return true;
	}
	FRequestPlaySessionParams Params;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;

	// Route PIE into the active level viewport (Play-In-Editor "Selected
	// Viewport" mode) and forward the viewport camera as the spawn transform.
	// The StartLocation matters when the level has no PlayerStart — without it
	// RequestPlaySession spawns at (0,0,0), collides, and the default pawn is
	// never possessed.
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LE = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<IAssetViewport> ActiveViewport = LE.GetFirstActiveViewport();
		if (ActiveViewport.IsValid())
		{
			Params.DestinationSlateViewport = ActiveViewport;
			FEditorViewportClient& VC = ActiveViewport->GetAssetViewportClient();
			Params.StartLocation = VC.GetViewLocation();
			Params.StartRotation = VC.GetViewRotation();
		}
	}

	GEditor->RequestPlaySession(Params);
	// The request is deferred to the next editor tick; kick it so PIE actually
	// starts (and spawns the default pawn) without requiring an external tick.
	GEditor->StartQueuedPlaySessionRequest();
	return true;
}

bool UTetherEditorLibrary::StopPIE()
{
	if (!GEditor)
	{
		return false;
	}
	GEditor->RequestEndPlayMap();
	return true;
}

bool UTetherEditorLibrary::PausePIE(bool bPaused)
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return false;
	}
	return GEditor->SetPIEWorldsPaused(bPaused);
}

bool UTetherEditorLibrary::StartSimulate()
{
	if (!GEditor)
	{
		return false;
	}
	if (GEditor->PlayWorld)
	{
		// Already playing — can't switch mode without stopping first.
		return true;
	}
	FRequestPlaySessionParams Params;
	Params.WorldType = EPlaySessionWorldType::SimulateInEditor;

	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LE = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<IAssetViewport> ActiveViewport = LE.GetFirstActiveViewport();
		if (ActiveViewport.IsValid())
		{
			Params.DestinationSlateViewport = ActiveViewport;
		}
	}
	GEditor->RequestPlaySession(Params);
	return true;
}

bool UTetherEditorLibrary::IsSimulating()
{
	return GEditor && GEditor->bIsSimulatingInEditor;
}

FString UTetherEditorLibrary::GetPIENetMode()
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return FString();
	}
	switch (GEditor->PlayWorld->GetNetMode())
	{
	case NM_Standalone:      return TEXT("Standalone");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	case NM_ListenServer:    return TEXT("ListenServer");
	case NM_Client:          return TEXT("Client");
	default:                 return TEXT("Standalone");
	}
}

float UTetherEditorLibrary::GetPIEWorldTime()
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return -1.0f;
	}
	return GEditor->PlayWorld->GetTimeSeconds();
}

// ─── Undo ───────────────────────────────────────────────────

bool UTetherEditorLibrary::Undo()
{
	if (!GEditor)
	{
		return false;
	}
	return GEditor->UndoTransaction();
}

bool UTetherEditorLibrary::Redo()
{
	if (!GEditor)
	{
		return false;
	}
	return GEditor->RedoTransaction();
}

// ─── Console / CVar ─────────────────────────────────────────

namespace TetherEditorImpl
{
	class FCaptureDevice : public FOutputDevice
	{
	public:
		FString Output;
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type, const FName&) override
		{
			Output += V;
			Output += TEXT("\n");
		}
	};
}

FString UTetherEditorLibrary::ExecuteConsoleCommand(const FString& Command)
{
	TetherEditorImpl::FCaptureDevice Dev;
	GLog->AddOutputDevice(&Dev);
	if (GEngine)
	{
		GEngine->Exec(TetherEditorImpl::GetEditorWorld(), *Command, Dev);
	}
	GLog->RemoveOutputDevice(&Dev);
	return Dev.Output;
}

FString UTetherEditorLibrary::ExecutePIEConsoleCommand(const FString& Command)
{
	if (!GEditor)
	{
		return TEXT("Tether: editor is unavailable.\n");
	}

	UWorld* PIEWorld = TetherAgentImpl::SelectFirstValidPIEWorld(GEditor->GetWorldContexts());
	if (!PIEWorld)
	{
		return TEXT("Tether: no begun-play PIE world is available.\n");
	}

	TetherEditorImpl::FCaptureDevice Dev;
	GLog->AddOutputDevice(&Dev);
	if (GEngine)
	{
		GEngine->Exec(PIEWorld, *Command, Dev);
	}
	else
	{
		Dev.Output = TEXT("Tether: engine is unavailable.\n");
	}
	GLog->RemoveOutputDevice(&Dev);
	return Dev.Output;
}

FString UTetherEditorLibrary::GetCVar(const FString& Name)
{
	IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CV)
	{
		return FString();
	}
	return CV->GetString();
}

bool UTetherEditorLibrary::SetCVar(const FString& Name, const FString& Value)
{
	IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CV)
	{
		return false;
	}
	CV->Set(*Value, ECVF_SetByConsole);
	return true;
}

// ─── Utility ────────────────────────────────────────────────

int32 UTetherEditorLibrary::FixupRedirectors(const TArray<FString>& Paths)
{
	if (Paths.Num() == 0)
	{
		return 0;
	}
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AR = ARM.Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
	for (const FString& P : Paths)
	{
		Filter.PackagePaths.Add(*P);
	}

	TArray<FAssetData> Found;
	AR.GetAssets(Filter, Found);

	TArray<UObjectRedirector*> Redirectors;
	Redirectors.Reserve(Found.Num());
	for (const FAssetData& AD : Found)
	{
		if (UObjectRedirector* R = Cast<UObjectRedirector>(AD.GetAsset()))
		{
			Redirectors.Add(R);
		}
	}
	if (Redirectors.Num() == 0)
	{
		return 0;
	}

	FAssetToolsModule& ATM = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	ATM.Get().FixupReferencers(Redirectors, /*bCheckoutDialogPrompt=*/false);
	return Redirectors.Num();
}

TArray<FTetherCompileResult> UTetherEditorLibrary::CompileBlueprints(const TArray<FString>& BlueprintPaths)
{
	TArray<FTetherCompileResult> Out;
	for (const FString& P : BlueprintPaths)
	{
		FTetherCompileResult R;
		R.Path = P;
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *P);
		if (!BP)
		{
			R.ErrorMessage = TEXT("Blueprint not found");
			Out.Add(R);
			continue;
		}
		// 仅做普通修改标记的程序化图编辑不会重建骨架类，因此编译前必须先使结构失效。
		// Programmatic graph edits marked only as modified do not rebuild the skeleton class, so invalidate structure before compiling.
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		switch (BP->Status)
		{
		case BS_UpToDate:
		case BS_UpToDateWithWarnings:
			R.bSuccess = true;
			break;
		case BS_Error:
			R.bSuccess = false;
			R.ErrorMessage = TEXT("Compile error");
			break;
		case BS_Unknown:
		case BS_Dirty:
		default:
			R.bSuccess = false;
			R.ErrorMessage = FString::Printf(TEXT("Status=%d"), (int32)BP->Status);
			break;
		}
		Out.Add(R);
	}
	return Out;
}

bool UTetherEditorLibrary::RecompileBlueprint(const FString& BlueprintPath)
{
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!BP) return false;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	return BP->Status == BS_UpToDate || BP->Status == BS_UpToDateWithWarnings;
}

TArray<FString> UTetherEditorLibrary::ListCVars(const FString& Keyword)
{
	TArray<FString> Out;
	IConsoleManager::Get().ForEachConsoleObjectThatContains(
		FConsoleObjectVisitor::CreateLambda([&Out](const TCHAR* CVName, IConsoleObject* Obj)
		{
			if (Obj)
			{
				if (IConsoleVariable* CV = Obj->AsVariable())
				{
					Out.Add(FString::Printf(TEXT("%s = %s"), CVName, *CV->GetString()));
				}
			}
		}),
		*Keyword);
	return Out;
}

// ─── Dirty-state tracking ──────────────────────────────────

namespace TetherEditorImpl
{
	UPackage* FindPackageForAssetPath(const FString& AssetPath)
	{
		UObject* Asset = TetherEditorImpl::LoadAssetFromPath(AssetPath);
		return Asset ? Asset->GetPackage() : nullptr;
	}
}

TArray<FString> UTetherEditorLibrary::GetDirtyPackageNames()
{
	TArray<FString> Out;
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Pkg = *It;
		if (Pkg && Pkg->IsDirty())
		{
			const FString Name = Pkg->GetName();
			// Skip script packages and engine transient buckets; keep /Game/ and plugin mounts.
			if (!Name.StartsWith(TEXT("/Script/")) && !Name.StartsWith(TEXT("/Temp/")) && Name != TEXT("/Engine/Transient"))
			{
				Out.Add(Name);
			}
		}
	}
	Out.Sort();
	return Out;
}

bool UTetherEditorLibrary::IsAssetDirty(const FString& AssetPath)
{
	const UPackage* Pkg = TetherEditorImpl::FindPackageForAssetPath(AssetPath);
	return Pkg && Pkg->IsDirty();
}

bool UTetherEditorLibrary::MarkAssetDirty(const FString& AssetPath)
{
	UPackage* Pkg = TetherEditorImpl::FindPackageForAssetPath(AssetPath);
	if (!Pkg)
	{
		return false;
	}
	Pkg->MarkPackageDirty();
	return true;
}

bool UTetherEditorLibrary::IsAssetEditorOpen(const FString& AssetPath)
{
	if (!GEditor)
	{
		return false;
	}
	UObject* Asset = TetherEditorImpl::LoadAssetFromPath(AssetPath);
	if (!Asset)
	{
		return false;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AES)
	{
		return false;
	}
	return AES->FindEditorForAsset(Asset, /*bFocusIfOpen=*/false) != nullptr;
}

int32 UTetherEditorLibrary::SaveAssets(const TArray<FString>& AssetPaths)
{
	TArray<UPackage*> Packages;
	Packages.Reserve(AssetPaths.Num());
	for (const FString& P : AssetPaths)
	{
		if (UPackage* Pkg = TetherEditorImpl::FindPackageForAssetPath(P))
		{
			Packages.AddUnique(Pkg);
		}
	}
	if (Packages.Num() == 0)
	{
		return 0;
	}
	const FEditorFileUtils::EPromptReturnCode Ret = FEditorFileUtils::PromptForCheckoutAndSave(
		Packages,
		/*bCheckDirty=*/false,
		/*bPromptToSave=*/false);
	if (Ret != FEditorFileUtils::PR_Success && Ret != FEditorFileUtils::PR_Declined)
	{
		return 0;
	}
	int32 Saved = 0;
	for (UPackage* Pkg : Packages)
	{
		if (Pkg && !Pkg->IsDirty())
		{
			++Saved;
		}
	}
	return Saved;
}

// ─── Level / map control ───────────────────────────────────

bool UTetherEditorLibrary::LoadLevel(const FString& LevelPath, bool bPromptSaveChanges)
{
	if (LevelPath.IsEmpty())
	{
		return false;
	}
	if (bPromptSaveChanges)
	{
		if (!FEditorFileUtils::SaveDirtyPackages(/*bPromptUserToSave=*/true, /*bSaveMapPackages=*/true, /*bSaveContentPackages=*/false))
		{
			// User cancelled save prompt.
			return false;
		}
	}
	return UEditorLoadingAndSavingUtils::LoadMap(LevelPath) != nullptr;
}

bool UTetherEditorLibrary::CreateNewLevel(bool bSaveExisting)
{
	return UEditorLoadingAndSavingUtils::NewBlankMap(bSaveExisting) != nullptr;
}

// ─── Asset editor / browser extras ─────────────────────────

bool UTetherEditorLibrary::CloseAssetEditor(const FString& AssetPath)
{
	if (!GEditor)
	{
		return false;
	}
	UObject* Asset = TetherEditorImpl::LoadAssetFromPath(AssetPath);
	if (!Asset)
	{
		return false;
	}
	UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AES)
	{
		return false;
	}
	if (AES->FindEditorForAsset(Asset, /*bFocusIfOpen=*/false) == nullptr)
	{
		return false;
	}
	AES->CloseAllEditorsForAsset(Asset);
	return true;
}

bool UTetherEditorLibrary::IsAssetLoaded(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return false;
	}
	return FindObject<UObject>(nullptr, *AssetPath) != nullptr;
}

bool UTetherEditorLibrary::SetContentBrowserPath(const FString& FolderPath)
{
	if (FolderPath.IsEmpty())
	{
		return false;
	}
	if (!FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		return false;
	}
	FContentBrowserModule& CBM = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FString> Folders;
	Folders.Add(FolderPath);
	CBM.Get().SyncBrowserToFolders(Folders);
	return true;
}

// ─── Viewport capture ──────────────────────────────────────

bool UTetherEditorLibrary::TakeHighResScreenshot(float ResolutionMultiplier)
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC || !VC->Viewport)
	{
		return false;
	}
	const float Mult = ResolutionMultiplier > 0.f ? ResolutionMultiplier : 1.f;
	GetHighResScreenshotConfig().SetResolution(
		FMath::Max(1, (int32)(VC->Viewport->GetSizeXY().X * Mult)),
		FMath::Max(1, (int32)(VC->Viewport->GetSizeXY().Y * Mult)),
		1.f);
	VC->Viewport->TakeHighResScreenShot();
	return true;
}

namespace TetherEditorImpl
{
	// Prefer the PIE game-window viewport when PIE is running (that's the
	// "game view" callers usually want); otherwise the active level editor
	// viewport. In-place/Immersive PIE is also covered by the editor path
	// because the level viewport there is the one the PIE world renders to.
	FViewport* GetCaptureViewport(FString& OutSource)
	{
		if (GEditor && GEditor->PlayWorld && GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			OutSource = TEXT("PIE");
			return GEngine->GameViewport->Viewport;
		}
		if (FLevelEditorViewportClient* VC = GetActiveViewportClient())
		{
			if (VC->Viewport)
			{
				OutSource = TEXT("LevelEditor");
				return VC->Viewport;
			}
		}
		OutSource.Reset();
		return nullptr;
	}

	// Resolve the Slate widget that presents the same viewport chosen by
	// GetCaptureViewport. Capturing this widget (instead of FViewport pixels)
	// includes the final editor/game overlays composed by Slate.
	TSharedPtr<SViewport> GetCaptureViewportWidget(FString& OutSource)
	{
		if (GEditor && GEditor->PlayWorld && GEngine)
		{
			TSharedPtr<SViewport> ViewportWidget = GEngine->GetGameViewportWidget();
			if (!ViewportWidget.IsValid() && GEngine->GameViewport)
			{
				ViewportWidget = GEngine->GameViewport->GetGameViewportWidget();
			}
			if (ViewportWidget.IsValid())
			{
				OutSource = TEXT("PIE");
				return ViewportWidget;
			}
		}

		if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			if (TSharedPtr<SLevelViewport> LevelViewport = LevelEditor.GetFirstActiveLevelViewport())
			{
				if (TSharedPtr<SViewport> ViewportWidget = LevelViewport->GetViewportWidget().Pin())
				{
					OutSource = TEXT("LevelEditor");
					return ViewportWidget;
				}
			}
		}

		OutSource.Reset();
		return nullptr;
	}

	bool CapturePresentedViewportPixels(
		const TSharedRef<SViewport>& ViewportWidget,
		TArray<FColor>& OutBitmap,
		FIntPoint& OutSize,
		FString& OutError)
	{
#if PLATFORM_WINDOWS
#if UE_VERSION_OLDER_THAN(5, 6, 0)
		OutBitmap.Reset();
		OutSize = FIntPoint::ZeroValue;
		OutError = TEXT("Native-window viewport readback requires Unreal Engine 5.6 or newer; using the Slate screenshot fallback.");
		UE_LOG(LogTemp, Warning, TEXT("Tether: CaptureActiveViewportAsDisplayed: %s"), *OutError);
		return false;
#else
		const TSharedPtr<SWindow> WidgetWindow = FSlateApplication::Get().FindWidgetWindow(ViewportWidget);
		if (!WidgetWindow.IsValid())
		{
			OutError = TEXT("The active viewport is not attached to a Slate window.");
			return false;
		}

		const TSharedPtr<FGenericWindow> NativeWindow = WidgetWindow->GetNativeWindow();
		if (!NativeWindow.IsValid() || !NativeWindow->GetOSWindowHandle())
		{
			OutError = TEXT("The active viewport has no native window handle.");
			return false;
		}

		uint32 WindowWidth = 0;
		uint32 WindowHeight = 0;
		FWindowsWindow* WindowsWindow = static_cast<FWindowsWindow*>(NativeWindow.Get());
		TArray<FColor> WindowPixels = WindowsWindow->GetWindowPixels(WindowWidth, WindowHeight);
		const int64 WindowPixelCount = static_cast<int64>(WindowWidth) * static_cast<int64>(WindowHeight);
		if (WindowWidth == 0 || WindowHeight == 0
			|| WindowPixelCount > MAX_int32 || WindowPixels.Num() < WindowPixelCount)
		{
			OutError = TEXT("Native window readback returned no usable pixels.");
			return false;
		}

		POINT ClientOrigin{0, 0};
		const HWND WindowHandle = static_cast<HWND>(NativeWindow->GetOSWindowHandle());
		if (!::ClientToScreen(WindowHandle, &ClientOrigin))
		{
			OutError = TEXT("Could not resolve the native window client origin.");
			return false;
		}

		const FGeometry& Geometry = ViewportWidget->GetCachedGeometry();
		const FVector2f AbsolutePosition = FVector2f(Geometry.GetAbsolutePosition());
		const FVector2f AbsoluteSize = FVector2f(Geometry.GetAbsoluteSize());
		const int32 MinX = FMath::RoundToInt(AbsolutePosition.X) - ClientOrigin.x;
		const int32 MinY = FMath::RoundToInt(AbsolutePosition.Y) - ClientOrigin.y;
		const int32 CaptureWidth = FMath::RoundToInt(AbsoluteSize.X);
		const int32 CaptureHeight = FMath::RoundToInt(AbsoluteSize.Y);
		const int64 CapturePixelCount = static_cast<int64>(CaptureWidth) * static_cast<int64>(CaptureHeight);
		if (CaptureWidth <= 0 || CaptureHeight <= 0 || CapturePixelCount > MAX_int32
			|| MinX < 0 || MinY < 0
			|| static_cast<int64>(MinX) + CaptureWidth > WindowWidth
			|| static_cast<int64>(MinY) + CaptureHeight > WindowHeight)
		{
			OutError = FString::Printf(
				TEXT("Viewport rect (%d,%d %dx%d) is outside native client pixels (%ux%u)."),
				MinX, MinY, CaptureWidth, CaptureHeight, WindowWidth, WindowHeight);
			return false;
		}

		OutBitmap.SetNumUninitialized(static_cast<int32>(CapturePixelCount));
		for (int32 Row = 0; Row < CaptureHeight; ++Row)
		{
			const FColor* SourceRow = WindowPixels.GetData()
				+ (MinY + Row) * static_cast<int32>(WindowWidth) + MinX;
			FColor* DestRow = OutBitmap.GetData() + Row * CaptureWidth;
			FMemory::Memcpy(DestRow, SourceRow, CaptureWidth * sizeof(FColor));
		}

		OutSize = FIntPoint(CaptureWidth, CaptureHeight);
		OutError.Reset();
		return true;
#endif
#else
		OutError = TEXT("Native-window viewport readback is unavailable on this platform.");
		return false;
#endif
	}

	FTetherScreenshotResult EncodeScreenshot(
		TArray<FColor>& Bitmap,
		const FIntPoint Size,
		const FString& Source,
		const FString& OutFilePath,
		const bool bIncludeBase64)
	{
		FTetherScreenshotResult R;
		R.Source = Source;
		R.Width = Size.X;
		R.Height = Size.Y;

		if (Size.X <= 0 || Size.Y <= 0)
		{
			R.Error = FString::Printf(TEXT("Screenshot has invalid size (%dx%d)."), Size.X, Size.Y);
			return R;
		}
		if (OutFilePath.IsEmpty() && !bIncludeBase64)
		{
			R.Error = TEXT("Either OutFilePath must be non-empty or bIncludeBase64 must be true.");
			return R;
		}

		const int64 RequiredPixelCount = static_cast<int64>(Size.X) * static_cast<int64>(Size.Y);
		if (RequiredPixelCount > MAX_int32 || Bitmap.Num() < RequiredPixelCount)
		{
			R.Error = FString::Printf(
				TEXT("Screenshot returned %d pixels for a %dx%d image."),
				Bitmap.Num(), Size.X, Size.Y);
			return R;
		}
		if (Bitmap.Num() > RequiredPixelCount)
		{
			Bitmap.SetNum(static_cast<int32>(RequiredPixelCount), EAllowShrinking::No);
		}

		// Viewport and Slate readback may carry alpha=0. The output represents
		// an opaque screen image, so normalize alpha before PNG compression.
		for (FColor& Color : Bitmap)
		{
			Color.A = 255;
		}

		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> PNG = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (!PNG.IsValid()
			|| !PNG->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), Size.X, Size.Y, ERGBFormat::BGRA, 8))
		{
			R.Error = TEXT("Failed to initialize PNG image wrapper.");
			return R;
		}

		const TArray64<uint8>& Compressed = PNG->GetCompressed();
		if (Compressed.Num() == 0)
		{
			R.Error = TEXT("PNG encoding produced no bytes.");
			return R;
		}

		if (!OutFilePath.IsEmpty())
		{
			const FString AbsPath = FPaths::ConvertRelativePathToFull(OutFilePath);
			const FString DirOnly = FPaths::GetPath(AbsPath);
			if (!DirOnly.IsEmpty())
			{
				IFileManager::Get().MakeDirectory(*DirOnly, /*Tree=*/true);
			}
			if (!FFileHelper::SaveArrayToFile(Compressed, *AbsPath))
			{
				R.Error = FString::Printf(TEXT("Failed to write PNG to '%s'."), *AbsPath);
				return R;
			}
			R.FilePath = AbsPath;
		}

		if (bIncludeBase64)
		{
			// FBase64 operates on 32-bit-sized buffers. Normal viewport PNGs are
			// far smaller, but retain an explicit guard before narrowing.
			const int64 Num = Compressed.Num();
			if (Num > MAX_int32)
			{
				R.Error = TEXT("PNG too large to base64-encode.");
				return R;
			}
			R.Base64 = FBase64::Encode(Compressed.GetData(), static_cast<uint32>(Num));
		}

		R.bSuccess = true;
		return R;
	}
}

FTetherScreenshotResult UTetherEditorLibrary::CaptureActiveViewport(const FString& OutFilePath, bool bIncludeBase64)
{
	FTetherScreenshotResult R;

	FViewport* Viewport = TetherEditorImpl::GetCaptureViewport(R.Source);
	if (!Viewport)
	{
		R.Error = TEXT("No active viewport available.");
		return R;
	}
	if (OutFilePath.IsEmpty() && !bIncludeBase64)
	{
		R.Error = TEXT("Either OutFilePath must be non-empty or bIncludeBase64 must be true.");
		return R;
	}

	const FIntPoint Size = Viewport->GetSizeXY();
	if (Size.X <= 0 || Size.Y <= 0)
	{
		R.Error = FString::Printf(TEXT("Viewport has zero size (%dx%d)."), Size.X, Size.Y);
		return R;
	}

	// Force a redraw so ReadPixels sees a fresh frame even when the
	// viewport isn't in realtime mode.
	Viewport->Draw();

	TArray<FColor> Bitmap;
	FReadSurfaceDataFlags Flags;
	Flags.SetLinearToGamma(false);
	const FIntRect Rect(0, 0, Size.X, Size.Y);
	if (!Viewport->ReadPixels(Bitmap, Flags, Rect) || Bitmap.Num() == 0)
	{
		R.Error = TEXT("Viewport->ReadPixels returned no data.");
		return R;
	}
	return TetherEditorImpl::EncodeScreenshot(Bitmap, Size, R.Source, OutFilePath, bIncludeBase64);
}

FTetherScreenshotResult UTetherEditorLibrary::CaptureActiveViewportAsDisplayed(
	const FString& OutFilePath,
	bool bIncludeBase64)
{
	FTetherScreenshotResult R;
	if (OutFilePath.IsEmpty() && !bIncludeBase64)
	{
		R.Error = TEXT("Either OutFilePath must be non-empty or bIncludeBase64 must be true.");
		return R;
	}
	if (!FSlateApplication::IsInitialized())
	{
		R.Error = TEXT("Slate application is not initialized.");
		return R;
	}

	TSharedPtr<SViewport> ViewportWidget = TetherEditorImpl::GetCaptureViewportWidget(R.Source);
	if (!ViewportWidget.IsValid())
	{
		R.Error = TEXT("No active viewport Slate widget available.");
		return R;
	}

	TArray<FColor> Bitmap;
	FIntPoint PresentedSize = FIntPoint::ZeroValue;
	FString PresentedCaptureError;
	if (TetherEditorImpl::CapturePresentedViewportPixels(
		ViewportWidget.ToSharedRef(), Bitmap, PresentedSize, PresentedCaptureError))
	{
		return TetherEditorImpl::EncodeScreenshot(
			Bitmap, PresentedSize, R.Source, OutFilePath, bIncludeBase64);
	}

	// Cross-platform fallback. This repaints the Slate window, so a one-frame
	// debug primitive may already have expired; native readback above is the
	// strict WYSIWYG path on Windows.
	Bitmap.Reset();
	FIntVector SlateSize = FIntVector::ZeroValue;
	if (!FSlateApplication::Get().TakeScreenshot(ViewportWidget.ToSharedRef(), Bitmap, SlateSize)
		|| Bitmap.Num() == 0)
	{
		R.Error = FString::Printf(
			TEXT("Native window capture failed (%s); Slate fallback returned no data."),
			*PresentedCaptureError);
		return R;
	}

	return TetherEditorImpl::EncodeScreenshot(
		Bitmap,
		FIntPoint(SlateSize.X, SlateSize.Y),
		R.Source,
		OutFilePath,
		bIncludeBase64);
}

// ─── GBuffer channel capture ───────────────────────────────

namespace TetherEditorImpl
{
	struct FChannelSpec
	{
		ESceneCaptureSource Source;
		bool bFloatRT;
		bool bIsDepth;
		bool bDisablePostProcess;  // GBuffer channels look wrong after tonemap
	};

	bool ResolveChannel(const FString& Channel, FChannelSpec& Out)
	{
		if (Channel.Equals(TEXT("SceneColor"), ESearchCase::IgnoreCase))
		{
			Out = { ESceneCaptureSource::SCS_FinalColorLDR, false, false, false };
			return true;
		}
		if (Channel.Equals(TEXT("SceneColorHDR"), ESearchCase::IgnoreCase))
		{
			Out = { ESceneCaptureSource::SCS_SceneColorHDR, true, false, false };
			return true;
		}
		if (Channel.Equals(TEXT("Depth"), ESearchCase::IgnoreCase))
		{
			Out = { ESceneCaptureSource::SCS_SceneDepth, true, true, true };
			return true;
		}
		if (Channel.Equals(TEXT("DeviceDepth"), ESearchCase::IgnoreCase))
		{
			Out = { ESceneCaptureSource::SCS_DeviceDepth, true, true, true };
			return true;
		}
		if (Channel.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
		{
			Out = { ESceneCaptureSource::SCS_Normal, false, false, true };
			return true;
		}
		if (Channel.Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase))
		{
			Out = { ESceneCaptureSource::SCS_BaseColor, false, false, true };
			return true;
		}
		return false;
	}

	UWorld* GetCaptureWorld()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		if (GEditor->PlayWorld)
		{
			return GEditor->PlayWorld;
		}
		return GEditor->GetEditorWorldContext().World();
	}
}

namespace TetherEditorImpl
{
	// Core scene-capture render + encode path shared between the
	// viewport-pose and explicit-pose channel capture APIs. Caller is
	// responsible for resolving World, pose, dims, and channel spec;
	// this function does the spawn → CaptureScene → readback → encode
	// → write → cleanup cycle.
	FTetherChannelCaptureResult CaptureChannelInternal(
		UWorld* World,
		const FVector& CamLoc,
		const FRotator& CamRot,
		float FOV,
		int32 W, int32 H,
		const FChannelSpec& Spec,
		const FString& ChannelEcho,
		float MaxDepthClamp,
		const FString& OutFilePath,
		bool bIncludeBase64);
}

FTetherChannelCaptureResult UTetherEditorLibrary::CaptureViewportChannel(
	const FString& Channel,
	const FString& OutFilePath,
	int32 Width,
	int32 Height,
	float MaxDepthClamp,
	bool bIncludeBase64)
{
	FTetherChannelCaptureResult R;
	R.Channel = Channel;

	TetherEditorImpl::FChannelSpec Spec;
	if (!TetherEditorImpl::ResolveChannel(Channel, Spec))
	{
		R.Error = FString::Printf(
			TEXT("Unknown channel '%s'. Valid: SceneColor, SceneColorHDR, Depth, DeviceDepth, Normal, BaseColor."),
			*Channel);
		return R;
	}

	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC || !VC->Viewport)
	{
		R.Error = TEXT("No active level editor viewport. For PIE captures use CaptureChannelFromPose with an explicit pose.");
		return R;
	}

	const FVector CamLoc = VC->GetViewLocation();
	const FRotator CamRot = VC->GetViewRotation();
	const float FOV = VC->ViewFOV > 0.f ? VC->ViewFOV : 90.f;

	const FIntPoint VpSize = VC->Viewport->GetSizeXY();
	const int32 W = Width > 0 ? Width : VpSize.X;
	const int32 H = Height > 0 ? Height : VpSize.Y;

	UWorld* World = TetherEditorImpl::GetCaptureWorld();
	if (!World)
	{
		R.Error = TEXT("No capture world available.");
		return R;
	}
	return TetherEditorImpl::CaptureChannelInternal(
		World, CamLoc, CamRot, FOV, W, H, Spec, Channel, MaxDepthClamp, OutFilePath, bIncludeBase64);
}

FTetherChannelCaptureResult UTetherEditorLibrary::CaptureChannelFromPose(
	const FString& Channel,
	FVector Location,
	FRotator Rotation,
	float FOV,
	int32 Width,
	int32 Height,
	float MaxDepthClamp,
	const FString& OutFilePath,
	bool bIncludeBase64)
{
	FTetherChannelCaptureResult R;
	R.Channel = Channel;

	TetherEditorImpl::FChannelSpec Spec;
	if (!TetherEditorImpl::ResolveChannel(Channel, Spec))
	{
		R.Error = FString::Printf(
			TEXT("Unknown channel '%s'. Valid: SceneColor, SceneColorHDR, Depth, DeviceDepth, Normal, BaseColor."),
			*Channel);
		return R;
	}

	UWorld* World = TetherEditorImpl::GetCaptureWorld();
	if (!World)
	{
		R.Error = TEXT("No capture world available.");
		return R;
	}

	// Resolve W/H: caller's value > active editor viewport size > 1920x1080.
	int32 W = Width;
	int32 H = Height;
	if (W <= 0 || H <= 0)
	{
		FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
		if (VC && VC->Viewport)
		{
			const FIntPoint VpSize = VC->Viewport->GetSizeXY();
			if (W <= 0) { W = VpSize.X; }
			if (H <= 0) { H = VpSize.Y; }
		}
		if (W <= 0) { W = 1920; }
		if (H <= 0) { H = 1080; }
	}

	const float ResolvedFOV = FOV > 0.f ? FOV : 90.f;
	return TetherEditorImpl::CaptureChannelInternal(
		World, Location, Rotation, ResolvedFOV, W, H, Spec, Channel, MaxDepthClamp, OutFilePath, bIncludeBase64);
}

FTetherChannelCaptureResult TetherEditorImpl::CaptureChannelInternal(
	UWorld* World,
	const FVector& CamLoc,
	const FRotator& CamRot,
	float FOV,
	int32 W, int32 H,
	const FChannelSpec& Spec,
	const FString& ChannelEcho,
	float MaxDepthClamp,
	const FString& OutFilePath,
	bool bIncludeBase64)
{
	FTetherChannelCaptureResult R;
	R.Channel = ChannelEcho;

	if (!World)
	{
		R.Error = TEXT("No capture world.");
		return R;
	}
	if (W <= 0 || H <= 0)
	{
		R.Error = FString::Printf(TEXT("Invalid resolution %dx%d."), W, H);
		return R;
	}
	if (OutFilePath.IsEmpty() && !bIncludeBase64)
	{
		R.Error = TEXT("Either OutFilePath must be non-empty or bIncludeBase64 must be true.");
		return R;
	}

	// 1) Spawn transient SceneCapture2D at the requested pose.
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.Name = MakeUniqueObjectName(
		World, ASceneCapture2D::StaticClass(), TEXT("TetherChannelCaptureCam"));
	ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(CamLoc, CamRot, SpawnInfo);
	if (!CaptureActor || !CaptureActor->GetCaptureComponent2D())
	{
		R.Error = TEXT("Failed to spawn ASceneCapture2D.");
		return R;
	}

	// 2) Configure the render target.
	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(
		GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(),
			UTextureRenderTarget2D::StaticClass(), TEXT("TetherChannelCaptureRT")));
	RT->ClearColor = FLinearColor::Black;
	RT->bAutoGenerateMips = false;
	const EPixelFormat Fmt = Spec.bFloatRT ? PF_FloatRGBA : PF_B8G8R8A8;
	RT->InitCustomFormat(W, H, Fmt, /*bForceLinearGamma=*/ Spec.bFloatRT);
	RT->UpdateResourceImmediate(true);

	// 3) Configure the scene capture component.
	USceneCaptureComponent2D* SCC = CaptureActor->GetCaptureComponent2D();
	SCC->ProjectionType = ECameraProjectionMode::Perspective;
	SCC->FOVAngle = FOV;
	SCC->CaptureSource = Spec.Source;
	SCC->bCaptureEveryFrame = false;
	SCC->bCaptureOnMovement = false;
	SCC->TextureTarget = RT;
	if (Spec.bDisablePostProcess)
	{
		SCC->ShowFlags.SetPostProcessing(false);
		SCC->ShowFlags.SetTonemapper(false);
		SCC->ShowFlags.SetEyeAdaptation(false);
		SCC->ShowFlags.SetMotionBlur(false);
		SCC->ShowFlags.SetBloom(false);
		SCC->ShowFlags.SetAntiAliasing(false);
	}

	// 4) Render + flush so ReadPixels sees the result.
	SCC->CaptureScene();
	FlushRenderingCommands();

	FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();
	if (!Res)
	{
		CaptureActor->Destroy();
		R.Error = TEXT("GameThread_GetRenderTargetResource returned null.");
		return R;
	}

	// 5) Encode per channel family.
	TArray64<uint8> Compressed;
	IImageWrapperModule& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	if (Spec.bIsDepth)
	{
		// Read float pixels — depth value lives in the R channel.
		TArray<FLinearColor> LinearPixels;
		FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
		ReadFlags.SetLinearToGamma(false);
		if (!Res->ReadLinearColorPixels(LinearPixels, ReadFlags) || LinearPixels.Num() != W * H)
		{
			CaptureActor->Destroy();
			R.Error = TEXT("ReadLinearColorPixels (depth) failed or wrong size.");
			return R;
		}

		// Optional pre-clamp: sky pixels come back as fp16 ∞ (~65504),
		// and without clamping they eat ~99% of the 16-bit PNG range.
		// Clamping to a scene-sized ceiling (e.g. 10000 cm = 100 m) keeps
		// the useful foreground range resolvable.
		const bool bClamp = (MaxDepthClamp > 0.f);

		float DMin = FLT_MAX;
		float DMax = -FLT_MAX;
		for (const FLinearColor& C : LinearPixels)
		{
			float D = C.R;
			if (!FMath::IsFinite(D))
			{
				continue;
			}
			if (bClamp && D > MaxDepthClamp)
			{
				D = MaxDepthClamp;
			}
			DMin = FMath::Min(DMin, D);
			DMax = FMath::Max(DMax, D);
		}
		if (DMin == FLT_MAX) { DMin = 0.f; DMax = 0.f; }
		R.DepthMin = DMin;
		R.DepthMax = DMax;

		// Map [DMin, DMax] → [0, 65535]. Flat depth (all pixels equal)
		// degenerates to all-zero PNG, caller sees DepthMin == DepthMax.
		const float Range = FMath::Max(DMax - DMin, 1e-6f);
		TArray<uint16> DepthPixels;
		DepthPixels.SetNumUninitialized(W * H);
		for (int32 i = 0; i < W * H; ++i)
		{
			float D = LinearPixels[i].R;
			if (bClamp && D > MaxDepthClamp)
			{
				D = MaxDepthClamp;
			}
			const float NormD = FMath::Clamp((D - DMin) / Range, 0.f, 1.f);
			DepthPixels[i] = (uint16)FMath::RoundToInt(NormD * 65535.f);
		}
		// (No vertical flip needed — FTextureRenderTargetResource::ReadLinearColorPixels
		// already returns rows top-down, which is what PNG expects.
		// Verified 2026-04-20 by A/B comparison against FViewport::ReadPixels
		// at the same pose.)

		TSharedPtr<IImageWrapper> PNG = IWM.CreateImageWrapper(EImageFormat::PNG);
		if (!PNG.IsValid() ||
			!PNG->SetRaw(DepthPixels.GetData(), DepthPixels.Num() * sizeof(uint16),
				W, H, ERGBFormat::Gray, 16))
		{
			CaptureActor->Destroy();
			R.Error = TEXT("PNG wrapper setup failed (16-bit depth).");
			return R;
		}
		Compressed = PNG->GetCompressed();
		R.Format = TEXT("PNG16");
	}
	else if (Spec.bFloatRT)
	{
		// HDR color — quantize to 8-bit BGRA for now. A future pass could
		// emit EXR when OutFilePath ends with .exr; PNG keeps payload small.
		TArray<FLinearColor> LinearPixels;
		FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
		ReadFlags.SetLinearToGamma(false);
		if (!Res->ReadLinearColorPixels(LinearPixels, ReadFlags) || LinearPixels.Num() != W * H)
		{
			CaptureActor->Destroy();
			R.Error = TEXT("ReadLinearColorPixels (HDR) failed or wrong size.");
			return R;
		}
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(W * H);
		for (int32 i = 0; i < W * H; ++i)
		{
			Pixels[i] = LinearPixels[i].ToFColor(/*bSRGB=*/ true);
			Pixels[i].A = 255;
		}

		TSharedPtr<IImageWrapper> PNG = IWM.CreateImageWrapper(EImageFormat::PNG);
		if (!PNG.IsValid() ||
			!PNG->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor),
				W, H, ERGBFormat::BGRA, 8))
		{
			CaptureActor->Destroy();
			R.Error = TEXT("PNG wrapper setup failed (HDR).");
			return R;
		}
		Compressed = PNG->GetCompressed();
		R.Format = TEXT("PNG");
	}
	else
	{
		// 8-bit BGRA readback — SceneColor / Normal / BaseColor.
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
		ReadFlags.SetLinearToGamma(false);
		if (!Res->ReadPixels(Pixels, ReadFlags) || Pixels.Num() != W * H)
		{
			CaptureActor->Destroy();
			R.Error = TEXT("ReadPixels failed or wrong size.");
			return R;
		}
		for (FColor& C : Pixels)
		{
			C.A = 255;
		}

		TSharedPtr<IImageWrapper> PNG = IWM.CreateImageWrapper(EImageFormat::PNG);
		if (!PNG.IsValid() ||
			!PNG->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor),
				W, H, ERGBFormat::BGRA, 8))
		{
			CaptureActor->Destroy();
			R.Error = TEXT("PNG wrapper setup failed (8-bit).");
			return R;
		}
		Compressed = PNG->GetCompressed();
		R.Format = TEXT("PNG");
	}

	// 6) Write file + base64.
	R.Width = W;
	R.Height = H;

	if (!OutFilePath.IsEmpty())
	{
		FString Abs = FPaths::ConvertRelativePathToFull(OutFilePath);
		const FString Dir = FPaths::GetPath(Abs);
		if (!Dir.IsEmpty())
		{
			IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
		}
		if (!FFileHelper::SaveArrayToFile(Compressed, *Abs))
		{
			CaptureActor->Destroy();
			R.Error = FString::Printf(TEXT("Failed to write PNG to '%s'."), *Abs);
			return R;
		}
		R.FilePath = Abs;
	}

	if (bIncludeBase64)
	{
		if (Compressed.Num() > (int64)MAX_int32)
		{
			CaptureActor->Destroy();
			R.Error = TEXT("Compressed PNG too large to base64-encode.");
			return R;
		}
		R.Base64 = FBase64::Encode(Compressed.GetData(), (uint32)Compressed.Num());
	}

	// 7) Cleanup.
	CaptureActor->Destroy();

	R.bSuccess = true;
	return R;
}

// ─── HitProxy viewport → actor-ID ──────────────────────────

namespace TetherEditorImpl
{
	// Resolve HHitProxy → AActor* if it's an HActor-style proxy. Returns
	// nullptr for non-actor hit proxies (gizmos, widgets, etc.).
	AActor* ResolveHitProxyActor(HHitProxy* Proxy)
	{
		if (!Proxy)
		{
			return nullptr;
		}
		if (HActor* AP = HitProxyCast<HActor>(Proxy))
		{
			return AP->Actor;
		}
		return nullptr;
	}

	// Deterministic "index → distinct color" so the PNG is readable.
	// Low indices get high-contrast primary/secondary colors; higher
	// indices fall back to a hashed palette.
	FColor ColorForActorIndex(int32 Index)
	{
		if (Index <= 0)
		{
			return FColor(0, 0, 0, 255);  // 0 = background (no actor)
		}
		// Golden-angle hue stepping gives good visual separation.
		const float H = FMath::Fmod(Index * 137.508f, 360.0f);
		const FLinearColor HSV(H, 0.85f, 0.95f);
		return HSV.HSVToLinearRGB().ToFColor(/*bSRGB=*/ true);
	}
}

FTetherScreenshotResult UTetherEditorLibrary::CaptureViewportHitProxyMap(
	const FString& OutFilePath, bool bIncludeBase64)
{
	FTetherScreenshotResult R;
	R.Source = TEXT("HitProxy");

	if (GEditor && GEditor->PlayWorld)
	{
		R.Error = TEXT("HitProxy map is not available while PIE is active.");
		return R;
	}

	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC || !VC->Viewport)
	{
		R.Error = TEXT("No active level editor viewport.");
		return R;
	}

	// Force a draw so the hit proxy cache is fresh.
	VC->Viewport->Draw();

	const FIntPoint Size = VC->Viewport->GetSizeXY();
	if (Size.X <= 0 || Size.Y <= 0)
	{
		R.Error = FString::Printf(TEXT("Viewport has zero size (%dx%d)."), Size.X, Size.Y);
		return R;
	}

	if (OutFilePath.IsEmpty() && !bIncludeBase64)
	{
		R.Error = TEXT("Either OutFilePath must be non-empty or bIncludeBase64 must be true.");
		return R;
	}

	TArray<HHitProxy*> HitProxies;
	const FIntRect Rect(0, 0, Size.X, Size.Y);
	VC->Viewport->GetHitProxyMap(Rect, HitProxies);
	if (HitProxies.Num() != Size.X * Size.Y)
	{
		R.Error = FString::Printf(
			TEXT("GetHitProxyMap returned %d entries, expected %d."),
			HitProxies.Num(), Size.X * Size.Y);
		return R;
	}

	// Assign a stable small index per unique actor this frame.
	TMap<AActor*, int32> ActorToIndex;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(HitProxies.Num());
	for (int32 i = 0; i < HitProxies.Num(); ++i)
	{
		AActor* Actor = TetherEditorImpl::ResolveHitProxyActor(HitProxies[i]);
		int32 Idx = 0;
		if (Actor)
		{
			int32* Found = ActorToIndex.Find(Actor);
			if (Found)
			{
				Idx = *Found;
			}
			else
			{
				Idx = ActorToIndex.Num() + 1;  // reserve 0 for "no actor"
				ActorToIndex.Add(Actor, Idx);
			}
		}
		Pixels[i] = TetherEditorImpl::ColorForActorIndex(Idx);
	}

	IImageWrapperModule& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> PNG = IWM.CreateImageWrapper(EImageFormat::PNG);
	if (!PNG.IsValid() ||
		!PNG->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor),
			Size.X, Size.Y, ERGBFormat::BGRA, 8))
	{
		R.Error = TEXT("PNG wrapper setup failed (hit-proxy map).");
		return R;
	}
	const TArray64<uint8>& Compressed = PNG->GetCompressed();
	if (Compressed.Num() == 0)
	{
		R.Error = TEXT("PNG encoding produced no bytes.");
		return R;
	}

	R.Width = Size.X;
	R.Height = Size.Y;

	if (!OutFilePath.IsEmpty())
	{
		FString Abs = FPaths::ConvertRelativePathToFull(OutFilePath);
		const FString Dir = FPaths::GetPath(Abs);
		if (!Dir.IsEmpty())
		{
			IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
		}
		if (!FFileHelper::SaveArrayToFile(Compressed, *Abs))
		{
			R.Error = FString::Printf(TEXT("Failed to write PNG to '%s'."), *Abs);
			return R;
		}
		R.FilePath = Abs;
	}

	if (bIncludeBase64)
	{
		if (Compressed.Num() > (int64)MAX_int32)
		{
			R.Error = TEXT("Compressed PNG too large to base64-encode.");
			return R;
		}
		R.Base64 = FBase64::Encode(Compressed.GetData(), (uint32)Compressed.Num());
	}

	R.bSuccess = true;
	return R;
}

FString UTetherEditorLibrary::GetActorUnderViewportPixel(int32 X, int32 Y)
{
	if (GEditor && GEditor->PlayWorld)
	{
		return FString();
	}
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC || !VC->Viewport)
	{
		return FString();
	}
	const FIntPoint Size = VC->Viewport->GetSizeXY();
	if (X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y)
	{
		return FString();
	}

	// Ensure hit proxy cache is current.
	VC->Viewport->Draw();

	HHitProxy* Proxy = VC->Viewport->GetHitProxy(X, Y);
	AActor* Actor = TetherEditorImpl::ResolveHitProxyActor(Proxy);
	if (!Actor)
	{
		return FString();
	}
	return Actor->GetPathName();
}

// ─── Live Coding ───────────────────────────────────────────

#if PLATFORM_WINDOWS
namespace TetherEditorImpl
{
	const TCHAR* LiveCodingResultToString(ELiveCodingCompileResult R)
	{
		switch (R)
		{
			case ELiveCodingCompileResult::Success:            return TEXT("Success");
			case ELiveCodingCompileResult::NoChanges:          return TEXT("NoChanges");
			case ELiveCodingCompileResult::InProgress:         return TEXT("InProgress");
			case ELiveCodingCompileResult::CompileStillActive: return TEXT("CompileStillActive");
			case ELiveCodingCompileResult::NotStarted:         return TEXT("NotStarted");
			case ELiveCodingCompileResult::Failure:            return TEXT("Failure");
			case ELiveCodingCompileResult::Cancelled:          return TEXT("Cancelled");
		}
		return TEXT("Unknown");
	}

	ILiveCodingModule* GetLiveCoding()
	{
		if (!FModuleManager::Get().IsModuleLoaded(TEXT("LiveCoding")))
		{
			return nullptr;
		}
		return FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding"));
	}
}
#endif

bool UTetherEditorLibrary::IsLiveCodingEnabled()
{
#if PLATFORM_WINDOWS
	if (ILiveCodingModule* LC = TetherEditorImpl::GetLiveCoding())
	{
		return LC->IsEnabledForSession();
	}
#endif
	return false;
}

bool UTetherEditorLibrary::IsLiveCodingCompiling()
{
#if PLATFORM_WINDOWS
	if (ILiveCodingModule* LC = TetherEditorImpl::GetLiveCoding())
	{
		return LC->IsCompiling();
	}
#endif
	return false;
}

FTetherLiveCodingResult UTetherEditorLibrary::TriggerLiveCodingCompile(bool bWaitForCompletion)
{
	FTetherLiveCodingResult R;
#if PLATFORM_WINDOWS
	ILiveCodingModule* LC = TetherEditorImpl::GetLiveCoding();
	if (!LC)
	{
		R.Status = TEXT("NotStarted");
		R.Error = TEXT("LiveCoding module not loaded. Check Editor Preferences → Live Coding.");
		return R;
	}
	if (!LC->IsEnabledForSession())
	{
		if (!LC->CanEnableForSession())
		{
			R.Status = TEXT("NotStarted");
			R.Error = LC->GetEnableErrorText().ToString();
			if (R.Error.IsEmpty())
			{
				R.Error = TEXT("Live Coding cannot be enabled for this session.");
			}
			return R;
		}
		LC->EnableForSession(true);
	}

	ELiveCodingCompileResult Result = ELiveCodingCompileResult::NotStarted;
	const ELiveCodingCompileFlags Flags = bWaitForCompletion
		? ELiveCodingCompileFlags::WaitForCompletion
		: ELiveCodingCompileFlags::None;
	const bool bAccepted = LC->Compile(Flags, &Result);

	R.bTriggered = bAccepted;
	R.Status = TetherEditorImpl::LiveCodingResultToString(Result);
	R.bCompleted = (Result == ELiveCodingCompileResult::Success
		|| Result == ELiveCodingCompileResult::NoChanges);

	if (!bAccepted && Result == ELiveCodingCompileResult::CompileStillActive)
	{
		R.Error = TEXT("A Live Coding compile is already in progress.");
	}
	else if (Result == ELiveCodingCompileResult::Failure)
	{
		R.Error = TEXT("Live Coding compile failed. See Output Log for details.");
	}
	else if (Result == ELiveCodingCompileResult::Cancelled)
	{
		R.Error = TEXT("Live Coding compile was cancelled.");
	}
#else
	R.Status = TEXT("Unavailable");
	R.Error = TEXT("Live Coding is only available on Windows.");
#endif
	return R;
}

// ─── Viewport render / display ─────────────────────────────

namespace TetherEditorImpl
{
	EViewModeIndex ParseViewMode(const FString& Mode)
	{
		const FString M = Mode.TrimStartAndEnd();
		if (M.Equals(TEXT("Lit"), ESearchCase::IgnoreCase)) return VMI_Lit;
		if (M.Equals(TEXT("Unlit"), ESearchCase::IgnoreCase)) return VMI_Unlit;
		if (M.Equals(TEXT("Wireframe"), ESearchCase::IgnoreCase)) return VMI_BrushWireframe;
		if (M.Equals(TEXT("BrushWireframe"), ESearchCase::IgnoreCase)) return VMI_BrushWireframe;
		if (M.Equals(TEXT("CSGWireframe"), ESearchCase::IgnoreCase)) return VMI_Wireframe;
		if (M.Equals(TEXT("DetailLighting"), ESearchCase::IgnoreCase)) return VMI_Lit_DetailLighting;
		if (M.Equals(TEXT("LightingOnly"), ESearchCase::IgnoreCase)) return VMI_LightingOnly;
		if (M.Equals(TEXT("LightComplexity"), ESearchCase::IgnoreCase)) return VMI_LightComplexity;
		if (M.Equals(TEXT("ShaderComplexity"), ESearchCase::IgnoreCase)) return VMI_ShaderComplexity;
		if (M.Equals(TEXT("LightmapDensity"), ESearchCase::IgnoreCase)) return VMI_LightmapDensity;
		if (M.Equals(TEXT("ReflectionOverride"), ESearchCase::IgnoreCase)) return VMI_ReflectionOverride;
		if (M.Equals(TEXT("CollisionPawn"), ESearchCase::IgnoreCase)) return VMI_CollisionPawn;
		if (M.Equals(TEXT("CollisionVisibility"), ESearchCase::IgnoreCase)) return VMI_CollisionVisibility;
		if (M.Equals(TEXT("LODColoration"), ESearchCase::IgnoreCase)) return VMI_LODColoration;
		if (M.Equals(TEXT("QuadOverdraw"), ESearchCase::IgnoreCase)) return VMI_QuadOverdraw;
		return VMI_Unknown;
	}

	FString ViewModeToString(EViewModeIndex Mode)
	{
		switch (Mode)
		{
		case VMI_Lit: return TEXT("Lit");
		case VMI_Unlit: return TEXT("Unlit");
		case VMI_BrushWireframe: return TEXT("Wireframe");
		case VMI_Wireframe: return TEXT("CSGWireframe");
		case VMI_Lit_DetailLighting: return TEXT("DetailLighting");
		case VMI_LightingOnly: return TEXT("LightingOnly");
		case VMI_LightComplexity: return TEXT("LightComplexity");
		case VMI_ShaderComplexity: return TEXT("ShaderComplexity");
		case VMI_LightmapDensity: return TEXT("LightmapDensity");
		case VMI_ReflectionOverride: return TEXT("ReflectionOverride");
		case VMI_CollisionPawn: return TEXT("CollisionPawn");
		case VMI_CollisionVisibility: return TEXT("CollisionVisibility");
		case VMI_LODColoration: return TEXT("LODColoration");
		case VMI_QuadOverdraw: return TEXT("QuadOverdraw");
		default: return FString::Printf(TEXT("VMI_%d"), (int32)Mode);
		}
	}

	bool ParseViewportType(const FString& Type, ELevelViewportType& Out)
	{
		const FString T = Type.TrimStartAndEnd();
		if (T.Equals(TEXT("Perspective"), ESearchCase::IgnoreCase)) { Out = LVT_Perspective; return true; }
		if (T.Equals(TEXT("Top"), ESearchCase::IgnoreCase) || T.Equals(TEXT("OrthoXY"), ESearchCase::IgnoreCase)) { Out = LVT_OrthoXY; return true; }
		if (T.Equals(TEXT("Front"), ESearchCase::IgnoreCase) || T.Equals(TEXT("OrthoXZ"), ESearchCase::IgnoreCase)) { Out = LVT_OrthoXZ; return true; }
		if (T.Equals(TEXT("Side"), ESearchCase::IgnoreCase) || T.Equals(TEXT("OrthoYZ"), ESearchCase::IgnoreCase)) { Out = LVT_OrthoYZ; return true; }
		if (T.Equals(TEXT("OrthoFreelook"), ESearchCase::IgnoreCase)) { Out = LVT_OrthoFreelook; return true; }
		return false;
	}

	FString ViewportTypeToString(ELevelViewportType Type)
	{
		switch (Type)
		{
		case LVT_Perspective: return TEXT("Perspective");
		case LVT_OrthoXY: return TEXT("Top");
		case LVT_OrthoXZ: return TEXT("Front");
		case LVT_OrthoYZ: return TEXT("Side");
		case LVT_OrthoFreelook: return TEXT("OrthoFreelook");
		default: return TEXT("Unknown");
		}
	}
}

bool UTetherEditorLibrary::SetViewportRealtime(bool bRealtime)
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	VC->SetRealtime(bRealtime);
	VC->Invalidate();
	return true;
}

bool UTetherEditorLibrary::IsViewportRealtime()
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	return VC && VC->IsRealtime();
}

FVector2D UTetherEditorLibrary::GetViewportSize()
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC || !VC->Viewport)
	{
		return FVector2D::ZeroVector;
	}
	const FIntPoint S = VC->Viewport->GetSizeXY();
	return FVector2D(S.X, S.Y);
}

bool UTetherEditorLibrary::SetViewportViewMode(const FString& Mode)
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	const EViewModeIndex Parsed = TetherEditorImpl::ParseViewMode(Mode);
	if (Parsed == VMI_Unknown)
	{
		return false;
	}
	VC->SetViewMode(Parsed);
	VC->Invalidate();
	return true;
}

FString UTetherEditorLibrary::GetViewportViewMode()
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return FString();
	}
	return TetherEditorImpl::ViewModeToString(VC->GetViewMode());
}

bool UTetherEditorLibrary::SetViewportShowFlag(const FString& FlagName, bool bEnabled)
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	const int32 Index = FEngineShowFlags::FindIndexByName(*FlagName);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	VC->EngineShowFlags.SetSingleFlag((uint32)Index, bEnabled);
	VC->Invalidate();
	return true;
}

bool UTetherEditorLibrary::GetViewportShowFlag(const FString& FlagName)
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	const int32 Index = FEngineShowFlags::FindIndexByName(*FlagName);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	return VC->EngineShowFlags.GetSingleFlag((uint32)Index);
}

bool UTetherEditorLibrary::SetViewportType(const FString& ViewportType)
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return false;
	}
	ELevelViewportType Parsed;
	if (!TetherEditorImpl::ParseViewportType(ViewportType, Parsed))
	{
		return false;
	}
	VC->SetViewportType(Parsed);
	VC->Invalidate();
	return true;
}

FString UTetherEditorLibrary::GetViewportType()
{
	FLevelEditorViewportClient* VC = TetherEditorImpl::GetActiveViewportClient();
	if (!VC)
	{
		return FString();
	}
	return TetherEditorImpl::ViewportTypeToString(VC->GetViewportType());
}

// ─── Editor UX + plugin introspection ──────────────────────────────────

bool UTetherEditorLibrary::ShowEditorNotification(const FString& Message, float DurationSeconds, bool bSuccess)
{
	if (Message.IsEmpty())
	{
		return false;
	}
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = FMath::Clamp(DurationSeconds, 1.0f, 60.0f);
	Info.bUseSuccessFailIcons = true;
	Info.bFireAndForget = true;
	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		return true;
	}
	return false;
}

TArray<FString> UTetherEditorLibrary::GetEnabledPlugins()
{
	TArray<FString> Out;
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		Out.Add(Plugin->GetName());
	}
	Out.Sort();
	return Out;
}

bool UTetherEditorLibrary::IsPluginEnabled(const FString& PluginName)
{
	if (PluginName.IsEmpty())
	{
		return false;
	}
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		if (Plugin->GetName().Equals(PluginName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

FString UTetherEditorLibrary::GetEditorBuildConfig()
{
	switch (FApp::GetBuildConfiguration())
	{
	case EBuildConfiguration::Debug:       return TEXT("Debug");
	case EBuildConfiguration::DebugGame:   return TEXT("DebugGame");
	case EBuildConfiguration::Development: return TEXT("Development");
	case EBuildConfiguration::Shipping:    return TEXT("Shipping");
	case EBuildConfiguration::Test:        return TEXT("Test");
	default:                               return TEXT("Unknown");
	}
}

DEFINE_LOG_CATEGORY_STATIC(LogTetherPy, Log, All);

bool UTetherEditorLibrary::WriteLogMessage(const FString& Message, const FString& Severity)
{
	if (Message.IsEmpty())
	{
		return false;
	}
	const FString SevLower = Severity.ToLower();
	if (SevLower == TEXT("verbose"))
	{
		UE_LOG(LogTetherPy, Verbose, TEXT("%s"), *Message);
	}
	else if (SevLower == TEXT("warning"))
	{
		UE_LOG(LogTetherPy, Warning, TEXT("%s"), *Message);
	}
	else if (SevLower == TEXT("error"))
	{
		UE_LOG(LogTetherPy, Error, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogTetherPy, Log, TEXT("%s"), *Message);
	}
	return true;
}

FString UTetherEditorLibrary::GetLogFilePath()
{
	return FPaths::ConvertRelativePathToFull(FPlatformOutputDevices::GetAbsoluteLogFilename());
}

FString UTetherEditorLibrary::GetScreenshotDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir());
}

float UTetherEditorLibrary::GetFrameRate()
{
	const double Delta = FApp::GetDeltaTime();
	if (Delta < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	return static_cast<float>(1.0 / Delta);
}

float UTetherEditorLibrary::GetMemoryUsageMB()
{
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	return static_cast<float>(Stats.UsedPhysical) / (1024.0f * 1024.0f);
}

float UTetherEditorLibrary::GetEngineUptime()
{
	return static_cast<float>(FApp::GetCurrentTime() - GStartTime);
}

bool UTetherEditorLibrary::TriggerGarbageCollection(bool bFullPurge)
{
	CollectGarbage(RF_NoFlags, bFullPurge);
	return true;
}

// ─── Project metadata + paths ──────────────────────────────────────────

FString UTetherEditorLibrary::GetProjectVersion()
{
	FString Value;
	GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("ProjectVersion"),
		Value,
		GGameIni);
	return Value;
}

FString UTetherEditorLibrary::GetProjectCompanyName()
{
	FString Value;
	GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("CompanyName"),
		Value,
		GGameIni);
	return Value;
}

FString UTetherEditorLibrary::GetProjectID()
{
	return FApp::GetProjectName();
	// Note: UE 5.7 exposes ProjectID via FDesktopPlatformModule; for broad
	// compatibility we fall back to the project's short name which is a
	// stable-enough per-project key. Callers that need the .uproject GUID
	// specifically should read it from the .uproject JSON themselves.
}

FString UTetherEditorLibrary::GetAutoSaveDirectory()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Autosaves")));
}

// ─── Editor tab management + window title ──────────────────────────────

bool UTetherEditorLibrary::OpenEditorTab(const FString& TabName)
{
	if (TabName.IsEmpty() || !FGlobalTabmanager::Get()->HasTabSpawner(FName(*TabName)))
	{
		return false;
	}
	const TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(FName(*TabName));
	return Tab.IsValid();
}

bool UTetherEditorLibrary::CloseEditorTab(const FString& TabName)
{
	if (TabName.IsEmpty())
	{
		return false;
	}
	const TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(FName(*TabName));
	if (!Tab.IsValid())
	{
		return false;
	}
	Tab->RequestCloseTab();
	return true;
}

bool UTetherEditorLibrary::IsEditorTabOpen(const FString& TabName)
{
	if (TabName.IsEmpty())
	{
		return false;
	}
	return FGlobalTabmanager::Get()->FindExistingLiveTab(FName(*TabName)).IsValid();
}

FString UTetherEditorLibrary::GetOSVersion()
{
	FString Label, Sub;
	FPlatformMisc::GetOSVersions(Label, Sub);
	if (Sub.IsEmpty())
	{
		return Label;
	}
	return FString::Printf(TEXT("%s (%s)"), *Label, *Sub);
}

FString UTetherEditorLibrary::GetCPUBrand()
{
	return FPlatformMisc::GetCPUBrand();
}

int32 UTetherEditorLibrary::GetCPUCoreCount()
{
	return FPlatformMisc::NumberOfCoresIncludingHyperthreads();
}

float UTetherEditorLibrary::GetTotalPhysicalMemoryMB()
{
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	return static_cast<float>(Stats.TotalPhysical) / (1024.0f * 1024.0f);
}

// ─── Shader + asset compile state ──────────────────────────────────────

int32 UTetherEditorLibrary::GetShaderCompileJobCount()
{
	return GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
}

int32 UTetherEditorLibrary::GetAssetCompileJobCount()
{
	return static_cast<int32>(FAssetCompilingManager::Get().GetNumRemainingAssets());
}

bool UTetherEditorLibrary::IsCompiling()
{
	const bool bShaders = GShaderCompilingManager && GShaderCompilingManager->IsCompiling();
	const bool bAssets = FAssetCompilingManager::Get().GetNumRemainingAssets() > 0;
	return bShaders || bAssets;
}

bool UTetherEditorLibrary::FlushCompilation()
{
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
	return true;
}

// ─── Output log tail (ring buffer) ─────────────────────────────────────

namespace TetherLogImpl
{
	struct FTetherLogEntry
	{
		FString Line;
		uint8 Verbosity = ELogVerbosity::Log;
	};

	class FTetherLogDevice : public FOutputDevice
	{
	public:
		static constexpr int32 kCapacity = 500;

		FTetherLogDevice()
		{
			Entries.Reserve(kCapacity);
		}

		virtual void Serialize(const TCHAR* Msg, ELogVerbosity::Type V, const class FName& Category) override
		{
			if (!Msg) return;
			FScopeLock Lock(&Mutex);
			FTetherLogEntry E;
			E.Verbosity = static_cast<uint8>(V & ELogVerbosity::VerbosityMask);
			E.Line = FString::Printf(TEXT("[%s][%s] %s"),
				*Category.ToString(),
				ToString(static_cast<ELogVerbosity::Type>(E.Verbosity)),
				Msg);
			if (Entries.Num() < kCapacity)
			{
				Entries.Add(MoveTemp(E));
			}
			else
			{
				Entries[Head] = MoveTemp(E);
				Head = (Head + 1) % kCapacity;
			}
		}

		virtual bool CanBeUsedOnAnyThread() const override { return true; }
		virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

		void Snapshot(TArray<FTetherLogEntry>& Out) const
		{
			FScopeLock Lock(&Mutex);
			const int32 N = Entries.Num();
			Out.Reset(N);
			if (N < kCapacity)
			{
				Out.Append(Entries);
				return;
			}
			// Ring: oldest is at Head, walk forward capacity times.
			for (int32 i = 0; i < N; ++i)
			{
				Out.Add(Entries[(Head + i) % kCapacity]);
			}
		}

		int32 Clear()
		{
			FScopeLock Lock(&Mutex);
			const int32 N = Entries.Num();
			Entries.Reset();
			Head = 0;
			return N;
		}

		int32 Count() const
		{
			FScopeLock Lock(&Mutex);
			return Entries.Num();
		}

	private:
		static const TCHAR* ToString(ELogVerbosity::Type V)
		{
			switch (V)
			{
			case ELogVerbosity::Fatal:       return TEXT("Fatal");
			case ELogVerbosity::Error:       return TEXT("Error");
			case ELogVerbosity::Warning:     return TEXT("Warning");
			case ELogVerbosity::Display:     return TEXT("Display");
			case ELogVerbosity::Log:         return TEXT("Log");
			case ELogVerbosity::Verbose:     return TEXT("Verbose");
			case ELogVerbosity::VeryVerbose: return TEXT("VeryVerbose");
			default:                         return TEXT("Log");
			}
		}

		mutable FCriticalSection Mutex;
		TArray<FTetherLogEntry> Entries;
		int32 Head = 0;
	};

	static FTetherLogDevice* GLogDevice = nullptr;
	static FTetherLogDevice& EnsureDevice()
	{
		if (!GLogDevice)
		{
			GLogDevice = new FTetherLogDevice();
			if (GLog)
			{
				GLog->AddOutputDevice(GLogDevice);
			}
		}
		return *GLogDevice;
	}

	static uint8 ParseMinSeverity(const FString& S, bool& bOk)
	{
		bOk = true;
		const FString L = S.ToLower();
		if (L.IsEmpty())         return static_cast<uint8>(ELogVerbosity::VeryVerbose);
		if (L == TEXT("fatal"))  return static_cast<uint8>(ELogVerbosity::Fatal);
		if (L == TEXT("error"))  return static_cast<uint8>(ELogVerbosity::Error);
		if (L == TEXT("warning"))return static_cast<uint8>(ELogVerbosity::Warning);
		if (L == TEXT("display"))return static_cast<uint8>(ELogVerbosity::Display);
		if (L == TEXT("log"))    return static_cast<uint8>(ELogVerbosity::Log);
		if (L == TEXT("verbose"))return static_cast<uint8>(ELogVerbosity::Verbose);
		bOk = false;
		return static_cast<uint8>(ELogVerbosity::VeryVerbose);
	}
}

TArray<FString> UTetherEditorLibrary::GetRecentLogLines(int32 NumLines, const FString& MinSeverity)
{
	TArray<FString> Out;
	bool bOk = true;
	const uint8 MinV = TetherLogImpl::ParseMinSeverity(MinSeverity, bOk);
	if (!bOk) return Out;

	TetherLogImpl::FTetherLogDevice& Dev = TetherLogImpl::EnsureDevice();
	TArray<TetherLogImpl::FTetherLogEntry> Snap;
	Dev.Snapshot(Snap);

	// Filter by severity (numerically, smaller = higher severity in UE enum).
	TArray<FString> Filtered;
	Filtered.Reserve(Snap.Num());
	for (const TetherLogImpl::FTetherLogEntry& E : Snap)
	{
		if (E.Verbosity <= MinV)
		{
			Filtered.Add(E.Line);
		}
	}
	const int32 Total = Filtered.Num();
	const int32 Take = (NumLines <= 0) ? Total : FMath::Min(NumLines, Total);
	Out.Reserve(Take);
	for (int32 i = Total - Take; i < Total; ++i)
	{
		Out.Add(Filtered[i]);
	}
	return Out;
}

int32 UTetherEditorLibrary::GetLogBufferSize()
{
	return TetherLogImpl::EnsureDevice().Count();
}

int32 UTetherEditorLibrary::GetLogBufferCapacity()
{
	return TetherLogImpl::FTetherLogDevice::kCapacity;
}

int32 UTetherEditorLibrary::ClearLogBuffer()
{
	return TetherLogImpl::EnsureDevice().Clear();
}

// ─── Engine module introspection ───────────────────────────────────────

bool UTetherEditorLibrary::IsModuleLoaded(const FString& ModuleName)
{
	if (ModuleName.IsEmpty()) return false;
	return FModuleManager::Get().IsModuleLoaded(FName(*ModuleName));
}

TArray<FString> UTetherEditorLibrary::GetRegisteredModuleNames()
{
	TArray<FName> NameNames;
	FModuleManager::Get().FindModules(TEXT("*"), NameNames);
	TArray<FString> Out;
	Out.Reserve(NameNames.Num());
	for (const FName& N : NameNames)
	{
		Out.Add(N.ToString());
	}
	Out.Sort();
	return Out;
}

bool UTetherEditorLibrary::LoadModule(const FString& ModuleName)
{
	if (ModuleName.IsEmpty()) return false;
	const FName N(*ModuleName);
	if (!FModuleManager::Get().ModuleExists(*ModuleName))
	{
		return false;
	}
	FModuleManager::Get().LoadModule(N);
	return FModuleManager::Get().IsModuleLoaded(N);
}

FString UTetherEditorLibrary::GetModuleBinaryPath(const FString& ModuleName)
{
	if (ModuleName.IsEmpty()) return FString();
	const FName N(*ModuleName);
	if (!FModuleManager::Get().IsModuleLoaded(N))
	{
		return FString();
	}
	return FModuleManager::Get().GetModuleFilename(N);
}

// ─── Viewport gizmo mode + coord system ────────────────────────────────

FString UTetherEditorLibrary::GetWidgetMode()
{
	const UE::Widget::EWidgetMode Mode = GLevelEditorModeTools().GetWidgetMode();
	switch (Mode)
	{
	case UE::Widget::WM_Translate:         return TEXT("Translate");
	case UE::Widget::WM_Rotate:            return TEXT("Rotate");
	case UE::Widget::WM_Scale:             return TEXT("Scale");
	case UE::Widget::WM_TranslateRotateZ:  return TEXT("TranslateRotateZ");
	case UE::Widget::WM_2D:                return TEXT("2D");
	case UE::Widget::WM_None:              return TEXT("None");
	default:                               return TEXT("Unknown");
	}
}

bool UTetherEditorLibrary::SetWidgetMode(const FString& Mode)
{
	const FString L = Mode.ToLower();
	UE::Widget::EWidgetMode M;
	if      (L == TEXT("translate"))        M = UE::Widget::WM_Translate;
	else if (L == TEXT("rotate"))           M = UE::Widget::WM_Rotate;
	else if (L == TEXT("scale"))            M = UE::Widget::WM_Scale;
	else if (L == TEXT("translaterotatez")) M = UE::Widget::WM_TranslateRotateZ;
	else if (L == TEXT("2d"))               M = UE::Widget::WM_2D;
	else if (L == TEXT("none"))             M = UE::Widget::WM_None;
	else return false;

	GLevelEditorModeTools().SetWidgetMode(M);
	return true;
}

FString UTetherEditorLibrary::GetCoordSystem()
{
	const ECoordSystem CS = GLevelEditorModeTools().GetCoordSystem();
	return (CS == COORD_World) ? TEXT("World") : TEXT("Local");
}

bool UTetherEditorLibrary::SetCoordSystem(const FString& System)
{
	const FString L = System.ToLower();
	if (L == TEXT("world"))
	{
		GLevelEditorModeTools().SetCoordSystem(COORD_World);
		return true;
	}
	if (L == TEXT("local"))
	{
		GLevelEditorModeTools().SetCoordSystem(COORD_Local);
		return true;
	}
	return false;
}

// ─── Viewport grid / snap ──────────────────────────────────────────────

float UTetherEditorLibrary::GetLocationGridSize()
{
	const ULevelEditorViewportSettings* S = GetDefault<ULevelEditorViewportSettings>();
	if (!S) return 0.0f;
	const TArray<float>& Sizes = S->bUsePowerOf2SnapSize ? S->Pow2GridSizes : S->DecimalGridSizes;
	if (!Sizes.IsValidIndex(S->CurrentPosGridSize))
	{
		return 0.0f;
	}
	return Sizes[S->CurrentPosGridSize];
}

float UTetherEditorLibrary::GetRotationGridSize()
{
	const ULevelEditorViewportSettings* S = GetDefault<ULevelEditorViewportSettings>();
	if (!S)
	{
		return 0.0f;
	}
	const TArray<float>& Sizes = (S->CurrentRotGridMode == GridMode_DivisionsOf360)
		? S->DivisionsOf360RotGridSizes
		: S->CommonRotGridSizes;
	if (!Sizes.IsValidIndex(S->CurrentRotGridSize))
	{
		return 0.0f;
	}
	return Sizes[S->CurrentRotGridSize];
}

bool UTetherEditorLibrary::IsGridSnapEnabled()
{
	const ULevelEditorViewportSettings* S = GetDefault<ULevelEditorViewportSettings>();
	return S && S->GridEnabled;
}

bool UTetherEditorLibrary::SetGridSnapEnabled(bool bEnabled)
{
	ULevelEditorViewportSettings* S = GetMutableDefault<ULevelEditorViewportSettings>();
	if (!S) return false;
	S->GridEnabled = bEnabled;
	S->PostEditChange();
	S->SaveConfig();
	return true;
}

// ─── Autosave settings ─────────────────────────────────────────────────

bool UTetherEditorLibrary::IsAutoSaveEnabled()
{
	const UEditorLoadingSavingSettings* S = GetDefault<UEditorLoadingSavingSettings>();
	return S && S->bAutoSaveEnable;
}

bool UTetherEditorLibrary::SetAutoSaveEnabled(bool bEnabled)
{
	UEditorLoadingSavingSettings* S = GetMutableDefault<UEditorLoadingSavingSettings>();
	if (!S) return false;
	S->bAutoSaveEnable = bEnabled;
	S->PostEditChange();
	S->SaveConfig();
	return true;
}

int32 UTetherEditorLibrary::GetAutoSaveIntervalMinutes()
{
	const UEditorLoadingSavingSettings* S = GetDefault<UEditorLoadingSavingSettings>();
	return S ? S->AutoSaveTimeMinutes : -1;
}

bool UTetherEditorLibrary::SetAutoSaveIntervalMinutes(int32 Minutes)
{
	if (Minutes < 1 || Minutes > 120) return false;
	UEditorLoadingSavingSettings* S = GetMutableDefault<UEditorLoadingSavingSettings>();
	if (!S) return false;
	S->AutoSaveTimeMinutes = Minutes;
	S->PostEditChange();
	S->SaveConfig();
	return true;
}

// ─── Asset-on-disk metadata ────────────────────────────────────────────

namespace TetherAssetFileImpl
{
	/** Resolve a package path to an absolute filesystem path. Empty on failure. */
	static FString ResolveFilename(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty()) return FString();
		// Strip object suffix (`/Game/Foo/Bar.Bar` → `/Game/Foo/Bar`).
		FString PackagePath = AssetPath;
		int32 DotIdx;
		if (PackagePath.FindChar('.', DotIdx))
		{
			PackagePath.LeftInline(DotIdx);
		}
		FString Filename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, Filename))
		{
			return FString();
		}
		// Try .uasset first, then .umap.
		const FString WithUAsset = Filename + FPackageName::GetAssetPackageExtension();
		if (FPaths::FileExists(WithUAsset))
		{
			return FPaths::ConvertRelativePathToFull(WithUAsset);
		}
		const FString WithUMap = Filename + FPackageName::GetMapPackageExtension();
		if (FPaths::FileExists(WithUMap))
		{
			return FPaths::ConvertRelativePathToFull(WithUMap);
		}
		return FString();
	}
}

bool UTetherEditorLibrary::DoesAssetExistOnDisk(const FString& AssetPath)
{
	return !TetherAssetFileImpl::ResolveFilename(AssetPath).IsEmpty();
}

FString UTetherEditorLibrary::GetAssetDiskPath(const FString& AssetPath)
{
	return TetherAssetFileImpl::ResolveFilename(AssetPath);
}

int64 UTetherEditorLibrary::GetAssetFileSize(const FString& AssetPath)
{
	const FString Filename = TetherAssetFileImpl::ResolveFilename(AssetPath);
	if (Filename.IsEmpty()) return -1;
	return IFileManager::Get().FileSize(*Filename);
}

FString UTetherEditorLibrary::GetAssetLastModifiedTime(const FString& AssetPath)
{
	const FString Filename = TetherAssetFileImpl::ResolveFilename(AssetPath);
	if (Filename.IsEmpty()) return FString();
	const FDateTime Ts = IFileManager::Get().GetTimeStamp(*Filename);
	if (Ts == FDateTime::MinValue()) return FString();
	return Ts.ToIso8601();
}

// ─── Session identity + timestamp ──────────────────────────────────────

FString UTetherEditorLibrary::GetOSUserName()
{
	return FPlatformProcess::UserName();
}

FString UTetherEditorLibrary::GetMachineName()
{
	return FPlatformProcess::ComputerName();
}

FString UTetherEditorLibrary::GetNowUTC()
{
	return FDateTime::UtcNow().ToIso8601();
}

int32 UTetherEditorLibrary::GetEditorProcessID()
{
	return static_cast<int32>(FPlatformProcess::GetCurrentProcessId());
}

// ─── Source control basics ─────────────────────────────────────────────

bool UTetherEditorLibrary::IsSourceControlEnabled()
{
	return ISourceControlModule::Get().IsEnabled()
		&& ISourceControlModule::Get().GetProvider().IsAvailable();
}

FString UTetherEditorLibrary::GetSourceControlProviderName()
{
	ISourceControlModule& Mod = ISourceControlModule::Get();
	if (!Mod.IsEnabled()) return FString();
	return Mod.GetProvider().GetName().ToString();
}

FString UTetherEditorLibrary::GetAssetSourceControlState(const FString& AssetPath)
{
	const FString Filename = TetherAssetFileImpl::ResolveFilename(AssetPath);
	if (Filename.IsEmpty()) return FString();
	ISourceControlModule& Mod = ISourceControlModule::Get();
	if (!Mod.IsEnabled()) return FString();

	FSourceControlStatePtr State = Mod.GetProvider().GetState(Filename, EStateCacheUsage::Use);
	if (!State.IsValid()) return TEXT("Unknown");

	if (State->IsCheckedOut())        return TEXT("CheckedOut");
	if (State->IsCheckedOutOther())   return TEXT("CheckedOutOther");
	if (State->IsAdded())             return TEXT("Added");
	if (State->IsDeleted())           return TEXT("Deleted");
	if (State->IsIgnored())           return TEXT("Ignored");
	if (!State->IsSourceControlled()) return TEXT("NotControlled");
	if (State->CanCheckout())         return TEXT("NotCheckedOut");
	return TEXT("Unknown");
}

bool UTetherEditorLibrary::CheckOutAsset(const FString& AssetPath)
{
	const FString Filename = TetherAssetFileImpl::ResolveFilename(AssetPath);
	if (Filename.IsEmpty()) return false;
	ISourceControlModule& Mod = ISourceControlModule::Get();
	if (!Mod.IsEnabled()) return false;
	const ECommandResult::Type R = Mod.GetProvider().Execute(
		ISourceControlOperation::Create<FCheckOut>(), Filename);
	return R == ECommandResult::Succeeded;
}

// ─── Project / engine paths ────────────────────────────────────────────

FString UTetherEditorLibrary::GetEngineDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
}

FString UTetherEditorLibrary::GetProjectContentDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
}

FString UTetherEditorLibrary::GetProjectIntermediateDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir());
}

FString UTetherEditorLibrary::GetProjectPluginsDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
}

// ─── Editor world state ────────────────────────────────────────────────

FString UTetherEditorLibrary::GetEditorWorldName()
{
	if (!GEditor) return FString();
	UWorld* World = GEditor->GetEditorWorldContext(false).World();
	return World ? World->GetMapName() : FString();
}

bool UTetherEditorLibrary::IsEditorWorldDirty()
{
	if (!GEditor) return false;
	UWorld* World = GEditor->GetEditorWorldContext(false).World();
	if (!World) return false;
	UPackage* Pkg = World->GetOutermost();
	return Pkg && Pkg->IsDirty();
}

int32 UTetherEditorLibrary::GetLoadedLevelCount()
{
	if (!GEditor) return 0;
	UWorld* World = GEditor->GetEditorWorldContext(false).World();
	return World ? World->GetLevels().Num() : 0;
}

int32 UTetherEditorLibrary::GetCurrentWorldActorCount()
{
	if (!GEditor) return 0;
	UWorld* World = GEditor->GetEditorWorldContext(false).World();
	if (!World) return 0;
	int32 Count = 0;
	for (ULevel* L : World->GetLevels())
	{
		if (!L) continue;
		for (AActor* A : L->Actors)
		{
			if (A) ++Count;
		}
	}
	return Count;
}

// ─── Engine build info ─────────────────────────────────────────────────

FString UTetherEditorLibrary::GetEditorBuildDate()
{
	return FApp::GetBuildDate();
}

int32 UTetherEditorLibrary::GetEngineChangelist()
{
	return static_cast<int32>(FEngineVersion::Current().GetChangelist());
}

bool UTetherEditorLibrary::IsEngineInstalled()
{
	return FApp::IsEngineInstalled();
}

bool UTetherEditorLibrary::IsUnattendedMode()
{
	return FApp::IsUnattended();
}

FString UTetherEditorLibrary::GetMainWindowTitle()
{
	if (!FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		return FString();
	}
	IMainFrameModule& MF = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	const TSharedPtr<SWindow> Window = MF.GetParentWindow();
	if (!Window.IsValid())
	{
		return FString();
	}
	return Window->GetTitle().ToString();
}

bool UTetherEditorLibrary::BringEditorToFront()
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	TSharedPtr<SWindow> Active = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!Active.IsValid())
	{
		// Fall back to any visible top-level window.
		TArray<TSharedRef<SWindow>> Windows = FSlateApplication::Get().GetTopLevelWindows();
		for (const TSharedRef<SWindow>& W : Windows)
		{
			if (W->IsVisible())
			{
				Active = W;
				break;
			}
		}
	}
	if (!Active.IsValid())
	{
		return false;
	}
	Active->BringToFront(/*bForce=*/ true);
	Active->HACK_ForceToFront();
	return true;
}

// ─── Tether call log UFUNCTIONs ──────────────────────────────

namespace TetherCallLogImpl
{
	static FTetherCallLogEntry ToUStruct(const FTetherCallRecord& R)
	{
		FTetherCallLogEntry E;
		E.RequestId        = R.RequestId;
		E.Command          = R.Command;
		E.ScriptPreview    = R.ScriptPreview;
		E.UnixSecondsUtc   = R.UnixSeconds;
		E.TotalDurationMs  = static_cast<float>(R.TotalDurationMs);
		E.ExecDurationMs   = static_cast<float>(R.ExecDurationMs);
		E.bSuccess         = R.bSuccess;
		E.OutputBytes      = R.OutputBytes;
		E.ErrorBytes       = R.ErrorBytes;
		E.Endpoint         = R.Endpoint;
		E.ErrorPreview     = R.ErrorPreview;
		return E;
	}
}

TArray<FTetherCallLogEntry> UTetherEditorLibrary::GetTetherCallLog(int32 MaxEntries)
{
	const TArray<FTetherCallRecord> Snap = FTetherCallLog::Get().Snapshot(MaxEntries);
	TArray<FTetherCallLogEntry> Out;
	Out.Reserve(Snap.Num());
	for (const FTetherCallRecord& R : Snap)
	{
		Out.Add(TetherCallLogImpl::ToUStruct(R));
	}
	return Out;
}

FTetherCallStats UTetherEditorLibrary::GetTetherCallStats()
{
	FTetherCallStats S;
	const TArray<FTetherCallRecord> Snap = FTetherCallLog::Get().Snapshot(0);
	S.TotalCalls = Snap.Num();
	S.Capacity = FTetherCallLog::Get().GetCapacity();
	S.TotalDropped = FTetherCallLog::Get().GetTotalDropped();

	if (Snap.Num() == 0)
	{
		return S;
	}

	double Total = 0.0;
	double Mx = 0.0;
	double Mn = TNumericLimits<double>::Max();
	TArray<double> Durations;
	Durations.Reserve(Snap.Num());
	for (const FTetherCallRecord& R : Snap)
	{
		Total += R.TotalDurationMs;
		Mx = FMath::Max(Mx, R.TotalDurationMs);
		Mn = FMath::Min(Mn, R.TotalDurationMs);
		Durations.Add(R.TotalDurationMs);
		if (R.bSuccess) ++S.SuccessCount; else ++S.FailureCount;
	}
	S.AvgDurationMs = static_cast<float>(Total / Snap.Num());
	S.MaxDurationMs = static_cast<float>(Mx);
	S.MinDurationMs = static_cast<float>(Mn);

	Durations.Sort();
	const int32 P95Idx = FMath::Clamp(
		FMath::FloorToInt32(0.95f * (Durations.Num() - 1)),
		0, Durations.Num() - 1);
	S.P95DurationMs = static_cast<float>(Durations[P95Idx]);

	return S;
}

int32 UTetherEditorLibrary::ClearTetherCallLog()
{
	return FTetherCallLog::Get().Clear();
}

int32 UTetherEditorLibrary::GetTetherCallLogCapacity()
{
	return FTetherCallLog::Get().GetCapacity();
}

int32 UTetherEditorLibrary::SetTetherCallLogCapacity(int32 Capacity)
{
	const int32 Clamped = FMath::Clamp(Capacity, 10, 5000);
	return FTetherCallLog::Get().SetCapacity(Clamped);
}

// ─── Signature registry dump ──────────────────────────────────

namespace TetherSignatureImpl
{
	/** Pythonize by inserting `_` at every lower→upper boundary, then lowercasing.
	 *  This mirrors the observed behaviour of UE's internal PythonizeName for
	 *  the tether's naming style (CamelCase / UObject-style initialisms).
	 *  Not a perfect copy of the engine's CamelCaseBreakIterator (which fuses
	 *  runs of uppercase), but matches what agents actually see in `unreal.*`. */
	static FString ToPythonName(const FString& Name)
	{
		FString Out;
		Out.Reserve(Name.Len() + 8);
		for (int32 i = 0; i < Name.Len(); ++i)
		{
			const TCHAR C = Name[i];
			if (i > 0 && FChar::IsUpper(C))
			{
				Out.AppendChar(TEXT('_'));
			}
			Out.AppendChar(FChar::ToLower(C));
		}
		return Out;
	}

	static TSharedRef<FJsonObject> ParamToJson(FProperty* Prop, UFunction* Func)
	{
		TSharedRef<FJsonObject> PObj = MakeShared<FJsonObject>();
		PObj->SetStringField(TEXT("name"), Prop->GetName());
		PObj->SetStringField(TEXT("python_name"), ToPythonName(Prop->GetName()));

		FString TypeStr;
		if (Prop->IsA<FStrProperty>()) TypeStr = TEXT("FString");
		else if (Prop->IsA<FTextProperty>()) TypeStr = TEXT("FText");
		else if (Prop->IsA<FNameProperty>()) TypeStr = TEXT("FName");
		else TypeStr = Prop->GetCPPType();
		PObj->SetStringField(TEXT("type"), TypeStr);

		if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			PObj->SetBoolField(TEXT("is_return"), true);
		}
		else if (Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ConstParm))
		{
			PObj->SetBoolField(TEXT("is_out"), true);
		}

		// UHT stores default values as "CPP_Default_<ParamName>" metadata on the function.
		const FString DefKey = FString::Printf(TEXT("CPP_Default_%s"), *Prop->GetName());
		if (Func->HasMetaData(*DefKey))
		{
			PObj->SetStringField(TEXT("default"), Func->GetMetaData(*DefKey));
		}
		return PObj;
	}
}

FString UTetherEditorLibrary::DumpTetherSignatureRegistry()
{
	using namespace TetherSignatureImpl;

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("generated_utc"), FDateTime::UtcNow().ToIso8601());
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());

	TArray<TSharedPtr<FJsonValue>> LibrariesArr;

	// Collect eligible library classes: subclasses of UBlueprintFunctionLibrary
	// that live in the /Script/Tether package (i.e. our own module).
	TArray<UClass*> LibClasses;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Cls = *It;
		if (!Cls || !Cls->IsChildOf(UBlueprintFunctionLibrary::StaticClass())) continue;
		if (Cls == UBlueprintFunctionLibrary::StaticClass()) continue;
		if (Cls->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated)) continue;
		UPackage* Pkg = Cls->GetPackage();
		if (!Pkg) continue;
		if (Pkg->GetName() != TEXT("/Script/Tether")) continue;
		LibClasses.Add(Cls);
	}

	LibClasses.Sort([](const UClass& A, const UClass& B)
	{
		return A.GetName() < B.GetName();
	});

	for (UClass* Cls : LibClasses)
	{
		TSharedRef<FJsonObject> LibObj = MakeShared<FJsonObject>();
		// Cls->GetName() already drops the "U" prefix (UE convention) — no extra strip needed.
		LibObj->SetStringField(TEXT("class_name"), Cls->GetName());
		LibObj->SetStringField(TEXT("python_name"), ToPythonName(Cls->GetName()));

		TArray<UFunction*> Funcs;
		for (TFieldIterator<UFunction> F(Cls, EFieldIteratorFlags::ExcludeSuper); F; ++F)
		{
			UFunction* Func = *F;
			if (!Func->HasAnyFunctionFlags(FUNC_BlueprintCallable)) continue;
			Funcs.Add(Func);
		}
		Funcs.Sort([](const UFunction& A, const UFunction& B)
		{
			return A.GetName() < B.GetName();
		});

		TArray<TSharedPtr<FJsonValue>> FuncArr;
		for (UFunction* Func : Funcs)
		{
			TSharedRef<FJsonObject> FObj = MakeShared<FJsonObject>();
			FObj->SetStringField(TEXT("name"), Func->GetName());
			FObj->SetStringField(TEXT("python_name"), ToPythonName(Func->GetName()));
			FObj->SetStringField(TEXT("category"), Func->GetMetaData(TEXT("Category")));
			FObj->SetStringField(TEXT("tooltip"), Func->GetMetaData(TEXT("ToolTip")));

			TArray<TSharedPtr<FJsonValue>> Params;
			for (TFieldIterator<FProperty> P(Func); P && P->HasAnyPropertyFlags(CPF_Parm); ++P)
			{
				Params.Add(MakeShared<FJsonValueObject>(ParamToJson(*P, Func)));
			}
			FObj->SetArrayField(TEXT("params"), Params);
			FuncArr.Add(MakeShared<FJsonValueObject>(FObj));
		}
		LibObj->SetArrayField(TEXT("functions"), FuncArr);
		LibrariesArr.Add(MakeShared<FJsonValueObject>(LibObj));
	}

	Root->SetArrayField(TEXT("libraries"), LibrariesArr);

	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

#undef LOCTEXT_NAMESPACE
