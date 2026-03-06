#include "UI/FretDanceModulePropertiesPanel.h"

#include "FretDanceControlRigProcessor.h"
#include "FretDanceUnreal.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SFretDanceModulePropertiesPanel"

void SFretDanceModulePropertiesPanel::Construct(const FArguments& InArgs) {
	// 调用基类构造函数，使用基类的参数类型
	SModulePropertiesPanel::FArguments BaseArgs;
	SModulePropertiesPanel::Construct(BaseArgs);
}

void SFretDanceModulePropertiesPanel::SetActor(AActor* InActor) {
	FretDanceActor = Cast<AFretDanceUnreal>(InActor);
	RefreshProperties();
}

bool SFretDanceModulePropertiesPanel::CanHandleActor(
	const AActor* InActor) const {
	return InActor && InActor->IsA<AFretDanceUnreal>();
}

void SFretDanceModulePropertiesPanel::RefreshProperties() {
	CreatePropertyWidgets();
}

void SFretDanceModulePropertiesPanel::CreatePropertyWidgets() {
	auto Container = GetPropertyContainer();
	if (!Container.IsValid()) {
		return;
	}

	Container->ClearChildren();

	if (!FretDanceActor.IsValid()) {
		Container->AddSlot().AutoHeight().Padding(
			5.0f)[SNew(STextBlock)
					  .Text(LOCTEXT("NoActorSelected",
							"No FretDance Actor Selected"))
					  .ColorAndOpacity(FLinearColor::Yellow)];
		return;
	}

	AFretDanceUnreal* FretDance = FretDanceActor.Get();

	// Basic Configuration
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
		TEXT("Basic Configuration"))];

	Container->AddSlot().AutoHeight().Padding(
		5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
		TEXT("StringNumber"), FretDance->StringNumber, TEXT("StringNumber"),
		FSimpleDelegate())];

	// Instrument Configuration
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
		TEXT("Instrument Configuration"))];

	// Instrument type selection
	InstrumentTypeOptions.Empty();
	InstrumentTypeOptions.Add(MakeShareable(new FString(TEXT("Finger Style Guitar"))));
	InstrumentTypeOptions.Add(MakeShareable(new FString(TEXT("Electric Guitar"))));
	InstrumentTypeOptions.Add(MakeShareable(new FString(TEXT("Bass"))));

	int32 CurrentInstrumentIndex = (int32)FretDance->InstrumentType;
	TSharedPtr<FString> SelectedInstrument =
		(CurrentInstrumentIndex >= 0 &&
		 CurrentInstrumentIndex < InstrumentTypeOptions.Num())
			? InstrumentTypeOptions[CurrentInstrumentIndex]
			: InstrumentTypeOptions[0];

	Container->AddSlot().AutoHeight().Padding(
		5.0f)[SNew(SHorizontalBox) +
			SHorizontalBox::Slot().AutoWidth().Padding(
				5.0f)[SNew(STextBlock)
						.Text(FText::FromString(TEXT("Instrument Type")))
						.MinDesiredWidth(150.0f)] +
			SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
				[SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&InstrumentTypeOptions)
					.OnSelectionChanged_Lambda(
						[this](TSharedPtr<FString> SelectedOption,
							ESelectInfo::Type SelectInfo) {
							if (SelectedOption.IsValid() &&
								FretDanceActor.IsValid()) {
								int32 SelectedIndex =
									InstrumentTypeOptions.IndexOfByKey(
										SelectedOption);
								if (SelectedIndex != INDEX_NONE) {
									OnInstrumentTypeChanged(SelectedIndex);
								}
							}
						})
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
						return SNew(STextBlock)
							.Text(FText::FromString(*Item));
					})[SNew(STextBlock).Text_Lambda([SelectedInstrument]() {
						return FText::FromString(SelectedInstrument.IsValid()
							? *SelectedInstrument
							: TEXT("None"));
					})]]];

	// Instrument info display
	Container->AddSlot().AutoHeight().Padding(
		5.0f)[SNew(STextBlock)
				.Text_Lambda([FretDance]() {
					FString InstrumentTypeName;
					switch (FretDance->InstrumentType) {
						case EFretDanceInstrumentType::FINGER_STYLE_GUITAR:
							InstrumentTypeName = TEXT("Finger Style Guitar");
							break;
						case EFretDanceInstrumentType::ELECTRIC_GUITAR:
							InstrumentTypeName = TEXT("Electric Guitar");
							break;
						case EFretDanceInstrumentType::BASS:
							InstrumentTypeName = TEXT("Bass");
							break;
						default:
							InstrumentTypeName = TEXT("Unknown");
							break;
					}
					FString InfoStr = FString::Printf(
						TEXT("Instrument Type: %s"), *InstrumentTypeName);
					return FText::FromString(InfoStr);
				})
				.ColorAndOpacity(FLinearColor::Green)];

	// File Paths
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 15.0f, 5.0f,
		5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("File Paths"))];

	// IOFilePath comes from base class AInstrumentBase
	Container->AddSlot().AutoHeight().Padding(5.0f)
		[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
			TEXT("IO File Path"), FretDance->IOFilePath, TEXT("IOFilePath"),
			TEXT(".json"),
			[this](const FString& NewPath) {
				if (FretDanceActor.IsValid()) {
					FretDanceActor->Modify();
					FretDanceActor->IOFilePath = NewPath;
					UE_LOG(LogTemp, Warning, TEXT("FretDance: IO File Path updated to: %s"), *NewPath);
				}
			},
			true)];  // bAllowCreateNew = true

	// Initialization Operations
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 15.0f, 5.0f,
		5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Initialization"))];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SButton)
			 .Text(LOCTEXT("CheckObjectsStatusButton", "Check Objects Status"))
			 .OnClicked(this,
				&SFretDanceModulePropertiesPanel::OnCheckObjectsStatus)
			 .HAlign(HAlign_Center)
			 .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	Container->AddSlot().AutoHeight().Padding(
		5.0f)[SNew(SButton)
				  .Text(LOCTEXT("SetupAllObjectsButton", "Setup All Objects"))
				  .OnClicked(
					  this,
					  &SFretDanceModulePropertiesPanel::OnSetupAllObjects)
				  .HAlign(HAlign_Center)
				  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	// Import/Export
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 15.0f, 5.0f,
		5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Import/Export"))];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SButton)
			 .Text(LOCTEXT("ExportRecorderInfoButton", "Export Recorder Info"))
			 .OnClicked(this,
				&SFretDanceModulePropertiesPanel::OnExportRecorderInfo)
			 .HAlign(HAlign_Center)
			 .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SButton)
			 .Text(LOCTEXT("ImportRecorderInfoButton", "Import Recorder Info"))
			 .OnClicked(this,
				&SFretDanceModulePropertiesPanel::OnImportRecorderInfo)
			 .HAlign(HAlign_Center)
			 .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

void SFretDanceModulePropertiesPanel::OnNumericPropertyChanged(
	const FString& PropertyPath, int32 NewValue) {
	if (!FretDanceActor.IsValid()) {
		return;
	}

	AFretDanceUnreal* FretDance = FretDanceActor.Get();
	FretDance->Modify();

	if (PropertyPath == TEXT("StringNumber"))
		FretDance->StringNumber = NewValue;
}

void SFretDanceModulePropertiesPanel::OnFilePathChanged(
	const FString& PropertyPath, const FString& NewFilePath) {
	if (!FretDanceActor.IsValid()) {
		return;
	}

	AFretDanceUnreal* FretDance = FretDanceActor.Get();
	FretDance->Modify();

	if (PropertyPath == TEXT("IOFilePath"))
		FretDance->IOFilePath = NewFilePath;
}

void SFretDanceModulePropertiesPanel::OnInstrumentTypeChanged(
	int32 SelectedType) {
	if (!FretDanceActor.IsValid()) {
		return;
	}

	AFretDanceUnreal* FretDance = FretDanceActor.Get();
	FretDance->Modify();

	// 使用 SetInstrumentType 方法，会自动更新 GuideLines 等配置
	switch (SelectedType) {
		case 0:
			FretDance->SetInstrumentType(EFretDanceInstrumentType::FINGER_STYLE_GUITAR);
			break;
		case 1:
			FretDance->SetInstrumentType(EFretDanceInstrumentType::ELECTRIC_GUITAR);
			break;
		case 2:
			FretDance->SetInstrumentType(EFretDanceInstrumentType::BASS);
			break;
		default:
			break;
	}

	RefreshProperties();
}

FReply SFretDanceModulePropertiesPanel::OnCheckObjectsStatus() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for check objects status"));
		return FReply::Handled();
	}

	UFretDanceControlRigProcessor::CheckObjectsStatus(FretDanceActor.Get());
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Check Objects Status operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModulePropertiesPanel::OnSetupAllObjects() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for setup all objects"));
		return FReply::Handled();
	}

	UFretDanceControlRigProcessor::SetupAllObjects(FretDanceActor.Get());
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Setup All Objects operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModulePropertiesPanel::OnExportRecorderInfo() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for export recorder info"));
		return FReply::Handled();
	}

	if (FretDanceActor->IOFilePath.IsEmpty()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: IO file path is empty"));
		return FReply::Handled();
	}

	FretDanceActor->ExportRecorderInfo(FretDanceActor->IOFilePath);
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Export Recorder Info operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModulePropertiesPanel::OnImportRecorderInfo() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for import recorder info"));
		return FReply::Handled();
	}

	if (FretDanceActor->IOFilePath.IsEmpty()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: IO file path is empty"));
		return FReply::Handled();
	}

	FretDanceActor->ImportRecorderInfo(FretDanceActor->IOFilePath);
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Import Recorder Info operation triggered"));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
