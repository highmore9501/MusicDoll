#include "UI/FretDanceModuleOperationsPanel.h"

#include "FretDanceAnimationProcessor.h"
#include "FretDanceControlRigProcessor.h"
#include "FretDanceMusicInstrumentProcessor.h"
#include "FretDanceUnreal.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SFretDanceModuleOperationsPanel"

void SFretDanceModuleOperationsPanel::CreateOperationWidgets() {
    auto Container = GetOperationContainer();
    if (!Container.IsValid()) {
        return;
    }

    Container->ClearChildren();

    if (!FretDanceActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActorSelected",
                                    "No FretDance Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AFretDanceUnreal* FretDance = FretDanceActor.Get();

    // NOTE:
    // The original implementation included UI controls for left/right hand
    // position and fret/string indices copied from other modules. Those
    // enums and pairing logic differ for FretDance and are complex.
    // Per instruction, clear that UI area and leave a placeholder for
    // later rework.

    // Hand State Configuration
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Hand State Configuration"))];

    // 标签行（第一行）
    Container->AddSlot().AutoHeight().Padding(5.0f, 5.0f, 5.0f, 2.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot()
             .FillWidth(1.0f)
             .Padding(5.0f, 0.0f)
             .VAlign(VAlign_Center)[SNew(STextBlock)
                                        .Text(LOCTEXT("LeftBasePositionLabel",
                                                      "Left Base Position:"))
                                        .Justification(ETextJustify::Center)] +
         SHorizontalBox::Slot()
             .FillWidth(1.0f)
             .Padding(5.0f, 0.0f)
             .VAlign(VAlign_Center)[SNew(STextBlock)
                                        .Text(LOCTEXT("LeftHandStateLabel",
                                                      "Left Hand State:"))
                                        .Justification(ETextJustify::Center)] +
         SHorizontalBox::Slot()
             .FillWidth(1.0f)
             .Padding(5.0f, 0.0f)
             .VAlign(VAlign_Center)[SNew(STextBlock)
                                        .Text(LOCTEXT("RightHandStateLabel",
                                                      "Right Hand State:"))
                                        .Justification(ETextJustify::Center)]];

    // 下拉菜单行（第二行）
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox)
         // Left Base Position
         +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&BasePositionOptions)
                  .OnSelectionChanged_Lambda([this](
                                                 TSharedPtr<FString>
                                                     NewSelection,
                                                 ESelectInfo::Type SelectInfo) {
                      if (FretDanceActor.IsValid() && NewSelection.IsValid()) {
                          FretDanceActor->Modify();
                          if (*NewSelection == TEXT("P0"))
                              FretDanceActor->CurrentBasePosition =
                                  EFretDanceBasePosition::P0;
                          else if (*NewSelection == TEXT("P1"))
                              FretDanceActor->CurrentBasePosition =
                                  EFretDanceBasePosition::P1;
                          else if (*NewSelection == TEXT("P2"))
                              FretDanceActor->CurrentBasePosition =
                                  EFretDanceBasePosition::P2;
                          else if (*NewSelection == TEXT("P3"))
                              FretDanceActor->CurrentBasePosition =
                                  EFretDanceBasePosition::P3;
                          else
                              FretDanceActor->CurrentBasePosition =
                                  EFretDanceBasePosition::P4;

                          // 更新左手状态选项
                          UpdateLeftHandStateOptions();

                          // 如果当前选择的左手状态不再有效，切换到第一个有效状态
                          if (!FretDanceActor->IsValidLeftHandCombination(
                                  FretDanceActor->CurrentBasePosition,
                                  FretDanceActor->CurrentLeftHandState)) {
                              for (const auto& Option : LeftHandStateOptions) {
                                  if (Option.IsValid()) {
                                      if (*Option == TEXT("NORMAL"))
                                          FretDanceActor->CurrentLeftHandState =
                                              EFretDanceLeftHandState::NORMAL;
                                      else if (*Option == TEXT("OUTER"))
                                          FretDanceActor->CurrentLeftHandState =
                                              EFretDanceLeftHandState::OUTER;
                                      else if (*Option == TEXT("INNER"))
                                          FretDanceActor->CurrentLeftHandState =
                                              EFretDanceLeftHandState::INNER;
                                      else if (*Option == TEXT("BARRE"))
                                          FretDanceActor->CurrentLeftHandState =
                                              EFretDanceLeftHandState::BARRE;
                                      break;
                                  }
                              }
                          }
                      }
                  })
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                      return SNew(STextBlock).Text(FText::FromString(*Item));
                  })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                      if (!FretDanceActor.IsValid())
                          return FText::FromString(TEXT(""));
                      FString Result;
                      switch (FretDanceActor->CurrentBasePosition) {
                          case EFretDanceBasePosition::P0:
                              Result = TEXT("P0");
                              break;
                          case EFretDanceBasePosition::P1:
                              Result = TEXT("P1");
                              break;
                          case EFretDanceBasePosition::P2:
                              Result = TEXT("P2");
                              break;
                          case EFretDanceBasePosition::P3:
                              Result = TEXT("P3");
                              break;
                          default:
                              Result = TEXT("P4");
                              break;
                      }
                      return FText::FromString(Result);
                  })]]
         // Left Hand State
         + SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               5.0f,
               0.0f)[SNew(SComboBox<TSharedPtr<FString>>)
                         .OptionsSource(&LeftHandStateOptions)
                         .OnSelectionChanged_Lambda(
                             [this](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type SelectInfo) {
                                 if (FretDanceActor.IsValid() &&
                                     NewSelection.IsValid()) {
                                     FretDanceActor->Modify();
                                     if (*NewSelection == TEXT("NORMAL"))
                                         FretDanceActor->CurrentLeftHandState =
                                             EFretDanceLeftHandState::NORMAL;
                                     else if (*NewSelection == TEXT("OUTER"))
                                         FretDanceActor->CurrentLeftHandState =
                                             EFretDanceLeftHandState::OUTER;
                                     else if (*NewSelection == TEXT("INNER"))
                                         FretDanceActor->CurrentLeftHandState =
                                             EFretDanceLeftHandState::INNER;
                                     else
                                         FretDanceActor->CurrentLeftHandState =
                                             EFretDanceLeftHandState::BARRE;
                                 }
                             })
                         .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                             return SNew(STextBlock)
                                 .Text(FText::FromString(*Item));
                         })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                             if (!FretDanceActor.IsValid())
                                 return FText::FromString(TEXT(""));
                             FString Result;
                             switch (FretDanceActor->CurrentLeftHandState) {
                                 case EFretDanceLeftHandState::NORMAL:
                                     Result = TEXT("NORMAL");
                                     break;
                                 case EFretDanceLeftHandState::OUTER:
                                     Result = TEXT("OUTER");
                                     break;
                                 case EFretDanceLeftHandState::INNER:
                                     Result = TEXT("INNER");
                                     break;
                                 default:
                                     Result = TEXT("BARRE");
                                     break;
                             }
                             return FText::FromString(Result);
                         })]]
         // Right Hand State
         + SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               5.0f,
               0.0f)[SNew(SComboBox<TSharedPtr<FString>>)
                         .OptionsSource(&RightHandStateOptions)
                         .OnSelectionChanged_Lambda(
                             [this](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type SelectInfo) {
                                 if (FretDanceActor.IsValid() &&
                                     NewSelection.IsValid()) {
                                     FretDanceActor->Modify();
                                     if (*NewSelection == TEXT("LOW"))
                                         FretDanceActor->CurrentRightHandState =
                                             EFretDanceRightHandState::LOW;
                                     else if (*NewSelection == TEXT("END"))
                                         FretDanceActor->CurrentRightHandState =
                                             EFretDanceRightHandState::END;
                                     else
                                         FretDanceActor->CurrentRightHandState =
                                             EFretDanceRightHandState::HIGH;
                                 }
                             })
                         .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                             return SNew(STextBlock)
                                 .Text(FText::FromString(*Item));
                         })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                             if (!FretDanceActor.IsValid())
                                 return FText::FromString(TEXT(""));
                             FString Result;
                             switch (FretDanceActor->CurrentRightHandState) {
                                 case EFretDanceRightHandState::LOW:
                                     Result = TEXT("LOW");
                                     break;
                                 case EFretDanceRightHandState::END:
                                     Result = TEXT("END");
                                     break;
                                 default:
                                     Result = TEXT("HIGH");
                                     break;
                             }
                             return FText::FromString(Result);
                         })]]];

    // Animation File Path Section (moved above generation UI)
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 0.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Animation File Path"))];

    // State Management Buttons
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("State Management"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("SaveLeftButton", "Save Left"))
                  .OnClicked(this, &SFretDanceModuleOperationsPanel::OnSaveLeft)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(
             5.0f, 0.0f, 0.0f,
             0.0f)[SNew(SButton)
                       .Text(LOCTEXT("SaveRightButton", "Save Right"))
                       .OnClicked(this,
                                  &SFretDanceModuleOperationsPanel::OnSaveRight)
                       .HAlign(HAlign_Center)
                       .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("LoadStateButton", "Load State"))
                  .OnClicked(this,
                             &SFretDanceModuleOperationsPanel::OnLoadState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    TSharedPtr<SEditableTextBox> AnimationFilePathBox;
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
             [SAssignNew(AnimationFilePathBox, SEditableTextBox)
                  .Text_Lambda([this]() -> FText {
                      if (FretDanceActor.IsValid()) {
                          return FText::FromString(
                              FretDanceActor->AnimationFilePath);
                      }
                      return FText::FromString(TEXT(""));
                  })
                  .OnTextCommitted_Lambda([this](const FText& InText,
                                                 ETextCommit::Type CommitType) {
                      if (CommitType == ETextCommit::OnEnter ||
                          CommitType == ETextCommit::OnUserMovedFocus) {
                          if (FretDanceActor.IsValid()) {
                              FretDanceActor->AnimationFilePath =
                                  InText.ToString();
                              FretDanceActor->Modify();
                          }
                      }
                  })] +
         SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("BrowseButton", "Browse"))
                  .OnClicked_Lambda([this, AnimationFilePathBox]() -> FReply {
                      if (!FretDanceActor.IsValid()) {
                          return FReply::Handled();
                      }

                      FString FilePath;
                      if (FCommonPanelUtility::BrowseForFile(TEXT(".json"),
                                                             FilePath, false)) {
                          if (AnimationFilePathBox.IsValid()) {
                              AnimationFilePathBox->SetText(
                                  FText::FromString(FilePath));
                              FretDanceActor->AnimationFilePath = FilePath;
                              FretDanceActor->Modify();
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
                  .OnClicked(this, &SFretDanceModuleOperationsPanel::
                                       OnGeneratePerformerAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenerateStringAnimationButton",
                                "Generate String Animation"))
                  .OnClicked(this, &SFretDanceModuleOperationsPanel::
                                       OnGenerateStringAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenerateAllAnimationButton",
                                "Generate All Animation"))
                  .OnClicked(
                      this,
                      &SFretDanceModuleOperationsPanel::OnGenerateAllAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Control Rig Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        15.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Control Rig"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("InitGuitarInstrumentButton",
                                "Initialize Guitar Instrument"))
                  .OnClicked(
                      this,
                      &SFretDanceModuleOperationsPanel::OnInitGuitarInstrument)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("TriggerControlRigReregistrationButton",
                                "Trigger Control Rig Re-registration"))
                  .OnClicked(this, &SFretDanceModuleOperationsPanel::
                                       OnTriggerControlRigReregistration)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

void SFretDanceModuleOperationsPanel::UpdateLeftHandStateOptions() {
    // 根据当前选择的 P 位置过滤左手状态选项
    LeftHandStateOptions.Empty();

    if (!FretDanceActor.IsValid()) {
        // 如果 Actor 无效，添加所有选项
        LeftHandStateOptions.Add(MakeShareable(new FString(TEXT("NORMAL"))));
        LeftHandStateOptions.Add(MakeShareable(new FString(TEXT("OUTER"))));
        LeftHandStateOptions.Add(MakeShareable(new FString(TEXT("INNER"))));
        LeftHandStateOptions.Add(MakeShareable(new FString(TEXT("BARRE"))));
        return;
    }

    AFretDanceUnreal* FretDance = FretDanceActor.Get();
    EFretDanceBasePosition CurrentPosition = FretDance->CurrentBasePosition;

    // 遍历所有左手状态，使用 IsValidLeftHandCombination 检查有效性
    TArray<TPair<FString, EFretDanceLeftHandState>> AllStates;
    AllStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("NORMAL"), EFretDanceLeftHandState::NORMAL));
    AllStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("OUTER"), EFretDanceLeftHandState::OUTER));
    AllStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("INNER"), EFretDanceLeftHandState::INNER));
    AllStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("BARRE"), EFretDanceLeftHandState::BARRE));

    for (const auto& StatePair : AllStates) {
        // 直接复用业务逻辑方法，避免重复实现
        if (FretDance->IsValidLeftHandCombination(CurrentPosition,
                                                  StatePair.Value)) {
            LeftHandStateOptions.Add(MakeShareable(new FString(StatePair.Key)));
        }
    }
}

void SFretDanceModuleOperationsPanel::Construct(const FArguments& InArgs) {
    // Call base class constructor with base parameter types
    SModuleOperationsPanel::FArguments BaseArgs;
    SModuleOperationsPanel::Construct(BaseArgs);

    // 初始化下拉菜单选项
    // 左手基础位置：P0, P1, P2, P3, P4
    BasePositionOptions.Add(MakeShareable(new FString(TEXT("P0"))));
    BasePositionOptions.Add(MakeShareable(new FString(TEXT("P1"))));
    BasePositionOptions.Add(MakeShareable(new FString(TEXT("P2"))));
    BasePositionOptions.Add(MakeShareable(new FString(TEXT("P3"))));
    BasePositionOptions.Add(MakeShareable(new FString(TEXT("P4"))));

    // 初始化左手状态选项（会根据 P 位置动态过滤）
    UpdateLeftHandStateOptions();

    // 右手状态：LOW, END, HIGH
    RightHandStateOptions.Add(MakeShareable(new FString(TEXT("LOW"))));
    RightHandStateOptions.Add(MakeShareable(new FString(TEXT("END"))));
    RightHandStateOptions.Add(MakeShareable(new FString(TEXT("HIGH"))));
}

void SFretDanceModuleOperationsPanel::SetActor(AActor* InActor) {
    FretDanceActor = Cast<AFretDanceUnreal>(InActor);
    RefreshOperations();
}

bool SFretDanceModuleOperationsPanel::CanHandleActor(
    const AActor* InActor) const {
    return Cast<const AFretDanceUnreal>(InActor) != nullptr;
}

void SFretDanceModuleOperationsPanel::RefreshOperations() {
    // 更新左手状态选项以反映当前的 P 位置选择
    UpdateLeftHandStateOptions();
    CreateOperationWidgets();
}

FReply SFretDanceModuleOperationsPanel::OnSaveLeft() {
    if (!FretDanceActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("FretDance: No actor selected for save left"));
        return FReply::Handled();
    }

    TMap<FString, FTransform> OutState;
    UFretDanceControlRigProcessor::SaveLeftHandState(FretDanceActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("FretDance: Save Left operation triggered"));
    return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnSaveRight() {
    if (!FretDanceActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("FretDance: No actor selected for save right"));
        return FReply::Handled();
    }

    TMap<FString, FTransform> OutState;
    UFretDanceControlRigProcessor::SaveRightHandState(FretDanceActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("FretDance: Save Right operation triggered"));
    return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnLoadState() {
    if (!FretDanceActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("FretDance: No actor selected for load state"));
        return FReply::Handled();
    }

    // Provide an empty state map for loading; caller can adapt to different
    // sources if needed
    TMap<FString, FTransform> StateData;
    UFretDanceControlRigProcessor::LoadState(FretDanceActor.Get(), StateData);
    UE_LOG(LogTemp, Warning, TEXT("FretDance: Load State operation triggered"));
    return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnGeneratePerformerAnimation() {
    if (!FretDanceActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("FretDance: No actor selected for generate performer "
                    "animation"));
        return FReply::Handled();
    }

    UFretDanceAnimationProcessor::GeneratePerformerAnimation(
        FretDanceActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("FretDance: Generate Performer Animation operation triggered"));
    return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnGenerateStringAnimation() {
    if (!FretDanceActor.IsValid()) {
        UE_LOG(
            LogTemp, Error,
            TEXT("FretDance: No actor selected for generate string animation"));
        return FReply::Handled();
    }

    UFretDanceAnimationProcessor::GenerateInstrumentAnimation(
        FretDanceActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("FretDance: Generate String Animation operation triggered"));
    return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnGenerateAllAnimation() {
    if (!FretDanceActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("FretDance: No actor selected for generate all animation"));
        return FReply::Handled();
    }

    UFretDanceAnimationProcessor::GenerateAllAnimation(FretDanceActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("FretDance: Generate All Animation operation triggered"));
    return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnInitGuitarInstrument() {
    if (!FretDanceActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("FretDance: No actor selected for initialize guitar "
                    "instrument"));
        return FReply::Handled();
    }

    UFretDanceMusicInstrumentProcessor::InitializeGuitarInstrument(
        FretDanceActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("FretDance: Initialize Guitar Instrument operation triggered"));
    return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnTriggerControlRigReregistration() {
    if (!FretDanceActor.IsValid()) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "FretDance: No actor selected for ControlRig re-registration"));
        return FReply::Handled();
    }

    FretDanceActor->TriggerControlRigReregistration(
        TEXT("Manual trigger from UI panel"));
    UE_LOG(LogTemp, Warning,
           TEXT("FretDance: Trigger Control Rig Re-registration operation "
                "triggered"));
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
