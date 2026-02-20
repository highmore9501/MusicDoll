#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "BoneControlPair.h"
#include "BoneControlMappingUtility.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCommon, Log, All);

class FCommonModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};