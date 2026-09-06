#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TetherPropertyLibrary.generated.h"

/**
 * Reflection metadata for one UPROPERTY. Returned by ListUProperties so an
 * agent can see — without writing-then-failing — which fields are
 * Edit / Visible / Blueprint / protected / private / EditDefaultsOnly etc.
 *
 * The tether's `TetherLevelLibrary.list_class_properties` returns a
 * lighter `TetherPropertyInfo` (just `editable / read_only / transient`
 * booleans). This struct adds C++ access specifier + full `EPropertyFlags`
 * decoded to strings + raw metadata key/value map, which is what you need
 * to decide between `Set...` paths.
 */
USTRUCT(BlueprintType)
struct FTetherUPropertyInfo
{
	GENERATED_BODY()

	/** PascalCase C++ name, e.g. "Modifiers". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Property")
	FString Name;

	/** UE C++ type string from `FProperty::GetCPPType()`,
	 *  e.g. "FString" / "TArray<FGameplayModifierInfo>" / "FGameplayTagContainer". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Property")
	FString TypeName;

	/** Path of the class that DECLARED this property (for inherited props,
	 *  this is the parent class — useful for "is this a project-defined
	 *  field or inherited from engine"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Property")
	FString DeclaringClassPath;

	/** "Public" / "Protected" / "Private" / "Unknown".
	 *
	 *  C++ access modifier from `EPropertyFlags::CPF_NativeAccessSpecifier*`.
	 *  Note: UE Python's `get_editor_property` rejects BOTH protected and
	 *  private with the same "is protected and cannot be read" error string,
	 *  so this field is the only way to distinguish them at runtime. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Property")
	FString CppAccess;

	/** EPropertyFlags decoded — common ones in human-readable form, e.g.
	 *  ["EditAnywhere", "BlueprintReadWrite", "Transient"] or
	 *  ["EditDefaultsOnly", "Category=Modifiers"].
	 *
	 *  Use this to decide:
	 *  - EditDefaultsOnly → write only on CDO via `set_uproperty`, not on instances
	 *  - EditInstanceOnly → opposite: only writable on world/PIE actor instances
	 *  - VisibleAnywhere / VisibleDefaultsOnly → read-only
	 *  - Transient → not saved with asset
	 *  - Config → loaded/saved via .ini
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Property")
	TArray<FString> Flags;

	/** Raw metadata map, e.g. {"Category": "Modifiers", "ToolTip": "..."}.
	 *  Useful for `EditCondition`, `ClampMin/Max`, custom keys. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Property")
	TMap<FString, FString> Metadata;

	/** True for TArray / TMap / TSet — these support array_append /
	 *  array_remove / array_clear; leaf primitives + struct values use
	 *  set_uproperty. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Property")
	bool bIsContainer = false;

	/** True when the property is a USTRUCT value (not a pointer to UObject)
	 *  — caller may want to recurse via dotted path
	 *  (e.g. "Modifiers[0].ModifierMagnitude.ScalableFloatMagnitude.Value"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|Property")
	bool bIsStructValue = false;
};

/**
 * Generic UPROPERTY read / write surface. Operates one level below UE
 * Python's `get_editor_property` / `set_editor_property` — directly on
 * FProperty pointers in C++ — so it bypasses Python's access checks
 * (protected / private / EditDefaultsOnly-on-struct-copy) that block
 * many otherwise-legitimate edits.
 *
 * **Privileged**: these APIs intentionally bypass UE's editor safety
 * net. C++ access modifiers exist for invariant protection; abusing
 * write_uproperty on a `private:` field can corrupt class state.
 *
 * **Native-CDO gate**: when a write target resolves to the class-default
 * object of a native /Script/ class, the write entry points refuse it
 * unless `bAllowNativeCdo=true` is passed — such writes are memory-only
 * (never saved, lost on restart) yet affect every live instance of the
 * class in the process. Blueprint CDOs are asset-backed and exempt.
 *
 * Uses export-text strings as the universal value format (UE's native
 * serialisation) — works for any property type, including structs and
 * containers, without needing per-type Python wrappers.
 */
UCLASS()
class TETHER_API UTetherPropertyLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ── Discovery ──────────────────────────────────────────────────

	/**
	 * Full reflection dump of every UPROPERTY on a class or object. Includes
	 * private / protected / bare-UPROPERTY fields that `dir()` and
	 * `get_editor_property` don't expose to Python.
	 *
	 * @param ObjectOrClassPath  One of:
	 *                            - Asset content path (`/Game/Foo/Bar`) — auto-resolves to CDO
	 *                            - Full object path (`/Game/Foo/Bar.Default__Bar_C`)
	 *                            - UClass path (`/Script/Engine.Actor`)
	 * @param bIncludeInherited  When true, walk the class hierarchy.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Property")
	static TArray<FTetherUPropertyInfo> ListUProperties(
		const FString& ObjectOrClassPath,
		bool bIncludeInherited = true);

	// ── Read / write ───────────────────────────────────────────────

	/**
	 * Read any UPROPERTY by dotted path — returns export-text. Bypasses
	 * UE Python's protected / private check.
	 *
	 * Path grammar: dotted nesting + `[N]` array indexing, e.g.
	 *   "RootComponent.RelativeLocation"
	 *   "Modifiers[0].ModifierMagnitude.ScalableFloatMagnitude.Value"
	 *
	 * @param ObjectOrClassPath  Same accepted formats as ListUProperties.
	 * @param PropertyPath       Dotted path with optional `[N]` indices.
	 * @param OutSuccess         True iff the path resolved AND the value
	 *                           could be exported to text.
	 * @return Export-text. For floats: "50.0". For FVector:
	 *         "(X=1.000000,Y=2.000000,Z=3.000000)". Empty when OutSuccess
	 *         is false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Property")
	static FString GetUPropertyAsExportText(
		const FString& ObjectOrClassPath,
		const FString& PropertyPath,
		bool& OutSuccess);

	/**
	 * Write any UPROPERTY from export-text. Bypasses Python access checks
	 * (protected / private) AND UE Python's "cannot be edited on instances"
	 * gate that blocks EditDefaultsOnly sub-fields of struct copies.
	 *
	 * Wraps in `FScopedTransaction` so writes are undoable. Calls
	 * `Object->Modify()` to mark dirty.
	 *
	 * @param bFireChangeNotify  When true, calls `PostEditChangeChainProperty`
	 *                           with a chain covering the full property path
	 *                           — open editor windows refresh in real time.
	 *                           Set false for batch edits where you only want
	 *                           one notify at the end (lower thrash).
	 * @param bAllowNativeCdo    When the resolved write target is the CDO of
	 *                           a native /Script/ class, the write is refused
	 *                           unless this is explicitly true. Native-CDO
	 *                           writes are memory-only (lost on editor
	 *                           restart, saved nowhere) yet instantly affect
	 *                           every live instance of the class — the
	 *                           refusal makes that trade-off a deliberate
	 *                           choice instead of an accident. Blueprint
	 *                           CDOs (`Default__Foo_C`) are asset-backed and
	 *                           unaffected by this gate.
	 *
	 * @return true on success; false if path couldn't resolve, ImportText
	 *         failed, the target object was null, or a native-CDO target was
	 *         refused (see bAllowNativeCdo).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Property")
	static bool SetUPropertyFromExportText(
		const FString& ObjectOrClassPath,
		const FString& PropertyPath,
		const FString& ValueExportText,
		bool bFireChangeNotify = true,
		bool bAllowNativeCdo = false);

	// ── Container ops ──────────────────────────────────────────────
	//
	// All three target a property whose final element is TArray<T> OR
	// FGameplayTagContainer (the latter is auto-detected and routed
	// through `Container.AddTag()` / `RemoveTag()` so the internal
	// ParentTags cache stays consistent).

	/** Append one element to a TArray<T> or one tag to FGameplayTagContainer.
	 *
	 *  ElementExportText is the export-text of a single element:
	 *    - for TArray<float>:    "1.5"
	 *    - for TArray<FVector>:  "(X=1,Y=2,Z=3)"
	 *    - for FGameplayTagContainer: "Combat.Hit" (also accepts (TagName="..."))
	 *
	 *  @param bAllowNativeCdo  See SetUPropertyFromExportText — native
	 *                          /Script/ CDO targets are refused unless true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Property")
	static bool ArrayAppendUProperty(
		const FString& ObjectOrClassPath,
		const FString& PropertyPath,
		const FString& ElementExportText,
		bool bFireChangeNotify = true,
		bool bAllowNativeCdo = false);

	/** Remove element at Index. Negative Index counts from the end (-1 = last).
	 *
	 *  @param bAllowNativeCdo  See SetUPropertyFromExportText — native
	 *                          /Script/ CDO targets are refused unless true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Property")
	static bool ArrayRemoveUProperty(
		const FString& ObjectOrClassPath,
		const FString& PropertyPath,
		int32 Index,
		bool bFireChangeNotify = true,
		bool bAllowNativeCdo = false);

	/** Empty the array / container.
	 *
	 *  @param bAllowNativeCdo  See SetUPropertyFromExportText — native
	 *                          /Script/ CDO targets are refused unless true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Property")
	static bool ArrayClearUProperty(
		const FString& ObjectOrClassPath,
		const FString& PropertyPath,
		bool bFireChangeNotify = true,
		bool bAllowNativeCdo = false);

	// ── CDO helper ─────────────────────────────────────────────────

	/**
	 * Resolve an asset path to its real CDO object path. Solves the trap
	 * where `bp.generated_class().get_default_object()` in UE Python often
	 * returns the BlueprintGeneratedClass meta-instance, NOT the
	 * `Default__Foo_C` CDO. This API does the C++-side resolution
	 * correctly and returns the path you can hand back to other tether
	 * APIs (or `unreal.load_object`).
	 *
	 * @return Empty string when the path doesn't load to a Blueprint
	 *         asset with a valid GeneratedClass.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|Property")
	static FString GetAssetCDOPath(const FString& AssetPath);
};
