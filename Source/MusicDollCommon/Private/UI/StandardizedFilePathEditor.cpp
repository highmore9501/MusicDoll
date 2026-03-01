#include "UI/StandardizedFilePathEditor.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SFilePathEditor::Construct(const FArguments& InArgs) {
    FilePathAttr = InArgs._FilePath;
    FileDescriptionAttr = InArgs._FileDescription;
    FileFilterAttr = InArgs._FileFilter;
    AllowEditingAttr = InArgs._AllowEditing;
    OnFilePathChangedDelegate = InArgs._OnFilePathChanged;

    ChildSlot
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
             [SAssignNew(PathTextBox, SEditableTextBox)
                  .Text_Lambda([this]() -> FText {
                      return FText::FromString(FilePathAttr.Get());
                  })
                  .OnTextCommitted(this, &SFilePathEditor::OnPathTextCommitted)
                  .IsEnabled_Lambda([this]() { return AllowEditingAttr.Get(); })
                  .HintText(FText::FromString(
                      TEXT("Click Browse or enter file path")))] +
         SHorizontalBox::Slot().AutoWidth().Padding(
             5.0f, 0.0f, 0.0f,
             0.0f)[SNew(SButton)
                       .Text(FText::FromString(TEXT("Browse")))
                       .OnClicked(this, &SFilePathEditor::OnBrowseButtonClicked)
                       .IsEnabled_Lambda(
                           [this]() { return AllowEditingAttr.Get(); })]];
}

FReply SFilePathEditor::OnBrowseButtonClicked() {
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform) {
        FString FileFilter = FileFilterAttr.Get();
        FString DefaultPath = FPaths::GetPath(FilePathAttr.Get());

        TArray<FString> OutFilenames;
        bool bOpened = DesktopPlatform->OpenFileDialog(
            nullptr,
            FString::Printf(TEXT("Select %s"), *FileDescriptionAttr.Get()),
            DefaultPath, TEXT(""), FileFilter, EFileDialogFlags::None,
            OutFilenames);

        if (bOpened && OutFilenames.Num() > 0) {
            FString SelectedFile = OutFilenames[0];
            if (OnFilePathChangedDelegate.IsBound()) {
                OnFilePathChangedDelegate.Execute(SelectedFile);
            }

            // Update the text box
            if (PathTextBox.IsValid()) {
                PathTextBox->SetText(FText::FromString(SelectedFile));
            }
        }
    }
    return FReply::Handled();
}

void SFilePathEditor::OnPathTextCommitted(const FText& NewText,
                                          ETextCommit::Type CommitType) {
    if (CommitType == ETextCommit::OnEnter ||
        CommitType == ETextCommit::OnUserMovedFocus) {
        if (OnFilePathChangedDelegate.IsBound()) {
            OnFilePathChangedDelegate.Execute(NewText.ToString());
        }
    }
}