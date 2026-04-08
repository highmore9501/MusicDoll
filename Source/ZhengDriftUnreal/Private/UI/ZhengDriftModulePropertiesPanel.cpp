#include "UI/ZhengDriftModulePropertiesPanel.h"

#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ZhengDriftControlRigProcessor.h"
#include "ZhengDriftMusicInstrumentProcessor.h"

#define LOCTEXT_NAMESPACE "SZhengDriftModulePropertiesPanel"

void SZhengDriftModulePropertiesPanel::Construct(const FArguments& InArgs) {
    SModulePropertiesPanel::FArguments BaseArgs;
    SModulePropertiesPanel::Construct(BaseArgs);
}

void SZhengDriftModulePropertiesPanel::SetActor(AActor* InActor) {
    ZhengDriftActor = Cast<AZhengDriftUnreal>(InActor);
    RefreshProperties();
}

bool SZhengDriftModulePropertiesPanel::CanHandleActor(
    const AActor* InActor) const {
    return InActor && InActor->IsA<AZhengDriftUnreal>();
}

void SZhengDriftModulePropertiesPanel::RefreshProperties() {
    CreatePropertyWidgets();
}

void SZhengDriftModulePropertiesPanel::CreatePropertyWidgets() {
    auto Container = GetPropertyContainer();
    if (!Container.IsValid()) return;

    Container->ClearChildren();

    if (!ZhengDriftActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActor", "No ZhengDrift Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AZhengDriftUnreal* ZhengDrift = ZhengDriftActor.Get();

    // ---- Basic Configuration ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Basic Configuration"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("StringNumber"), ZhengDrift->StringNumber, TEXT("StringNumber"),
        FSimpleDelegate())];

    // ---- File Paths ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("File Paths"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT("IO File Path"), ZhengDrift->IOFilePath, TEXT("IOFilePath"),
        TEXT(".zheng_master"),
        [this](const FString& NewPath) {
            if (ZhengDriftActor.IsValid()) {
                ZhengDriftActor->Modify();
                ZhengDriftActor->IOFilePath = NewPath;
            }
        },
        true)];

    // ---- Control Rig ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Control Rig"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("CheckStatusBtn", "Check Player Control Rig Status"))
             .OnClicked(this,
                        &SZhengDriftModulePropertiesPanel::OnCheckObjectsStatus)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SetupAllBtn", "Setup Player Control Rig"))
                  .OnClicked(
                      this,
                      &SZhengDriftModulePropertiesPanel::OnSetupAllObjects)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ---- Instrument ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Instrument"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("InitZhengBtn", "Initialize Zheng Instrument"))
                  .OnClicked(
                      this,
                      &SZhengDriftModulePropertiesPanel::OnInitZhengInstrument)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ---- Import / Export ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Import / Export"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("ExportBtn", "Export Player Info"))
                  .OnClicked(
                      this,
                      &SZhengDriftModulePropertiesPanel::OnExportRecorderInfo)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("ImportBtn", "Import Player Info"))
                  .OnClicked(
                      this,
                      &SZhengDriftModulePropertiesPanel::OnImportRecorderInfo)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

FReply SZhengDriftModulePropertiesPanel::OnCheckObjectsStatus() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    UZhengDriftControlRigProcessor::CheckObjectsStatus(ZhengDriftActor.Get());
    return FReply::Handled();
}

FReply SZhengDriftModulePropertiesPanel::OnSetupAllObjects() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    UZhengDriftControlRigProcessor::SetupAllObjects(ZhengDriftActor.Get());
    return FReply::Handled();
}

FReply SZhengDriftModulePropertiesPanel::OnInitZhengInstrument() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    UZhengDriftMusicInstrumentProcessor::InitializeZhengInstrument(
        ZhengDriftActor.Get());
    return FReply::Handled();
}

FReply SZhengDriftModulePropertiesPanel::OnExportRecorderInfo() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    if (ZhengDriftActor->IOFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("ZhengDrift: IOFilePath is empty"));
        return FReply::Handled();
    }
    ZhengDriftActor->ExportRecorderInfo(ZhengDriftActor->IOFilePath);
    return FReply::Handled();
}

FReply SZhengDriftModulePropertiesPanel::OnImportRecorderInfo() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    if (ZhengDriftActor->IOFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("ZhengDrift: IOFilePath is empty"));
        return FReply::Handled();
    }
    ZhengDriftActor->ImportRecorderInfo(ZhengDriftActor->IOFilePath);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
