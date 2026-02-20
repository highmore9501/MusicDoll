#include "KeyRipplePropertiesPanel.h"

#include "CommonPropertiesPanelUtility.h"
#include "Details/SBoneControlMappingEditPanel.h"
#include "KeyRippleControlRigProcessor.h"
#include "KeyRippleOperationsPanel.h"
#include "KeyRippleUnreal.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SKeyRipplePropertiesPanel"

void SKeyRipplePropertiesPanel::Construct(const FArguments& InArgs) {
    InitializeTabPanel(LOCTEXT("PropertiesTabLabel", "Properties"),
                       LOCTEXT("OperationsTabLabel", "Operations"),
                       LOCTEXT("BoneControlMappingTabLabel", "Bone Control Mapping"));

    // Create operations panel
    OperationsPanel = SNew(SKeyRippleOperationsPanel);

    // Set the operations panel as the operations content
    // Note: The operations panel (SKeyRippleOperationsPanel) has its own
    // internal structure and will handle its content via SetActor callback
    if (OperationsPanel.IsValid()) {
        SetOperationsContent(OperationsPanel.ToSharedRef());
    }

    // Create Bone Control Mapping panel
    BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);
    if (BoneControlMappingPanel.IsValid()) {
        SetThirdTabContent(BoneControlMappingPanel.ToSharedRef());
    }

    RefreshPropertyList();
}

TSharedPtr<SWidget> SKeyRipplePropertiesPanel::GetWidget() {
    return AsShared();
}

void SKeyRipplePropertiesPanel::SetActor(AActor* InActor) {
    KeyRippleActor = Cast<AKeyRippleUnreal>(InActor);
    RefreshPropertyList();

    if (OperationsPanel.IsValid()) {
        OperationsPanel->SetActor(InActor);
    }

    if (BoneControlMappingPanel.IsValid()) {
        BoneControlMappingPanel->SetActor(InActor);
    }
}

bool SKeyRipplePropertiesPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AKeyRippleUnreal>();
}

void SKeyRipplePropertiesPanel::RefreshPropertyList() {
    auto Container = GetPropertiesContainer();
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
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("OneHandFingerNumber"), KeyRipple->OneHandFingerNumber,
        TEXT("OneHandFingerNumber"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("LeftestPosition"), KeyRipple->LeftestPosition,
        TEXT("LeftestPosition"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("LeftPosition"), KeyRipple->LeftPosition, TEXT("LeftPosition"),
        FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("MiddleLeftPosition"), KeyRipple->MiddleLeftPosition,
        TEXT("MiddleLeftPosition"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("MiddleRightPosition"), KeyRipple->MiddleRightPosition,
        TEXT("MiddleRightPosition"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("RightPosition"), KeyRipple->RightPosition, TEXT("RightPosition"),
        FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("RightestPosition"), KeyRipple->RightestPosition,
        TEXT("RightestPosition"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("MinKey"), KeyRipple->MinKey, TEXT("MinKey"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("MaxKey"), KeyRipple->MaxKey, TEXT("MaxKey"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateNumericPropertyRow(
        TEXT("HandRange"), KeyRipple->HandRange, TEXT("HandRange"),
        FSimpleDelegate())];

    // Vector3 properties for hand original directions
    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateVector3PropertyRow(
        TEXT("RightHandOriginalDirection"),
        KeyRipple->RightHandOriginalDirection,
        TEXT("RightHandOriginalDirection"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPropertiesPanelUtility::CreateVector3PropertyRow(
        TEXT("LeftHandOriginalDirection"), KeyRipple->LeftHandOriginalDirection,
        TEXT("LeftHandOriginalDirection"), FSimpleDelegate())];

    // File path properties with specific extensions
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPropertiesPanelUtility::CreateSectionHeader(
        TEXT("File Paths"))];

    TSharedPtr<SEditableTextBox> IOFilePathTextBox;
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().AutoWidth().Padding(
             5.0f)[SNew(STextBlock)
                       .Text(FText::FromString(TEXT("IOFilePath")))
                       .MinDesiredWidth(150.0f)] +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
             [SAssignNew(IOFilePathTextBox, SEditableTextBox)
                  .Text(FText::FromString(KeyRipple->IOFilePath))
                  .OnTextCommitted_Lambda([this](const FText& InText,
                                                 ETextCommit::Type CommitType) {
                      if (CommitType == ETextCommit::OnEnter ||
                          CommitType == ETextCommit::OnUserMovedFocus) {
                          if (KeyRippleActor.IsValid()) {
                              KeyRippleActor->IOFilePath = InText.ToString();
                              KeyRippleActor->Modify();
                          }
                      }
                  })] +
         SHorizontalBox::Slot().AutoWidth().Padding(
             5.0f, 0.0f, 0.0f,
             0.0f)[SNew(SButton)
                       .Text(FText::FromString(TEXT("Browse")))
                       .OnClicked_Lambda([this, IOFilePathTextBox]() -> FReply {
                           if (!KeyRippleActor.IsValid()) {
                               return FReply::Handled();
                           }

                           FString OutFilePath;
                           if (FCommonPropertiesPanelUtility::BrowseForFile(
                                   TEXT(".avatar"), OutFilePath, true)) {
                               if (IOFilePathTextBox.IsValid()) {
                                   IOFilePathTextBox->SetText(
                                       FText::FromString(OutFilePath));
                                   KeyRippleActor->IOFilePath = OutFilePath;
                                   KeyRippleActor->Modify();
                               }
                           }
                           return FReply::Handled();
                       })]];

    // Initialization Operations Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPropertiesPanelUtility::CreateSectionHeader(
        TEXT("Initialization"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("CheckObjectsStatusButton", "Check Objects Status"))
             .OnClicked(this, &SKeyRipplePropertiesPanel::OnCheckObjectsStatus)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SetupAllObjectsButton", "Setup All Objects"))
                  .OnClicked(this,
                             &SKeyRipplePropertiesPanel::OnSetupAllObjects)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Import/Export Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPropertiesPanelUtility::CreateSectionHeader(
        TEXT("Import/Export"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("ExportRecorderInfoButton", "Export Recorder Info"))
             .OnClicked(this, &SKeyRipplePropertiesPanel::OnExportRecorderInfo)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("ImportRecorderInfoButton", "Import Recorder Info"))
             .OnClicked(this, &SKeyRipplePropertiesPanel::OnImportRecorderInfo)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

TSharedRef<SWidget> SKeyRipplePropertiesPanel::CreateEnumPropertyRow(
    const FString& PropertyName, uint8 Value, const FString& EnumTypeName,
    const FString& PropertyPath) {
    TArray<FString> EnumOptions;

    if (EnumTypeName == TEXT("EKeyType")) {
        EnumOptions.Add(TEXT("WHITE"));
        EnumOptions.Add(TEXT("BLACK"));
    } else if (EnumTypeName == TEXT("EPositionType")) {
        EnumOptions.Add(TEXT("HIGH"));
        EnumOptions.Add(TEXT("LOW"));
        EnumOptions.Add(TEXT("MIDDLE"));
    }

    if (EnumOptions.Num() == 0) {
        return SNew(STextBlock).Text(FText::FromString(TEXT("Unknown Enum")));
    }

    // Create shared copies that will persist
    TSharedPtr<TArray<TSharedPtr<FString>>> OptionStrings =
        MakeShareable(new TArray<TSharedPtr<FString>>);

    for (const FString& Option : EnumOptions) {
        OptionStrings->Add(MakeShareable(new FString(Option)));
    }

    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(PropertyName))
                         .MinDesiredWidth(150.0f)] +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
               [SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(OptionStrings.Get())
                    .OnGenerateWidget_Lambda(
                        [OptionStrings](TSharedPtr<FString> InOption) {
                            return SNew(STextBlock)
                                .Text(FText::FromString(
                                    InOption.IsValid() ? *InOption : TEXT("")));
                        })
                    .OnSelectionChanged_Lambda(
                        [this, PropertyPath, EnumTypeName, OptionStrings](
                            TSharedPtr<FString> NewSelection,
                            ESelectInfo::Type SelectInfo) {
                            if (NewSelection.IsValid()) {
                                uint8 NewValue = 0;

                                if (EnumTypeName == TEXT("EKeyType")) {
                                    NewValue = (*NewSelection == TEXT("WHITE"))
                                                   ? 0
                                                   : 1;
                                } else if (EnumTypeName ==
// ... existing code ...
                                           TEXT("EPositionType")) {
                                    if (*NewSelection == TEXT("HIGH"))
                                        NewValue = 0;
                                    else if (*NewSelection == TEXT("LOW"))
                                        NewValue = 1;
                                    else
                                        NewValue = 2;
                                }

                                OnEnumPropertyChanged(PropertyPath, NewValue);
                            }
                        })[SNew(STextBlock)
                               .Text_Lambda([this, PropertyPath, EnumTypeName,
                                             OptionStrings]() -> FText {
                                   if (!KeyRippleActor.IsValid()) {
                                       return FText::FromString(TEXT(""));
                                   }

                                   AKeyRippleUnreal* KeyRipple =
                                       KeyRippleActor.Get();
                                   uint8 CurrentValue = 0;

                                   if (PropertyPath ==
                                       TEXT("LeftHandKeyType")) {
                                       CurrentValue = static_cast<uint8>(
                                           KeyRipple->LeftHandKeyType);
                                   } else if (PropertyPath ==
                                              TEXT("LeftHandPositionType")) {
                                       CurrentValue = static_cast<uint8>(
                                           KeyRipple->LeftHandPositionType);
                                   } else if (PropertyPath ==
                                              TEXT("RightHandKeyType")) {
                                       CurrentValue = static_cast<uint8>(
                                           KeyRipple->RightHandKeyType);
                                   } else if (PropertyPath ==
                                              TEXT("RightHandPositionType")) {
                                       CurrentValue = static_cast<uint8>(
                                           KeyRipple->RightHandPositionType);
                                   }

                                   if (OptionStrings->IsValidIndex(
                                           CurrentValue)) {
                                       return FText::FromString(
                                           *OptionStrings->operator[](
                                               CurrentValue));
                                   }

                                   return FText::FromString(TEXT("Unknown"));
                               })]];
}

void SKeyRipplePropertiesPanel::OnNumericPropertyChanged(
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

void SKeyRipplePropertiesPanel::OnStringPropertyChanged(
    const FString& PropertyPath, const FText& NewValue) {
    if (!KeyRippleActor.IsValid()) {
        return;
    }

    AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
    KeyRipple->Modify();

    if (PropertyPath == TEXT("IOFilePath"))
        KeyRipple->IOFilePath = NewValue.ToString();
    else if (PropertyPath == TEXT("KeyRippleFilePath"))
        KeyRipple->AnimationFilePath = NewValue.ToString();
}

void SKeyRipplePropertiesPanel::OnEnumPropertyChanged(
    const FString& PropertyPath, uint8 NewValue) {
    if (!KeyRippleActor.IsValid()) {
        return;
    }

    AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
    KeyRipple->Modify();

    if (PropertyPath == TEXT("LeftHandKeyType")) {
        KeyRipple->LeftHandKeyType = static_cast<EKeyType>(NewValue);
    } else if (PropertyPath == TEXT("LeftHandPositionType")) {
        KeyRipple->LeftHandPositionType = static_cast<EPositionType>(NewValue);
    } else if (PropertyPath == TEXT("RightHandKeyType")) {
        KeyRipple->RightHandKeyType = static_cast<EKeyType>(NewValue);
    } else if (PropertyPath == TEXT("RightHandPositionType")) {
        KeyRipple->RightHandPositionType = static_cast<EPositionType>(NewValue);
    }
}

void SKeyRipplePropertiesPanel::OnFilePathChanged(const FString& PropertyPath,
                                                  const FString& NewFilePath) {
    if (!KeyRippleActor.IsValid()) {
        return;
    }

    AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
    KeyRipple->Modify();

    if (PropertyPath == TEXT("IOFilePath")) {
        KeyRipple->IOFilePath = NewFilePath;
    } else if (PropertyPath == TEXT("KeyRippleFilePath")) {
        KeyRipple->AnimationFilePath = NewFilePath;
    }
}

void SKeyRipplePropertiesPanel::OnVector3PropertyChanged(
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

FReply SKeyRipplePropertiesPanel::OnCheckObjectsStatus() {
    if (!KeyRippleActor.IsValid()) {
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::CheckObjectsStatus(KeyRippleActor.Get());
    return FReply::Handled();
}

FReply SKeyRipplePropertiesPanel::OnSetupAllObjects() {
    if (!KeyRippleActor.IsValid()) {
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::SetupAllObjects(KeyRippleActor.Get());
    return FReply::Handled();
}

FReply SKeyRipplePropertiesPanel::OnExportRecorderInfo() {
    if (!KeyRippleActor.IsValid()) {
        return FReply::Handled();
    }

    EAppReturnType::Type UserConfirm = FMessageDialog::Open(
        EAppMsgType::YesNo,
        FText::FromString(
            TEXT("Are you sure you want to export recorder "
                 "information?\n\nThis will overwrite existing data.")));

    if (UserConfirm == EAppReturnType::Yes) {
        KeyRippleActor->ExportRecorderInfo();
    }

    return FReply::Handled();
}

FReply SKeyRipplePropertiesPanel::OnImportRecorderInfo() {
    if (!KeyRippleActor.IsValid()) {
        return FReply::Handled();
    }

    EAppReturnType::Type UserConfirm = FMessageDialog::Open(
        EAppMsgType::YesNo,
        FText::FromString(TEXT(
            "Are you sure you want to import recorder information?\n\nThis "
            "will overwrite existing actor properties.")));

    if (UserConfirm == EAppReturnType::Yes) {
        KeyRippleActor->ImportRecorderInfo();
    }

    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE