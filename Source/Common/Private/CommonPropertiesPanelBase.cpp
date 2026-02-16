#include "CommonPropertiesPanelBase.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/StyleColors.h"
#include "CommonPropertiesPanelUtility.h"

void SCommonPropertiesPanelBase::InitializeTabPanel(
	const FText& InPropertiesLabel,
	const FText& InOperationsLabel)
{
	PropertiesTabLabel = InPropertiesLabel;
	OperationsTabLabel = InOperationsLabel;
	ActiveTab = EActiveTab::Properties;

	ChildSlot
		[SNew(SVerticalBox)
			// Tab Buttons
			+ SVerticalBox::Slot().AutoHeight().Padding(5.0f)
				[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.5f, 0.0f)
						[SNew(SButton)
							.Text(InPropertiesLabel)
							.OnClicked(this, &SCommonPropertiesPanelBase::OnPropertiesTabClicked)
							.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
							.ForegroundColor_Lambda([this]() { 
								return GetTabButtonTextColor(true); 
							})]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.5f, 0.0f)
						[SNew(SButton)
							.Text(InOperationsLabel)
							.OnClicked(this, &SCommonPropertiesPanelBase::OnOperationsTabClicked)
							.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
							.ForegroundColor_Lambda([this]() { 
								return GetTabButtonTextColor(false); 
							})]]
			// Content Container
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(5.0f)
				[SAssignNew(ContentContainer, SVerticalBox)]];

	// Create properties container
	PropertiesContainer = SNew(SVerticalBox);
	OperationsContainer = SNew(SVerticalBox);

	// Set initial content to properties panel
	if (ContentContainer.IsValid())
	{
		ContentContainer->AddSlot().FillHeight(1.0f)
			[SNew(SScrollBox)
				+ SScrollBox::Slot()
					[PropertiesContainer.ToSharedRef()]];
	}
}

void SCommonPropertiesPanelBase::SetPropertiesContent(TSharedRef<SWidget> InContent)
{
	if (PropertiesContainer.IsValid())
	{
		PropertiesContainer->ClearChildren();
		PropertiesContainer->AddSlot().AutoHeight()
			[InContent];
	}
}

void SCommonPropertiesPanelBase::SetOperationsContent(TSharedRef<SWidget> InContent)
{
	if (OperationsContainer.IsValid())
	{
		OperationsContainer->ClearChildren();
		OperationsContainer->AddSlot().FillHeight(1.0f)
			[InContent];
	}
}

void SCommonPropertiesPanelBase::ShowPropertiesTab()
{
	if (ActiveTab == EActiveTab::Properties)
	{
		return;
	}

	ActiveTab = EActiveTab::Properties;

	if (ContentContainer.IsValid())
	{
		ContentContainer->ClearChildren();
		ContentContainer->AddSlot().FillHeight(1.0f)
			[SNew(SScrollBox)
				+ SScrollBox::Slot()
					[PropertiesContainer.ToSharedRef()]];
	}
}

void SCommonPropertiesPanelBase::ShowOperationsTab()
{
	if (ActiveTab == EActiveTab::Operations)
	{
		return;
	}

	ActiveTab = EActiveTab::Operations;

	if (ContentContainer.IsValid())
	{
		ContentContainer->ClearChildren();
		// 直接添加 OperationsContainer，因为它内部已经是 SScrollBox 的内容
		ContentContainer->AddSlot().FillHeight(1.0f)
			[OperationsContainer.ToSharedRef()];
	}
}

FReply SCommonPropertiesPanelBase::OnPropertiesTabClicked()
{
	ShowPropertiesTab();
	return FReply::Handled();
}

FReply SCommonPropertiesPanelBase::OnOperationsTabClicked()
{
	ShowOperationsTab();
	return FReply::Handled();
}

FLinearColor SCommonPropertiesPanelBase::GetTabButtonTextColor(bool bIsPropertiesTab) const
{
	bool bIsActive =
		(bIsPropertiesTab && ActiveTab == EActiveTab::Properties) ||
		(!bIsPropertiesTab && ActiveTab == EActiveTab::Operations);

	return FCommonPropertiesPanelUtility::GetTabButtonTextColor(bIsActive);
}
