#include "TetherReactiveAdapter.h"
#include "TetherReactiveSubsystem.h"
#include "Reactive/TetherReactiveShared.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"

DEFINE_LOG_CATEGORY_STATIC(LogTetherReactiveAsset, Log, All);

/**
 * Binds AssetRegistry's Added/Removed/Renamed/Updated multicasts. All handlers
 * share a single binding per event (refcount-by-handler-count); dispatch is
 * per-event via the subsystem's (Subject=null, Selector=event-name) match.
 *
 * Selector = NAME_None on a handler means "match any asset event" (dispatcher
 * treats none-selector as a wildcard). Otherwise Selector is the event name
 * the handler cares about.
 */
class FTetherAssetEventAdapter : public ITetherReactiveAdapter
{
public:
	virtual ETetherTrigger GetTriggerType() const override { return ETetherTrigger::AssetEvent; }
	virtual FString GetTriggerName() const override { return TEXT("AssetEvent"); }

	virtual void OnHandlerAdded(const FTetherHandlerRecord& /*Record*/) override
	{
		HandlerCount += 1;
		if (HandlerCount == 1)
		{
			BindDelegates();
		}
	}

	virtual void OnHandlerRemoved(const FTetherHandlerRecord& /*Record*/) override
	{
		HandlerCount = FMath::Max(0, HandlerCount - 1);
		if (HandlerCount == 0)
		{
			UnbindDelegates();
		}
	}

	virtual void Shutdown() override
	{
		HandlerCount = 0;
		UnbindDelegates();
	}

	virtual TMap<FString, FString> DescribeContext() const override
	{
		TMap<FString, FString> D;
		D.Add(TEXT("trigger"),      TEXT("str — always 'asset_event'"));
		D.Add(TEXT("event"),        TEXT("str — one of 'Added', 'Removed', 'Renamed', 'Updated'"));
		D.Add(TEXT("asset_path"),   TEXT("str — object path of the asset (e.g. '/Game/Foo/Bar.Bar')"));
		D.Add(TEXT("asset_class"),  TEXT("str — short class name (e.g. 'Material', 'Blueprint')"));
		D.Add(TEXT("package_name"), TEXT("str — package name (e.g. '/Game/Foo/Bar')"));
		D.Add(TEXT("old_path"),     TEXT("str — previous object path; empty unless event=='Renamed'"));
		return D;
	}

private:
	void BindDelegates()
	{
		FAssetRegistryModule& Reg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AR = Reg.Get();
		AddedHandle   = AR.OnAssetAdded().AddRaw(this, &FTetherAssetEventAdapter::OnAssetAdded);
		RemovedHandle = AR.OnAssetRemoved().AddRaw(this, &FTetherAssetEventAdapter::OnAssetRemoved);
		RenamedHandle = AR.OnAssetRenamed().AddRaw(this, &FTetherAssetEventAdapter::OnAssetRenamed);
		UpdatedHandle = AR.OnAssetUpdated().AddRaw(this, &FTetherAssetEventAdapter::OnAssetUpdated);
	}

	void UnbindDelegates()
	{
		FAssetRegistryModule* Reg = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
		if (!Reg) return;
		IAssetRegistry& AR = Reg->Get();
		if (AddedHandle.IsValid())   { AR.OnAssetAdded().Remove(AddedHandle);     AddedHandle.Reset();   }
		if (RemovedHandle.IsValid()) { AR.OnAssetRemoved().Remove(RemovedHandle); RemovedHandle.Reset(); }
		if (RenamedHandle.IsValid()) { AR.OnAssetRenamed().Remove(RenamedHandle); RenamedHandle.Reset(); }
		if (UpdatedHandle.IsValid()) { AR.OnAssetUpdated().Remove(UpdatedHandle); UpdatedHandle.Reset(); }
	}

	void OnAssetAdded(const FAssetData& Data)   { DispatchEvent(TEXT("Added"),   Data, FString()); }
	void OnAssetRemoved(const FAssetData& Data) { DispatchEvent(TEXT("Removed"), Data, FString()); }
	void OnAssetUpdated(const FAssetData& Data) { DispatchEvent(TEXT("Updated"), Data, FString()); }
	void OnAssetRenamed(const FAssetData& Data, const FString& OldObjectPath)
	{
		DispatchEvent(TEXT("Renamed"), Data, OldObjectPath);
	}

	void DispatchEvent(const TCHAR* EventName, const FAssetData& Data, const FString& OldObjectPath)
	{
		// Hot path (OnAssetUpdated fires per resave): skip all context-string
		// construction when no handler could possibly match.
		if (HandlerCount == 0)
		{
			return;
		}

		UTetherReactiveSubsystem* Sub = UTetherReactiveSubsystem::Get();
		if (!Sub) return;

		const FString AssetPath   = Data.GetObjectPathString();
		const FString AssetClass  = Data.AssetClassPath.GetAssetName().ToString();
		const FString PackageName = Data.PackageName.ToString();

		TMap<FString, FString> Ctx;
		Ctx.Add(TEXT("trigger"),      TEXT("'asset_event'"));
		Ctx.Add(TEXT("event"),        FString::Printf(TEXT("'%s'"), EventName));
		Ctx.Add(TEXT("asset_path"),   FString::Printf(TEXT("'%s'"),
			*TetherReactiveUtil::EscapePythonStringLiteral(AssetPath)));
		Ctx.Add(TEXT("asset_class"),  FString::Printf(TEXT("'%s'"),
			*TetherReactiveUtil::EscapePythonStringLiteral(AssetClass)));
		Ctx.Add(TEXT("package_name"), FString::Printf(TEXT("'%s'"),
			*TetherReactiveUtil::EscapePythonStringLiteral(PackageName)));
		Ctx.Add(TEXT("old_path"),     FString::Printf(TEXT("'%s'"),
			*TetherReactiveUtil::EscapePythonStringLiteral(OldObjectPath)));

		Sub->Dispatch(ETetherTrigger::AssetEvent,
			TWeakObjectPtr<UObject>(),   // always global
			FName(EventName),
			Ctx);
	}

	int32 HandlerCount = 0;
	FDelegateHandle AddedHandle;
	FDelegateHandle RemovedHandle;
	FDelegateHandle RenamedHandle;
	FDelegateHandle UpdatedHandle;
};

namespace TetherReactiveAdapters
{
	TUniquePtr<ITetherReactiveAdapter> MakeAssetEventAdapter()
	{
		return MakeUnique<FTetherAssetEventAdapter>();
	}
}
