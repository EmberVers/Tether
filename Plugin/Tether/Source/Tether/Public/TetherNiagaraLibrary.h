#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TetherNiagaraLibrary.generated.h"

/** Result shared by Niagara authoring operations. */
USTRUCT(BlueprintType)
struct FTetherNiagaraOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString AssetPath;

	/** Emitter handle, module node, renderer object, or preview handle. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	TArray<FString> Warnings;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraTemplateInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString AssetPath;

	/** System or Emitter. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString AssetType;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	TArray<FString> Tags;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraScriptInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Keywords;

	/** Module, DynamicInput, Function, or another reflected Niagara usage. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Usage;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 MajorVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 MinorVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bLibraryVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bDeprecated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bExperimental = false;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraSystemInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bReadyToRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bDirty = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bFixedBounds = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FBox FixedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	float WarmupTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	float WarmupTickDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 WarmupTickCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 EmitterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 EnabledEmitterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 ModuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 RendererCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 UserParameterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString EffectTypePath;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraEmitterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString SourceAssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bLocalSpace = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bDeterministic = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 RandomSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString SimTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString InterpolatedSpawnMode;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bFixedBounds = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FBox FixedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 ModuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 RendererCount = 0;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraModuleInfo
{
	GENERATED_BODY()

	/** Stable graph node GUID used by all module mutation calls. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString EmitterId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString EmitterName;

	/** EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate, or another Niagara usage. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Usage;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString UsageId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 Index = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString ScriptPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString VersionId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bAssignment = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bDeprecated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 InputCount = 0;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraModuleInputInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString ModuleId;

	/** Unaliased module input name, for example SpawnRate or Lifetime.Min. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Type;

	/** Default, Local, Linked, Dynamic, DataInterface, or Object. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Mode;

	/** Export-text value for Local/Default, or target path/name for another mode. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bStatic = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bHidden = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString VariableId;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraRendererInfo
{
	GENERATED_BODY()

	/** Renderer UObject name, used by renderer mutation calls. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString EmitterId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString MaterialPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	TArray<FString> MaterialPaths;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	TArray<FString> MeshPaths;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 BindingCount = 0;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraPropertyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bEditable = false;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraParameterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bUserParameter = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bDataInterface = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bObject = false;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraCompileMessage
{
	GENERATED_BODY()

	/** Log, Display, Warning, Error, or Validation. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Severity;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString ShortDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString EmitterId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString ScriptUsage;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString PinId;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraCompileResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bReadyToRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 ErrorCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 WarningCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	TArray<FTetherNiagaraCompileMessage> Messages;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Error;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraAuditResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bPassed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 ErrorCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 WarningCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 EmitterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 ModuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 RendererCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 GpuEmitterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 TranslucentRendererCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	TArray<FTetherNiagaraCompileMessage> Issues;
};

/** Generic name/value entry used by renderer and data-interface recipes. */
USTRUCT(BlueprintType)
struct FTetherNiagaraPropertyValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Name;

	/** Unreal export-text value. Object properties also accept a content path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Value;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraInputValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Name;

	/** Local, Linked, Dynamic, Object, or DataInterface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Mode = TEXT("Local");

	/** Optional type override. Normally inferred from the module input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Type;

	/** Export-text local value, linked parameter name, or object asset path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Value;

	/** Dynamic input script path or data-interface class path. Configure nested inputs with a later module-input call. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString SourcePath;

	/** Optional reflected properties applied to a newly created data-interface input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	TArray<FTetherNiagaraPropertyValue> Properties;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraModuleSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString ScriptPath;

	/** EmitterSpawn, EmitterUpdate, ParticleSpawn, or ParticleUpdate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Usage = TEXT("ParticleUpdate");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	int32 Index = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	TArray<FTetherNiagaraInputValue> Inputs;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraRendererSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Name;

	/** Sprite, Ribbon, Mesh, Light, Decal, or Component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Type = TEXT("Sprite");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString MaterialPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString MeshPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	TArray<FTetherNiagaraPropertyValue> Properties;

	/** Renderer binding property name -> Niagara variable name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	TArray<FTetherNiagaraPropertyValue> Bindings;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraEmitterSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Name = TEXT("Emitter");

	/** Optional emitter template/asset. Empty creates a standard initialized emitter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString TemplatePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	bool bLocalSpace = false;

	/** CPU or GPU. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString SimTarget = TEXT("CPU");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	bool bDeterministic = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	int32 RandomSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	bool bUseFixedBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FBox FixedBounds = FBox(FVector(-500.0), FVector(500.0));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	TArray<FTetherNiagaraModuleSpec> Modules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	TArray<FTetherNiagaraRendererSpec> Renderers;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraParameterSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString Type = TEXT("Float");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString DefaultValue;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraSystemRecipe
{
	GENERATED_BODY()

	/** Optional system template to duplicate before applying the recipe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString TemplatePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	float WarmupTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	float WarmupTickDelta = 1.0f / 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	bool bUseFixedBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FBox FixedBounds = FBox(FVector(-1000.0), FVector(1000.0));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	FString EffectTypePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	TArray<FTetherNiagaraParameterSpec> UserParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether|Niagara")
	TArray<FTetherNiagaraEmitterSpec> Emitters;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraEmitterRuntimeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString ExecutionState;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 ParticleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int64 BytesUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	float CpuTimeMs = 0.0f;
};

USTRUCT(BlueprintType)
struct FTetherNiagaraPreviewInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FString SystemPath;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	FTransform Transform;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	bool bComplete = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	float DesiredAge = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int32 TotalParticleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	int64 TotalBytesUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Niagara")
	TArray<FTetherNiagaraEmitterRuntimeInfo> Emitters;
};

/**
 * Niagara/VFX authoring, diagnostics, preset delivery, and transient preview.
 * The functional implementation targets UE 5.7+; older supported engines
 * expose the same calls as logged safe stubs so the plugin still builds.
 */
UCLASS()
class TETHER_API UTetherNiagaraLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara")
	static bool IsNiagaraApiAvailable();

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara")
	static FString GetLastNiagaraError();

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Discovery")
	static TArray<FTetherNiagaraTemplateInfo> ListNiagaraTemplates(const FString& AssetType = TEXT("All"), const FString& Query = TEXT(""), int32 MaxResults = 200);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Discovery")
	static TArray<FTetherNiagaraScriptInfo> ListNiagaraScripts(const FString& Usage = TEXT("Module"), const FString& Query = TEXT(""), int32 MaxResults = 500);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Discovery")
	static FTetherNiagaraScriptInfo GetNiagaraScriptInfo(const FString& ScriptPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Assets")
	static FTetherNiagaraOperationResult CreateNiagaraSystem(const FString& AssetPath, const FString& TemplateSystemPath = TEXT(""), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Assets")
	static FTetherNiagaraOperationResult CreateNiagaraEmitter(const FString& AssetPath, const FString& TemplateEmitterPath = TEXT(""), bool bAddDefaultModulesAndRenderer = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Assets")
	static FTetherNiagaraOperationResult CreateNiagaraSystemFromRecipe(const FString& AssetPath, const FTetherNiagaraSystemRecipe& Recipe, bool bCompile = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Assets")
	static FTetherNiagaraOperationResult DeleteNiagaraAsset(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Assets")
	static FTetherNiagaraSystemInfo GetNiagaraSystemInfo(const FString& SystemPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Emitters")
	static TArray<FTetherNiagaraEmitterInfo> ListNiagaraEmitters(const FString& SystemPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Emitters")
	static FTetherNiagaraOperationResult AddNiagaraEmitter(const FString& SystemPath, const FString& Name, const FString& EmitterAssetOrTemplatePath = TEXT(""), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Emitters")
	static FTetherNiagaraOperationResult DuplicateNiagaraEmitter(const FString& SystemPath, const FString& EmitterIdOrName, const FString& NewName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Emitters")
	static bool RemoveNiagaraEmitter(const FString& SystemPath, const FString& EmitterIdOrName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Emitters")
	static bool RenameNiagaraEmitter(const FString& SystemPath, const FString& EmitterIdOrName, const FString& NewName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Emitters")
	static bool SetNiagaraEmitterEnabled(const FString& SystemPath, const FString& EmitterIdOrName, bool bEnabled, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Emitters")
	static bool SetNiagaraEmitterProperties(const FString& SystemPath, const FString& EmitterIdOrName, bool bLocalSpace, const FString& SimTarget, bool bDeterministic, int32 RandomSeed, bool bUseFixedBounds, const FBox& FixedBounds, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static TArray<FTetherNiagaraModuleInfo> ListNiagaraModules(const FString& SystemPath, const FString& EmitterIdOrName = TEXT(""), const FString& Usage = TEXT("All"));

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static TArray<FTetherNiagaraModuleInputInfo> ListNiagaraModuleInputs(const FString& SystemPath, const FString& ModuleId, bool bIncludeHidden = false);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static FTetherNiagaraOperationResult AddNiagaraModule(const FString& SystemPath, const FString& EmitterIdOrName, const FString& Usage, const FString& ScriptPath, const FString& SuggestedName = TEXT(""), int32 Index = -1, bool bEnabled = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static bool RemoveNiagaraModule(const FString& SystemPath, const FString& ModuleId, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static bool SetNiagaraModuleEnabled(const FString& SystemPath, const FString& ModuleId, bool bEnabled, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static bool SetNiagaraModuleInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& Value, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static bool LinkNiagaraModuleInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& LinkedParameter, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static FTetherNiagaraOperationResult SetNiagaraModuleDynamicInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& DynamicInputScriptPath, const FString& SuggestedName = TEXT(""), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static bool SetNiagaraModuleObjectInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& ObjectPath, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static FTetherNiagaraOperationResult SetNiagaraModuleDataInterfaceInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& DataInterfaceClassPath, const TArray<FTetherNiagaraPropertyValue>& Properties, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static TArray<FTetherNiagaraPropertyInfo> ListNiagaraModuleInputObjectProperties(const FString& SystemPath, const FString& ModuleId, const FString& InputName, bool bIncludeAdvanced = false);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static bool SetNiagaraModuleInputObjectProperty(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& PropertyName, const FString& Value, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static bool ResetNiagaraModuleInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Modules")
	static FTetherNiagaraOperationResult AddNiagaraParameterAssignment(const FString& SystemPath, const FString& EmitterIdOrName, const FString& Usage, const FString& ParameterName, const FString& Type, const FString& Value, int32 Index = -1, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Parameters")
	static TArray<FTetherNiagaraParameterInfo> ListNiagaraUserParameters(const FString& SystemPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Parameters")
	static bool AddNiagaraUserParameter(const FString& SystemPath, const FString& Name, const FString& Type, const FString& DefaultValue, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Parameters")
	static bool SetNiagaraUserParameterDefault(const FString& SystemPath, const FString& Name, const FString& Value, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Parameters")
	static bool RenameNiagaraUserParameter(const FString& SystemPath, const FString& OldName, const FString& NewName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Parameters")
	static bool RemoveNiagaraUserParameter(const FString& SystemPath, const FString& Name, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Renderers")
	static TArray<FTetherNiagaraRendererInfo> ListNiagaraRenderers(const FString& SystemPath, const FString& EmitterIdOrName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Renderers")
	static FTetherNiagaraOperationResult AddNiagaraRenderer(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererType, const FString& Name = TEXT(""), const FString& MaterialPath = TEXT(""), const FString& MeshPath = TEXT(""), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Renderers")
	static bool RemoveNiagaraRenderer(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Renderers")
	static bool SetNiagaraRendererEnabled(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, bool bEnabled, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Renderers")
	static TArray<FTetherNiagaraPropertyInfo> ListNiagaraRendererProperties(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, bool bIncludeAdvanced = false);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Renderers")
	static FString GetNiagaraRendererProperty(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, const FString& PropertyName);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Renderers")
	static bool SetNiagaraRendererProperty(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, const FString& PropertyName, const FString& Value, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Renderers")
	static bool SetNiagaraRendererMaterial(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, const FString& MaterialPath, int32 MaterialIndex = 0, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Renderers")
	static bool SetNiagaraRendererBinding(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, const FString& BindingProperty, const FString& VariableName, const FString& SourceMode = TEXT("Particles"), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Settings")
	static bool SetNiagaraSystemWarmup(const FString& SystemPath, float WarmupTime, float TickDelta = 0.033333333f, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Settings")
	static bool SetNiagaraSystemFixedBounds(const FString& SystemPath, bool bEnabled, const FBox& Bounds, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Settings")
	static bool SetNiagaraSystemEffectType(const FString& SystemPath, const FString& EffectTypePath, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Compile")
	static FTetherNiagaraCompileResult CompileNiagaraSystem(const FString& SystemPath, bool bForce = true, bool bWaitForGpuShaders = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Compile")
	static FTetherNiagaraCompileResult GetNiagaraCompileDiagnostics(const FString& SystemPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Compile")
	static FTetherNiagaraAuditResult ValidateNiagaraSystem(const FString& SystemPath, bool bCheckMaterials = true, bool bCheckBounds = true, int32 MaxEmitters = 16, int32 MaxRenderersPerEmitter = 8, int32 MaxModulesPerEmitter = 64);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Presets")
	static FTetherNiagaraOperationResult CreateWeaponTrailEffect(const FString& AssetPath, const FString& Style = TEXT("Ribbon"), const FString& MaterialPath = TEXT(""), const FLinearColor& Color = FLinearColor(1.0f, 0.35f, 0.05f, 1.0f), float Width = 12.0f, float Lifetime = 0.35f, float SpawnRate = 90.0f, bool bLocalSpace = false, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Presets")
	static FTetherNiagaraOperationResult CreateSparkEffect(const FString& AssetPath, const FString& Style = TEXT("Directional"), const FString& MaterialPath = TEXT(""), const FLinearColor& Color = FLinearColor(1.0f, 0.45f, 0.05f, 1.0f), int32 Count = 48, float Speed = 900.0f, float Lifetime = 0.6f, float Gravity = -980.0f, bool bCollision = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Presets")
	static FTetherNiagaraOperationResult CreateExplosionEffect(const FString& AssetPath, const FString& Style = TEXT("Layered"), const FString& MaterialPath = TEXT(""), const FLinearColor& CoreColor = FLinearColor(1.0f, 0.12f, 0.01f, 1.0f), float Scale = 1.0f, float Duration = 1.5f, int32 DebrisCount = 64, bool bShockwave = true, bool bLight = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Presets")
	static FTetherNiagaraOperationResult CreateDissolveEffect(const FString& AssetPath, const FString& Style = TEXT("Ash"), const FString& MaterialPath = TEXT(""), const FLinearColor& Color = FLinearColor(0.08f, 0.8f, 1.0f, 1.0f), int32 Count = 128, float Duration = 2.0f, float Radius = 100.0f, const FVector& Direction = FVector(0.0f, 0.0f, 1.0f), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Preview")
	static FTetherNiagaraOperationResult SpawnNiagaraPreview(const FString& SystemPath, const FTransform& Transform, bool bAutoActivate = true, bool bResetOnChange = true);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Preview")
	static TArray<FTetherNiagaraPreviewInfo> ListNiagaraPreviews();

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Preview")
	static FTetherNiagaraPreviewInfo GetNiagaraPreviewInfo(const FString& Handle);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Preview")
	static bool AdvanceNiagaraPreview(const FString& Handle, float Seconds, float TickDelta = 0.016666667f);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Preview")
	static bool SetNiagaraPreviewTransform(const FString& Handle, const FTransform& Transform, bool bTeleport = false);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Preview")
	static bool SetNiagaraPreviewVariable(const FString& Handle, const FString& Name, const FString& Type, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Preview")
	static bool ControlNiagaraPreview(const FString& Handle, const FString& Action);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Preview")
	static bool RemoveNiagaraPreview(const FString& Handle);

	UFUNCTION(BlueprintCallable, Category = "Tether|Niagara|Preview")
	static int32 RemoveAllNiagaraPreviews();
};
