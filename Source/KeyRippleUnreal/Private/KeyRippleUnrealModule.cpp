#include "KeyRippleUnrealModule.h"

#include "KeyRippleUnreal.h"
#include "LevelEditor.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FKeyRippleUnrealModule"

void FKeyRippleUnrealModule::StartupModule() {
    UE_LOG(LogTemp, Warning, TEXT("KeyRippleUnreal Editor Module Startup"));
}

void FKeyRippleUnrealModule::ShutdownModule() {
    UE_LOG(LogTemp, Warning, TEXT("KeyRippleUnreal Editor Module Shutdown"));
}

IMPLEMENT_MODULE(FKeyRippleUnrealModule, KeyRippleUnreal)
#undef LOCTEXT_NAMESPACE