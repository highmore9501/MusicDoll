#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AActor;

/**
 * Interface for module main panels
 * Defines the standard interface that all module main panels should implement
 * This creates a unified way to handle different modules in the MusicDoll UI system
 */
class MUSICDOLLCOMMON_API IModuleMainPanel
{
public:
	virtual ~IModuleMainPanel() = default;

	/**
	 * Get the Slate widget for this main panel
	 */
	virtual TSharedPtr<SWidget> GetWidget() = 0;

	/**
	 * Set the actor to display content for
	 * @param InActor The actor instance to display, or nullptr to clear
	 */
	virtual void SetActor(AActor* InActor) = 0;

	/**
	 * Check if this panel can handle the given actor type
	 */
	virtual bool CanHandleActor(const AActor* InActor) const = 0;

	/**
	 * Get the module name for display purposes
	 */
	virtual FString GetModuleName() const = 0;

	/**
	 * Refresh/update the panel content
	 */
	virtual void RefreshPanel() = 0;
};