
#include "StringFlowUnrealModule.h"

#define LOCTEXT_NAMESPACE "FStringFlowUnrealModule"

void FStringFlowUnrealModule::StartupModule() {
    // This code will execute after your module is loaded into memory;
    // the exact timing is specified in the .uplugin file per-module
}

void FStringFlowUnrealModule::ShutdownModule() {
    // This function may be called during shutdown to clean up your module.
    // For modules that support dynamic reloading, we are called when the module
    // is being unloaded
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStringFlowUnrealModule, StringFlowUnreal)
