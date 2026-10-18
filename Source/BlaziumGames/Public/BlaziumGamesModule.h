#pragma once

#include "Modules/ModuleManager.h"

class FBlaziumGamesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
