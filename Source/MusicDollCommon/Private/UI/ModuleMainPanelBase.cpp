#include "UI/ModuleMainPanelBase.h"

#include "Styling/StyleColors.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SModuleMainPanelBase::InitializeModulePanel(
    const FString& InModuleName, const FText& InPropertiesLabel,
    const FText& InOperationsLabel) {
    ModuleName = InModuleName;

    // Clear any existing panels
    ClearPanels();

    // Create main layout
    ChildSlot[SNew(SVerticalBox)
              // Tab Buttons container
              + SVerticalBox::Slot().AutoHeight().Padding(
                    5.0f)[SAssignNew(TabButtonsContainer, SHorizontalBox)]
              // Content Container
              + SVerticalBox::Slot().FillHeight(1.0f).Padding(
                    5.0f)[SAssignNew(ContentContainer, SVerticalBox)]];

    // Set initial active tab
    ActiveTab = 0;
}

void SModuleMainPanelBase::InitializeModulePanel(
    const FString& InModuleName, const FText& InPropertiesLabel,
    const FText& InOperationsLabel, const FText& InBoneControlMappingLabel) {
    InitializeModulePanel(InModuleName, InPropertiesLabel, InOperationsLabel);

    // Add bone control mapping tab
    // This will be handled by RegisterPanel in derived classes
}

void SModuleMainPanelBase::InitializeModulePanel(
    const FString& InModuleName, const FText& InPropertiesLabel,
    const FText& InOperationsLabel, const FText& InBoneControlMappingLabel,
    const FText& InFourthTabLabel) {
    InitializeModulePanel(InModuleName, InPropertiesLabel, InOperationsLabel);

    // Add fourth tab
    // This will be handled by RegisterPanel in derived classes
}

void SModuleMainPanelBase::RegisterPanel(TSharedPtr<SWidget> InPanel,
                                         const FText& InTabLabel) {
    if (!InPanel.IsValid()) {
        return;
    }

    // Create container for this panel
    TSharedPtr<SVerticalBox> Container = SNew(SVerticalBox);
    Container->AddSlot().FillHeight(1.0f)[InPanel.ToSharedRef()];

    // Store panel info
    FRegisteredPanel NewPanel;
    NewPanel.PanelWidget = InPanel;
    NewPanel.TabLabel = InTabLabel;
    NewPanel.Container = Container;
    RegisteredPanels.Add(NewPanel);

    // Create tab button
    int32 PanelIndex = RegisteredPanels.Num() - 1;
    TSharedPtr<SButton> TabButton =
        SNew(SButton)
            .Text(InTabLabel)
            .OnClicked(this, &SModuleMainPanelBase::OnTabClicked, PanelIndex)
            .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
            .ForegroundColor_Lambda([this, PanelIndex]() {
                return GetTabButtonTextColor(PanelIndex);
            });

    // Add button to tab container
    if (TabButtonsContainer.IsValid()) {
        TabButtonsContainer->AddSlot().FillWidth(1.0f).Padding(
            2.5f, 0.0f)[TabButton.ToSharedRef()];
    }
}

void SModuleMainPanelBase::ClearPanels() {
    RegisteredPanels.Empty();
    ActiveTab = 0;

    if (TabButtonsContainer.IsValid()) {
        TabButtonsContainer->ClearChildren();
    }

    if (ContentContainer.IsValid()) {
        ContentContainer->ClearChildren();
    }
}

TSharedPtr<SVerticalBox> SModuleMainPanelBase::GetPanelContainer(
    int32 Index) const {
    if (RegisteredPanels.IsValidIndex(Index)) {
        return RegisteredPanels[Index].Container;
    }
    return nullptr;
}

void SModuleMainPanelBase::ShowPanel(int32 PanelIndex, bool bForceRefresh) {
    if (!RegisteredPanels.IsValidIndex(PanelIndex)) {
        return;
    }

    // Only skip if not forcing refresh and already active
    if (!bForceRefresh && ActiveTab == PanelIndex) {
        return;
    }

    ActiveTab = PanelIndex;

    if (ContentContainer.IsValid() &&
        RegisteredPanels[PanelIndex].Container.IsValid()) {
        ContentContainer->ClearChildren();
        ContentContainer->AddSlot().FillHeight(
            1.0f)[SNew(SScrollBox) +
                  SScrollBox::Slot()[RegisteredPanels[PanelIndex]
                                         .Container.ToSharedRef()]];
    }
}

void SModuleMainPanelBase::ShowFirstPanel() {
    if (RegisteredPanels.Num() > 0 && ContentContainer.IsValid()) {
        ShowPanel(0, true);  // Force refresh when showing first panel
    }
}

TSharedPtr<SWidget> SModuleMainPanelBase::GetWidget() { return AsShared(); }

FReply SModuleMainPanelBase::OnTabClicked(int32 TabIndex) {
    ShowPanel(TabIndex, false);
    return FReply::Handled();
}

FLinearColor SModuleMainPanelBase::GetTabButtonTextColor(int32 TabIndex) const {
    bool bIsActive = (ActiveTab == TabIndex);
    return FCommonPanelUtility::GetTabButtonTextColor(bIsActive);
}