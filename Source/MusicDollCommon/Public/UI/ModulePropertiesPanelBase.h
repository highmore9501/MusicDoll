#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AActor;

/**
 * Base class for module property panels
 * Provides standardized interface for displaying and editing module properties
 */
class MUSICDOLLCOMMON_API SModulePropertiesPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SModulePropertiesPanel)
	{}
	SLATE_END_ARGS()

	/**
	 * Construct the property panel
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Set the actor to display properties for
	 */
	virtual void SetActor(AActor* InActor) {}

	/**
	 * Check if this panel can handle the given actor type
	 */
	virtual bool CanHandleActor(const AActor* InActor) const { return false; }

	/**
	 * Refresh/update the property display
	 */
	virtual void RefreshProperties() {}

protected:
	/**
	 * Create the property widgets and layout
	 * Override this in derived classes to define specific properties
	 */
	virtual void CreatePropertyWidgets() {}

	/**
	 * Get the container for property widgets
	 */
	TSharedPtr<SVerticalBox> GetPropertyContainer() const { return PropertyContainer; }

private:
	// Main container for properties
	TSharedPtr<SVerticalBox> PropertyContainer;
};