#include "UI/StringFlowModuleOperationsPanel.h"

#include "StringFlowAnimationProcessor.h"
#include "StringFlowControlRigProcessor.h"
#include "StringFlowMusicInstrumentProcessor.h"
#include "StringFlowUnreal.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SStringFlowModuleOperationsPanel"

void SStringFlowModuleOperationsPanel::CreateOperationWidgets() {
    auto Container = GetOperationContainer();
    if (!Container.IsValid()) {
        return;
    }

    Container->ClearChildren();

    if (!StringFlowActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActorSelected",
                                    "No StringFlow Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AStringFlowUnreal* StringFlow = StringFlowActor.Get();

    // Initialize combo box options
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

    // Hand State Configuration Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 0.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Hand State Configuration"))];

    // Right Hand String Index (Full Width)
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(1.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&RightHandStringIndexOptions)
                  .OnSelectionChanged_Lambda(
                      [this](TSharedPtr<FString> NewSelection,
                             ESelectInfo::Type SelectInfo) {
                          if (StringFlowActor.IsValid() &&
                              NewSelection.IsValid()) {
                              AStringFlowUnreal* StringFlow =
                                  StringFlowActor.Get();
                              StringFlow->Modify();
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
                          }
                      })
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                      return SNew(STextBlock).Text(FText::FromString(*Item));
                  })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                      if (!StringFlowActor.IsValid())
                          return FText::FromString(TEXT(""));

                      AStringFlowUnreal* StringFlow = StringFlowActor.Get();
                      FString StringStr;
                      switch (StringFlow->RightHandStringIndex) {
                          case EStringFlowRightHandStringIndex::STRING_0:
                              StringStr = TEXT("STRING_0");
                              break;
                          case EStringFlowRightHandStringIndex::STRING_1:
                              StringStr = TEXT("STRING_1");
                              break;
                          case EStringFlowRightHandStringIndex::STRING_2:
                              StringStr = TEXT("STRING_2");
                              break;
                          default:
                              StringStr = TEXT("STRING_3");
                              break;
                      }
                      return FText::FromString(StringStr);
                  })]]];

    // Left Fret and Right Position (Two columns)
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox)
         // Left column with Left Fret dropdown
         +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SHorizontalBox) +
              SHorizontalBox::Slot().FillWidth(1.0f)
                  [SNew(SComboBox<TSharedPtr<FString>>)
                       .OptionsSource(&LeftHandFretIndexOptions)
                       .OnSelectionChanged_Lambda(
                           [this](TSharedPtr<FString> NewSelection,
                                  ESelectInfo::Type SelectInfo) {
                               if (StringFlowActor.IsValid() &&
                                   NewSelection.IsValid()) {
                                   AStringFlowUnreal* StringFlow =
                                       StringFlowActor.Get();
                                   StringFlow->Modify();
                                   if (*NewSelection == TEXT("FRET_1"))
                                       StringFlow->LeftHandFretIndex =
                                           EStringFlowLeftHandFretIndex::FRET_1;
                                   else if (*NewSelection == TEXT("FRET_9"))
                                       StringFlow->LeftHandFretIndex =
                                           EStringFlowLeftHandFretIndex::FRET_9;
                                   else
                                       StringFlow->LeftHandFretIndex =
                                           EStringFlowLeftHandFretIndex::
                                               FRET_12;
                               }
                           })
                       .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                           return SNew(STextBlock)
                               .Text(FText::FromString(*Item));
                       })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                           if (!StringFlowActor.IsValid())
                               return FText::FromString(TEXT(""));

                           AStringFlowUnreal* StringFlow =
                               StringFlowActor.Get();
                           FString FretStr;
                           switch (StringFlow->LeftHandFretIndex) {
                               case EStringFlowLeftHandFretIndex::FRET_1:
                                   FretStr = TEXT("FRET_1");
                                   break;
                               case EStringFlowLeftHandFretIndex::FRET_9:
                                   FretStr = TEXT("FRET_9");
                                   break;
                               default:
                                   FretStr = TEXT("FRET_12");
                                   break;
                           }
                           return FText::FromString(FretStr);
                       })]]]
         // Right column with Right Position dropdown
         +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&RightHandPositionOptions)
                  .OnSelectionChanged_Lambda(
                      [this](TSharedPtr<FString> NewSelection,
                             ESelectInfo::Type SelectInfo) {
                          if (StringFlowActor.IsValid() &&
                              NewSelection.IsValid()) {
                              AStringFlowUnreal* StringFlow =
                                  StringFlowActor.Get();
                              StringFlow->Modify();
                              if (*NewSelection == TEXT("NEAR"))
                                  StringFlow->RightHandPositionType =
                                      EStringFlowRightHandPositionType::NEAR;
                              else if (*NewSelection == TEXT("FAR"))
                                  StringFlow->RightHandPositionType =
                                      EStringFlowRightHandPositionType::FAR;
                              else
                                  StringFlow->RightHandPositionType =
                                      EStringFlowRightHandPositionType::
                                          PIZZICATO;
                          }
                      })
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                      return SNew(STextBlock).Text(FText::FromString(*Item));
                  })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                      if (!StringFlowActor.IsValid())
                          return FText::FromString(TEXT(""));

                      AStringFlowUnreal* StringFlow = StringFlowActor.Get();
                      FString PosStr;
                      switch (StringFlow->RightHandPositionType) {
                          case EStringFlowRightHandPositionType::NEAR:
                              PosStr = TEXT("NEAR");
                              break;
                          case EStringFlowRightHandPositionType::FAR:
                              PosStr = TEXT("FAR");
                              break;
                          default:
                              PosStr = TEXT("PIZZICATO");
                              break;
                      }
                      return FText::FromString(PosStr);
                  })]]];

    // Save Left and Save Right Buttons
    Container->AddSlot().AutoHeight().Padding(5.0f, 10.0f, 5.0f, 15.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(
             0.0f, 0.0f, 5.0f,
             0.0f)[SNew(SButton)
                       .Text(LOCTEXT("SaveLeftButton", "Save Left"))
                       .OnClicked(this,
                                  &SStringFlowModuleOperationsPanel::OnSaveLeft)
                       .HAlign(HAlign_Center)] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(
             5.0f, 0.0f, 0.0f,
             0.0f)[SNew(SButton)
                       .Text(LOCTEXT("SaveRightButton", "Save Right"))
                       .OnClicked(
                           this, &SStringFlowModuleOperationsPanel::OnSaveRight)
                       .HAlign(HAlign_Center)]];

    // Load State Button
    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("LoadStateButton", "Load State"))
                  .OnClicked(this,
                             &SStringFlowModuleOperationsPanel::OnLoadState)
                  .HAlign(HAlign_Center)];

    // Animation File Path Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Animation File"))];

    TSharedPtr<SEditableTextBox> AnimationFilePathBox;
    Container->AddSlot().AutoHeight().Padding(5.0f)
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
                      if (FCommonPanelUtility::BrowseForFile(
                              TEXT(".string_flow"), FilePath, false)) {
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
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Animation Generation"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GeneratePerformerAnimationButton",
                                "Generate Performer Animation"))
                  .OnClicked(this, &SStringFlowModuleOperationsPanel::
                                       OnGeneratePerformerAnimation)
                  .HAlign(HAlign_Center)];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenerateStringAnimationButton",
                                "Generate String Animation"))
                  .OnClicked(this, &SStringFlowModuleOperationsPanel::
                                       OnGenerateStringAnimation)
                  .HAlign(HAlign_Center)];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenerateAllAnimationButton",
                                "Generate All Animation"))
                  .OnClicked(
                      this,
                      &SStringFlowModuleOperationsPanel::OnGenerateAllAnimation)
                  .HAlign(HAlign_Center)];

    // Control Rig Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        15.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Control Rig"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("InitStringInstrumentButton",
                                "Initialize String Instrument"))
                  .OnClicked(
                      this,
                      &SStringFlowModuleOperationsPanel::OnInitStringInstrument)
                  .HAlign(HAlign_Center)];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("TriggerControlRigReregistrationButton",
                                "Trigger Control Rig Re-registration"))
                  .OnClicked(
                      this,
                      &SStringFlowModuleOperationsPanel::OnTriggerControlRigReregistration)
                  .HAlign(HAlign_Center)];
}

void SStringFlowModuleOperationsPanel::Construct(const FArguments& InArgs) {
    // 调用基类构造函数，使用基类的参数类型
    SModuleOperationsPanel::FArguments BaseArgs;
    SModuleOperationsPanel::Construct(BaseArgs);
}

void SStringFlowModuleOperationsPanel::SetActor(AActor* InActor) {
    StringFlowActor = Cast<AStringFlowUnreal>(InActor);
    RefreshOperations();
}

bool SStringFlowModuleOperationsPanel::CanHandleActor(
    const AActor* InActor) const {
    return Cast<const AStringFlowUnreal>(InActor) != nullptr;
}

void SStringFlowModuleOperationsPanel::RefreshOperations() {
    CreateOperationWidgets();
}

FReply SStringFlowModuleOperationsPanel::OnSaveState() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: No actor selected for save state"));
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::SaveState(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("StringFlow: Save State operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModuleOperationsPanel::OnSaveLeft() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: No actor selected for save left"));
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::SaveLeft(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("StringFlow: Save Left operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModuleOperationsPanel::OnSaveRight() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: No actor selected for save right"));
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::SaveRight(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("StringFlow: Save Right operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModuleOperationsPanel::OnLoadState() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: No actor selected for load state"));
        return FReply::Handled();
    }

    UStringFlowControlRigProcessor::LoadState(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("StringFlow: Load State operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModuleOperationsPanel::OnGeneratePerformerAnimation() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: No actor selected for generate performer animation"));
        return FReply::Handled();
    }

    UStringFlowAnimationProcessor::GeneratePerformerAnimation(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("StringFlow: Generate Performer Animation operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModuleOperationsPanel::OnGenerateStringAnimation() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: No actor selected for generate string animation"));
        return FReply::Handled();
    }

    UStringFlowAnimationProcessor::GenerateInstrumentAnimation(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("StringFlow: Generate String Animation operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModuleOperationsPanel::OnGenerateAllAnimation() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: No actor selected for generate all animation"));
        return FReply::Handled();
    }

    UStringFlowAnimationProcessor::GenerateAllAnimation(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("StringFlow: Generate All Animation operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModuleOperationsPanel::OnInitStringInstrument() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: No actor selected for initialize string instrument"));
        return FReply::Handled();
    }

    UStringFlowMusicInstrumentProcessor::InitializeStringInstrument(StringFlowActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("StringFlow: Initialize String Instrument operation triggered"));
    return FReply::Handled();
}

FReply SStringFlowModuleOperationsPanel::OnTriggerControlRigReregistration() {
    if (!StringFlowActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("StringFlow: No actor selected for ControlRig re-registration"));
        return FReply::Handled();
    }

    StringFlowActor->TriggerControlRigReregistration(TEXT("Manual trigger from UI panel"));
    UE_LOG(LogTemp, Warning, TEXT("StringFlow: Trigger ControlRig Re-registration operation triggered"));
    return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE