#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FTetherServer;
class FTetherDiscoveryService;

class FTetherModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FTetherServer, ESPMode::ThreadSafe> Server;
	TUniquePtr<FTetherDiscoveryService> Discovery;
	FDelegateHandle MainFrameReadyHandle;
};
