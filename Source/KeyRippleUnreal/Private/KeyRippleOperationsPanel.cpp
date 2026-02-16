#include "KeyRippleOperationsPanel.h"

#include "DesktopPlatformModule.h"
#include "ISequencer.h"
#include "KeyRippleAnimationProcessor.h"
#include "KeyRippleControlRigProcessor.h"
#include "KeyRipplePianoProcessor.h"
#include "KeyRippleUnreal.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SKeyRippleOperationsPanel"

void SKeyRippleOperationsPanel::Construct(const FArguments& InArgs) {
    // Initialize persistent option arrays
    KeyTypeOptions.Empty();
    KeyTypeOptions.Add(MakeShareable(new FString(TEXT("WHITE"))));
    KeyTypeOptions.Add(MakeShareable(new FString(TEXT("BLACK"))));

    PositionTypeOptions.Empty();
    PositionTypeOptions.Add(MakeShareable(new FString(TEXT("HIGH"))));
    PositionTypeOptions.Add(MakeShareable(new FString(TEXT("MIDDLE"))));
    PositionTypeOptions.Add(MakeShareable(new FString(TEXT("LOW"))));

    ChildSlot
        [SNew(SVerticalBox) +
         SVerticalBox::Slot().AutoHeight().Padding(5.0f)
             [SNew(STextBlock)
                  .Text(LOCTEXT("OperationsLabel", "KeyRipple Operations:"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))] +
         SVerticalBox::Slot().FillHeight(1.0f).Padding(5.0f)
             [SNew(SScrollBox) +
              SScrollBox::Slot()
                  [SAssignNew(OperationsContainer, SVerticalBox)
                   // Hand State Section
                   + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 15.0f,
                                                               5.0f, 5.0f)
                         [SNew(STextBlock)
                              .Text(LOCTEXT("HandStateLabel", "Hand State:"))
                              .Font(FAppStyle::GetFontStyle(
                                  "DetailsView.CategoryFont"))]
                   // Left Hand Row
                   + SVerticalBox::Slot().AutoHeight().Padding(5.0f)
                         [SNew(SHorizontalBox) +
                          SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                              [SNew(STextBlock)
                                   .Text(LOCTEXT("LeftHandLabel", "Left ->"))
                                   .MinDesiredWidth(50.0f)] +
                          SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f,
                                                                         0.0f)
                              [SNew(SComboBox<TSharedPtr<FString>>)
                                   .OptionsSource(GetKeyTypeOptions())
                                   .OnGenerateWidget_Lambda(
                                       [](TSharedPtr<FString> InOption) {
                                           return SNew(STextBlock)
                                               .Text(FText::FromString(
                                                   InOption.IsValid()
                                                       ? *InOption
                                                       : TEXT("")));
                                       })
                                   .OnSelectionChanged_Lambda(
                                       [this](TSharedPtr<FString> NewSelection,
                                              ESelectInfo::Type SelectInfo) {
                                           if (!KeyRippleActor.IsValid() ||
                                               !NewSelection.IsValid())
                                               return;

                                           AKeyRippleUnreal* KeyRipple =
                                               KeyRippleActor.Get();
                                           if (*NewSelection == TEXT("WHITE"))
                                               KeyRipple->LeftHandKeyType =
                                                   EKeyType::WHITE;
                                           else
                                               KeyRipple->LeftHandKeyType =
                                                   EKeyType::BLACK;
                                       })[SNew(STextBlock)
                                              .Text_Lambda([this]() -> FText {
                                                  if (!KeyRippleActor.IsValid())
                                                      return FText::FromString(
                                                          TEXT(""));

                                                  AKeyRippleUnreal* KeyRipple =
                                                      KeyRippleActor.Get();
                                                  FString KeyStr =
                                                      (KeyRipple
                                                           ->LeftHandKeyType ==
                                                       EKeyType::WHITE)
                                                          ? TEXT("WHITE")
                                                          : TEXT("BLACK");
                                                  return FText::FromString(
                                                      KeyStr);
                                              })]] +
                          SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f,
                                                                         0.0f)
                              [SNew(SComboBox<TSharedPtr<FString>>)
                                   .OptionsSource(GetPositionTypeOptions())
                                   .OnGenerateWidget_Lambda(
                                       [](TSharedPtr<FString> InOption) {
                                           return SNew(STextBlock)
                                               .Text(FText::FromString(
                                                   InOption.IsValid()
                                                       ? *InOption
                                                       : TEXT("")));
                                       })
                                   .OnSelectionChanged_Lambda(
                                       [this](TSharedPtr<FString> NewSelection,
                                              ESelectInfo::Type SelectInfo) {
                                           if (!KeyRippleActor.IsValid() ||
                                               !NewSelection.IsValid())
                                               return;

                                           AKeyRippleUnreal* KeyRipple =
                                               KeyRippleActor.Get();
                                           if (*NewSelection == TEXT("HIGH"))
                                               KeyRipple->LeftHandPositionType =
                                                   EPositionType::HIGH;
                                           else if (*NewSelection ==
                                                    TEXT("LOW"))
                                               KeyRipple->LeftHandPositionType =
                                                   EPositionType::LOW;
                                           else
                                               KeyRipple->LeftHandPositionType =
                                                   EPositionType::MIDDLE;
                                       })
                                       [SNew(STextBlock)
                                            .Text_Lambda([this]() -> FText {
                                                if (!KeyRippleActor.IsValid())
                                                    return FText::FromString(
                                                        TEXT(""));

                                                AKeyRippleUnreal* KeyRipple =
                                                    KeyRippleActor.Get();
                                                FString PosStr =
                                                    (KeyRipple
                                                         ->LeftHandPositionType ==
                                                     EPositionType::HIGH)
                                                        ? TEXT("HIGH")
                                                    : (KeyRipple
                                                           ->LeftHandPositionType ==
                                                       EPositionType::LOW)
                                                        ? TEXT("LOW")
                                                        : TEXT("MIDDLE");
                                                return FText::FromString(
                                                    PosStr);
                                            })]]]
                   // Right Hand Row
                   +
                   SVerticalBox::Slot().AutoHeight().Padding(5.0f)
                       [SNew(SHorizontalBox) +
                        SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                            [SNew(STextBlock)
                                 .Text(LOCTEXT("RightHandLabel", "Right ->"))
                                 .MinDesiredWidth(50.0f)] +
                        SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f,
                                                                       0.0f)
                            [SNew(SComboBox<TSharedPtr<FString>>)
                                 .OptionsSource(GetKeyTypeOptions())
                                 .OnGenerateWidget_Lambda(
                                     [](TSharedPtr<FString> InOption) {
                                         return SNew(STextBlock)
                                             .Text(FText::FromString(
                                                 InOption.IsValid()
                                                     ? *InOption
                                                     : TEXT("")));
                                     })
                                 .OnSelectionChanged_Lambda(
                                     [this](TSharedPtr<FString> NewSelection,
                                            ESelectInfo::Type SelectInfo) {
                                         if (!KeyRippleActor.IsValid() ||
                                             !NewSelection.IsValid())
                                             return;

                                         AKeyRippleUnreal* KeyRipple =
                                             KeyRippleActor.Get();
                                         if (*NewSelection == TEXT("WHITE"))
                                             KeyRipple->RightHandKeyType =
                                                 EKeyType::WHITE;
                                         else
                                             KeyRipple->RightHandKeyType =
                                                 EKeyType::BLACK;
                                     })[SNew(STextBlock)
                                            .Text_Lambda([this]() -> FText {
                                                if (!KeyRippleActor.IsValid())
                                                    return FText::FromString(
                                                        TEXT(""));

                                                AKeyRippleUnreal* KeyRipple =
                                                    KeyRippleActor.Get();
                                                FString KeyStr =
                                                    (KeyRipple
                                                         ->RightHandKeyType ==
                                                     EKeyType::WHITE)
                                                        ? TEXT("WHITE")
                                                        : TEXT("BLACK");
                                                return FText::FromString(
                                                    KeyStr);
                                            })]] +
                        SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f,
                                                                       0.0f)
                            [SNew(SComboBox<TSharedPtr<FString>>)
                                 .OptionsSource(GetPositionTypeOptions())
                                 .OnGenerateWidget_Lambda(
                                     [](TSharedPtr<FString> InOption) {
                                         return SNew(STextBlock)
                                             .Text(FText::FromString(
                                                 InOption.IsValid()
                                                     ? *InOption
                                                     : TEXT("")));
                                     })
                                 .OnSelectionChanged_Lambda(
                                     [this](TSharedPtr<FString> NewSelection,
                                            ESelectInfo::Type SelectInfo) {
                                         if (!KeyRippleActor.IsValid() ||
                                             !NewSelection.IsValid())
                                             return;

                                         AKeyRippleUnreal* KeyRipple =
                                             KeyRippleActor.Get();
                                         if (*NewSelection == TEXT("HIGH"))
                                             KeyRipple->RightHandPositionType =
                                                 EPositionType::HIGH;
                                         else if (*NewSelection == TEXT("LOW"))
                                             KeyRipple->RightHandPositionType =
                                                 EPositionType::LOW;
                                         else
                                             KeyRipple->RightHandPositionType =
                                                 EPositionType::MIDDLE;
                                     })
                                     [SNew(STextBlock)
                                          .Text_Lambda([this]() -> FText {
                                              if (!KeyRippleActor.IsValid())
                                                  return FText::FromString(
                                                      TEXT(""));

                                              AKeyRippleUnreal* KeyRipple =
                                                  KeyRippleActor.Get();
                                              FString PosStr =
                                                  (KeyRipple
                                                       ->RightHandPositionType ==
                                                   EPositionType::HIGH)
                                                      ? TEXT("HIGH")
                                                  : (KeyRipple
                                                         ->RightHandPositionType ==
                                                     EPositionType::LOW)
                                                      ? TEXT("LOW")
                                                      : TEXT("MIDDLE");
                                              return FText::FromString(PosStr);
                                          })]]]

                   // State Management Section
                   + SVerticalBox::Slot().AutoHeight().Padding(
                         5.0f, 15.0f, 5.0f,
                         5.0f)[SNew(STextBlock)
                                   .Text(LOCTEXT("StateManagementLabel",
                                                 "State Management:"))
                                   .Font(FAppStyle::GetFontStyle(
                                       "DetailsView.CategoryFont"))] +
                   SVerticalBox::Slot().AutoHeight().Padding(5.0f)
                       [SNew(SHorizontalBox) +
                        SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.5f)
                            [SNew(SButton)
                                 .Text(LOCTEXT("SaveStateButton", "Save State"))
                                 .OnClicked(this, &SKeyRippleOperationsPanel::
                                                      OnSaveState)
                                 .HAlign(HAlign_Center)
                                 .ButtonStyle(FAppStyle::Get(),
                                              "FlatButton.Default")] +
                        SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.5f)
                            [SNew(SButton)
                                 .Text(LOCTEXT("LoadStateButton", "Load State"))
                                 .OnClicked(this, &SKeyRippleOperationsPanel::
                                                      OnLoadState)
                                 .HAlign(HAlign_Center)
                                 .ButtonStyle(FAppStyle::Get(),
                                              "FlatButton.Default")]]

                   // Animation Section
                   + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 15.0f,
                                                               5.0f, 5.0f)
                         [SNew(STextBlock)
                              .Text(LOCTEXT("AnimationLabel", "Animation:"))
                              .Font(FAppStyle::GetFontStyle(
                                  "DetailsView.CategoryFont"))] +
                   SVerticalBox::Slot().AutoHeight().Padding(5.0f)
                       [SNew(SHorizontalBox) +
                        SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
                            [SNew(SEditableTextBox)
                                 .Text_Lambda([this]() -> FText {
                                     if (!KeyRippleActor.IsValid())
                                         return FText::FromString(TEXT(""));
                                     return FText::FromString(
                                         KeyRippleActor->AnimationFilePath);
                                 })
                                 .OnTextCommitted_Lambda(
                                     [this](const FText& InText,
                                            ETextCommit::Type CommitType) {
                                         if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
                                         {
                                             if (!KeyRippleActor.IsValid()) return;
                                             KeyRippleActor->AnimationFilePath =
                                                 InText.ToString();
                                             KeyRippleActor->Modify();
                                         }
                                     })
                                 .ForegroundColor(
                                     FSlateColor::UseForeground())] +
                        SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f,
                                                                   0.0f, 0.0f)
                            [SNew(SButton)
                                 .Text(LOCTEXT("BrowseButton", "Browse"))
                                 .OnClicked(this, &SKeyRippleOperationsPanel::
                                                      OnKeyRippleFilePathBrowse)
                                 .ButtonStyle(FAppStyle::Get(),
                                              "FlatButton.Default")]] +
                   SVerticalBox::Slot().AutoHeight().Padding(5.0f)
                       [SNew(SButton)
                            .Text(LOCTEXT("GeneratePerformerAnimationButton",
                                          "Generate Performer Animation"))
                            .OnClicked(this, &SKeyRippleOperationsPanel::
                                                 OnGeneratePerformerAnimation)
                            .HAlign(HAlign_Center)
                            .ButtonStyle(FAppStyle::Get(),
                                         "FlatButton.Default")] +
                   SVerticalBox::Slot().AutoHeight().Padding(5.0f)
                       [SNew(SButton)
                            .Text(LOCTEXT("GeneratePianoKeyAnimationButton",
                                          "Generate Piano Key Animation"))
                            .OnClicked(this, &SKeyRippleOperationsPanel::
                                                 OnGeneratePianoKeyAnimation)
                            .HAlign(HAlign_Center)
                            .ButtonStyle(FAppStyle::Get(),
                                         "FlatButton.Default")] +
                   SVerticalBox::Slot().AutoHeight().Padding(
                       5.0f)[SNew(SButton)
                                 .Text(LOCTEXT("GenerateAllAnimationButton",
                                               "Generate All Animation"))
                                 .OnClicked(this, &SKeyRippleOperationsPanel::
                                                      OnGenerateAllAnimation)
                                 .HAlign(HAlign_Center)
                                 .ButtonStyle(FAppStyle::Get(),
                                              "FlatButton.Default")]

                   // Piano Section
                   + SVerticalBox::Slot().AutoHeight().Padding(
                         5.0f, 15.0f, 5.0f,
                         5.0f)[SNew(STextBlock)
                                   .Text(LOCTEXT("PianoLabel", "Piano:"))
                                   .Font(FAppStyle::GetFontStyle(
                                       "DetailsView.CategoryFont"))] +
                   SVerticalBox::Slot().AutoHeight().Padding(5.0f)
                       [SNew(SButton)
                            .Text(LOCTEXT("InitPianoButton", "Init Piano"))
                            .OnClicked(this,
                                       &SKeyRippleOperationsPanel::OnInitPiano)
                            .HAlign(HAlign_Center)
                            .ButtonStyle(FAppStyle::Get(),
                                         "FlatButton.Default")]

                   // Status Display
                   + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 15.0f,
                                                               5.0f, 5.0f)
                         [SAssignNew(StatusTextBlock, STextBlock)
                              .Text(this,
                                    &SKeyRippleOperationsPanel::GetStatusText)
                              .ColorAndOpacity(FLinearColor::Yellow)
                              .AutoWrapText(true)]]]];
}

TSharedPtr<SWidget> SKeyRippleOperationsPanel::GetWidget() {
    return AsShared();
}

void SKeyRippleOperationsPanel::SetActor(AActor* InActor) {
    KeyRippleActor = Cast<AKeyRippleUnreal>(InActor);
    LastStatusMessage = TEXT("Ready");
}

bool SKeyRippleOperationsPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AKeyRippleUnreal>();
}

FReply SKeyRippleOperationsPanel::OnSaveState() {
    if (!KeyRippleActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No KeyRipple actor selected");
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::SaveState(KeyRippleActor.Get());
    LastStatusMessage = TEXT("Saving state...");
    return FReply::Handled();
}

FReply SKeyRippleOperationsPanel::OnLoadState() {
    if (!KeyRippleActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No KeyRipple actor selected");
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::LoadState(KeyRippleActor.Get());
    LastStatusMessage = TEXT("Loading state...");
    return FReply::Handled();
}

FReply SKeyRippleOperationsPanel::OnClearControlRigKeyframes() {
    if (!KeyRippleActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No KeyRipple actor selected");
        return FReply::Handled();
    }

    // Get Level Sequence and Control Rig Instance
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!UKeyRippleControlRigProcessor::GetControlRigFromSkeletalMeshActor(
            KeyRippleActor->SkeletalMeshActor, ControlRigInstance,
            ControlRigBlueprint)) {
        LastStatusMessage =
            TEXT("Error: Failed to get Control Rig from Skeletal Mesh Actor");
        return FReply::Handled();
    }

    // Get the Level Sequence
    ULevelSequence* LevelSequence = nullptr;
    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
        TArray<TWeakPtr<ISequencer>> WeakSequencers =
            FLevelEditorSequencerIntegration::Get().GetSequencers();

        for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
            if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin()) {
                UMovieSceneSequence* RootSequence =
                    Sequencer->GetRootMovieSceneSequence();

                if (!RootSequence) {
                    continue;
                }

                LevelSequence = Cast<ULevelSequence>(RootSequence);
                if (LevelSequence) {
                    break;
                }
            }
        }
    }

    if (!LevelSequence) {
        LastStatusMessage = TEXT("Error: No Level Sequence is open");
        return FReply::Handled();
    }

    // Call the clear keyframes function
    UKeyRippleAnimationProcessor::ClearControlRigKeyframes(
        LevelSequence, ControlRigInstance, KeyRippleActor.Get());
    LastStatusMessage = TEXT("Control Rig keyframes cleared successfully");
    return FReply::Handled();
}

FReply SKeyRippleOperationsPanel::OnGeneratePerformerAnimation() {
    if (!KeyRippleActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No KeyRipple actor selected");
        return FReply::Handled();
    }

    UKeyRippleAnimationProcessor::GeneratePerformerAnimation(
        KeyRippleActor.Get());
    LastStatusMessage = TEXT("Generating performer animation...");
    return FReply::Handled();
}

FReply SKeyRippleOperationsPanel::OnGeneratePianoKeyAnimation() {
    if (!KeyRippleActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No KeyRipple actor selected");
        return FReply::Handled();
    }

    FString AnimationPath;
    FString KeyAnimationPath;

    if (!UKeyRippleAnimationProcessor::ParseKeyRippleFile(
            KeyRippleActor.Get(), AnimationPath, KeyAnimationPath)) {
        LastStatusMessage = TEXT("Error: Failed to parse KeyRipple file");
        return FReply::Handled();
    }

    if (KeyAnimationPath.IsEmpty()) {
        LastStatusMessage = TEXT("Error: No piano key animation path in file");
        return FReply::Handled();
    }

    // Use the new Level Sequencer method instead of the old animation asset
    // method
    UKeyRipplePianoProcessor::GenerateInstrumentAnimation(
        KeyRippleActor.Get(), KeyAnimationPath);
    LastStatusMessage =
        TEXT("Generating piano key animation in Level Sequencer...");
    return FReply::Handled();
}

FReply SKeyRippleOperationsPanel::OnGenerateAllAnimation() {
    if (!KeyRippleActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No KeyRipple actor selected");
        return FReply::Handled();
    }

    UKeyRippleAnimationProcessor::GenerateAllAnimation(KeyRippleActor.Get());
    LastStatusMessage = TEXT("Generating all animation...");
    return FReply::Handled();
}

FReply SKeyRippleOperationsPanel::OnInitPiano() {
    if (!KeyRippleActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No KeyRipple actor selected");
        return FReply::Handled();
    }

    UKeyRipplePianoProcessor::InitPiano(KeyRippleActor.Get());
    LastStatusMessage = TEXT("Initializing piano...");
    return FReply::Handled();
}

FReply SKeyRippleOperationsPanel::OnKeyRippleFilePathBrowse() {
    if (!KeyRippleActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No KeyRipple actor selected");
        return FReply::Handled();
    }

    FString FilePath;
    if (BrowseForFile(TEXT(".keyripple"), FilePath)) {
        KeyRippleActor->AnimationFilePath = FilePath;
    }
    return FReply::Handled();
}

bool SKeyRippleOperationsPanel::BrowseForFile(const FString& FileExtension,
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

FText SKeyRippleOperationsPanel::GetStatusText() const {
    return FText::FromString(LastStatusMessage);
}

// Persistent getter implementations
TArray<TSharedPtr<FString>>* SKeyRippleOperationsPanel::GetKeyTypeOptions() {
    return &KeyTypeOptions;
}

TArray<TSharedPtr<FString>>*
SKeyRippleOperationsPanel::GetPositionTypeOptions() {
    return &PositionTypeOptions;
}

#undef LOCTEXT_NAMESPACE
