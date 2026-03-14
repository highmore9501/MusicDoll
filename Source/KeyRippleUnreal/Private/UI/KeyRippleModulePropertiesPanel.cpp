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
        TEXT("OneHandFingerNumber"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("LeftestPosition"), KeyRipple->LeftestPosition,
        TEXT("LeftestPosition"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("LeftPosition"), KeyRipple->LeftPosition, TEXT("LeftPosition"),
        FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("MiddleLeftPosition"), KeyRipple->MiddleLeftPosition,
        TEXT("MiddleLeftPosition"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("MiddleRightPosition"), KeyRipple->MiddleRightPosition,
        TEXT("MiddleRightPosition"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("RightPosition"), KeyRipple->RightPosition, TEXT("RightPosition"),
        FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("RightestPosition"), KeyRipple->RightestPosition,
        TEXT("RightestPosition"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("MinKey"), KeyRipple->MinKey, TEXT("MinKey"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("MaxKey"), KeyRipple->MaxKey, TEXT("MaxKey"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("HandRange"), KeyRipple->HandRange, TEXT("HandRange"),
        FSimpleDelegate())];

    // Vector3 properties for hand original directions
    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateVector3PropertyRow(
        TEXT("RightHandOriginalDirection"),
        KeyRipple->RightHandOriginalDirection,
        TEXT("RightHandOriginalDirection"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateVector3PropertyRow(
        TEXT("LeftHandOriginalDirection"), KeyRipple->LeftHandOriginalDirection,
        TEXT("LeftHandOriginalDirection"), FSimpleDelegate())];

    // File path properties
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("File Paths"))];

    // IOFilePath comes from base class AInstrumentBase
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
            TEXT("IO File Path"), KeyRipple->IOFilePath, TEXT("IOFilePath"),
            TEXT(".avatar"),
            [this](const FString& NewPath) {
                if (KeyRippleActor.IsValid()) {
                    KeyRippleActor->Modify();
                    KeyRippleActor->IOFilePath = NewPath;
                    UE_LOG(LogTemp, Warning, TEXT("KeyRipple: IO File Path updated to: %s"), *NewPath);
                }
            },
            true)];

    // Initialization Operations Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Initialization"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("CheckObjectsStatusButton", "Check Player Control Rig Status"))
             .OnClicked(this,
                        &SKeyRippleModulePropertiesPanel::OnCheckObjectsStatus)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SetupAllObjectsButton", "Setup Player Control Rig"))
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

void SKeyRippleModulePropertiesPanel::OnVector3PropertyChanged(
    const FString& PropertyPath, int32 ComponentIndex, float NewValue) {
    if (!KeyRippleActor.IsValid()) {
        return;
    }

    AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
    KeyRipple->Modify();

    if (PropertyPath == TEXT("RightHandOriginalDirection")) {
        if (ComponentIndex == 0)
            KeyRipple->RightHandOriginalDirection.X = NewValue;
        else if (ComponentIndex == 1)
            KeyRipple->RightHandOriginalDirection.Y = NewValue;
        else if (ComponentIndex == 2)
            KeyRipple->RightHandOriginalDirection.Z = NewValue;
    } else if (PropertyPath == TEXT("LeftHandOriginalDirection")) {
        if (ComponentIndex == 0)
            KeyRipple->LeftHandOriginalDirection.X = NewValue;
        else if (ComponentIndex == 1)
            KeyRipple->LeftHandOriginalDirection.Y = NewValue;
        else if (ComponentIndex == 2)
            KeyRipple->LeftHandOriginalDirection.Z = NewValue;
    }
}

FReply SKeyRippleModulePropertiesPanel::OnCheckObjectsStatus() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("KeyRipple: No actor selected for check player control rig status"))
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::CheckObjectsStatus(KeyRippleActor.Get());
    return FReply::Handled();
}

FReply SKeyRippleModulePropertiesPanel::OnSetupAllObjects() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("KeyRipple: No actor selected for setup player control rig"))
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::SetupAllObjects(KeyRippleActor.Get());
    return FReply::Handled();
}

FReply SKeyRippleModulePropertiesPanel::OnExportRecorderInfo() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("KeyRipple: No actor selected for export player info"))
        return FReply::Handled();
    }

    KeyRippleActor->ExportRecorderInfo();
    return FReply::Handled();
}

FReply SKeyRippleModulePropertiesPanel::OnImportRecorderInfo() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("KeyRipple: No actor selected for import player info"))
        return FReply::Handled();
    }

    KeyRippleActor->ImportRecorderInfo();
    return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE