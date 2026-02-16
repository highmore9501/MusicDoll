#include "MusicDollUIModule.h"

#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "MusicDollWindow.h"
#include "MusicDollStyle.h"

#define LOCTEXT_NAMESPACE "MusicDollUIModule"

// Initialize static instance
FMusicDollUIModule* FMusicDollUIModule::GInstance = nullptr;

// Console command to manually register the window (backup method)
void MusicDollRegisterWindow() {
    UE_LOG(LogTemp, Warning,
           TEXT("Console: Attempting to register Music Doll window"));

    FMusicDollUIModule* Module = FMusicDollUIModule::GetInstance();
    if (Module) {
        // Only allow console command if registration hasn't been attempted yet
        // Otherwise, it would bypass the single-execution guarantee
        UE_LOG(LogTemp, Warning,
               TEXT("Console: Manual registration commands are not allowed after "
                    "module initialization. Use the Windows menu instead."));
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("Console: MusicDollUI module instance not available"));
    }
}

// Register console command
static FAutoConsoleCommand MusicDollRegisterWindowCmd(
    TEXT("MusicDoll.RegisterWindow"),
    TEXT("Register the Music Doll dockable window (deprecated - use Windows menu)"),
    FConsoleCommandDelegate::CreateStatic(&MusicDollRegisterWindow));

void FMusicDollUIModule::StartupModule() {
    UE_LOG(LogTemp, Warning, TEXT("MusicDollUI Module Startup"));

    // Initialize custom style
    FMusicDollStyle::Initialize();

    // Guard against multiple module initialization
    if (bStartupComplete) {
        UE_LOG(LogTemp, Warning,
               TEXT("MusicDollUI Module already initialized, skipping"));
        return;
    }

    // Store this instance as static
    GInstance = this;

    // Initialize flags early
    bWindowRegistrationAttempted = false;

    UE_LOG(LogTemp, Warning,
           TEXT("MusicDollUI: About to bind to OnLevelEditorCreated event"));

    FLevelEditorModule& LevelEditorModule =
        FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    // Store the handle so we can disconnect later
    OnLevelEditorCreatedHandle =
        LevelEditorModule.OnLevelEditorCreated().AddRaw(
            this, &FMusicDollUIModule::RegisterWindowWhenReady);

    UE_LOG(
        LogTemp, Warning,
        TEXT("MusicDollUI: Successfully bound to OnLevelEditorCreated event"));
    bStartupComplete = true;
    UE_LOG(LogTemp, Warning, TEXT("MusicDollUI Module Startup Complete"));
}

void FMusicDollUIModule::ShutdownModule() {
    UE_LOG(LogTemp, Warning, TEXT("MusicDollUI Module Shutdown"));

    // Disconnect the delegate to prevent re-triggering on hot reload
    if (FModuleManager::Get().IsModuleLoaded("LevelEditor")) {
        FLevelEditorModule* LevelEditorModule =
            FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
        if (LevelEditorModule && OnLevelEditorCreatedHandle.IsValid()) {
            LevelEditorModule->OnLevelEditorCreated().Remove(
                OnLevelEditorCreatedHandle);
            UE_LOG(LogTemp, Warning,
                   TEXT("MusicDollUI: Successfully disconnected "
                        "OnLevelEditorCreated delegate"));
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("MusicDollUI: OnLevelEditorCreatedHandle is invalid, "
                        "may have already been disconnected"));
        }
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("MusicDollUI: LevelEditor module not loaded, cannot "
                    "disconnect delegate"));
    }

    // Clean up the window
    if (MusicDollWindow.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("MusicDollUI: Unregistering window..."));
        MusicDollWindow->UnregisterWindow();
        MusicDollWindow.Reset();
        UE_LOG(LogTemp, Warning,
               TEXT("MusicDollUI: MusicDollWindow cleaned up"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("MusicDollUI: MusicDollWindow is already null"));
    }

    FMusicDollWindow::GInstance = nullptr;
    GInstance = nullptr;
    bStartupComplete = false;
    bWindowRegistrationAttempted = false;

    // Shutdown custom style
    FMusicDollStyle::Shutdown();

    UE_LOG(LogTemp, Warning, TEXT("MusicDollUI Module Shutdown Complete"));
}

FMusicDollUIModule* FMusicDollUIModule::GetInstance() { return GInstance; }

void FMusicDollUIModule::RegisterWindowWhenReady(
    TSharedPtr<class ILevelEditor> InLevelEditor) {
    RegisterCallCount++;
    UE_LOG(LogTemp, Warning,
           TEXT("RegisterWindowWhenReady: Event fired [%d times]"),
           RegisterCallCount);

    // Critical: Only attempt registration once, even if the event fires
    // multiple times
    if (bWindowRegistrationAttempted) {
        UE_LOG(LogTemp, Warning,
               TEXT("RegisterWindowWhenReady: Window registration already "
                    "attempted, ignoring duplicate call [Event #%d]"),
               RegisterCallCount);
        return;
    }

    bWindowRegistrationAttempted = true;

    UE_LOG(LogTemp, Log,
           TEXT("RegisterWindowWhenReady: InLevelEditor param is %s"),
           InLevelEditor.IsValid() ? TEXT("valid") : TEXT("null"));

    // Create the window manager here, after OnLevelEditorCreated event
    MusicDollWindow = MakeShareable(new FMusicDollWindow());
    FMusicDollWindow::GInstance = MusicDollWindow;

    UE_LOG(LogTemp, Warning, TEXT("MusicDollWindow instance created"));

    if (!MusicDollWindow.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("RegisterWindowWhenReady: Failed to create MusicDollWindow!"));
        return;
    }

    UE_LOG(LogTemp, Log,
           TEXT("RegisterWindowWhenReady: MusicDollWindow is valid, calling "
                "AttemptRegisterWindow"));

    // LevelEditor should be fully ready at this point
    AttemptRegisterWindow();

    UE_LOG(LogTemp, Warning,
           TEXT("RegisterWindowWhenReady: Registration attempt completed "
                "[Event #%d]"),
           RegisterCallCount);
}

void FMusicDollUIModule::AttemptRegisterWindow() {
    if (!MusicDollWindow.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("AttemptRegisterWindow: MusicDollWindow is invalid!"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("AttemptRegisterWindow: Attempting to register window"));

    // Register the window - single attempt only
    // If it fails, it will not be retried
    MusicDollWindow->RegisterWindow();

    if (MusicDollWindow->IsRegistered()) {
        UE_LOG(LogTemp, Warning,
               TEXT("AttemptRegisterWindow: SUCCESS! Window registered"));
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("AttemptRegisterWindow: FAILED! Window registration unsuccessful. "
                    "This will not be retried."));
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMusicDollUIModule, MusicDollUI)
