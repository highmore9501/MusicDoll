#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AActor;

/**
 * Base class for instrument properties panels
 * Provides common functionality for managing tabbed property displays
 */
class COMMON_API SCommonPropertiesPanelBase : public SCompoundWidget
{
public:
	/**
	 * Initialize the base panel with tab UI
	 * @param InPropertiesLabel Text for properties tab
	 * @param InOperationsLabel Text for operations tab
	 */
	void InitializeTabPanel(const FText& InPropertiesLabel, const FText& InOperationsLabel);

	/**
	 * Set the content of the properties container
	 * @param InContent Widget to display in properties container
	 */
	void SetPropertiesContent(TSharedRef<SWidget> InContent);

	/**
	 * Set the content of the operations container
	 * @param InContent Widget to display in operations container
	 */
	void SetOperationsContent(TSharedRef<SWidget> InContent);

	/**
	 * Refresh/update the property list display
	 * Override this in derived classes to populate property widgets
	 */
	virtual void RefreshPropertyList() {}

	/**
	 * Switch to the properties tab
	 */
	void ShowPropertiesTab();

	/**
	 * Switch to the operations tab
	 */
	void ShowOperationsTab();

protected:
	/**
	 * Get the properties container for adding property widgets
	 */
	TSharedPtr<SVerticalBox> GetPropertiesContainer() const { return PropertiesContainer; }

	/**
	 * Get the operations container for adding operation widgets
	 */
	TSharedPtr<SVerticalBox> GetOperationsContainer() const { return OperationsContainer; }

	/**
	 * Check if properties tab is currently active
	 */
	bool IsPropertiesTabActive() const { return ActiveTab == EActiveTab::Properties; }

	/**
	 * Check if operations tab is currently active
	 */
	bool IsOperationsTabActive() const { return ActiveTab == EActiveTab::Operations; }

private:
	// Tab management
	enum class EActiveTab
	{
		Properties,
		Operations
	};

	EActiveTab ActiveTab = EActiveTab::Properties;

	// Main containers
	TSharedPtr<SVerticalBox> ContentContainer;
	TSharedPtr<SVerticalBox> PropertiesContainer;
	TSharedPtr<SVerticalBox> OperationsContainer;

	// Tab labels
	FText PropertiesTabLabel;
	FText OperationsTabLabel;

	// Tab callbacks
	FReply OnPropertiesTabClicked();
	FReply OnOperationsTabClicked();
	FLinearColor GetTabButtonTextColor(bool bIsPropertiesTab) const;
};
