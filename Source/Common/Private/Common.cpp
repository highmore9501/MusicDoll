#include "Common.h"

DEFINE_LOG_CATEGORY(LogCommon);

void FCommonModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FCommonModule::ShutdownModule()
{
	// This code will execute after your module is unloaded
}

IMPLEMENT_MODULE(FCommonModule, Common);