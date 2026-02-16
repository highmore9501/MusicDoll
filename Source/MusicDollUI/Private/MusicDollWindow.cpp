#include "MusicDollWindow.h"

#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "MusicDollMainPanel.h"
#include "MusicDollStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "MusicDollWindow"

const FName FMusicDollWindow::TabName = FName(TEXT("MusicDollWindow"));

// Initialize singleton
TSharedPtr<FMusicDollWindow> FMusicDollWindow::GInstance = nullptr;

FMusicDollWindow::FMusicDollWindow()
    : bWindowRegistered(false), bRegistrationAttempted(false) {}

FMusicDollWindow::~FMusicDollWindow() { UnregisterWindow(); }

TSharedPtr<FMusicDollWindow> FMusicDollWindow::GetInstance() {
    return GInstance;
}

bool FMusicDollWindow::TryGetTabManager(FTabManager*& OutTabManager) {
    if (!FModuleManager::Get().IsModuleLoaded("LevelEditor")) {
        UE_LOG(LogTemp, Warning,
               TEXT("TryGetTabManager: LevelEditor module not loaded"));
        return false;
    }

    FLevelEditorModule* LevelEditorModule =
        FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
    if (!LevelEditorModule) {
        UE_LOG(
            LogTemp, Error,
            TEXT("TryGetTabManager: Failed to get LevelEditorModule pointer!"));
        return false;
    }

    TSharedPtr<FTabManager> TabManager =
        LevelEditorModule->GetLevelEditorTabManager();
    if (!TabManager.IsValid()) {
        UE_LOG(LogTemp, Warning,
               TEXT("TryGetTabManager: TabManager is not valid"));
        return false;
    }

    OutTabManager = TabManager.Get();
    return true;
}

void FMusicDollWindow::RegisterWindow() {
    static int32 RegisterCallCount = 0;
    RegisterCallCount++;

    if (bWindowRegistered) {
        UE_LOG(LogTemp, Warning,
               TEXT("RegisterWindow: Already registered, skipping "
                    "(bWindowRegistered = true) [Attempt #%d]"),
               RegisterCallCount);
        return;
    }

    // Critical: Only attempt registration once
    if (bRegistrationAttempted) {
        UE_LOG(LogTemp, Warning,
               TEXT("RegisterWindow: Registration already attempted, skipping "
                    "duplicate attempt [Attempt #%d]"),
               RegisterCallCount);
        return;
    }

    bRegistrationAttempted = true;

    FLevelEditorModule* LevelEditorModule =
        FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");

    if (!LevelEditorModule) {
        UE_LOG(LogTemp, Error,
               TEXT("RegisterWindow: Failed to get LevelEditorModule [Attempt "
                    "#%d]"),
               RegisterCallCount);
        return;
    }

    TSharedPtr<FTabManager> TabManager =
        LevelEditorModule->GetLevelEditorTabManager();

    if (!TabManager.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("RegisterWindow: TabManager is invalid [Attempt #%d]"),
               RegisterCallCount);
        return;
    }

    // Register the tab spawner in the LevelEditor group
    TabManager
        ->RegisterTabSpawner(
            TabName,
            FOnSpawnTab::CreateRaw(this, &FMusicDollWindow::SpawnMusicDollTab))
        .SetDisplayName(LOCTEXT("MusicDollTabLabel", "Music Doll"))
        .SetTooltipText(
            LOCTEXT("MusicDollTabTooltip", "Open Music Doll editing panel"))
        .SetIcon(FSlateIcon(FMusicDollStyle::GetStyleSetName(), "MusicDoll.Icon"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory());

    bWindowRegistered = true;

    UE_LOG(LogTemp, Warning,
           TEXT("RegisterWindow: Tab spawner registered successfully in "
                "LevelEditor group [Attempt #%d]"),
           RegisterCallCount);
}

void FMusicDollWindow::UnregisterWindow() {
    if (!bWindowRegistered) {
        UE_LOG(
            LogTemp, Log,
            TEXT("UnregisterWindow: Not registered, skipping unregistration"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("UnregisterWindow: Starting unregistration process"));

    // Close the tab if it's open
    CloseWindow();

    // Unregister the tab spawner
    FLevelEditorModule* LevelEditorModule =
        FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
    if (LevelEditorModule) {
        TSharedPtr<FTabManager> TabManager =
            LevelEditorModule->GetLevelEditorTabManager();
        if (TabManager.IsValid()) {
            // Double-check if the spawner exists before trying to unregister
            if (TabManager->HasTabSpawner(TabName)) {
                TabManager->UnregisterTabSpawner(TabName);
                UE_LOG(LogTemp, Warning,
                       TEXT("UnregisterWindow: TabSpawner unregistered "
                            "successfully"));
            } else {
                UE_LOG(LogTemp, Log,
                       TEXT("UnregisterWindow: TabSpawner not found in "
                            "TabManager (already unregistered?)"));
            }
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("UnregisterWindow: TabManager is not valid, cannot "
                        "unregister"));
        }
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("UnregisterWindow: LevelEditorModule not found, cannot "
                    "unregister"));
    }

    bWindowRegistered = false;
    UE_LOG(LogTemp, Warning,
           TEXT("UnregisterWindow: Unregistration complete (bWindowRegistered "
                "now = false)"));
}

void FMusicDollWindow::SpawnWindow() {
    FLevelEditorModule& LevelEditorModule =
        FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    TSharedPtr<FTabManager> TabManager =
        LevelEditorModule.GetLevelEditorTabManager();

    if (TabManager.IsValid()) {
        TabManager->TryInvokeTab(TabName);
        UE_LOG(LogTemp, Warning,
               TEXT("SpawnWindow: Tab spawned successfully!"));
    } else {
        UE_LOG(LogTemp, Error, TEXT("SpawnWindow: TabManager is not valid!"));
    }
}

void FMusicDollWindow::CloseWindow() {
    UE_LOG(LogTemp, Log, TEXT("CloseWindow: Attempting to close window"));

    TSharedPtr<SDockTab> Tab = MusicDollTab.Pin();
    if (Tab.IsValid()) {
        UE_LOG(LogTemp, Log, TEXT("CloseWindow: Tab found, requesting close"));
        Tab->RequestCloseTab();
    } else {
        UE_LOG(LogTemp, Log, TEXT("CloseWindow: Tab is not active"));
    }
}

TSharedRef<SDockTab> FMusicDollWindow::SpawnMusicDollTab(
    const FSpawnTabArgs& Args) {
    // Create the main panel if not already created
    if (!MainPanel.IsValid()) {
        UE_LOG(LogTemp, Log,
               TEXT("SpawnMusicDollTab: Creating new SMusicDollMainPanel"));
        MainPanel = SNew(SMusicDollMainPanel);
    }

    // Create and store the dock tab
    TSharedRef<SDockTab> DockTab =
        SNew(SDockTab)
            .TabRole(ETabRole::PanelTab)
            .Label(LOCTEXT("MusicDollWindowTitle",
                           "Music Doll"))[MainPanel.ToSharedRef()];

    MusicDollTab = DockTab;

    UE_LOG(LogTemp, Warning,
           TEXT("SpawnMusicDollTab: Tab created and stored successfully!"));
    return DockTab;
}

#undef LOCTEXT_NAMESPACE
