#include "CommonPropertiesPanelUtility.h"

#include "DesktopPlatformModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"

TSharedRef<SWidget> FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
	const FString& PropertyName,
	int32 Value,
	const FString& PropertyPath,
	FSimpleDelegate OnValueChanged)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
			[SNew(STextBlock)
				.Text(FText::FromString(PropertyName))
				.MinDesiredWidth(150.0f)]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
			[SNew(SSpinBox<int32>)
				.Value(Value)
				.MinValue(-10000)
				.MaxValue(10000)
				.Delta(1)];
}

TSharedRef<SWidget> FCommonPropertiesPanelUtility::CreateStringPropertyRow(
	const FString& PropertyName,
	const FString& Value,
	const FString& PropertyPath,
	FSimpleDelegate OnValueChanged)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
			[SNew(STextBlock)
				.Text(FText::FromString(PropertyName))
				.MinDesiredWidth(150.0f)]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
			[SNew(SEditableTextBox)
				.Text(FText::FromString(Value))];
}

TSharedRef<SWidget> FCommonPropertiesPanelUtility::CreateFilePathPropertyRow(
	const FString& PropertyName,
	const FString& FilePath,
	const FString& PropertyPath,
	const FString& FileExtension,
	FSimpleDelegate OnPathChanged,
	bool bAllowCreateNew)
{
	TSharedPtr<SEditableTextBox> TextBox;

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
			[SNew(STextBlock)
				.Text(FText::FromString(PropertyName))
				.MinDesiredWidth(150.0f)]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
			[SAssignNew(TextBox, SEditableTextBox)
				.Text(FText::FromString(FilePath))
				.OnTextCommitted_Lambda([PropertyPath, OnPathChanged](const FText& InText, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
					{
						OnPathChanged.ExecuteIfBound();
					}
				})]
		+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[SNew(SButton)
				.Text(FText::FromString(TEXT("Browse")))
				.OnClicked_Lambda([PropertyPath, FileExtension, OnPathChanged, TextBox]() -> FReply
				{
					FString OutFilePath;
					if (BrowseForFile(FileExtension, OutFilePath, false))
					{
						if (TextBox.IsValid())
						{
							TextBox->SetText(FText::FromString(OutFilePath));
						}
						OnPathChanged.ExecuteIfBound();
					}
					return FReply::Handled();
				})];
}

TSharedRef<SWidget> FCommonPropertiesPanelUtility::CreateVector3PropertyRow(
	const FString& PropertyName,
	const FVector& Value,
	const FString& PropertyPath,
	FSimpleDelegate OnComponentChanged)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
			[SNew(STextBlock)
				.Text(FText::FromString(PropertyName))
				.MinDesiredWidth(150.0f)]
		+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f)
			[SNew(SSpinBox<float>)
				.Value(Value.X)
				.MinValue(-10000.f)
				.MaxValue(10000.f)
				.Delta(0.01f)]
		+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f)
			[SNew(SSpinBox<float>)
				.Value(Value.Y)
				.MinValue(-10000.f)
				.MaxValue(10000.f)
				.Delta(0.01f)]
		+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f)
			[SNew(SSpinBox<float>)
				.Value(Value.Z)
				.MinValue(-10000.f)
				.MaxValue(10000.f)
				.Delta(0.01f)];
}

TSharedRef<SWidget> FCommonPropertiesPanelUtility::CreateSectionHeader(const FString& SectionTitle)
{
	return SNew(STextBlock)
		.Text(FText::FromString(SectionTitle))
		.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"));
}

TSharedRef<SWidget> FCommonPropertiesPanelUtility::CreateActionButton(
	const FText& ButtonText,
	FSimpleDelegate OnClicked)
{
	return SNew(SButton)
		.Text(ButtonText)
		.HAlign(HAlign_Center)
		.ButtonStyle(FAppStyle::Get(), "FlatButton.Default");
}

bool FCommonPropertiesPanelUtility::BrowseForFile(
	const FString& FileExtension,
	FString& OutFilePath,
	bool bAllowCreateNew)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return false;
	}

	FString FileFilter =
		FString::Printf(TEXT("Files (*%s)|*%s|All Files (*.*)|*.*"),
			*FileExtension, *FileExtension);
	FString DefaultPath = FPaths::ProjectDir();

	TArray<FString> OutFilenames;
	bool bOpened = false;

	if (bAllowCreateNew)
	{
		bOpened = DesktopPlatform->SaveFileDialog(
			nullptr,
			FString::Printf(TEXT("Select or Create %s File"), *FileExtension),
			DefaultPath, TEXT(""), FileFilter, EFileDialogFlags::None,
			OutFilenames);
	}
	else
	{
		bOpened = DesktopPlatform->OpenFileDialog(
			nullptr, FString::Printf(TEXT("Select %s File"), *FileExtension),
			DefaultPath, TEXT(""), FileFilter, EFileDialogFlags::None,
			OutFilenames);
	}

	if (bOpened && OutFilenames.Num() > 0)
	{
		OutFilePath = OutFilenames[0];
		return true;
	}

	return false;
}

FLinearColor FCommonPropertiesPanelUtility::GetTabButtonTextColor(bool bIsActive)
{
	if (bIsActive)
	{
		return FLinearColor(0.0f, 112.0f / 255.0f, 220.0f / 255.0f, 1.0f);
	}
	else
	{
		return FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
	}
}

TSharedRef<SWidget> FCommonPropertiesPanelUtility::CreateTabButtons(
	const FText& PropertiesLabel,
	const FText& OperationsLabel,
	FSimpleDelegate OnPropertiesClicked,
	FSimpleDelegate OnOperationsClicked,
	bool bIsPropertiesActive)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.5f, 0.0f)
			[SNew(SButton)
				.Text(PropertiesLabel)
				.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
				.ForegroundColor_Lambda([bIsPropertiesActive]() {
					return GetTabButtonTextColor(bIsPropertiesActive);
				})]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.5f, 0.0f)
			[SNew(SButton)
				.Text(OperationsLabel)
				.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
				.ForegroundColor_Lambda([bIsPropertiesActive]() {
					return GetTabButtonTextColor(!bIsPropertiesActive);
				})];
}
