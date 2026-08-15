#include "UI/KeyRippleModulePropertiesPanel.h"

#include "KeyRippleControlRigProcessor.h"
#include "KeyRippleUnreal.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SKeyRippleModulePropertiesPanel"

void SKeyRippleModulePropertiesPanel::Construct(const FArguments& InArgs) {
    // 调用基类构造函数，使用基类的参数类型
    SModulePropertiesPanel::FArguments BaseArgs;
    SModulePropertiesPanel::Construct(BaseArgs);
}

void SKeyRippleModulePropertiesPanel::SetActor(AActor* InActor) {
    KeyRippleActor = Cast<AKeyRippleUnreal>(InActor);
    RefreshProperties();
}

bool SKeyRippleModulePropertiesPanel::CanHandleActor(
    const AActor* InActor) const {
    return InActor && InActor->IsA<AKeyRippleUnreal>();
}

void SKeyRippleModulePropertiesPanel::RefreshProperties() {
    CreatePropertyWidgets();
}

void SKeyRippleModulePropertiesPanel::CreatePropertyWidgets() {
    auto Container = GetPropertyContainer();
    if (!Container.IsValid()) {
        return;
    }

    Container->ClearChildren();

    if (!KeyRippleActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActorSelected",
                                    "No KeyRipple Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();

    // Numeric properties
    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("OneHandFingerNumber"), KeyRipple->OneHandFingerNumber,
        TEXT("OneHandFingerNumber"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("LeftestPosition"), KeyRipple->LeftestPosition,
        TEXT("LeftestPosition"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("LeftPosition"), KeyRipple->LeftPosition, TEXT("LeftPosition"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("MiddleLeftPosition"), KeyRipple->MiddleLeftPosition,
        TEXT("MiddleLeftPosition"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("MiddleRightPosition"), KeyRipple->MiddleRightPosition,
        TEXT("MiddleRightPosition"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("RightPosition"), KeyRipple->RightPosition, TEXT("RightPosition"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("RightestPosition"), KeyRipple->RightestPosition,
        TEXT("RightestPosition"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("MinKey"), KeyRipple->MinKey, TEXT("MinKey"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("MaxKey"), KeyRipple->MaxKey, TEXT("MaxKey"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("HandRange"), KeyRipple->HandRange, TEXT("HandRange"),
        [this](const FString& PropertyPath, int32 NewValue) {
            OnNumericPropertyChanged(PropertyPath, NewValue);
        })];

    // File path properties
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("File Paths"))];

    // IOFilePath comes from base class AInstrumentBase
    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT("IO File Path"), KeyRipple->IOFilePath, TEXT("IOFilePath"),
        TEXT(".avatar"),
        [this](const FString& NewPath) {
            if (KeyRippleActor.IsValid()) {
                KeyRippleActor->Modify();
                KeyRippleActor->IOFilePath = NewPath;
                UE_LOG(LogTemp, Warning,
                       TEXT("KeyRipple: IO File Path updated to: %s"),
                       *NewPath);
            }
        },
        true)];

    // Initialization Operations Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Initialization"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("CheckObjectsStatusButton",
                                "Check Player Control Rig Status"))
                  .OnClicked(
                      this,
                      &SKeyRippleModulePropertiesPanel::OnCheckObjectsStatus)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SetupAllObjectsButton",
                                "Setup Player Control Rig"))
                  .OnClicked(
                      this, &SKeyRippleModulePropertiesPanel::OnSetupAllObjects)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Import/Export Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Import/Export"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("ExportRecorderInfoButton", "Export Player Info"))
             .OnClicked(this,
                        &SKeyRippleModulePropertiesPanel::OnExportRecorderInfo)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("ImportRecorderInfoButton", "Import Player Info"))
             .OnClicked(this,
                        &SKeyRippleModulePropertiesPanel::OnImportRecorderInfo)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Export to Blender Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Export to Blender"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT("Blender File Path"), BlenderExportFilePath,
        TEXT("BlenderExportFilePath"), TEXT(".avatar"),
        [this](const FString& NewPath) { BlenderExportFilePath = NewPath; },
        true)];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("ExportToBlenderButton", "Export to Blender"))
                  .OnClicked(
                      this, &SKeyRippleModulePropertiesPanel::OnExportToBlender)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

void SKeyRippleModulePropertiesPanel::OnNumericPropertyChanged(
    const FString& PropertyPath, int32 NewValue) {
    if (!KeyRippleActor.IsValid()) {
        return;
    }

    AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
    KeyRipple->Modify();

    if (PropertyPath == TEXT("OneHandFingerNumber"))
        KeyRipple->OneHandFingerNumber = NewValue;
    else if (PropertyPath == TEXT("LeftestPosition"))
        KeyRipple->LeftestPosition = NewValue;
    else if (PropertyPath == TEXT("LeftPosition"))
        KeyRipple->LeftPosition = NewValue;
    else if (PropertyPath == TEXT("MiddleLeftPosition"))
        KeyRipple->MiddleLeftPosition = NewValue;
    else if (PropertyPath == TEXT("MiddleRightPosition"))
        KeyRipple->MiddleRightPosition = NewValue;
    else if (PropertyPath == TEXT("RightPosition"))
        KeyRipple->RightPosition = NewValue;
    else if (PropertyPath == TEXT("RightestPosition"))
        KeyRipple->RightestPosition = NewValue;
    else if (PropertyPath == TEXT("MinKey"))
        KeyRipple->MinKey = NewValue;
    else if (PropertyPath == TEXT("MaxKey"))
        KeyRipple->MaxKey = NewValue;
    else if (PropertyPath == TEXT("HandRange"))
        KeyRipple->HandRange = NewValue;
}

void SKeyRippleModulePropertiesPanel::OnStringPropertyChanged(
    const FString& PropertyPath, const FString& NewValue) {
    if (!KeyRippleActor.IsValid()) {
        return;
    }

    AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
    KeyRipple->Modify();

    if (PropertyPath == TEXT("IOFilePath"))
        KeyRipple->IOFilePath = NewValue;
    else if (PropertyPath == TEXT("AnimationFilePath"))
        KeyRipple->AnimationFilePath = NewValue;
}

FReply SKeyRippleModulePropertiesPanel::OnCheckObjectsStatus() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for check player control rig "
                    "status"))
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::CheckObjectsStatus(KeyRippleActor.Get());
    return FReply::Handled();
}

FReply SKeyRippleModulePropertiesPanel::OnSetupAllObjects() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(
            LogTemp, Error,
            TEXT("KeyRipple: No actor selected for setup player control rig"))
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::SetupAllObjects(KeyRippleActor.Get());
    return FReply::Handled();
}

FReply SKeyRippleModulePropertiesPanel::OnExportRecorderInfo() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for export player info"))
        return FReply::Handled();
    }

    if (!FCommonPanelUtility::ConfirmExportOverwrite(
            KeyRippleActor->IOFilePath)) {
        return FReply::Handled();
    }

    KeyRippleActor->ExportRecorderInfo(KeyRippleActor->IOFilePath);
    return FReply::Handled();
}

FReply SKeyRippleModulePropertiesPanel::OnImportRecorderInfo() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for import player info"))
        return FReply::Handled();
    }

    KeyRippleActor->ImportRecorderInfo();
    return FReply::Handled();
}

FReply SKeyRippleModulePropertiesPanel::OnExportToBlender() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for export to blender"));
        return FReply::Handled();
    }

    if (BlenderExportFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: Blender export file path is empty"));
        return FReply::Handled();
    }

    if (!FCommonPanelUtility::ConfirmExportOverwrite(BlenderExportFilePath)) {
        return FReply::Handled();
    }

    KeyRippleActor->ExportRecorderInfo(BlenderExportFilePath, true);
    UE_LOG(LogTemp, Warning,
           TEXT("KeyRipple: Export to Blender triggered -> %s"),
           *BlenderExportFilePath);
    return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE