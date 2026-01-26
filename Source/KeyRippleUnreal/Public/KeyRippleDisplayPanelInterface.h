#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Interface for KeyRipple display panels
 * Allows KeyRipple module components to display and control their content in a unified way.
 * Can be used for both properties display and operation controls.
 */
class KEYRIPPLEUNREAL_API IKeyRippleDisplayPanel
{
public:
	virtual ~IKeyRippleDisplayPanel() = default;

	/**
	 * Get the Slate widget for this display panel
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
};
