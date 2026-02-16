#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Interface for StringFlow display panels
 * Allows StringFlow module components to display and control their content in a unified way.
 * Can be used for both properties display and operation controls.
 */
class STRINGFLOWUNREAL_API IStringFlowDisplayPanel
{
public:
	virtual ~IStringFlowDisplayPanel() = default;

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
