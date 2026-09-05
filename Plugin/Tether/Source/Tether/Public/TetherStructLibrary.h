// Copyright Tether. UserDefinedStruct (UDS) asset CRUD.
//
// Author-time editing of UserDefinedStruct assets — create the asset,
// add / remove / rename / retype / reorder fields, write defaults, set
// tooltips and per-field "editable on instance" flag. Read-side returns
// type strings compatible with TetherBlueprintLibrary's
// add_blueprint_variable, so a UDS created here can immediately be used
// as a DataTable rowtype or a Blueprint variable type.
//
// All write ops are PIE-guarded (UE leaves UserDefinedStruct in a
// half-baked state when fields are edited during PIE) and wrapped in
// FStructureEditorUtils calls — the same code path the native struct
// editor UI uses, so undo/recompile/propagation is consistent.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TetherStructLibrary.generated.h"

/** One field of a UUserDefinedStruct, in a Python-friendly shape. */
USTRUCT(BlueprintType)
struct FTetherStructVariableInfo
{
	GENERATED_BODY()

	/** Friendly name (UI display) — what RenameStructVariable accepts. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString Name;

	/** Canonical type string — round-trips through ParseTypeString. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString TypeString;

	/** Serialized default value (FProperty ExportText form). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString DefaultValue;

	/** Internal stable GUID — useful for precise addressing across renames. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString Guid;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString Tooltip;

	/** False if the field is marked 'Edit Anywhere = off' (bDontEditOnInstance). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	bool bEditOnInstance = true;
};

/** Top-level metadata about a UUserDefinedStruct asset. */
USTRUCT(BlueprintType)
struct FTetherStructInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString Path;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString Tooltip;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	int32 VariableCount = 0;

	/** "UpToDate" / "Dirty" / "Error" / "Unknown" — mirrors
	 *  FStructureEditorUtils::EStructureStatus. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString Status;
};

/** Result of CreateUserDefinedStruct — mirrors FTetherCreateAssetResult shape. */
USTRUCT(BlueprintType)
struct FTetherStructCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString Path;

	UPROPERTY(BlueprintReadOnly, Category = "Tether|Struct")
	FString Error;
};


UCLASS()
class TETHER_API UTetherStructLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ── Asset lifecycle ─────────────────────────────────────────

	/** Create a new UUserDefinedStruct asset at AssetPath (e.g. "/Game/Data/UDS_Foo").
	 *  UE forces the new asset to contain at least one placeholder field; call
	 *  RemoveStructVariable / RenameStructVariable as needed. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static FTetherStructCreateResult CreateUserDefinedStruct(const FString& AssetPath);

	// ── Field CRUD ──────────────────────────────────────────────

	/** Add a new field. type_string is parsed by TetherTypeParseImpl — supports
	 *  Bool / Byte / Int / Int64 / Float / Double / String / Name / Text /
	 *  Vector / Rotator / Transform / LinearColor / GameplayTag / any
	 *  UScriptStruct / any UClass, plus "Array of <type>" prefix. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static bool AddStructVariable(const FString& StructPath, const FString& Name,
		const FString& TypeString, const FString& DefaultValue);

	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static bool RemoveStructVariable(const FString& StructPath, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static bool RenameStructVariable(const FString& StructPath,
		const FString& OldName, const FString& NewName);

	/** Change a field's type. UE clears the field's default value as part of
	 *  the change; call SetStructVariableDefault afterward if needed. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static bool ChangeStructVariableType(const FString& StructPath,
		const FString& Name, const FString& NewTypeString);

	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static bool SetStructVariableDefault(const FString& StructPath,
		const FString& Name, const FString& DefaultValue);

	/** Move the named field to NewIndex (0 = top). Internally steps the
	 *  underlying FStructureEditorUtils Up/Down primitive until the field
	 *  reaches NewIndex. */
	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static bool MoveStructVariable(const FString& StructPath,
		const FString& Name, int32 NewIndex);

	// ── Reads ──────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static TArray<FTetherStructVariableInfo> GetStructVariables(const FString& StructPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static FTetherStructInfo GetStructInfo(const FString& StructPath);

	// ── Metadata ───────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static bool SetStructVariableTooltip(const FString& StructPath,
		const FString& Name, const FString& Tooltip);

	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static bool SetStructVariableEditOnInstance(const FString& StructPath,
		const FString& Name, bool bEditOnInstance);

	UFUNCTION(BlueprintCallable, Category = "Tether|Struct")
	static bool SetStructTooltip(const FString& StructPath, const FString& Tooltip);
};
