#include "UI/StringFlowModulePropertiesPanel.h"

#include "StringFlowControlRigProcessor.h"
#include "StringFlowUnreal.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SStringFlowModulePropertiesPanel"

void SStringFlowModulePropertiesPanel::Construct(const FArguments& InArgs) {
    // 调用基类构造函数，使用基类的参数类型
    SModulePropertiesPanel::FArguments BaseArgs;
    SModulePropertiesPanel::Construct(BaseArgs);
}

void SStringFlowModulePropertiesPanel::SetActor(AActor* InActor) {
    StringFlowActor = Cast<AStringFlowUnreal>(InActor);
    RefreshProperties();
}

bool SStringFlowModulePropertiesPanel::CanHandleActor(
    const AActor* InActor) const {
    return InActor && InActor->IsA<AStringFlowUnreal>();
}

void SStringFlowModulePropertiesPanel::RefreshProperties() {
    CreatePropertyWidgets();
}

void SStringFlowModulePropertiesPanel::CreatePropertyWidgets() {
    auto Container = GetPropertyContainer();
    if (!Container.IsValid()) {
        return;
    }

    Container->ClearChildren();

    if (!StringFlowActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActorSelected",
                                    "No StringFlow Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AStringFlowUnreal* StringFlow = StringFlowActor.Get();

    // Basic Configuration
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Basic Configuration"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("OneHandFingerNumber"), StringFlow->OneHandFingerNumber,
        TEXT("OneHandFingerNumber"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("StringNumber"), StringFlow->StringNumber, TEXT("StringNumber"),
        FSimpleDelegate())];

    // Instrument Configuration
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Instrument Configuration"))];

    // Instrument type selection
    InstrumentTypeOptions.Empty();
    InstrumentTypeOptions.Add(MakeShareable(new FString(TEXT("Violin"))));
    InstrumentTypeOptions.Add(MakeShareable(new FString(TEXT("Viola"))));
    InstrumentTypeOptions.Add(MakeShareable(new FString(TEXT("Cello"))));
    InstrumentTypeOptions.Add(MakeShareable(new FString(TEXT("Custom"))));

    int32 CurrentInstrumentIndex =
        (int32)StringFlow->CurrentInstrumentConfig.InstrumentType;
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
                                   StringFlowActor.IsValid()) {
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
                  .Text_Lambda([StringFlow]() {
                      FString InfoStr = FString::Printf(
                          TEXT("Instrument: %s | String Notes: %d, %d, %d, %d"),
                          *StringFlow->CurrentInstrumentConfig
                               .GetInstrumentName(),
                          StringFlow->CurrentInstrumentConfig.GetStringNote(0),
                          StringFlow->CurrentInstrumentConfig.GetStringNote(1),
                          StringFlow->CurrentInstrumentConfig.GetStringNote(2),
                          StringFlow->CurrentInstrumentConfig.GetStringNote(3));
                      return FText::FromString(InfoStr);
                  })
                  .ColorAndOpacity(FLinearColor::Green)];

    // Custom string notes input (only shown in CUSTOM mode)
    if (StringFlow->CurrentInstrumentConfig.InstrumentType ==
        EStringFlowInstrumentType::CUSTOM) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f, 10.0f, 5.0f,
            5.0f)[SNew(STextBlock)
                      .Text(FText::FromString(TEXT("Custom String Notes")))
                      .Font(FAppStyle::Get().GetFontStyle(
                          "PropertyWindow.NormalFont.Bold"))];

        for (int32 i = 0; i < 4; ++i) {
            FString PropertyName = FString::Printf(TEXT("String%dNote"), i);
            Container->AddSlot().AutoHeight().Padding(
                5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
                PropertyName,
                StringFlow->CurrentInstrumentConfig.StringNotes[i],
                PropertyName, FSimpleDelegate())];
        }
    }

    // File Paths
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("File Paths"))];

    // IOFilePath comes from base class AInstrumentBase
    // 直接创建文件路径编辑UI，确保路径能被正确保存
    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT("IO File Path"), StringFlow->IOFilePath, TEXT("IOFilePath"),
        TEXT(".violinist"),
        [this](const FString& NewPath) {
            if (StringFlowActor.IsValid()) {
                StringFlowActor->Modify();
                StringFlowActor->IOFilePath = NewPath;
                UE_LOG(LogTemp, Warning,
                       TEXT("StringFlow: IO File Path updated to: %s"),
                       *NewPath);
            }
        },
        true)];  // bAllowCreateNew = true

    // Initialization Operations
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Initialization"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("CheckObjectsStatusButton", "Check Player Control Rig Status"))
             .OnClicked(this,
                        &SStringFlowModulePropertiesPanel::OnCheckObjectsStatus)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SetupAllObjectsButton", "Setup Player Control Rig"))
                  .OnClicked(
                      this,
                      &SStringFlowModulePropertiesPanel::OnSetupAllObjects)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Import/Export
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Import/Export"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("ExportRecorderInfoButton", "Export Player Info"))
             .OnClicked(this,
                        &SStringFlowModulePropertiesPanel::OnExportRecorderInfo)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("ImportRecorderInfoButton", "Import Player Info"))
             .OnClicked(this,
                        &SStringFlowModulePropertiesPanel::OnImportRecorderInfo)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

void SStringFlowModulePropertiesPanel::OnNumericPropertyChanged(
    const FString& PropertyPath, int32 NewValue) {
    if (!StringFlowActor.IsValid()) {
        return;
    }

    AStringFlowUnreal* StringFlow = StringFlowActor.Get();
    StringFlow->Modify();

    if (PropertyPath == TEXT("OneHandFingerNumber"))
        StringFlow->OneHandFingerNumber = NewValue;
    else if (PropertyPath == TEXT("StringNumber"))
        StringFlow->StringNumber = NewValue;
}

void SStringFlowModulePropertiesPanel::OnFilePathChanged(
    const FString& PropertyPath, const FString& NewFilePath) {
    if (!StringFlowActor.IsValid()) {
        return;
    }

    AStringFlowUnreal* StringFlow = StringFlowActor.Get();
    StringFlow->Modify();

    if (PropertyPath == TEXT("IOFilePath"))
        StringFlow->IOFilePath = NewFilePath;
}

void SStringFlowModulePropertiesPanel::OnInstrumentTypeChanged(
    int32 SelectedType) {
    if (!StringFlowActor.IsValid()) {
        return;
    }

    StringFlowActor->Modify();

    switch (SelectedType) {
        case 0:
            StringFlowActor->SetInstrumentToViolin();
            break;
        case 1:
            StringFlowActor->SetInstrumentToViola();
            break;
        case 2:
            StringFlowActor->SetInstrumentToCello();
            break;
        case 3:
            StringFlowActor->CurrentInstrumentConfig.InstrumentType =
                EStringFlowInstrumentType::CUSTOM;
            break;
        default:
            break;
    }

    RefreshProperties();
}

FReply SStringFlowModulePropertiesPanel::OnCheckObjectsStatus() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlow: No actor selected for check objects status"));
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::CheckObjectsStatus(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("StringFlow: Check Objects Status operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModulePropertiesPanel::OnSetupAllObjects() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlow: No actor selected for setup player control rig"));
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::SetupAllObjects(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("StringFlow: Setup Player Control Rig operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModulePropertiesPanel::OnExportRecorderInfo() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlow: No actor selected for export recorder info"));
        return FReply::Handled();
    }

    if (StringFlowActor->IOFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: IO file path is empty"));
        return FReply::Handled();
    }

    StringFlowActor->ExportRecorderInfo(StringFlowActor->IOFilePath);
    UE_LOG(LogTemp, Warning,
           TEXT("StringFlow: Export Recorder Info operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModulePropertiesPanel::OnImportRecorderInfo() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlow: No actor selected for import recorder info"));
        return FReply::Handled();
    }

    if (StringFlowActor->IOFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: IO file path is empty"));
        return FReply::Handled();
    }

    StringFlowActor->ImportRecorderInfo(StringFlowActor->IOFilePath);
    UE_LOG(LogTemp, Warning,
           TEXT("StringFlow: Import Recorder Info operation triggered"));
    return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE