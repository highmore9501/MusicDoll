#pragma once

#include "CoreMinimal.h"
#include "ModuleMainPanelInterface.h"
#include "Widgets/SCompoundWidget.h"

class AActor;

/**
 * Base class for module main panels
 * Provides common functionality for managing tabbed module displays
 * This replaces the misleadingly named CommonPropertiesPanelBase
 */
class MUSICDOLLCOMMON_API SModuleMainPanelBase : public SCompoundWidget, public IModuleMainPanel
{
public:
	/**
	 * Initialize the module main panel with basic setup
	 * @param InModuleName Name of the module for display
	 * @param InPropertiesLabel Placeholder (not used in new system)
	 * @param InOperationsLabel Placeholder (not used in new system)
	 */
	void InitializeModulePanel(const FString& InModuleName, const FText& InPropertiesLabel, const FText& InOperationsLabel);

	/**
	 * Initialize the module main panel with three tabs setup
	 * @param InModuleName Name of the module for display
	 * @param InPropertiesLabel Placeholder (not used in new system)
	 * @param InOperationsLabel Placeholder (not used in new system)
	 * @param InBoneControlMappingLabel Placeholder (not used in new system)
	 */
	void InitializeModulePanel(const FString& InModuleName, const FText& InPropertiesLabel, const FText& InOperationsLabel, const FText& InBoneControlMappingLabel);

	/**
	 * Initialize the module main panel with four tabs setup
	 * @param InModuleName Name of the module for display
	 * @param InPropertiesLabel Placeholder (not used in new system)
	 * @param InOperationsLabel Placeholder (not used in new system)
	 * @param InBoneControlMappingLabel Placeholder (not used in new system)
	 * @param InFourthTabLabel Placeholder (not used in new system)
	 */
	void InitializeModulePanel(const FString& InModuleName, const FText& InPropertiesLabel, const FText& InOperationsLabel, const FText& InBoneControlMappingLabel, const FText& InFourthTabLabel);

	/**
	 * Refresh/update the panel display
	 * Override this in derived classes to update content
	 */
	virtual void RefreshPanel() override {}

	// IModuleMainPanel interface
	virtual TSharedPtr<SWidget> GetWidget() override;
	virtual void SetActor(AActor* InActor) override {}
	virtual bool CanHandleActor(const AActor* InActor) const override { return false; }
	virtual FString GetModuleName() const override { return ModuleName; }

protected:
	/**
	 * Register a panel with the main panel
	 * @param InPanel The panel to register
	 * @param InTabLabel The label to display on the tab button
	 */
	void RegisterPanel(TSharedPtr<SWidget> InPanel, const FText& InTabLabel);

	/**
	 * Clear all registered panels
	 */
	void ClearPanels();

	/**
	 * Show a specific panel by index
	 * @param PanelIndex The index of the panel to show
	 */
	void ShowPanel(int32 PanelIndex);

	/**
	 * Show the first registered panel
	 */
	void ShowFirstPanel();

	/**
	 * Get the number of registered panels
	 */
	int32 GetPanelCount() const { return RegisteredPanels.Num(); }

	/**
	 * Get panel container for a specific index
	 */
	TSharedPtr<SVerticalBox> GetPanelContainer(int32 Index) const;

private:
	// Registered panels management
	struct FRegisteredPanel
	{
		TSharedPtr<SWidget> PanelWidget;
		FText TabLabel;
		TSharedPtr<SVerticalBox> Container;
	};

	TArray<FRegisteredPanel> RegisteredPanels;
	TSharedPtr<SHorizontalBox> TabButtonsContainer;

	/**
	 * Check if properties tab is currently active
	 */
	bool IsPropertiesTabActive() const { return ActiveTab == 0 && RegisteredPanels.IsValidIndex(0); }

	/**
	 * Check if operations tab is currently active
	 */
	bool IsOperationsTabActive() const { return ActiveTab == 1 && RegisteredPanels.IsValidIndex(1); }

	/**
	 * Check if bone control mapping tab is currently active
	 */
	bool IsBoneControlMappingTabActive() const { return ActiveTab == 2 && RegisteredPanels.IsValidIndex(2); }

	/**
	 * Check if fourth tab is currently active
	 */
	bool IsFourthTabActive() const { return ActiveTab == 3 && RegisteredPanels.IsValidIndex(3); }

private:
	// Tab management
	int32 ActiveTab = 0;
	FString ModuleName;

	// Main containers
	TSharedPtr<SVerticalBox> ContentContainer;

	// Tab callbacks
	FReply OnTabClicked(int32 TabIndex);
	FLinearColor GetTabButtonTextColor(int32 TabIndex) const;
};