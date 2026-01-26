#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SMusicDollMainPanel;

/**
 * Music Doll Dockable Window Manager
 * Manages the lifecycle and registration of the Music Doll dockable window in the editor
 */
class FMusicDollWindow
{
public:
    FMusicDollWindow();
    ~FMusicDollWindow();

    /** Register the window in the editor - can be called manually */
    void RegisterWindow();

    /** Unregister the window from the editor */
    void UnregisterWindow();

    /** Spawn the window */
    void SpawnWindow();

    /** Close the window */
    void CloseWindow();

    /** Check if the window is registered */
    bool IsRegistered() const { return bWindowRegistered; }

    /** Get the singleton instance */
    static TSharedPtr<FMusicDollWindow> GetInstance();

    /** Singleton instance - public for module to access */
    static TSharedPtr<FMusicDollWindow> GInstance;

private:
    /** Called when the window is spawned */
    TSharedRef<class SDockTab> SpawnMusicDollTab(const class FSpawnTabArgs& Args);

    /** Try to get TabManager - returns true if successful */
    bool TryGetTabManager(class FTabManager*& OutTabManager);

    /** Window/Tab related members */
    static const FName TabName;
    TWeakPtr<class SDockTab> MusicDollTab;
    TSharedPtr<SMusicDollMainPanel> MainPanel;

    /** Whether the window is registered */
    bool bWindowRegistered;

    /** Flag to prevent retry attempts after a failed registration */
    bool bRegistrationAttempted = false;
};
