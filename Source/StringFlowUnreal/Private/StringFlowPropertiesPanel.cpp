#include "StringFlowPropertiesPanel.h"

#include "StringFlowControlRigProcessor.h"
#include "StringFlowOperationsPanel.h"
#include "StringFlowUnreal.h"
#include "Details/SBoneControlMappingEditPanel.h"
#include "Misc/MessageDialog.h"
#include "CommonPropertiesPanelUtility.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SStringFlowPropertiesPanel"

void SStringFlowPropertiesPanel::Construct(const FArguments& InArgs)
{
	InitializeTabPanel(
		LOCTEXT("PropertiesTabLabel", "Properties"),
		LOCTEXT("OperationsTabLabel", "Operations"),
		LOCTEXT("BoneControlMappingTabLabel", "Bone Control Mapping")
	);

	// Create operations panel
	OperationsPanel = SNew(SStringFlowOperationsPanel);

	// Initialize the operations container with the operations panel
	if (OperationsPanel.IsValid())
	{
		SetOperationsContent(OperationsPanel.ToSharedRef());
	}

	// Create Bone Control Mapping panel
	BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);
	if (BoneControlMappingPanel.IsValid())
	{
		SetThirdTabContent(BoneControlMappingPanel.ToSharedRef());
	}

	RefreshPropertyList();
}

TSharedPtr<SWidget> SStringFlowPropertiesPanel::GetWidget()
{
	return AsShared();
}

void SStringFlowPropertiesPanel::SetActor(AActor* InActor)
{
	StringFlowActor = Cast<AStringFlowUnreal>(InActor);
	RefreshPropertyList();

	if (OperationsPanel.IsValid())
	{
		OperationsPanel->SetActor(InActor);
	}

	if (BoneControlMappingPanel.IsValid())
	{
		BoneControlMappingPanel->SetActor(InActor);
	}
}

bool SStringFlowPropertiesPanel::CanHandleActor(const AActor* InActor) const
{
	return InActor && InActor->IsA<AStringFlowUnreal>();
}

void SStringFlowPropertiesPanel::RefreshPropertyList()
{
	auto Container = GetPropertiesContainer();
	if (!Container.IsValid())
	{
		return;
	}

	Container->ClearChildren();

	if (!StringFlowActor.IsValid())
	{
		Container->AddSlot().AutoHeight().Padding(5.0f)
			[SNew(STextBlock)
				.Text(LOCTEXT("NoActorSelected", "No StringFlow Actor Selected"))
				.ColorAndOpacity(FLinearColor::Yellow)];
		return;
	}

	AStringFlowUnreal* StringFlow = StringFlowActor.Get();

	// Basic Configuration
	Container->AddSlot().AutoHeight().Padding(5.0f, 15.0f, 5.0f, 5.0f)
		[FCommonPropertiesPanelUtility::CreateSectionHeader(TEXT("Basic Configuration"))];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
			TEXT("OneHandFingerNumber"),
			StringFlow->OneHandFingerNumber,
			TEXT("OneHandFingerNumber"),
			FSimpleDelegate())];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
			TEXT("StringNumber"),
			StringFlow->StringNumber,
			TEXT("StringNumber"),
			FSimpleDelegate())];

	// File Paths
	Container->AddSlot().AutoHeight().Padding(5.0f, 15.0f, 5.0f, 5.0f)
		[FCommonPropertiesPanelUtility::CreateSectionHeader(TEXT("File Paths"))];

	TSharedPtr<SEditableTextBox> IOFilePathTextBox;
	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
				[SNew(STextBlock)
					.Text(FText::FromString(TEXT("IOFilePath")))
					.MinDesiredWidth(150.0f)]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
				[SAssignNew(IOFilePathTextBox, SEditableTextBox)
					.Text(FText::FromString(StringFlow->IOFilePath))
					.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type CommitType)
					{
						if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
						{
							if (StringFlowActor.IsValid())
							{
								StringFlowActor->IOFilePath = InText.ToString();
								StringFlowActor->Modify();
							}
						}
					})]
			+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[SNew(SButton)
					.Text(FText::FromString(TEXT("Browse")))
					.OnClicked_Lambda([this, IOFilePathTextBox]() -> FReply
					{
						if (!StringFlowActor.IsValid())
						{
							return FReply::Handled();
						}

						FString OutFilePath;
						if (FCommonPropertiesPanelUtility::BrowseForFile(TEXT(".violinist"), OutFilePath, true))
						{
							if (IOFilePathTextBox.IsValid())
							{
								IOFilePathTextBox->SetText(FText::FromString(OutFilePath));
								StringFlowActor->IOFilePath = OutFilePath;
								StringFlowActor->Modify();
							}
						}
						return FReply::Handled();
					})]];

	// Initialization Operations Section
	Container->AddSlot().AutoHeight().Padding(5.0f, 15.0f, 5.0f, 5.0f)
		[FCommonPropertiesPanelUtility::CreateSectionHeader(TEXT("Initialization"))];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SButton)
			.Text(LOCTEXT("CheckObjectsStatusButton", "Check Objects Status"))
			.OnClicked(this, &SStringFlowPropertiesPanel::OnCheckObjectsStatus)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SButton)
			.Text(LOCTEXT("SetupAllObjectsButton", "Setup All Objects"))
			.OnClicked(this, &SStringFlowPropertiesPanel::OnSetupAllObjects)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	// Import/Export Section
	Container->AddSlot().AutoHeight().Padding(5.0f, 15.0f, 5.0f, 5.0f)
		[FCommonPropertiesPanelUtility::CreateSectionHeader(TEXT("Import/Export"))];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SButton)
			.Text(LOCTEXT("ExportRecorderInfoButton", "Export Recorder Info"))
			.OnClicked(this, &SStringFlowPropertiesPanel::OnExportRecorderInfo)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SButton)
			.Text(LOCTEXT("ImportRecorderInfoButton", "Import Recorder Info"))
			.OnClicked(this, &SStringFlowPropertiesPanel::OnImportRecorderInfo)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

void SStringFlowPropertiesPanel::OnNumericPropertyChanged(
	const FString& PropertyPath, int32 NewValue)
{
	if (!StringFlowActor.IsValid())
	{
		return;
	}

	AStringFlowUnreal* StringFlow = StringFlowActor.Get();
	StringFlow->Modify();

	if (PropertyPath == TEXT("OneHandFingerNumber"))
		StringFlow->OneHandFingerNumber = NewValue;
	else if (PropertyPath == TEXT("StringNumber"))
	 StringFlow->StringNumber = NewValue;
}

void SStringFlowPropertiesPanel::OnFilePathChanged(const FString& PropertyPath,
	const FString& NewFilePath)
{
	if (!StringFlowActor.IsValid())
	{
		return;
	}

	AStringFlowUnreal* StringFlow = StringFlowActor.Get();
	StringFlow->Modify();

	if (PropertyPath == TEXT("IOFilePath"))
		StringFlow->IOFilePath = NewFilePath;
}

FReply SStringFlowPropertiesPanel::OnCheckObjectsStatus()
{
	if (!StringFlowActor.IsValid())
	{
		return FReply::Handled();
	}

	UStringFlowControlRigProcessor::CheckObjectsStatus(StringFlowActor.Get());
	return FReply::Handled();
}

FReply SStringFlowPropertiesPanel::OnSetupAllObjects()
{
	if (!StringFlowActor.IsValid())
	{
		return FReply::Handled();
	}

	UStringFlowControlRigProcessor::SetupAllObjects(StringFlowActor.Get());
	return FReply::Handled();
}

FReply SStringFlowPropertiesPanel::OnExportRecorderInfo()
{
	if (!StringFlowActor.IsValid())
	{
		return FReply::Handled();
	}

	EAppReturnType::Type UserConfirm = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::FromString(
			TEXT("Are you sure you want to export recorder information?\n\nThis will overwrite existing data.")));

	if (UserConfirm == EAppReturnType::Yes)
	{
		// 直接调用Actor的ExportRecorderInfo方法，而不是通过Processor
		StringFlowActor->ExportRecorderInfo(StringFlowActor->IOFilePath);
	}

	return FReply::Handled();
}

FReply SStringFlowPropertiesPanel::OnImportRecorderInfo()
{
	if (!StringFlowActor.IsValid())
	{
		return FReply::Handled();
	}

	EAppReturnType::Type UserConfirm = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::FromString(TEXT(
			"Are you sure you want to import recorder information?\n\nThis will overwrite existing actor properties.")));

	if (UserConfirm == EAppReturnType::Yes)
	{
		// 直接调用Actor的ImportRecorderInfo方法，而不是通过Processor
		StringFlowActor->ImportRecorderInfo(StringFlowActor->IOFilePath);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
