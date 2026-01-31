#include "KeyRipplePropertiesPanel.h"

#include "DesktopPlatformModule.h"
#include "KeyRippleControlRigProcessor.h"
#include "KeyRippleOperationsPanel.h"
#include "KeyRippleUnreal.h"
#include "Misc/MessageDialog.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SKeyRipplePropertiesPanel"

void SKeyRipplePropertiesPanel::Construct(const FArguments& InArgs) {
    ActiveTab = EActiveTab::Properties;

    ChildSlot
        [SNew(SVerticalBox)
         // Tab Buttons
         +
         SVerticalBox::Slot().AutoHeight().Padding(5.0f)
             [SNew(SHorizontalBox) +
              SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.5f, 0.0f)
                  [SNew(SButton)
                       .Text(this,
                             &SKeyRipplePropertiesPanel::GetPropertiesTabLabel)
                       .OnClicked(this, &SKeyRipplePropertiesPanel::
                                            OnPropertiesTabClicked)
                       .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
                       .ForegroundColor_Lambda(
                           [this]() { return GetTabButtonTextColor(true); })] +
              SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.5f, 0.0f)
                  [SNew(SButton)
                       .Text(this,
                             &SKeyRipplePropertiesPanel::GetOperationsTabLabel)
                       .OnClicked(this, &SKeyRipplePropertiesPanel::
                                            OnOperationsTabClicked)
                       .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
                       .ForegroundColor_Lambda(
                           [this]() { return GetTabButtonTextColor(false); })]]
         // Content Container
         + SVerticalBox::Slot().FillHeight(1.0f).Padding(
               5.0f)[SAssignNew(ContentContainer, SVerticalBox)]];

    // Create both panels
    TSharedPtr<SVerticalBox> PropertiesPanel =
        SNew(SVerticalBox) +
        SVerticalBox::Slot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("PropertiesLabel", "KeyRipple Properties:"))
                      .Font(FAppStyle::GetFontStyle(
                          "DetailsView.CategoryFont"))] +
        SVerticalBox::Slot().FillHeight(1.0f).Padding(
            5.0f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(
                                         PropertiesContainer, SVerticalBox)]];

    OperationsPanel = SNew(SKeyRippleOperationsPanel);

    // Set initial content to properties panel
    if (ContentContainer.IsValid()) {
        ContentContainer->AddSlot().FillHeight(
            1.0f)[PropertiesPanel.ToSharedRef()];
    }
}

TSharedPtr<SWidget> SKeyRipplePropertiesPanel::GetWidget() {
    return AsShared();
}

void SKeyRipplePropertiesPanel::SetActor(AActor* InActor) {
    KeyRippleActor = Cast<AKeyRippleUnreal>(InActor);
    RefreshPropertyList();

    // 同时将 Actor 传递给操作面板
    if (OperationsPanel.IsValid()) {
        OperationsPanel->SetActor(InActor);
    }
}

bool SKeyRipplePropertiesPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AKeyRippleUnreal>();
}

void SKeyRipplePropertiesPanel::RefreshPropertyList() {
    if (!PropertiesContainer.IsValid()) {
        return;
    }

    PropertiesContainer->ClearChildren();

    if (!KeyRippleActor.IsValid()) {
        PropertiesContainer->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActorSelected",
                                    "No KeyRipple Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();

    // Numeric properties
    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(TEXT("OneHandFingerNumber"),
                                       KeyRipple->OneHandFingerNumber,
                                       TEXT("OneHandFingerNumber"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(TEXT("LeftestPosition"),
                                       KeyRipple->LeftestPosition,
                                       TEXT("LeftestPosition"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(
        TEXT("LeftPosition"), KeyRipple->LeftPosition, TEXT("LeftPosition"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(TEXT("MiddleLeftPosition"),
                                       KeyRipple->MiddleLeftPosition,
                                       TEXT("MiddleLeftPosition"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(TEXT("MiddleRightPosition"),
                                       KeyRipple->MiddleRightPosition,
                                       TEXT("MiddleRightPosition"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(TEXT("RightPosition"),
                                       KeyRipple->RightPosition,
                                       TEXT("RightPosition"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(TEXT("RightestPosition"),
                                       KeyRipple->RightestPosition,
                                       TEXT("RightestPosition"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(TEXT("MinKey"), KeyRipple->MinKey,
                                       TEXT("MinKey"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(TEXT("MaxKey"), KeyRipple->MaxKey,
                                       TEXT("MaxKey"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateNumericPropertyRow(TEXT("HandRange"), KeyRipple->HandRange,
                                       TEXT("HandRange"))];

    // Vector3 properties for hand original directions (moved here)
    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateVector3PropertyRow(TEXT("RightHandOriginalDirection"),
                                       KeyRipple->RightHandOriginalDirection,
                                       TEXT("RightHandOriginalDirection"))];
    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateVector3PropertyRow(TEXT("LeftHandOriginalDirection"),
                                       KeyRipple->LeftHandOriginalDirection,
                                       TEXT("LeftHandOriginalDirection"))];

    // File path properties with specific extensions
    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[CreateFilePathPropertyRow(TEXT("IOFilePath"),
                                        KeyRipple->IOFilePath,
                                        TEXT("IOFilePath"), TEXT(".avatar"))];

    // Initialization Operations Section
    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[SNew(STextBlock)
                  .Text(LOCTEXT("InitializationLabel", "Initialization:"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("CheckObjectsStatusButton", "Check Objects Status"))
             .OnClicked(this, &SKeyRipplePropertiesPanel::OnCheckObjectsStatus)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SetupAllObjectsButton", "Setup All Objects"))
                  .OnClicked(this,
                             &SKeyRipplePropertiesPanel::OnSetupAllObjects)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Import/Export Section
    PropertiesContainer->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[SNew(STextBlock)
                  .Text(LOCTEXT("ImportExportLabel", "Import/Export:"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))];

    PropertiesContainer->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("ExportRecorderInfoButton", "Export Recorder Info"))
             .OnClicked(this, &SKeyRipplePropertiesPanel::OnExportRecorderInfo)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    PropertiesContainer->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("ImportRecorderInfoButton", "Import Recorder Info"))
             .OnClicked(this, &SKeyRipplePropertiesPanel::OnImportRecorderInfo)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

TSharedRef<SWidget> SKeyRipplePropertiesPanel::CreateNumericPropertyRow(
    const FString& PropertyName, int32 Value, const FString& PropertyPath) {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(PropertyName))
                         .MinDesiredWidth(150.0f)] +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               5.0f, 0.0f)[SNew(SSpinBox<int32>)
                               .Value(Value)
                               .OnValueChanged_Lambda(
                                   [this, PropertyPath](int32 NewValue) {
                                       OnNumericPropertyChanged(PropertyPath,
                                                                NewValue);
                                   })
                               .MinValue(-10000)
                               .MaxValue(10000)
                               .Delta(1)];
}

TSharedRef<SWidget> SKeyRipplePropertiesPanel::CreateStringPropertyRow(
    const FString& PropertyName, const FString& Value,
    const FString& PropertyPath) {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(PropertyName))
                         .MinDesiredWidth(150.0f)] +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               5.0f, 0.0f)[SNew(SEditableTextBox)
                               .Text(FText::FromString(Value))
                               .OnTextCommitted_Lambda(
                                   [this, PropertyPath](
                                       const FText& InText,
                                       ETextCommit::Type CommitType) {
                                       if (CommitType == ETextCommit::OnEnter) {
                                           OnStringPropertyChanged(PropertyPath,
                                                                   InText);
                                       }
                                   })];
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
                                           TEXT("EPositionType")) {
                                    if (*NewSelection == TEXT("HIGH"))
                                        NewValue = 0;
                                    else if (*NewSelection == TEXT("LOW"))
                                        NewValue = 1;
                                    else
                                        NewValue = 2;  // MIDDLE
                                }

                                OnEnumPropertyChanged(PropertyPath, NewValue);
                            }
                        })[SNew(STextBlock)
                               // ? 修复：使用 Lambda
                               // 动态获取当前值而不是固定的初始值
                               .Text_Lambda([this, PropertyPath, EnumTypeName,
                                             OptionStrings]() -> FText {
                                   if (!KeyRippleActor.IsValid()) {
                                       return FText::FromString(TEXT(""));
                                   }

                                   AKeyRippleUnreal* KeyRipple =
                                       KeyRippleActor.Get();
                                   uint8 CurrentValue = 0;

                                   // 根据 PropertyPath 获取当前的 enum 值
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

                                   // 返回对应的显示文本
                                   if (OptionStrings->IsValidIndex(
                                           CurrentValue)) {
                                       return FText::FromString(
                                           *OptionStrings->operator[](
                                               CurrentValue));
                                   }

                                   return FText::FromString(TEXT("Unknown"));
                               })]];
}

TSharedRef<SWidget> SKeyRipplePropertiesPanel::CreateFilePathPropertyRow(
    const FString& PropertyName, const FString& FilePath,
    const FString& PropertyPath, const FString& FileExtension) {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(PropertyName))
                         .MinDesiredWidth(150.0f)] +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               5.0f, 0.0f)[SNew(SEditableTextBox)
                               .Text(FText::FromString(FilePath))
                               .OnTextCommitted_Lambda(
                                   [this, PropertyPath](
                                       const FText& InText,
                                       ETextCommit::Type CommitType) {
                                       if (CommitType == ETextCommit::OnEnter) {
                                           OnFilePathChanged(PropertyPath,
                                                             InText.ToString());
                                       }
                                   })] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f, 0.0f, 0.0f,
               0.0f)[SNew(SButton)
                         .Text(LOCTEXT("BrowseButton", "Browse"))
                         .OnClicked_Lambda(
                             [this, PropertyPath, FileExtension]() -> FReply {
                                 FString FilePath;
                                 if (BrowseForFile(FileExtension, FilePath)) {
                                     OnFilePathChanged(PropertyPath, FilePath);
                                     // Refresh to show new path
                                     RefreshPropertyList();
                                 }
                                 return FReply::Handled();
                             })];
}

TSharedRef<SWidget> SKeyRipplePropertiesPanel::CreateVector3PropertyRow(
    const FString& PropertyName, const FVector& Value,
    const FString& PropertyPath) {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(PropertyName))
                         .MinDesiredWidth(150.0f)] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f, 0.0f)[SNew(SSpinBox<float>)
                               .Value(Value.X)
                               .OnValueChanged_Lambda(
                                   [this, PropertyPath, &Value](float NewX) {
                                       OnVector3PropertyChanged(PropertyPath, 0,
                                                                NewX);
                                   })
                               .MinValue(-10000.f)
                               .MaxValue(10000.f)
                               .Delta(0.01f)] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f, 0.0f)[SNew(SSpinBox<float>)
                               .Value(Value.Y)
                               .OnValueChanged_Lambda(
                                   [this, PropertyPath, &Value](float NewY) {
                                       OnVector3PropertyChanged(PropertyPath, 1,
                                                                NewY);
                                   })
                               .MinValue(-10000.f)
                               .MaxValue(10000.f)
                               .Delta(0.01f)] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f, 0.0f)[SNew(SSpinBox<float>)
                               .Value(Value.Z)
                               .OnValueChanged_Lambda(
                                   [this, PropertyPath, &Value](float NewZ) {
                                       OnVector3PropertyChanged(PropertyPath, 2,
                                                                NewZ);
                                   })
                               .MinValue(-10000.f)
                               .MaxValue(10000.f)
                               .Delta(0.01f)];
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

bool SKeyRipplePropertiesPanel::BrowseForFile(const FString& FileExtension,
                                              FString& OutFilePath) {
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) {
        return false;
    }

    FString FileFilter =
        FString::Printf(TEXT("Files (*%s)|*%s|All Files (*.*)|*.*"),
                        *FileExtension, *FileExtension);
    FString DefaultPath = FPaths::ProjectDir();

    TArray<FString> OutFilenames;
    bool bOpened = DesktopPlatform->OpenFileDialog(
        nullptr, FString::Printf(TEXT("Select %s File"), *FileExtension),
        DefaultPath, TEXT(""), FileFilter, EFileDialogFlags::None,
        OutFilenames);

    if (bOpened && OutFilenames.Num() > 0) {
        OutFilePath = OutFilenames[0];
        return true;
    }

    return false;
}

FSlateColor SKeyRipplePropertiesPanel::GetTabButtonColor(
    bool bIsPropertiesTab) const {
    // 由于我们不再使用ButtonColorAndOpacity，这里返回默认颜色
    return FSlateColor::UseForeground();
}

FLinearColor SKeyRipplePropertiesPanel::GetTabButtonTextColor(
    bool bIsPropertiesTab) const {
    bool bIsActive =
        (bIsPropertiesTab && ActiveTab == EActiveTab::Properties) ||
        (!bIsPropertiesTab && ActiveTab == EActiveTab::Operations);

    if (bIsActive) {
        // 返回激活状态的颜色（蓝色）
        return FLinearColor(0.0f, 112.0f / 255.0f, 220.0f / 255.0f, 1.0f);
    } else {
        // 返回非激活状态的颜色（灰色）
        return FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
    }
}

FReply SKeyRipplePropertiesPanel::OnPropertiesTabClicked() {
    if (ActiveTab == EActiveTab::Properties) {
        return FReply::Handled();
    }

    ActiveTab = EActiveTab::Properties;

    if (ContentContainer.IsValid()) {
        ContentContainer->ClearChildren();
        ContentContainer->AddSlot().FillHeight(1.0f)
            [SNew(SVerticalBox) +
             SVerticalBox::Slot().AutoHeight().Padding(5.0f)
                 [SNew(STextBlock)
                      .Text(LOCTEXT("PropertiesLabel", "KeyRipple Properties:"))
                      .Font(FAppStyle::GetFontStyle(
                          "DetailsView.CategoryFont"))] +
             SVerticalBox::Slot().FillHeight(1.0f).Padding(
                 5.0f)[SNew(SScrollBox) +
                       SScrollBox::Slot()[PropertiesContainer.ToSharedRef()]]];
    }

    return FReply::Handled();
}

FReply SKeyRipplePropertiesPanel::OnOperationsTabClicked() {
    if (ActiveTab == EActiveTab::Operations) {
        return FReply::Handled();
    }

    ActiveTab = EActiveTab::Operations;

    if (ContentContainer.IsValid()) {
        ContentContainer->ClearChildren();
        ContentContainer->AddSlot().FillHeight(
            1.0f)[OperationsPanel.ToSharedRef()];
    }

    return FReply::Handled();
}

FText SKeyRipplePropertiesPanel::GetPropertiesTabLabel() const {
    return LOCTEXT("PropertiesTabLabel", "Properties");
}

FText SKeyRipplePropertiesPanel::GetOperationsTabLabel() const {
    return LOCTEXT("OperationsTabLabel", "Operations");
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

    // Show confirmation dialog
    EAppReturnType::Type UserConfirm = FMessageDialog::Open(
        EAppMsgType::YesNo,
        FText::FromString(
            TEXT("Are you sure you want to export recorder "
                 "information?\n\nThis will overwrite existing data.")));

    if (UserConfirm == EAppReturnType::Yes) {
        UKeyRippleControlRigProcessor::ExportRecorderInfo(KeyRippleActor.Get());
    }

    return FReply::Handled();
}

FReply SKeyRipplePropertiesPanel::OnImportRecorderInfo() {
    if (!KeyRippleActor.IsValid()) {
        return FReply::Handled();
    }

    // Show confirmation dialog
    EAppReturnType::Type UserConfirm = FMessageDialog::Open(
        EAppMsgType::YesNo,
        FText::FromString(TEXT(
            "Are you sure you want to import recorder information?\n\nThis "
            "will overwrite existing actor properties.")));

    if (UserConfirm == EAppReturnType::Yes) {
        UKeyRippleControlRigProcessor::ImportRecorderInfo(KeyRippleActor.Get());
    }

    return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE