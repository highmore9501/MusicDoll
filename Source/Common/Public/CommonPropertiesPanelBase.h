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
	 * Initialize the base panel with three tabs
	 * @param InPropertiesLabel Text for properties tab
	 * @param InOperationsLabel Text for operations tab
	 * @param InThirdTabLabel Text for third tab (e.g., "Bone Control Mapping")
	 */
	void InitializeTabPanel(const FText& InPropertiesLabel, const FText& InOperationsLabel, const FText& InThirdTabLabel);

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
	 * Set the content of the third tab container
	 * @param InContent Widget to display in third tab container
	 */
	void SetThirdTabContent(TSharedRef<SWidget> InContent);

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

	/**
	 * Switch to the third tab
	 */
	void ShowThirdTab();

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
	 * Get the third tab container for adding widgets
	 */
	TSharedPtr<SVerticalBox> GetThirdTabContainer() const { return ThirdTabContainer; }

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
		Operations,
		ThirdTab
	};

	EActiveTab ActiveTab = EActiveTab::Properties;

	// Main containers
	TSharedPtr<SVerticalBox> ContentContainer;
	TSharedPtr<SVerticalBox> PropertiesContainer;
	TSharedPtr<SVerticalBox> OperationsContainer;
	TSharedPtr<SVerticalBox> ThirdTabContainer;

	// Tab labels
	FText PropertiesTabLabel;
	FText OperationsTabLabel;
	FText ThirdTabLabel;

	// Tab callbacks
	FReply OnPropertiesTabClicked();
	FReply OnOperationsTabClicked();
	FReply OnThirdTabClicked();
	FLinearColor GetTabButtonTextColor(EActiveTab InTab) const;
};
