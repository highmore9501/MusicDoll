#include "UI/HarpGlideModulePropertiesPanel.h"

#include "HarpGlideControlRigProcessor.h"
#include "HarpGlideMusicInstrumentProcessor.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHarpGlideModulePropertiesPanel"

void SHarpGlideModulePropertiesPanel::Construct(const FArguments& InArgs) {
    SModulePropertiesPanel::FArguments BaseArgs;
    SModulePropertiesPanel::Construct(BaseArgs);
}

void SHarpGlideModulePropertiesPanel::SetActor(AActor* InActor) {
    HarpGlideActor = Cast<AHarpGlideUnreal>(InActor);
    RefreshProperties();
}

bool SHarpGlideModulePropertiesPanel::CanHandleActor(
    const AActor* InActor) const {
    return InActor && InActor->IsA<AHarpGlideUnreal>();
}

void SHarpGlideModulePropertiesPanel::RefreshProperties() {
    CreatePropertyWidgets();
}

void SHarpGlideModulePropertiesPanel::CreatePropertyWidgets() {
    auto Container = GetPropertyContainer();
    if (!Container.IsValid()) return;

    Container->ClearChildren();

    if (!HarpGlideActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActor", "No HarpGlide Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AHarpGlideUnreal* HarpGlide = HarpGlideActor.Get();

    // ---- Basic Configuration ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Basic Configuration"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("StringNumber"), HarpGlide->StringNumber, TEXT("StringNumber"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    // 手部位置参数（左右并排）
    Container->AddSlot().AutoHeight().Padding(5.0f, 5.0f, 5.0f, 2.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(STextBlock)
                  .Text(LOCTEXT("LeftHandPosLabel", "Left Hand"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(STextBlock)
                  .Text(LOCTEXT("RightHandPosLabel", "Right Hand"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))]];

    // 第一行：far / near
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 2.0f)[SNew(SHorizontalBox) +
                    SHorizontalBox::Slot().FillWidth(0.5f).Padding(
                        0.0f, 0.0f, 5.0f,
                        0.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
                        TEXT("Far"), HarpGlide->LeftFar, TEXT("LeftFar"),
                        [this](const FString& PropertyPath, int32 NewValue) {
                            OnNumericPropertyChanged(PropertyPath, NewValue);
                        })] +
                    SHorizontalBox::Slot().FillWidth(0.5f).Padding(
                        5.0f, 0.0f, 0.0f,
                        0.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
                        TEXT("Far"), HarpGlide->RightFar, TEXT("RightFar"),
                        [this](const FString& PropertyPath, int32 NewValue) {
                            OnNumericPropertyChanged(PropertyPath, NewValue);
                        })]];

    // 第二行：mid_far / (no right equivalent)
    Container->AddSlot().AutoHeight().Padding(
        5.0f,
        2.0f)[SNew(SHorizontalBox) +
              SHorizontalBox::Slot().FillWidth(0.5f).Padding(
                  0.0f, 0.0f, 5.0f,
                  0.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
                  TEXT("Mid Far"), HarpGlide->LeftMidFar, TEXT("LeftMidFar"),
                  [this](const FString& PropertyPath, int32 NewValue) {
                      OnNumericPropertyChanged(PropertyPath, NewValue);
                  })] +
              SHorizontalBox::Slot().FillWidth(0.5f).Padding(
                  5.0f, 0.0f, 0.0f,
                  0.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
                  TEXT("Near"), HarpGlide->RightNear, TEXT("RightNear"),
                  [this](const FString& PropertyPath, int32 NewValue) {
                      OnNumericPropertyChanged(PropertyPath, NewValue);
                  })]];

    // 第三行：mid_near / (---)
    Container->AddSlot().AutoHeight().Padding(
        5.0f,
        2.0f)[SNew(SHorizontalBox) +
              SHorizontalBox::Slot().FillWidth(0.5f).Padding(
                  0.0f, 0.0f, 5.0f,
                  0.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
                  TEXT("Mid Near"), HarpGlide->LeftMidNear, TEXT("LeftMidNear"),
                  [this](const FString& PropertyPath, int32 NewValue) {
                      OnNumericPropertyChanged(PropertyPath, NewValue);
                  })] +
              SHorizontalBox::Slot().FillWidth(0.5f).Padding(
                  5.0f, 0.0f, 0.0f,
                  0.0f)[SNew(STextBlock)
                            .Text(FText::GetEmpty())
                            .Visibility(EVisibility::Hidden)]];

    // 第四行：near / (---)
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 2.0f)[SNew(SHorizontalBox) +
                    SHorizontalBox::Slot().FillWidth(0.5f).Padding(
                        0.0f, 0.0f, 5.0f,
                        0.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
                        TEXT("Near"), HarpGlide->LeftNear, TEXT("LeftNear"),
                        [this](const FString& PropertyPath, int32 NewValue) {
                            OnNumericPropertyChanged(PropertyPath, NewValue);
                        })] +
                    SHorizontalBox::Slot().FillWidth(0.5f).Padding(
                        5.0f, 0.0f, 0.0f,
                        0.0f)[SNew(STextBlock)
                                  .Text(FText::GetEmpty())
                                  .Visibility(EVisibility::Hidden)]];

    // ---- File Paths ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("File Paths"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT("IO File Path"), HarpGlide->IOFilePath, TEXT("IOFilePath"),
        TEXT(".harpist"),
        [this](const FString& NewPath) {
            if (HarpGlideActor.IsValid()) {
                HarpGlideActor->Modify();
                HarpGlideActor->IOFilePath = NewPath;
            }
        },
        true)];

    // ---- Control Rig ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Control Rig"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("CheckStatusBtn",
                                "Check Performer Control Rig Status"))
                  .OnClicked(
                      this,
                      &SHarpGlideModulePropertiesPanel::OnCheckObjectsStatus)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SetupAllBtn", "Setup All Controllers"))
                  .OnClicked(
                      this, &SHarpGlideModulePropertiesPanel::OnSetupAllObjects)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ---- Instrument ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Instrument"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("InitHarpBtn", "Initialize Harp Instrument"))
                  .OnClicked(
                      this,
                      &SHarpGlideModulePropertiesPanel::OnInitHarpInstrument)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ---- Import / Export ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Import / Export"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("ExportBtn", "Export .harpist"))
                  .OnClicked(
                      this,
                      &SHarpGlideModulePropertiesPanel::OnExportRecorderInfo)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("ImportBtn", "Import .harpist"))
                  .OnClicked(
                      this,
                      &SHarpGlideModulePropertiesPanel::OnImportRecorderInfo)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ---- Export to Blender ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Export to Blender"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT("Blender File Path"), BlenderExportFilePath,
        TEXT("BlenderExportFilePath"), TEXT(".harpist"),
        [this](const FString& NewPath) { BlenderExportFilePath = NewPath; },
        true)];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("ExportToBlenderBtn", "Export to Blender"))
                  .OnClicked(
                      this, &SHarpGlideModulePropertiesPanel::OnExportToBlender)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

void SHarpGlideModulePropertiesPanel::OnNumericPropertyChanged(
    const FString& PropertyPath, int32 NewValue) {
    if (!HarpGlideActor.IsValid()) return;
    HarpGlideActor->Modify();

    if (PropertyPath == TEXT("StringNumber")) {
        HarpGlideActor->StringNumber = NewValue;
    } else if (PropertyPath == TEXT("LeftFar")) {
        HarpGlideActor->LeftFar = NewValue;
    } else if (PropertyPath == TEXT("LeftNear")) {
        HarpGlideActor->LeftNear = NewValue;
    } else if (PropertyPath == TEXT("LeftMidFar")) {
        HarpGlideActor->LeftMidFar = NewValue;
    } else if (PropertyPath == TEXT("LeftMidNear")) {
        HarpGlideActor->LeftMidNear = NewValue;
    } else if (PropertyPath == TEXT("RightFar")) {
        HarpGlideActor->RightFar = NewValue;
    } else if (PropertyPath == TEXT("RightNear")) {
        HarpGlideActor->RightNear = NewValue;
    }
}

FReply SHarpGlideModulePropertiesPanel::OnCheckObjectsStatus() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    UHarpGlideControlRigProcessor::CheckObjectsStatus(HarpGlideActor.Get());
    return FReply::Handled();
}

FReply SHarpGlideModulePropertiesPanel::OnSetupAllObjects() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    UHarpGlideControlRigProcessor::SetupAllObjects(HarpGlideActor.Get());
    return FReply::Handled();
}

FReply SHarpGlideModulePropertiesPanel::OnExportRecorderInfo() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    HarpGlideActor->ExportRecorderInfo(HarpGlideActor->IOFilePath);
    return FReply::Handled();
}

FReply SHarpGlideModulePropertiesPanel::OnImportRecorderInfo() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    HarpGlideActor->ImportRecorderInfo(HarpGlideActor->IOFilePath);
    return FReply::Handled();
}

FReply SHarpGlideModulePropertiesPanel::OnInitHarpInstrument() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    UHarpGlideMusicInstrumentProcessor::InitializeHarpInstrument(
        HarpGlideActor.Get());
    return FReply::Handled();
}

FReply SHarpGlideModulePropertiesPanel::OnExportToBlender() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();

    if (BlenderExportFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("HarpGlide: Blender export file path is empty"));
        return FReply::Handled();
    }

    if (!FCommonPanelUtility::ConfirmExportOverwrite(BlenderExportFilePath)) {
        return FReply::Handled();
    }

    HarpGlideActor->ExportRecorderInfo(BlenderExportFilePath, true);
    UE_LOG(LogTemp, Warning,
           TEXT("HarpGlide: Export to Blender triggered -> %s"),
           *BlenderExportFilePath);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
