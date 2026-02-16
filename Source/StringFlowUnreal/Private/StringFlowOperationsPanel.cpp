#include "StringFlowOperationsPanel.h"

#include "DesktopPlatformModule.h"
#include "ISequencer.h"
#include "InstrumentAnimationUtility.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "Misc/MessageDialog.h"
#include "StringFlowAnimationProcessor.h"
#include "StringFlowControlRigProcessor.h"
#include "StringFlowMusicInstrumentProcessor.h"
#include "StringFlowUnreal.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SStringFlowOperationsPanel"

void SStringFlowOperationsPanel::Construct(const FArguments& InArgs) {
    // Initialize persistent option arrays
    LeftHandPositionOptions.Empty();
    LeftHandPositionOptions.Add(MakeShareable(new FString(TEXT("NORMAL"))));
    LeftHandPositionOptions.Add(MakeShareable(new FString(TEXT("INNER"))));
    LeftHandPositionOptions.Add(MakeShareable(new FString(TEXT("OUTER"))));

    RightHandPositionOptions.Empty();
    RightHandPositionOptions.Add(MakeShareable(new FString(TEXT("NEAR"))));
    RightHandPositionOptions.Add(MakeShareable(new FString(TEXT("FAR"))));
    RightHandPositionOptions.Add(MakeShareable(new FString(TEXT("PIZZICATO"))));

    LeftHandFretIndexOptions.Empty();
    LeftHandFretIndexOptions.Add(MakeShareable(new FString(TEXT("FRET_1"))));
    LeftHandFretIndexOptions.Add(MakeShareable(new FString(TEXT("FRET_9"))));
    LeftHandFretIndexOptions.Add(MakeShareable(new FString(TEXT("FRET_12"))));

    RightHandStringIndexOptions.Empty();
    RightHandStringIndexOptions.Add(
        MakeShareable(new FString(TEXT("STRING_0"))));
    RightHandStringIndexOptions.Add(
        MakeShareable(new FString(TEXT("STRING_1"))));
    RightHandStringIndexOptions.Add(
        MakeShareable(new FString(TEXT("STRING_2"))));
    RightHandStringIndexOptions.Add(
        MakeShareable(new FString(TEXT("STRING_3"))));

    ChildSlot
        [SNew(SVerticalBox) +
         SVerticalBox::Slot().AutoHeight().Padding(5.0f)
             [SNew(STextBlock)
                  .Text(LOCTEXT("OperationsLabel", "StringFlow Operations:"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))] +
         SVerticalBox::Slot().FillHeight(1.0f).Padding(
             5.0f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(
                                          OperationsContainer, SVerticalBox)]] +
         SVerticalBox::Slot().AutoHeight().Padding(
             5.0f)[SAssignNew(StatusTextBlock, STextBlock)
                       .Text(this, &SStringFlowOperationsPanel::GetStatusText)
                       .ColorAndOpacity(FLinearColor::Yellow)
                       .AutoWrapText(true)]];
}

TSharedPtr<SWidget> SStringFlowOperationsPanel::GetWidget() {
    return AsShared();
}

void SStringFlowOperationsPanel::SetActor(AActor* InActor) {
    StringFlowActor = Cast<AStringFlowUnreal>(InActor);

    if (!OperationsContainer.IsValid()) {
        return;
    }

    OperationsContainer->ClearChildren();

    if (!StringFlowActor.IsValid()) {
        OperationsContainer->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActorSelected",
                                    "No StringFlow Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AStringFlowUnreal* StringFlow = StringFlowActor.Get();

    // Hand State Configuration Section
    OperationsContainer->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[SNew(STextBlock)
                  .Text(LOCTEXT("HandStateLabel", "Hand State Configuration:"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))];

    // String Index (Full Width, Wider)
    OperationsContainer->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(1.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(GetRightHandStringIndexOptions())
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> InOption) {
                      return SNew(STextBlock)
                          .Text(FText::FromString(
                              InOption.IsValid() ? *InOption : TEXT("")));
                  })
                  .OnSelectionChanged_Lambda(
                      [this](TSharedPtr<FString> NewSelection,
                             ESelectInfo::Type SelectInfo) {
                          if (!StringFlowActor.IsValid() ||
                              !NewSelection.IsValid())
                              return;

                          AStringFlowUnreal* StringFlow = StringFlowActor.Get();
                          if (*NewSelection == TEXT("STRING_0"))
                              StringFlow->RightHandStringIndex =
                                  EStringFlowRightHandStringIndex::STRING_0;
                          else if (*NewSelection == TEXT("STRING_1"))
                              StringFlow->RightHandStringIndex =
                                  EStringFlowRightHandStringIndex::STRING_1;
                          else if (*NewSelection == TEXT("STRING_2"))
                              StringFlow->RightHandStringIndex =
                                  EStringFlowRightHandStringIndex::STRING_2;
                          else
                              StringFlow->RightHandStringIndex =
                                  EStringFlowRightHandStringIndex::STRING_3;
                      })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                      if (!StringFlowActor.IsValid())
                          return FText::FromString(TEXT(""));

                      AStringFlowUnreal* StringFlow = StringFlowActor.Get();
                      FString StringStr =
                          (StringFlow->RightHandStringIndex ==
                           EStringFlowRightHandStringIndex::STRING_0)
                              ? TEXT("STRING_0")
                          : (StringFlow->RightHandStringIndex ==
                             EStringFlowRightHandStringIndex::STRING_1)
                              ? TEXT("STRING_1")
                          : (StringFlow->RightHandStringIndex ==
                             EStringFlowRightHandStringIndex::STRING_2)
                              ? TEXT("STRING_2")
                              : TEXT("STRING_3");
                      return FText::FromString(StringStr);
                  })]]];

    // Left Fret and Right Position (Two columns)
    OperationsContainer->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         // Left column with Left Fret dropdown
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SHorizontalBox) +
              SHorizontalBox::Slot().FillWidth(1.0f)
                  [SNew(SComboBox<TSharedPtr<FString>>)
                       .OptionsSource(GetLeftHandFretIndexOptions())
                       .OnGenerateWidget_Lambda(
                           [](TSharedPtr<FString> InOption) {
                               return SNew(STextBlock)
                                   .Text(FText::FromString(InOption.IsValid()
                                                               ? *InOption
                                                               : TEXT("")));
                           })
                       .OnSelectionChanged_Lambda(
                           [this](TSharedPtr<FString> NewSelection,
                                  ESelectInfo::Type SelectInfo) {
                               if (!StringFlowActor.IsValid() ||
                                   !NewSelection.IsValid())
                                   return;

                               AStringFlowUnreal* StringFlow =
                                   StringFlowActor.Get();
                               if (*NewSelection == TEXT("FRET_1"))
                                   StringFlow->LeftHandFretIndex =
                                       EStringFlowLeftHandFretIndex::FRET_1;
                               else if (*NewSelection == TEXT("FRET_9"))
                                   StringFlow->LeftHandFretIndex =
                                       EStringFlowLeftHandFretIndex::FRET_9;
                               else
                                   StringFlow->LeftHandFretIndex =
                                       EStringFlowLeftHandFretIndex::FRET_12;
                           })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                           if (!StringFlowActor.IsValid())
                               return FText::FromString(TEXT(""));

                           AStringFlowUnreal* StringFlow =
                               StringFlowActor.Get();
                           FString FretStr =
                               (StringFlow->LeftHandFretIndex ==
                                EStringFlowLeftHandFretIndex::FRET_1)
                                   ? TEXT("FRET_1")
                               : (StringFlow->LeftHandFretIndex ==
                                  EStringFlowLeftHandFretIndex::FRET_9)
                                   ? TEXT("FRET_9")
                                   : TEXT("FRET_12");
                           return FText::FromString(FretStr);
                       })]]] +
         // Right column with Right Position dropdown
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(GetRightHandPositionOptions())
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> InOption) {
                      return SNew(STextBlock)
                          .Text(FText::FromString(
                              InOption.IsValid() ? *InOption : TEXT("")));
                  })
                  .OnSelectionChanged_Lambda(
                      [this](TSharedPtr<FString> NewSelection,
                             ESelectInfo::Type SelectInfo) {
                          if (!StringFlowActor.IsValid() ||
                              !NewSelection.IsValid())
                              return;

                          AStringFlowUnreal* StringFlow = StringFlowActor.Get();
                          if (*NewSelection == TEXT("NEAR"))
                              StringFlow->RightHandPositionType =
                                  EStringFlowRightHandPositionType::NEAR;
                          else if (*NewSelection == TEXT("FAR"))
                              StringFlow->RightHandPositionType =
                                  EStringFlowRightHandPositionType::FAR;
                          else
                              StringFlow->RightHandPositionType =
                                  EStringFlowRightHandPositionType::PIZZICATO;
                      })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                      if (!StringFlowActor.IsValid())
                          return FText::FromString(TEXT(""));

                      AStringFlowUnreal* StringFlow = StringFlowActor.Get();
                      FString PosStr = (StringFlow->RightHandPositionType ==
                                        EStringFlowRightHandPositionType::NEAR)
                                           ? TEXT("NEAR")
                                       : (StringFlow->RightHandPositionType ==
                                          EStringFlowRightHandPositionType::FAR)
                                           ? TEXT("FAR")
                                           : TEXT("PIZZICATO");
                      return FText::FromString(PosStr);
                  })]]];

    // Save Left and Save Right Buttons
    OperationsContainer->AddSlot().AutoHeight().Padding(5.0f, 10.0f, 5.0f, 5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(
             0.0f, 0.0f, 5.0f,
             0.0f)[SNew(SButton)
                       .Text(LOCTEXT("SaveLeftButton", "Save Left"))
                       .OnClicked(this, &SStringFlowOperationsPanel::OnSaveLeft)
                       .HAlign(HAlign_Center)
                       .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("SaveRightButton", "Save Right"))
                  .OnClicked(this, &SStringFlowOperationsPanel::OnSaveRight)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

    // Load State Button
    OperationsContainer->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("LoadStateButton", "Load State"))
                  .OnClicked(this, &SStringFlowOperationsPanel::OnLoadState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // File Path Section
    OperationsContainer->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[SNew(STextBlock)
                  .Text(LOCTEXT("AnimationFileLabel", "Animation File:"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))];

    TSharedPtr<SEditableTextBox> AnimationFilePathBox;
    OperationsContainer->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
             [SAssignNew(AnimationFilePathBox, SEditableTextBox)
                  .Text_Lambda([this]() -> FText {
                      if (StringFlowActor.IsValid()) {
                          return FText::FromString(
                              StringFlowActor->AnimationFilePath);
                      }
                      return FText::FromString(TEXT(""));
                  })
                  .OnTextCommitted_Lambda([this](const FText& InText,
                                                 ETextCommit::Type CommitType) {
                      if (CommitType == ETextCommit::OnEnter ||
                          CommitType == ETextCommit::OnUserMovedFocus) {
                          if (StringFlowActor.IsValid()) {
                              StringFlowActor->AnimationFilePath =
                                  InText.ToString();
                              StringFlowActor->Modify();
                          }
                      }
                  })] +
         SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("BrowseButton", "Browse"))
                  .OnClicked_Lambda([this, AnimationFilePathBox]() -> FReply {
                      if (!StringFlowActor.IsValid()) {
                          return FReply::Handled();
                      }

                      FString FilePath;
                      if (BrowseForFile(TEXT(".string_flow"), FilePath)) {
                          if (AnimationFilePathBox.IsValid()) {
                              AnimationFilePathBox->SetText(
                                  FText::FromString(FilePath));
                              StringFlowActor->AnimationFilePath = FilePath;
                              StringFlowActor->Modify();
                          }
                      }
                      return FReply::Handled();
                  })]];

    // Animation Generation Section
    OperationsContainer->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[SNew(STextBlock)
                  .Text(LOCTEXT("AnimationGenerationLabel",
                                "Animation Generation:"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))];

    OperationsContainer->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GeneratePerformerAnimationButton",
                                "Generate Performer Animation"))
                  .OnClicked(
                      this,
                      &SStringFlowOperationsPanel::OnGeneratePerformerAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    OperationsContainer->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenerateInstrumentAnimationButton",
                                "Generate Instrument Animation"))
                  .OnClicked(this, &SStringFlowOperationsPanel::
                                       OnGenerateInstrumentAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    OperationsContainer->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenerateAllAnimationButton",
                                "Generate All Animation"))
                  .OnClicked(
                      this, &SStringFlowOperationsPanel::OnGenerateAllAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Maintenance Section
    OperationsContainer->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[SNew(STextBlock)
                  .Text(LOCTEXT("MaintenanceLabel", "Maintenance:"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))];

    OperationsContainer->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("InitializeStringInstrumentButton",
                                "Initialize String Instrument"))
                  .OnClicked(this, &SStringFlowOperationsPanel::
                                       OnInitializeStringInstrument)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

bool SStringFlowOperationsPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AStringFlowUnreal>();
}

FReply SStringFlowOperationsPanel::OnStringFlowFilePathBrowse() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No StringFlow actor selected");
        return FReply::Handled();
    }

    FString FilePath;
    if (BrowseForFile(TEXT(".string_flow"), FilePath)) {
        StringFlowActor->AnimationFilePath = FilePath;
        StringFlowActor->Modify();
    }

    return FReply::Handled();
}

bool SStringFlowOperationsPanel::BrowseForFile(const FString& FileExtension,
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

FReply SStringFlowOperationsPanel::OnSaveState() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No StringFlow actor selected");
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::SaveState(StringFlowActor.Get());
    LastStatusMessage = TEXT("Saving state...");
    return FReply::Handled();
}

FReply SStringFlowOperationsPanel::OnSaveLeft() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No StringFlow actor selected");
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::SaveLeft(StringFlowActor.Get());
    LastStatusMessage = TEXT("Saving left hand state...");
    return FReply::Handled();
}

FReply SStringFlowOperationsPanel::OnSaveRight() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No StringFlow actor selected");
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::SaveRight(StringFlowActor.Get());
    LastStatusMessage = TEXT("Saving right hand state...");
    return FReply::Handled();
}

FReply SStringFlowOperationsPanel::OnLoadState() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No StringFlow actor selected");
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::LoadState(StringFlowActor.Get());
    LastStatusMessage = TEXT("Loading state...");
    return FReply::Handled();
}

FReply SStringFlowOperationsPanel::OnGeneratePerformerAnimation() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("No actor selected");
        return FReply::Handled();
    }

    LastStatusMessage = TEXT("Generating performer animation...");
    UStringFlowAnimationProcessor::GeneratePerformerAnimation(
        StringFlowActor.Get());
    LastStatusMessage = TEXT("Performer animation generation complete");

    return FReply::Handled();
}

FReply SStringFlowOperationsPanel::OnGenerateInstrumentAnimation() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("No actor selected");
        return FReply::Handled();
    }

    if (StringFlowActor->AnimationFilePath.IsEmpty()) {
        LastStatusMessage = TEXT("Animation file path not set");
        return FReply::Handled();
    }

    LastStatusMessage = TEXT("Generating instrument animation...");
    UStringFlowAnimationProcessor::GenerateInstrumentAnimation(
        StringFlowActor.Get());
    LastStatusMessage = TEXT("Instrument animation generation complete");

    return FReply::Handled();
}

FReply SStringFlowOperationsPanel::OnGenerateAllAnimation() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("No actor selected");
        return FReply::Handled();
    }

    LastStatusMessage = TEXT("Generating all animations...");
    UStringFlowAnimationProcessor::GenerateAllAnimation(StringFlowActor.Get());
    LastStatusMessage = TEXT("All animations generation complete");

    return FReply::Handled();
}

FReply SStringFlowOperationsPanel::OnClearStringControlRigKeyframes() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("No actor selected");
        return FReply::Handled();
    }

    EAppReturnType::Type UserConfirm = FMessageDialog::Open(
        EAppMsgType::YesNo,
        FText::FromString(TEXT("Are you sure you want to clear all Control Rig "
                               "keyframes?\n\nThis action cannot be undone.")));

    if (UserConfirm == EAppReturnType::Yes) {
        LastStatusMessage = TEXT("Clearing keyframes...");

        // Get Level Sequence and Control Rig Instance
        UControlRig* ControlRigInstance = nullptr;
        UControlRigBlueprint* ControlRigBlueprint = nullptr;

        if (!UStringFlowControlRigProcessor::GetControlRigFromStringInstrument(
                StringFlowActor->StringInstrument, ControlRigInstance,
                ControlRigBlueprint)) {
            LastStatusMessage =
                TEXT("Error: Failed to get Control Rig from StringInstrument");
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

        // Call the clear keyframes function using Common module's method
        TSet<FString> ControlNamesToClean;
        
        // Collect left hand controllers
        for (const auto& Pair : StringFlowActor->LeftFingerControllers)
        {
            ControlNamesToClean.Add(Pair.Value);
        }
        for (const auto& Pair : StringFlowActor->LeftHandControllers)
        {
            ControlNamesToClean.Add(Pair.Value);
        }

        // Collect right hand controllers
        for (const auto& Pair : StringFlowActor->RightFingerControllers)
        {
            ControlNamesToClean.Add(Pair.Value);
        }
        for (const auto& Pair : StringFlowActor->RightHandControllers)
        {
            ControlNamesToClean.Add(Pair.Value);
        }

        // Collect other controllers
        for (const auto& Pair : StringFlowActor->OtherControllers)
        {
            ControlNamesToClean.Add(Pair.Value);
        }

        // Use Common module's method to clear keyframes
        UInstrumentAnimationUtility::ClearControlRigKeyframes(
            LevelSequence, ControlRigInstance, ControlNamesToClean);
        LastStatusMessage = TEXT("Control Rig keyframes cleared successfully");
    }

    return FReply::Handled();
}

FReply SStringFlowOperationsPanel::OnInitializeStringInstrument() {
    if (!StringFlowActor.IsValid()) {
        LastStatusMessage = TEXT("Error: No StringFlow actor selected");
        return FReply::Handled();
    }

    EAppReturnType::Type UserConfirm = FMessageDialog::Open(
        EAppMsgType::YesNo,
        FText::FromString(TEXT("Initialize String Instrument?\n\nThis will:\n"
                               "- Clean existing animations\n"
                               "- Initialize string materials\n"
                               "- Setup Control Rig channels\n"
                               "- Create material animation tracks")));

    if (UserConfirm == EAppReturnType::Yes) {
        LastStatusMessage = TEXT("Initializing String Instrument...");
        
        // 调用StringFlowMusicInstrumentProcessor的初始化方法
        UStringFlowMusicInstrumentProcessor::InitializeStringInstrument(StringFlowActor.Get());
        
        LastStatusMessage = TEXT("String Instrument initialized successfully");
    }

    return FReply::Handled();
}

FText SStringFlowOperationsPanel::GetStatusText() const {
    return FText::FromString(LastStatusMessage);
}

// Persistent getter implementations
TArray<TSharedPtr<FString>>*
SStringFlowOperationsPanel::GetLeftHandPositionOptions() {
    return &LeftHandPositionOptions;
}

TArray<TSharedPtr<FString>>*
SStringFlowOperationsPanel::GetRightHandPositionOptions() {
    return &RightHandPositionOptions;
}

TArray<TSharedPtr<FString>>*
SStringFlowOperationsPanel::GetLeftHandFretIndexOptions() {
    return &LeftHandFretIndexOptions;
}

TArray<TSharedPtr<FString>>*
SStringFlowOperationsPanel::GetRightHandStringIndexOptions() {
    return &RightHandStringIndexOptions;
}

#undef LOCTEXT_NAMESPACE
