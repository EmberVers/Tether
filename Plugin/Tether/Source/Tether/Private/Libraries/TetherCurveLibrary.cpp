#include "TetherCurveLibrary.h"

#include "Curves/CurveBase.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "Curves/RealCurve.h"
#include "Curves/SimpleCurve.h"
#include "Engine/CurveTable.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "TetherCurve"

// ─── Enum <-> string mapping ────────────────────────────────

namespace TetherCurveImpl
{
	const TCHAR* InterpModeToStr(ERichCurveInterpMode M)
	{
		switch (M)
		{
		case RCIM_Linear:   return TEXT("Linear");
		case RCIM_Constant: return TEXT("Constant");
		case RCIM_Cubic:    return TEXT("Cubic");
		case RCIM_None:     return TEXT("None");
		default:            return TEXT("None");
		}
	}

	bool InterpModeFromStr(const FString& S, ERichCurveInterpMode& Out)
	{
		if (S.Equals(TEXT("Linear"),   ESearchCase::IgnoreCase)) { Out = RCIM_Linear;   return true; }
		if (S.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)) { Out = RCIM_Constant; return true; }
		if (S.Equals(TEXT("Cubic"),    ESearchCase::IgnoreCase)) { Out = RCIM_Cubic;    return true; }
		if (S.Equals(TEXT("None"),     ESearchCase::IgnoreCase)) { Out = RCIM_None;     return true; }
		return false;
	}

	const TCHAR* TangentModeToStr(ERichCurveTangentMode M)
	{
		switch (M)
		{
		case RCTM_Auto:      return TEXT("Auto");
		case RCTM_User:      return TEXT("User");
		case RCTM_Break:     return TEXT("Break");
		case RCTM_None:      return TEXT("None");
		case RCTM_SmartAuto: return TEXT("SmartAuto");
		default:             return TEXT("None");
		}
	}

	bool TangentModeFromStr(const FString& S, ERichCurveTangentMode& Out)
	{
		if (S.Equals(TEXT("Auto"),      ESearchCase::IgnoreCase)) { Out = RCTM_Auto;      return true; }
		if (S.Equals(TEXT("User"),      ESearchCase::IgnoreCase)) { Out = RCTM_User;      return true; }
		if (S.Equals(TEXT("Break"),     ESearchCase::IgnoreCase)) { Out = RCTM_Break;     return true; }
		if (S.Equals(TEXT("None"),      ESearchCase::IgnoreCase)) { Out = RCTM_None;      return true; }
		if (S.Equals(TEXT("SmartAuto"), ESearchCase::IgnoreCase)) { Out = RCTM_SmartAuto; return true; }
		return false;
	}

	const TCHAR* TangentWeightModeToStr(ERichCurveTangentWeightMode M)
	{
		switch (M)
		{
		case RCTWM_WeightedNone:   return TEXT("None");
		case RCTWM_WeightedArrive: return TEXT("Arrive");
		case RCTWM_WeightedLeave:  return TEXT("Leave");
		case RCTWM_WeightedBoth:   return TEXT("Both");
		default:                   return TEXT("None");
		}
	}

	bool TangentWeightModeFromStr(const FString& S, ERichCurveTangentWeightMode& Out)
	{
		if (S.Equals(TEXT("None"),   ESearchCase::IgnoreCase)) { Out = RCTWM_WeightedNone;   return true; }
		if (S.Equals(TEXT("Arrive"), ESearchCase::IgnoreCase)) { Out = RCTWM_WeightedArrive; return true; }
		if (S.Equals(TEXT("Leave"),  ESearchCase::IgnoreCase)) { Out = RCTWM_WeightedLeave;  return true; }
		if (S.Equals(TEXT("Both"),   ESearchCase::IgnoreCase)) { Out = RCTWM_WeightedBoth;   return true; }
		return false;
	}

	const TCHAR* ExtrapToStr(ERichCurveExtrapolation E)
	{
		switch (E)
		{
		case RCCE_Cycle:           return TEXT("Cycle");
		case RCCE_CycleWithOffset: return TEXT("CycleWithOffset");
		case RCCE_Oscillate:       return TEXT("Oscillate");
		case RCCE_Linear:          return TEXT("Linear");
		case RCCE_Constant:        return TEXT("Constant");
		case RCCE_None:            return TEXT("None");
		default:                   return TEXT("None");
		}
	}

	bool ExtrapFromStr(const FString& S, ERichCurveExtrapolation& Out)
	{
		if (S.Equals(TEXT("Cycle"),           ESearchCase::IgnoreCase)) { Out = RCCE_Cycle;           return true; }
		if (S.Equals(TEXT("CycleWithOffset"), ESearchCase::IgnoreCase)) { Out = RCCE_CycleWithOffset; return true; }
		if (S.Equals(TEXT("Oscillate"),       ESearchCase::IgnoreCase)) { Out = RCCE_Oscillate;       return true; }
		if (S.Equals(TEXT("Linear"),          ESearchCase::IgnoreCase)) { Out = RCCE_Linear;          return true; }
		if (S.Equals(TEXT("Constant"),        ESearchCase::IgnoreCase)) { Out = RCCE_Constant;        return true; }
		if (S.Equals(TEXT("None"),            ESearchCase::IgnoreCase)) { Out = RCCE_None;            return true; }
		return false;
	}

	// ─── Channel access on UCurveBase subclasses ────────────

	/**
	 * Returns the channel name for a UCurveBase subclass. Index-in-bounds per class:
	 *   UCurveFloat       — {"Value"}
	 *   UCurveVector      — {"X","Y","Z"}
	 *   UCurveLinearColor — {"R","G","B","A"}
	 */
	TArray<FString> GetChannelNames(const UCurveBase* Curve)
	{
		TArray<FString> Names;
		if (!Curve) return Names;
		if (Curve->IsA<UCurveFloat>())             Names = { TEXT("Value") };
		else if (Curve->IsA<UCurveVector>())       Names = { TEXT("X"), TEXT("Y"), TEXT("Z") };
		else if (Curve->IsA<UCurveLinearColor>())  Names = { TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };
		return Names;
	}

	FRichCurve* GetChannel(UCurveBase* Curve, int32 ChannelIndex)
	{
		if (!Curve) return nullptr;
		if (UCurveFloat* CF = Cast<UCurveFloat>(Curve))
		{
			if (ChannelIndex == 0) return &CF->FloatCurve;
		}
		else if (UCurveVector* CV = Cast<UCurveVector>(Curve))
		{
			if (ChannelIndex >= 0 && ChannelIndex < 3) return &CV->FloatCurves[ChannelIndex];
		}
		else if (UCurveLinearColor* CL = Cast<UCurveLinearColor>(Curve))
		{
			if (ChannelIndex >= 0 && ChannelIndex < 4) return &CL->FloatCurves[ChannelIndex];
		}
		return nullptr;
	}

	int32 GetChannelCount(const UCurveBase* Curve)
	{
		if (!Curve) return 0;
		if (Curve->IsA<UCurveFloat>())             return 1;
		if (Curve->IsA<UCurveVector>())            return 3;
		if (Curve->IsA<UCurveLinearColor>())       return 4;
		return 0;
	}

	/** Collect all channels of a UCurveBase for bulk ops (e.g. SetCurveInfinityExtrap). */
	TArray<FRichCurve*> GetAllChannels(UCurveBase* Curve)
	{
		TArray<FRichCurve*> Out;
		const int32 N = GetChannelCount(Curve);
		for (int32 i = 0; i < N; ++i)
		{
			if (FRichCurve* C = GetChannel(Curve, i)) Out.Add(C);
		}
		return Out;
	}

	// ─── FTetherRichCurveKey <-> FRichCurveKey ───────────────

	FTetherRichCurveKey ToTetherKey(const FRichCurveKey& K)
	{
		FTetherRichCurveKey Out;
		Out.Time                 = K.Time;
		Out.Value                = K.Value;
		Out.InterpMode           = InterpModeToStr(K.InterpMode);
		Out.TangentMode          = TangentModeToStr(K.TangentMode);
		Out.TangentWeightMode    = TangentWeightModeToStr(K.TangentWeightMode);
		Out.ArriveTangent        = K.ArriveTangent;
		Out.LeaveTangent         = K.LeaveTangent;
		Out.ArriveTangentWeight  = K.ArriveTangentWeight;
		Out.LeaveTangentWeight   = K.LeaveTangentWeight;
		return Out;
	}

	FRichCurveKey FromTetherKey(const FTetherRichCurveKey& K)
	{
		FRichCurveKey Out;
		Out.Time  = K.Time;
		Out.Value = K.Value;

		ERichCurveInterpMode Interp = RCIM_Linear;
		if (!K.InterpMode.IsEmpty()) InterpModeFromStr(K.InterpMode, Interp);
		Out.InterpMode = Interp;

		ERichCurveTangentMode Tangent = RCTM_Auto;
		if (!K.TangentMode.IsEmpty()) TangentModeFromStr(K.TangentMode, Tangent);
		Out.TangentMode = Tangent;

		ERichCurveTangentWeightMode WeightMode = RCTWM_WeightedNone;
		if (!K.TangentWeightMode.IsEmpty()) TangentWeightModeFromStr(K.TangentWeightMode, WeightMode);
		Out.TangentWeightMode = WeightMode;

		Out.ArriveTangent        = K.ArriveTangent;
		Out.LeaveTangent         = K.LeaveTangent;
		Out.ArriveTangentWeight  = K.ArriveTangentWeight;
		Out.LeaveTangentWeight   = K.LeaveTangentWeight;
		return Out;
	}

	// ─── Asset loaders ───────────────────────────────────────

	UCurveBase* LoadCurve(const FString& Path)
	{
		UCurveBase* C = LoadObject<UCurveBase>(nullptr, *Path);
		if (!C)
			UE_LOG(LogTemp, Warning, TEXT("Tether: Could not load UCurveBase '%s'"), *Path);
		return C;
	}

	UCurveTable* LoadCT(const FString& Path)
	{
		UCurveTable* T = LoadObject<UCurveTable>(nullptr, *Path);
		if (!T)
			UE_LOG(LogTemp, Warning, TEXT("Tether: Could not load UCurveTable '%s'"), *Path);
		return T;
	}

	/** Broadcast curve-owner notifications so open editors redraw. */
	void NotifyCurveEdited(UCurveBase* Curve)
	{
		if (!Curve) return;
		Curve->ModifyOwner();                 // marks package dirty via FCurveOwnerInterface
		TArray<FRichCurveEditInfo> Changed = Curve->GetCurves();
		Curve->OnCurveChanged(Changed);       // fires refresh on open editors
		Curve->MarkPackageDirty();
	}

	void NotifyCurveTableEdited(UCurveTable* Table)
	{
		if (!Table) return;
		Table->ModifyOwner();
		Table->OnCurveTableChanged().Broadcast();
		Table->MarkPackageDirty();
	}
}

// ─── GetCurveInfo ───────────────────────────────────────────

FTetherCurveInfo UTetherCurveLibrary::GetCurveInfo(const FString& CurvePath)
{
	FTetherCurveInfo Result;
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return Result;

	Result.Name = Curve->GetName();
	Result.ClassName = Curve->GetClass()->GetName();
	Result.ChannelNames = TetherCurveImpl::GetChannelNames(Curve);

	const int32 N = TetherCurveImpl::GetChannelCount(Curve);
	Result.NumKeysPerChannel.Reserve(N);
	for (int32 i = 0; i < N; ++i)
	{
		if (FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, i))
			Result.NumKeysPerChannel.Add(Ch->Keys.Num());
		else
			Result.NumKeysPerChannel.Add(0);
	}

	if (FRichCurve* Ch0 = TetherCurveImpl::GetChannel(Curve, 0))
	{
		Result.PreInfinityExtrap  = TetherCurveImpl::ExtrapToStr(Ch0->PreInfinityExtrap);
		Result.PostInfinityExtrap = TetherCurveImpl::ExtrapToStr(Ch0->PostInfinityExtrap);
	}
	return Result;
}

// ─── GetCurveKeys ───────────────────────────────────────────

TArray<FTetherRichCurveKey> UTetherCurveLibrary::GetCurveKeys(
	const FString& CurvePath, int32 ChannelIndex)
{
	TArray<FTetherRichCurveKey> Out;
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return Out;

	FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch)
	{
		UE_LOG(LogTemp, Warning, TEXT("Tether: channel %d out of range for '%s'"),
			ChannelIndex, *CurvePath);
		return Out;
	}

	Out.Reserve(Ch->Keys.Num());
	for (const FRichCurveKey& K : Ch->Keys)
		Out.Add(TetherCurveImpl::ToTetherKey(K));
	return Out;
}

// ─── GetCurveAsJSONString ───────────────────────────────────

FString UTetherCurveLibrary::GetCurveAsJSONString(const FString& CurvePath)
{
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return FString();

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("name"), Curve->GetName());
	Root->SetStringField(TEXT("class"), Curve->GetClass()->GetName());

	const TArray<FString> Channels = TetherCurveImpl::GetChannelNames(Curve);
	TArray<TSharedPtr<FJsonValue>> ChannelsJson;
	for (int32 i = 0; i < Channels.Num(); ++i)
	{
		FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, i);
		if (!Ch) continue;

		TSharedPtr<FJsonObject> ChObj = MakeShared<FJsonObject>();
		ChObj->SetStringField(TEXT("name"), Channels[i]);
		ChObj->SetStringField(TEXT("pre_infinity"),  TetherCurveImpl::ExtrapToStr(Ch->PreInfinityExtrap));
		ChObj->SetStringField(TEXT("post_infinity"), TetherCurveImpl::ExtrapToStr(Ch->PostInfinityExtrap));

		TArray<TSharedPtr<FJsonValue>> KeysJson;
		for (const FRichCurveKey& K : Ch->Keys)
		{
			TSharedPtr<FJsonObject> KObj = MakeShared<FJsonObject>();
			KObj->SetNumberField(TEXT("time"), K.Time);
			KObj->SetNumberField(TEXT("value"), K.Value);
			KObj->SetStringField(TEXT("interp_mode"),        TetherCurveImpl::InterpModeToStr(K.InterpMode));
			KObj->SetStringField(TEXT("tangent_mode"),       TetherCurveImpl::TangentModeToStr(K.TangentMode));
			KObj->SetStringField(TEXT("tangent_weight_mode"),TetherCurveImpl::TangentWeightModeToStr(K.TangentWeightMode));
			KObj->SetNumberField(TEXT("arrive_tangent"),        K.ArriveTangent);
			KObj->SetNumberField(TEXT("leave_tangent"),         K.LeaveTangent);
			KObj->SetNumberField(TEXT("arrive_tangent_weight"), K.ArriveTangentWeight);
			KObj->SetNumberField(TEXT("leave_tangent_weight"),  K.LeaveTangentWeight);
			KeysJson.Add(MakeShared<FJsonValueObject>(KObj));
		}
		ChObj->SetArrayField(TEXT("keys"), KeysJson);
		ChannelsJson.Add(MakeShared<FJsonValueObject>(ChObj));
	}
	Root->SetArrayField(TEXT("channels"), ChannelsJson);

	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Out;
}

// ─── SetCurveKeys ───────────────────────────────────────────

bool UTetherCurveLibrary::SetCurveKeys(
	const FString& CurvePath, int32 ChannelIndex,
	const TArray<FTetherRichCurveKey>& Keys)
{
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch)
	{
		UE_LOG(LogTemp, Warning, TEXT("Tether: channel %d out of range for '%s'"),
			ChannelIndex, *CurvePath);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("SetCurveKeys", "Set Curve Keys"));
	Curve->Modify();

	TArray<FRichCurveKey> NewKeys;
	NewKeys.Reserve(Keys.Num());
	for (const FTetherRichCurveKey& K : Keys)
		NewKeys.Add(TetherCurveImpl::FromTetherKey(K));

	// FRichCurve::SetKeys expects sorted input — honor that contract.
	NewKeys.Sort([](const FRichCurveKey& A, const FRichCurveKey& B) { return A.Time < B.Time; });
	Ch->SetKeys(NewKeys);

	TetherCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── AddCurveKey ────────────────────────────────────────────

int32 UTetherCurveLibrary::AddCurveKey(
	const FString& CurvePath, int32 ChannelIndex, const FTetherRichCurveKey& Key)
{
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return -1;

	FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return -1;

	FScopedTransaction Transaction(LOCTEXT("AddCurveKey", "Add Curve Key"));
	Curve->Modify();

	const FKeyHandle H = Ch->UpdateOrAddKey(Key.Time, Key.Value);

	// Apply non-default fields from the tether key onto the newly added key.
	FRichCurveKey& AddedKey = Ch->GetKey(H);
	const FRichCurveKey Source = TetherCurveImpl::FromTetherKey(Key);
	AddedKey.InterpMode           = Source.InterpMode;
	AddedKey.TangentMode          = Source.TangentMode;
	AddedKey.TangentWeightMode    = Source.TangentWeightMode;
	AddedKey.ArriveTangent        = Source.ArriveTangent;
	AddedKey.LeaveTangent         = Source.LeaveTangent;
	AddedKey.ArriveTangentWeight  = Source.ArriveTangentWeight;
	AddedKey.LeaveTangentWeight   = Source.LeaveTangentWeight;

	TetherCurveImpl::NotifyCurveEdited(Curve);

	// Find the index of the new key in the (already-sorted) array.
	const TArray<FRichCurveKey>& Arr = Ch->Keys;
	for (int32 i = 0; i < Arr.Num(); ++i)
	{
		if (FMath::IsNearlyEqual(Arr[i].Time, Key.Time))
			return i;
	}
	return -1;
}

// ─── RemoveCurveKeyByIndex ──────────────────────────────────

bool UTetherCurveLibrary::RemoveCurveKeyByIndex(
	const FString& CurvePath, int32 ChannelIndex, int32 Index)
{
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return false;
	if (!Ch->Keys.IsValidIndex(Index)) return false;

	FScopedTransaction Transaction(LOCTEXT("RemoveCurveKey", "Remove Curve Key"));
	Curve->Modify();

	const float KeyTime = Ch->Keys[Index].Time;
	const FKeyHandle H = Ch->FindKey(KeyTime);
	if (!Ch->IsKeyHandleValid(H)) return false;
	Ch->DeleteKey(H);

	TetherCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── ClearCurveKeys ─────────────────────────────────────────

bool UTetherCurveLibrary::ClearCurveKeys(const FString& CurvePath, int32 ChannelIndex)
{
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return false;

	FScopedTransaction Transaction(LOCTEXT("ClearCurveKeys", "Clear Curve Keys"));
	Curve->Modify();

	Ch->Reset();

	TetherCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── SetCurveKeyTangents ────────────────────────────────────

bool UTetherCurveLibrary::SetCurveKeyTangents(
	const FString& CurvePath, int32 ChannelIndex, int32 Index,
	const FString& TangentMode, const FString& TangentWeightMode,
	float ArriveTangent, float LeaveTangent,
	float ArriveTangentWeight, float LeaveTangentWeight)
{
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch || !Ch->Keys.IsValidIndex(Index)) return false;

	FScopedTransaction Transaction(LOCTEXT("SetCurveKeyTangents", "Set Curve Key Tangents"));
	Curve->Modify();

	FRichCurveKey& K = Ch->Keys[Index];
	if (!TangentMode.IsEmpty())
	{
		ERichCurveTangentMode Mode = RCTM_Auto;
		if (TetherCurveImpl::TangentModeFromStr(TangentMode, Mode))
			K.TangentMode = Mode;
	}
	if (!TangentWeightMode.IsEmpty())
	{
		ERichCurveTangentWeightMode Wm = RCTWM_WeightedNone;
		if (TetherCurveImpl::TangentWeightModeFromStr(TangentWeightMode, Wm))
			K.TangentWeightMode = Wm;
	}
	if (!FMath::IsNaN(ArriveTangent))        K.ArriveTangent        = ArriveTangent;
	if (!FMath::IsNaN(LeaveTangent))         K.LeaveTangent         = LeaveTangent;
	if (!FMath::IsNaN(ArriveTangentWeight))  K.ArriveTangentWeight  = ArriveTangentWeight;
	if (!FMath::IsNaN(LeaveTangentWeight))   K.LeaveTangentWeight   = LeaveTangentWeight;

	TetherCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── SetCurveInfinityExtrap ─────────────────────────────────

bool UTetherCurveLibrary::SetCurveInfinityExtrap(
	const FString& CurvePath,
	const FString& PreInfinityExtrap,
	const FString& PostInfinityExtrap)
{
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	ERichCurveExtrapolation Pre = RCCE_Constant, Post = RCCE_Constant;
	const bool bHasPre  = !PreInfinityExtrap.IsEmpty()  && TetherCurveImpl::ExtrapFromStr(PreInfinityExtrap,  Pre);
	const bool bHasPost = !PostInfinityExtrap.IsEmpty() && TetherCurveImpl::ExtrapFromStr(PostInfinityExtrap, Post);
	if (!bHasPre && !bHasPost) return false;

	FScopedTransaction Transaction(LOCTEXT("SetCurveInfinityExtrap", "Set Curve Infinity Extrap"));
	Curve->Modify();

	for (FRichCurve* Ch : TetherCurveImpl::GetAllChannels(Curve))
	{
		if (bHasPre)  Ch->PreInfinityExtrap  = Pre;
		if (bHasPost) Ch->PostInfinityExtrap = Post;
	}

	TetherCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── AutoSetCurveTangents ───────────────────────────────────

bool UTetherCurveLibrary::AutoSetCurveTangents(const FString& CurvePath, float Tension)
{
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return false;

	FScopedTransaction Transaction(LOCTEXT("AutoSetCurveTangents", "Auto Set Curve Tangents"));
	Curve->Modify();

	for (FRichCurve* Ch : TetherCurveImpl::GetAllChannels(Curve))
		Ch->AutoSetTangents(Tension);

	TetherCurveImpl::NotifyCurveEdited(Curve);
	return true;
}

// ─── EvaluateCurve ──────────────────────────────────────────

TArray<float> UTetherCurveLibrary::EvaluateCurve(
	const FString& CurvePath, int32 ChannelIndex, const TArray<float>& Times)
{
	TArray<float> Out;
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return Out;

	FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return Out;

	Out.Reserve(Times.Num());
	for (float T : Times)
		Out.Add(Ch->Eval(T, 0.f));
	return Out;
}

// ─── SampleCurveUniform ─────────────────────────────────────

TArray<float> UTetherCurveLibrary::SampleCurveUniform(
	const FString& CurvePath, int32 ChannelIndex,
	float StartTime, float EndTime, int32 NumSamples)
{
	TArray<float> Out;
	UCurveBase* Curve = TetherCurveImpl::LoadCurve(CurvePath);
	if (!Curve) return Out;

	FRichCurve* Ch = TetherCurveImpl::GetChannel(Curve, ChannelIndex);
	if (!Ch) return Out;

	if (NumSamples <= 1)
	{
		Out.Add(Ch->Eval(StartTime, 0.f));
		return Out;
	}
	Out.Reserve(NumSamples);
	const float Step = (EndTime - StartTime) / static_cast<float>(NumSamples - 1);
	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float T = StartTime + Step * static_cast<float>(i);
		Out.Add(Ch->Eval(T, 0.f));
	}
	return Out;
}

// ─── Curve Table helpers ────────────────────────────────────

namespace TetherCurveImpl
{
	/** Copy a FSimpleCurve row's keys into tether rich-curve-key form. */
	void SimpleCurveToTetherKeys(const FSimpleCurve& Src, TArray<FTetherRichCurveKey>& Out)
	{
		const TCHAR* InterpStr = InterpModeToStr(Src.GetKeyInterpMode());
		for (const FSimpleCurveKey& Sk : Src.GetConstRefOfKeys())
		{
			FTetherRichCurveKey K;
			K.Time = Sk.Time;
			K.Value = Sk.Value;
			K.InterpMode = InterpStr;
			K.TangentMode = TEXT("None");
			K.TangentWeightMode = TEXT("None");
			Out.Add(K);
		}
	}

	/** Write tether keys into a FRichCurve row, sorted by Time. */
	void TetherKeysToRichCurve(const TArray<FTetherRichCurveKey>& Src, FRichCurve& Dst)
	{
		TArray<FRichCurveKey> New;
		New.Reserve(Src.Num());
		for (const FTetherRichCurveKey& K : Src) New.Add(FromTetherKey(K));
		New.Sort([](const FRichCurveKey& A, const FRichCurveKey& B) { return A.Time < B.Time; });
		Dst.SetKeys(New);
	}

	/** Write tether keys into a FSimpleCurve row (tangent fields ignored, uniform interp). */
	void TetherKeysToSimpleCurve(const TArray<FTetherRichCurveKey>& Src, FSimpleCurve& Dst)
	{
		Dst.Reset();
		// Use the interp from the first key as the row-wide interp (SimpleCurve is uniform).
		if (Src.Num() > 0 && !Src[0].InterpMode.IsEmpty())
		{
			ERichCurveInterpMode M = RCIM_Linear;
			if (InterpModeFromStr(Src[0].InterpMode, M))
				Dst.SetKeyInterpMode(M);
		}
		TArray<FTetherRichCurveKey> Sorted = Src;
		Sorted.Sort([](const FTetherRichCurveKey& A, const FTetherRichCurveKey& B) { return A.Time < B.Time; });
		for (const FTetherRichCurveKey& K : Sorted)
			Dst.AddKey(K.Time, K.Value);
	}
}

// ─── GetCurveTableInfo ──────────────────────────────────────

FTetherCurveTableInfo UTetherCurveLibrary::GetCurveTableInfo(const FString& CurveTablePath)
{
	FTetherCurveTableInfo Result;
	UCurveTable* Table = TetherCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return Result;

	Result.Name = Table->GetName();
	switch (Table->GetCurveTableMode())
	{
	case ECurveTableMode::Empty:        Result.CurveTableMode = TEXT("Empty");        break;
	case ECurveTableMode::SimpleCurves: Result.CurveTableMode = TEXT("SimpleCurves"); break;
	case ECurveTableMode::RichCurves:   Result.CurveTableMode = TEXT("RichCurves");   break;
	default:                            Result.CurveTableMode = TEXT("Unknown");      break;
	}

	for (const auto& Pair : Table->GetRowMap())
	{
		Result.RowNames.Add(Pair.Key.ToString());
		Result.NumKeysPerRow.Add(Pair.Value ? Pair.Value->GetNumKeys() : 0);
	}
	return Result;
}

// ─── GetCurveTableRowKeys ───────────────────────────────────

TArray<FTetherRichCurveKey> UTetherCurveLibrary::GetCurveTableRowKeys(
	const FString& CurveTablePath, const FString& RowName)
{
	TArray<FTetherRichCurveKey> Out;
	UCurveTable* Table = TetherCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return Out;

	const FName Key(*RowName);
	if (Table->GetCurveTableMode() == ECurveTableMode::RichCurves)
	{
		if (FRichCurve* R = Table->FindRichCurve(Key, TEXT("TetherGetCurveTableRowKeys")))
		{
			Out.Reserve(R->Keys.Num());
			for (const FRichCurveKey& K : R->Keys)
				Out.Add(TetherCurveImpl::ToTetherKey(K));
		}
	}
	else if (Table->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
	{
		if (FSimpleCurve* S = Table->FindSimpleCurve(Key, TEXT("TetherGetCurveTableRowKeys")))
			TetherCurveImpl::SimpleCurveToTetherKeys(*S, Out);
	}
	return Out;
}

// ─── SetCurveTableRowKeys ───────────────────────────────────

bool UTetherCurveLibrary::SetCurveTableRowKeys(
	const FString& CurveTablePath, const FString& RowName,
	const TArray<FTetherRichCurveKey>& Keys)
{
	UCurveTable* Table = TetherCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return false;

	const FName Key(*RowName);
	const ECurveTableMode Mode = Table->GetCurveTableMode();

	FScopedTransaction Transaction(LOCTEXT("SetCurveTableRowKeys", "Set Curve Table Row Keys"));
	Table->Modify();

	if (Mode == ECurveTableMode::RichCurves)
	{
		FRichCurve* R = Table->FindRichCurve(Key, TEXT("TetherSetCurveTableRowKeys"));
		if (!R) return false;
		TetherCurveImpl::TetherKeysToRichCurve(Keys, *R);
	}
	else if (Mode == ECurveTableMode::SimpleCurves)
	{
		FSimpleCurve* S = Table->FindSimpleCurve(Key, TEXT("TetherSetCurveTableRowKeys"));
		if (!S) return false;
		TetherCurveImpl::TetherKeysToSimpleCurve(Keys, *S);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Tether: table '%s' is empty — use AddCurveTableRow first"),
			*CurveTablePath);
		return false;
	}

	TetherCurveImpl::NotifyCurveTableEdited(Table);
	return true;
}

// ─── AddCurveTableRow ───────────────────────────────────────

bool UTetherCurveLibrary::AddCurveTableRow(
	const FString& CurveTablePath, const FString& RowName,
	const TArray<FTetherRichCurveKey>& Keys)
{
	UCurveTable* Table = TetherCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return false;

	const FName NewKey(*RowName);
	// UCurveTable::AddRichCurve / AddSimpleCurve assumes the name is not present; check up front.
	if (Table->GetRowMap().Contains(NewKey))
	{
		UE_LOG(LogTemp, Warning, TEXT("Tether: row '%s' already exists in '%s'"),
			*RowName, *CurveTablePath);
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("AddCurveTableRow", "Add Curve Table Row"));
	Table->Modify();

	// Empty tables default to RichCurves — the flexible choice for tether callers.
	if (Table->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
	{
		FSimpleCurve& S = Table->AddSimpleCurve(NewKey);
		TetherCurveImpl::TetherKeysToSimpleCurve(Keys, S);
	}
	else
	{
		FRichCurve& R = Table->AddRichCurve(NewKey);
		TetherCurveImpl::TetherKeysToRichCurve(Keys, R);
	}

	TetherCurveImpl::NotifyCurveTableEdited(Table);
	return true;
}

// ─── RemoveCurveTableRow ────────────────────────────────────

bool UTetherCurveLibrary::RemoveCurveTableRow(
	const FString& CurveTablePath, const FString& RowName)
{
	UCurveTable* Table = TetherCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return false;

	const FName Key(*RowName);
	if (!Table->GetRowMap().Contains(Key)) return false;

	FScopedTransaction Transaction(LOCTEXT("RemoveCurveTableRow", "Remove Curve Table Row"));
	Table->Modify();
	Table->RemoveRow(Key);

	TetherCurveImpl::NotifyCurveTableEdited(Table);
	return true;
}

// ─── RenameCurveTableRow ────────────────────────────────────

bool UTetherCurveLibrary::RenameCurveTableRow(
	const FString& CurveTablePath,
	const FString& OldRowName, const FString& NewRowName)
{
	UCurveTable* Table = TetherCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return false;

	FName Old(*OldRowName);
	FName New(*NewRowName);
	if (!Table->GetRowMap().Contains(Old))  return false;
	if (Table->GetRowMap().Contains(New))   return false;

	FScopedTransaction Transaction(LOCTEXT("RenameCurveTableRow", "Rename Curve Table Row"));
	Table->Modify();
	Table->RenameRow(Old, New);

	TetherCurveImpl::NotifyCurveTableEdited(Table);
	return true;
}

// ─── EvaluateCurveTableRow ──────────────────────────────────

TArray<float> UTetherCurveLibrary::EvaluateCurveTableRow(
	const FString& CurveTablePath, const FString& RowName, const TArray<float>& Times)
{
	TArray<float> Out;
	UCurveTable* Table = TetherCurveImpl::LoadCT(CurveTablePath);
	if (!Table) return Out;

	FRealCurve* Curve = Table->FindCurveUnchecked(FName(*RowName));
	if (!Curve) return Out;

	Out.Reserve(Times.Num());
	for (float T : Times)
		Out.Add(Curve->Eval(T, 0.f));
	return Out;
}

#undef LOCTEXT_NAMESPACE
