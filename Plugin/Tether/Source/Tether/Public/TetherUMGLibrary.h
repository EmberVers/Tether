#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TetherUMGLibrary.generated.h"

/** Describes a single widget in a Widget Blueprint hierarchy. */
USTRUCT(BlueprintType)
struct FTetherWidgetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString WidgetClass;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString ParentName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString SlotType;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") bool bIsVariable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Visibility;
};

/** A reflected property value on a widget or its panel slot. */
USTRUCT(BlueprintType)
struct FTetherWidgetPropertyValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Type;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Value;
};

/** A discoverable UWidget class that can be passed to AddWidget. */
USTRUCT(BlueprintType)
struct FTetherWidgetClassInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString ClassPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Category;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") bool bIsPanel = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") bool bCanHaveMultipleChildren = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") bool bIsAbstract = false;
};

/** Common result for Widget Blueprint and widget-tree authoring operations. */
USTRUCT(BlueprintType)
struct FTetherWidgetOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Path;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Error;
};

/** One static validation or compiler finding for a Widget Blueprint. */
USTRUCT(BlueprintType)
struct FTetherWidgetValidationIssue
{
	GENERATED_BODY()

	/** "error", "warning", or "info". */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Severity;
	/** Stable machine-readable identifier. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Code;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString WidgetName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Message;
};

/** Compile + deliverability validation result for a Widget Blueprint. */
USTRUCT(BlueprintType)
struct FTetherWidgetValidationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") bool bCompiled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") bool bCompileSucceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") bool bSaved = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString CompileStatus;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") TArray<FTetherWidgetValidationIssue> Issues;
};

/** A track within a widget animation. */
USTRUCT(BlueprintType)
struct FTetherWidgetAnimTrack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString WidgetName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString TrackType;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString PropertyName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") int32 KeyCount = 0;
};

/** Describes a widget animation. */
USTRUCT(BlueprintType)
struct FTetherWidgetAnimationInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") float Duration = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") float DisplayRate = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") TArray<FTetherWidgetAnimTrack> Tracks;
};

/** A legacy UMG property binding on a widget. */
USTRUCT(BlueprintType)
struct FTetherWidgetBindingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString WidgetName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString PropertyName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString FunctionName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString Kind;
};

/** An event binding on a widget (OnClicked, OnHovered, etc.). */
USTRUCT(BlueprintType)
struct FTetherWidgetEventInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString WidgetName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString EventName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG") FString HandlerName;
};

/** One float key used by AddWidgetAnimationFloatKeys. */
USTRUCT(BlueprintType)
struct FTetherWidgetFloatKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Tether|UMG") float Time = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Tether|UMG") float Value = 0.f;
};

/** One color key used by AddWidgetAnimationColorKeys. */
USTRUCT(BlueprintType)
struct FTetherWidgetColorKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Tether|UMG") float Time = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Tether|UMG") FLinearColor Value = FLinearColor::White;
};

/** One complete render-transform key used by AddWidgetAnimationTransformKeys. */
USTRUCT(BlueprintType)
struct FTetherWidgetTransformKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Tether|UMG") float Time = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Tether|UMG") FVector2D Translation = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Tether|UMG") FVector2D Scale = FVector2D(1.f, 1.f);
	UPROPERTY(BlueprintReadWrite, Category = "Tether|UMG") FVector2D Shear = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Tether|UMG") float Angle = 0.f;
};

/** One MVVM ViewModel source configured on a Widget Blueprint. */
USTRUCT(BlueprintType)
struct FTetherMVVMViewModelInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") FString ClassPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") FString CreationType;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") bool bOptional = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") bool bCreateGetter = true;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") bool bCreateSetter = false;
};

/** One compiled MVVM binding configured on a Widget Blueprint. */
USTRUCT(BlueprintType)
struct FTetherMVVMBindingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") FString SourcePath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") FString DestinationPath;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") FString Mode;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") bool bEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") bool bCompile = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") TArray<FString> Errors;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|MVVM") TArray<FString> Warnings;
};

/** Live widget state from an instance spawned by SpawnWidgetInstance. */
USTRUCT(BlueprintType)
struct FTetherLiveWidgetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") FString WidgetClass;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") FString ParentName;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") FString Visibility;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") bool bEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") bool bHasKeyboardFocus = false;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") float RenderOpacity = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") FVector2D DesiredSize = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") FVector2D AbsolutePosition = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Tether|UMG|Runtime") FVector2D AbsoluteSize = FVector2D::ZeroVector;
};

/**
 * Complete Widget Blueprint authoring, MVVM, animation, UI-material assignment,
 * and live validation surface for Tether.
 */
UCLASS()
class TETHER_API UTetherUMGLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** List loaded native/widget classes accepted by AddWidget. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static TArray<FTetherWidgetClassInfo> ListWidgetClasses(
		const FString& Query, bool bIncludeAbstract, int32 MaxResults);

	/** Create a Widget Blueprint. ParentClassPath defaults to UserWidget. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static FTetherWidgetOperationResult CreateWidgetBlueprint(
		const FString& AssetPath, const FString& ParentClassPath);

	/** Add a widget to a tree. Empty ParentName creates the root and is only valid when no root exists. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static FTetherWidgetOperationResult AddWidget(
		const FString& WidgetBlueprintPath, const FString& WidgetClassPath,
		const FString& WidgetName, const FString& ParentName, int32 InsertIndex);

	/** Remove a widget and its descendants from a Widget Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static bool RemoveWidget(const FString& WidgetBlueprintPath, const FString& WidgetName);

	/** Rename a widget and update Blueprint variable, animation, legacy binding, and MVVM references. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static bool RenameWidget(
		const FString& WidgetBlueprintPath, const FString& WidgetName, const FString& NewName);

	/** Move a non-root widget under another panel, optionally at a specific child index. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static bool ReparentWidget(
		const FString& WidgetBlueprintPath, const FString& WidgetName,
		const FString& NewParentName, int32 InsertIndex);

	/** Control whether the compiled UserWidget exposes this widget as a member variable. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static bool SetWidgetIsVariable(
		const FString& WidgetBlueprintPath, const FString& WidgetName, bool bIsVariable);

	/** Get the widget hierarchy as a flat parent-linked list. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static TArray<FTetherWidgetInfo> GetWidgetTree(const FString& WidgetBlueprintPath);

	/** Get non-default properties for a widget template. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static TArray<FTetherWidgetPropertyValue> GetWidgetProperties(
		const FString& WidgetBlueprintPath, const FString& WidgetName);

	/** Get non-default properties for a widget's panel slot. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static TArray<FTetherWidgetPropertyValue> GetWidgetSlotProperties(
		const FString& WidgetBlueprintPath, const FString& WidgetName);

	/** Search widgets by name or class substring. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static TArray<FTetherWidgetInfo> SearchWidgets(
		const FString& WidgetBlueprintPath, const FString& Query);

	/** Set a widget-template UPROPERTY from UE ImportText syntax. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static bool SetWidgetProperty(
		const FString& WidgetBlueprintPath, const FString& WidgetName,
		const FString& PropertyName, const FString& Value);

	/** Set a panel-slot UPROPERTY from UE ImportText syntax. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static bool SetWidgetSlotProperty(
		const FString& WidgetBlueprintPath, const FString& WidgetName,
		const FString& PropertyName, const FString& Value);

	/** Configure a CanvasPanelSlot without hand-authoring ImportText strings. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static bool SetCanvasSlotLayout(
		const FString& WidgetBlueprintPath, const FString& WidgetName,
		FVector2D Position, FVector2D Size, FVector2D AnchorMinimum,
		FVector2D AnchorMaximum, FVector2D Alignment, bool bAutoSize, int32 ZOrder);

	/**
	 * Assign an asset to a FSlateBrush property. BrushPropertyPath supports dotted
	 * style members such as "WidgetStyle.Normal". ResourcePath may be a texture,
	 * material, or material instance; empty clears the resource.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static bool SetWidgetBrush(
		const FString& WidgetBlueprintPath, const FString& WidgetName,
		const FString& BrushPropertyPath, const FString& ResourcePath,
		FLinearColor Tint, const FString& DrawAs, FVector2D ImageSize, FMargin Margin);

	/** Compile, validate, and optionally save a deliverable Widget Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static FTetherWidgetValidationReport CompileAndValidateWidgetBlueprint(
		const FString& WidgetBlueprintPath, bool bSave, bool bCheckAccessibility);

	// Animation authoring -----------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Animation")
	static TArray<FTetherWidgetAnimationInfo> GetWidgetAnimations(const FString& WidgetBlueprintPath);

	/** Create an animation with a finite playback range. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Animation")
	static bool CreateWidgetAnimation(
		const FString& WidgetBlueprintPath, const FString& AnimationName,
		float DurationSeconds, int32 DisplayRate);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Animation")
	static bool RemoveWidgetAnimation(
		const FString& WidgetBlueprintPath, const FString& AnimationName);

	/** Batch-add keys to a float property track, e.g. RenderOpacity. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Animation")
	static bool AddWidgetAnimationFloatKeys(
		const FString& WidgetBlueprintPath, const FString& AnimationName,
		const FString& WidgetName, const FString& PropertyName,
		const TArray<FTetherWidgetFloatKey>& Keys, const FString& Interpolation);

	/** Batch-add keys to a color property track, e.g. ColorAndOpacity or BrushColor. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Animation")
	static bool AddWidgetAnimationColorKeys(
		const FString& WidgetBlueprintPath, const FString& AnimationName,
		const FString& WidgetName, const FString& PropertyName,
		const TArray<FTetherWidgetColorKey>& Keys, const FString& Interpolation);

	/** Batch-add complete RenderTransform keys. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Animation")
	static bool AddWidgetAnimationTransformKeys(
		const FString& WidgetBlueprintPath, const FString& AnimationName,
		const FString& WidgetName, const TArray<FTetherWidgetTransformKey>& Keys,
		const FString& Interpolation);

	/** Remove all animation tracks for PropertyName on WidgetName. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Animation")
	static int32 RemoveWidgetAnimationTrack(
		const FString& WidgetBlueprintPath, const FString& AnimationName,
		const FString& WidgetName, const FString& PropertyName);

	// Blueprint events and legacy bindings -----------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static TArray<FTetherWidgetBindingInfo> GetWidgetBindings(const FString& WidgetBlueprintPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG")
	static TArray<FTetherWidgetEventInfo> GetWidgetEvents(const FString& WidgetBlueprintPath);

	// MVVM authoring (functional on UE 5.7+; older engines log safe no-ops) ----

	/** Create a Blueprint derived from MVVMViewModelBase (UE 5.7+). */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|MVVM")
	static FTetherWidgetOperationResult CreateMVVMViewModelBlueprint(
		const FString& AssetPath, const FString& ParentClassPath);

	/** Enable/disable FieldNotify metadata on a ViewModel Blueprint variable. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|MVVM")
	static bool SetViewModelFieldNotify(
		const FString& ViewModelBlueprintPath, const FString& VariableName, bool bEnabled);

	/**
	 * Add a ViewModel source. CreationType: CreateInstance, Manual,
	 * GlobalViewModelCollection, PropertyPath, or Resolver.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|MVVM")
	static FString AddMVVMViewModel(
		const FString& WidgetBlueprintPath, const FString& ViewModelName,
		const FString& ViewModelClassPath, const FString& CreationType,
		const FString& CreationData,
		bool bOptional, bool bCreateGetter, bool bCreateSetter);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|MVVM")
	static bool RemoveMVVMViewModel(
		const FString& WidgetBlueprintPath, const FString& ViewModelName);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|MVVM")
	static TArray<FTetherMVVMViewModelInfo> GetMVVMViewModels(const FString& WidgetBlueprintPath);

	/**
	 * Create an MVVM property binding. SourceFieldPath and DestinationFieldPath
	 * are dot-separated reflected property paths. DestinationWidgetName may be
	 * "self" for the UserWidget itself. Mode: OneTimeToDestination,
	 * OneWayToDestination, TwoWay, or OneWayToSource.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|MVVM")
	static FString AddMVVMBinding(
		const FString& WidgetBlueprintPath, const FString& ViewModelName,
		const FString& SourceFieldPath, const FString& DestinationWidgetName,
		const FString& DestinationFieldPath, const FString& Mode);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|MVVM")
	static bool RemoveMVVMBinding(
		const FString& WidgetBlueprintPath, const FString& BindingId);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|MVVM")
	static TArray<FTetherMVVMBindingInfo> GetMVVMBindings(const FString& WidgetBlueprintPath);

	/** Configure automatic source/binding/event initialization for the generated view. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|MVVM")
	static bool SetMVVMViewSettings(
		const FString& WidgetBlueprintPath, bool bInitializeSourcesOnConstruct,
		bool bInitializeBindingsOnConstruct, bool bInitializeEventsOnConstruct,
		bool bCreateViewWithoutBindings);

	// Live validation ---------------------------------------------------------

	/** Spawn a compiled Widget Blueprint in PIE and add it to the viewport. Returns a session-local handle. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static FString SpawnWidgetInstance(const FString& WidgetBlueprintPath, int32 ZOrder);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool RemoveWidgetInstance(const FString& InstanceHandle);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static int32 RemoveAllWidgetInstances();

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static TArray<FTetherLiveWidgetInfo> GetLiveWidgetTree(const FString& InstanceHandle);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static FString GetLiveWidgetProperty(
		const FString& InstanceHandle, const FString& WidgetName, const FString& PropertyPath);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool SetLiveWidgetProperty(
		const FString& InstanceHandle, const FString& WidgetName,
		const FString& PropertyPath, const FString& Value);

	/** Invoke a UButton's semantic OnClicked delegate (useful for deterministic functional validation). */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool ClickLiveButton(const FString& InstanceHandle, const FString& WidgetName);

	/** Set TextBlock, EditableText, or EditableTextBox text using the widget's public setter. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool SetLiveWidgetText(
		const FString& InstanceHandle, const FString& WidgetName, const FString& Text);

	/** Set a Slider or ProgressBar value using its public setter. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool SetLiveWidgetValue(
		const FString& InstanceHandle, const FString& WidgetName, float Value);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool SetLiveWidgetChecked(
		const FString& InstanceHandle, const FString& WidgetName, bool bChecked);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool FocusLiveWidget(const FString& InstanceHandle, const FString& WidgetName);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool PlayLiveWidgetAnimation(
		const FString& InstanceHandle, const FString& AnimationName,
		float StartTime, int32 NumLoops, const FString& PlayMode, float PlaybackSpeed);

	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool StopLiveWidgetAnimation(
		const FString& InstanceHandle, const FString& AnimationName);

	/** Set a scalar parameter on the dynamic UI material used by an Image or Border. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool SetLiveWidgetMaterialScalar(
		const FString& InstanceHandle, const FString& WidgetName,
		const FString& ParameterName, float Value);

	/** Set a vector parameter on the dynamic UI material used by an Image or Border. */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime")
	static bool SetLiveWidgetMaterialVector(
		const FString& InstanceHandle, const FString& WidgetName,
		const FString& ParameterName, FLinearColor Value);

	/** Read a reflected property from a live CreateInstance/Manual MVVM source (UE 5.7+). */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime|MVVM")
	static FString GetLiveViewModelProperty(
		const FString& InstanceHandle, const FString& ViewModelName, const FString& PropertyPath);

	/** Write a live ViewModel property, broadcast FieldNotify, and execute its bindings (UE 5.7+). */
	UFUNCTION(BlueprintCallable, Category = "Tether|UMG|Runtime|MVVM")
	static bool SetLiveViewModelProperty(
		const FString& InstanceHandle, const FString& ViewModelName,
		const FString& PropertyPath, const FString& Value);
};
