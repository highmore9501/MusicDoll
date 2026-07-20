#include "UI/CommonPanelUtility.h"

#include "DesktopPlatformModule.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> FCommonPanelUtility::CreateNumericPropertyRow(
    const FString& PropertyName, int32 Value, const FString& PropertyPath,
    TFunction<void(const FString&, int32)> OnValueChanged) {
    TSharedPtr<SSpinBox<int32>> SpinBox;

    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(PropertyName))
                         .MinDesiredWidth(150.0f)] +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               5.0f,
               0.0f)[SAssignNew(SpinBox, SSpinBox<int32>)
                         .Value(Value)
                         .MinValue(-10000)
                         .MaxValue(10000)
                         .Delta(1)
                         .OnValueCommitted_Lambda(
                             [PropertyPath, OnValueChanged](
                                 int32 InValue, ETextCommit::Type CommitType) {
                                 if (CommitType == ETextCommit::OnEnter ||
                                     CommitType ==
                                         ETextCommit::OnUserMovedFocus) {
                                     // Execute the callback with property path
                                     // and new value
                                     if (OnValueChanged) {
                                         OnValueChanged(PropertyPath, InValue);
                                     }
                                 }
                             })];
}

TSharedRef<SWidget> FCommonPanelUtility::CreateStringPropertyRow(
    const FString& PropertyName, const FString& Value,
    const FString& PropertyPath,
    TFunction<void(const FString&, const FString&)> OnValueChanged) {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(PropertyName))
                         .MinDesiredWidth(150.0f)] +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               5.0f, 0.0f)[SNew(SEditableTextBox)
                               .Text(FText::FromString(Value))
                               .OnTextCommitted_Lambda(
                                   [PropertyPath, OnValueChanged](
                                       const FText& InText,
                                       ETextCommit::Type CommitType) {
                                       if (CommitType == ETextCommit::OnEnter ||
                                           CommitType ==
                                               ETextCommit::OnUserMovedFocus) {
                                           if (OnValueChanged) {
                                               OnValueChanged(
                                                   PropertyPath,
                                                   InText.ToString());
                                           }
                                       }
                                   })];
}

/**
 * 创建文件路径编辑行 - 增强版本，支持回调接收新路径值
 * 这个版本允许调用者通过 OnPathUpdated 回调接收新的文件路径值
 *
 * @param PropertyName 属性显示名称
 * @param FilePath 当前文件路径
 * @param PropertyPath 属性路径标识符
 * @param FileExtension 文件扩展名过滤（例如 ".json"）
 * @param OnPathUpdated 当路径改变时的回调，接收新的文件路径
 * @param bAllowCreateNew 是否允许创建新文件
 * @return 创建的Widget引用
 */
TSharedRef<SWidget> FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
    const FString& PropertyName, const FString& FilePath,
    const FString& PropertyPath, const FString& FileExtension,
    TFunction<void(const FString&)> OnPathUpdated, bool bAllowCreateNew) {
    TSharedPtr<SEditableTextBox> FilePathBox;

    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(PropertyName))
                         .MinDesiredWidth(150.0f)] +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               5.0f, 0.0f)[SAssignNew(FilePathBox, SEditableTextBox)
                               .Text(FText::FromString(FilePath))
                               .OnTextCommitted_Lambda(
                                   [FilePathBox, OnPathUpdated](
                                       const FText& InText,
                                       ETextCommit::Type CommitType) {
                                       if (CommitType == ETextCommit::OnEnter ||
                                           CommitType ==
                                               ETextCommit::OnUserMovedFocus) {
                                           if (FilePathBox.IsValid() &&
                                               OnPathUpdated) {
                                               OnPathUpdated(InText.ToString());
                                           }
                                       }
                                   })] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f, 0.0f, 0.0f,
               0.0f)[SNew(SButton)
                         .Text(FText::FromString(TEXT("Browse")))
                         .OnClicked_Lambda([FileExtension, FilePathBox,
                                            OnPathUpdated,
                                            bAllowCreateNew]() -> FReply {
                             FString OutFilePath;
                             if (BrowseForFile(FileExtension, OutFilePath,
                                               bAllowCreateNew)) {
                                 if (FilePathBox.IsValid()) {
                                     FilePathBox->SetText(
                                         FText::FromString(OutFilePath));
                                     if (OnPathUpdated) {
                                         OnPathUpdated(OutFilePath);
                                     }
                                 }
                             }
                             return FReply::Handled();
                         })];
};

TSharedRef<SWidget> FCommonPanelUtility::CreateVector3PropertyRow(
    const FString& PropertyName, const FVector& Value,
    const FString& PropertyPath, FSimpleDelegate OnComponentChanged) {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(PropertyName))
                         .MinDesiredWidth(150.0f)] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f, 0.0f)[SNew(SSpinBox<float>)
                               .Value(Value.X)
                               .MinValue(-10000.f)
                               .MaxValue(10000.f)
                               .Delta(0.01f)] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f, 0.0f)[SNew(SSpinBox<float>)
                               .Value(Value.Y)
                               .MinValue(-10000.f)
                               .MaxValue(10000.f)
                               .Delta(0.01f)] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f, 0.0f)[SNew(SSpinBox<float>)
                               .Value(Value.Z)
                               .MinValue(-10000.f)
                               .MaxValue(10000.f)
                               .Delta(0.01f)];
}

TSharedRef<SWidget> FCommonPanelUtility::CreateSectionHeader(
    const FString& SectionTitle) {
    return SNew(STextBlock)
        .Text(FText::FromString(SectionTitle))
        .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"));
}

TSharedRef<SWidget> FCommonPanelUtility::CreateActionButton(
    const FText& ButtonText, FSimpleDelegate OnClicked) {
    return SNew(SButton)
        .Text(ButtonText)
        .HAlign(HAlign_Center)
        .ButtonStyle(FAppStyle::Get(), "FlatButton.Default");
}

bool FCommonPanelUtility::BrowseForFile(const FString& FileExtension,
                                        FString& OutFilePath,
                                        bool bAllowCreateNew) {
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) {
        return false;
    }

    FString FileFilter =
        FString::Printf(TEXT("Files (*%s)|*%s|All Files (*.*)|*.*"),
                        *FileExtension, *FileExtension);
    FString DefaultPath = FPaths::ProjectDir();

    TArray<FString> OutFilenames;
    bool bOpened = false;

    if (bAllowCreateNew) {
        bOpened = DesktopPlatform->SaveFileDialog(
            nullptr,
            FString::Printf(TEXT("Select or Create %s File"), *FileExtension),
            DefaultPath, TEXT(""), FileFilter, EFileDialogFlags::None,
            OutFilenames);
    } else {
        bOpened = DesktopPlatform->OpenFileDialog(
            nullptr, FString::Printf(TEXT("Select %s File"), *FileExtension),
            DefaultPath, TEXT(""), FileFilter, EFileDialogFlags::None,
            OutFilenames);
    }

    if (bOpened && OutFilenames.Num() > 0) {
        OutFilePath = OutFilenames[0];
        return true;
    }

    return false;
}

bool FCommonPanelUtility::ConfirmExportOverwrite(const FString& FilePath) {
    FText Message = FText::Format(
        NSLOCTEXT("CommonPanelUtility", "ConfirmExportOverwrite",
                  "本操作会覆盖下面文件中的所有数据:\n{0}\n\n您确定要继续吗？"),
        FText::FromString(FilePath));

    EAppReturnType::Type Result =
        FMessageDialog::Open(EAppMsgType::YesNo, Message,
                             NSLOCTEXT("CommonPanelUtility",
                                       "ConfirmExportTitle", "Confirm Export"));

    return Result == EAppReturnType::Yes;
}

FLinearColor FCommonPanelUtility::GetTabButtonTextColor(bool bIsActive) {
    if (bIsActive) {
        return FLinearColor(0.0f, 112.0f / 255.0f, 220.0f / 255.0f, 1.0f);
    } else {
        return FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
    }
}

TSharedRef<SWidget> FCommonPanelUtility::CreateTabButtons(
    const FText& PropertiesLabel, const FText& OperationsLabel,
    FSimpleDelegate OnPropertiesClicked, FSimpleDelegate OnOperationsClicked,
    bool bIsPropertiesActive) {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               2.5f,
               0.0f)[SNew(SButton)
                         .Text(PropertiesLabel)
                         .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
                         .ForegroundColor_Lambda([bIsPropertiesActive]() {
                             return GetTabButtonTextColor(bIsPropertiesActive);
                         })] +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               2.5f,
               0.0f)[SNew(SButton)
                         .Text(OperationsLabel)
                         .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
                         .ForegroundColor_Lambda([bIsPropertiesActive]() {
                             return GetTabButtonTextColor(!bIsPropertiesActive);
                         })];
}