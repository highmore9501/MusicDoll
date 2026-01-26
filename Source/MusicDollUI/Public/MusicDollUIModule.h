#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMusicDollWindow;

/**
 * Music Doll UI Module
 * Provides dockable windows for Music Doll editing functionality
 * independent of Edit Mode system. Can be reused by different instrument plugins.
 */
class FMusicDollUIModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** Attempt to register the window */
    void AttemptRegisterWindow();

    /** Called when LevelEditor is ready - accepts the TSharedPtr parameter */
    void RegisterWindowWhenReady(TSharedPtr<class ILevelEditor> InLevelEditor);

    /** Get the module instance */
    static FMusicDollUIModule* GetInstance();

private:
    /** Window manager */
    TSharedPtr<FMusicDollWindow> MusicDollWindow;

    /** Static instance pointer */
    static FMusicDollUIModule* GInstance;

    /** Counter to track how many times RegisterWindowWhenReady is called */
    int32 RegisterCallCount = 0;

    /** Handle to the OnLevelEditorCreated delegate so we can disconnect it */
    FDelegateHandle OnLevelEditorCreatedHandle;

    /** Flag to prevent redundant startup initialization */
    bool bStartupComplete = false;

    /** Flag to prevent RegisterWindowWhenReady from executing multiple times */
    bool bWindowRegistrationAttempted = false;
};
