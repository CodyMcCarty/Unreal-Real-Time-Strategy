// Copyright Cody McCarty. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FSandCoreLogToolsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
