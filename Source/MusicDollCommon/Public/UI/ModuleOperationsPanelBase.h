#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AActor;

/**
 * Base class for module operation panels
 * Provides standardized interface for displaying module operations and actions
 */
class MUSICDOLLCOMMON_API SModuleOperationsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SModuleOperationsPanel)
	{}
	SLATE_END_ARGS()

	/**
	 * Construct the operations panel
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Set the actor to perform operations on
	 */
	virtual void SetActor(AActor* InActor) {}

	/**
	 * Check if this panel can handle the given actor type
	 */
	virtual bool CanHandleActor(const AActor* InActor) const { return false; }

	/**
	 * Refresh/update the operations display
	 */
	virtual void RefreshOperations() {}

protected:
	/**
	 * Create the operation widgets and layout
	 * Override this in derived classes to define specific operations
	 */
	virtual void CreateOperationWidgets() {}

	/**
	 * Get the container for operation widgets
	 */
	TSharedPtr<SVerticalBox> GetOperationContainer() const { return OperationContainer; }

private:
	// Main container for operations
	TSharedPtr<SVerticalBox> OperationContainer;
};