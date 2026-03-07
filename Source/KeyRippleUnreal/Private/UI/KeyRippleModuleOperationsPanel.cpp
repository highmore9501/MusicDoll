#include "UI/KeyRippleModuleOperationsPanel.h"

#include "ControlRigCacheSubsystem.h"
#include "KeyRippleControlRigProcessor.h"
#include "KeyRipplePianoProcessor.h"
#include "KeyRippleUnreal.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SKeyRippleModuleOperationsPanel"

void SKeyRippleModuleOperationsPanel::Construct(const FArguments& InArgs) {
    // 调用基类构造函数，使用基类的参数类型
    SModuleOperationsPanel::FArguments BaseArgs;
    SModuleOperationsPanel::Construct(BaseArgs);
}

void SKeyRippleModuleOperationsPanel::SetActor(AActor* InActor) {
    KeyRippleActor = Cast<AKeyRippleUnreal>(InActor);
    RefreshOperations();
}

bool SKeyRippleModuleOperationsPanel::CanHandleActor(
    const AActor* InActor) const {
    return InActor && InActor->IsA<AKeyRippleUnreal>();
}

void SKeyRippleModuleOperationsPanel::RefreshOperations() {
    CreateOperationWidgets();
}

void SKeyRippleModuleOperationsPanel::CreateOperationWidgets() {
    auto Container = GetOperationContainer();
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

    // Initialize combo box options
    KeyTypeOptions.Empty();
    KeyTypeOptions.Add(MakeShareable(new FString(TEXT("WHITE"))));
    KeyTypeOptions.Add(MakeShareable(new FString(TEXT("BLACK"))));

    PositionTypeOptions.Empty();
    PositionTypeOptions.Add(MakeShareable(new FString(TEXT("HIGH"))));
    PositionTypeOptions.Add(MakeShareable(new FString(TEXT("MIDDLE"))));
    PositionTypeOptions.Add(MakeShareable(new FString(TEXT("LOW"))));

    // Hand State Configuration Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 0.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Hand State Configuration"))];

    // Left Hand Configuration
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().AutoWidth().Padding(
             5.0f)[SNew(STextBlock)
                       .Text(FText::FromString(TEXT("Left Hand Key Type:")))] +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&KeyTypeOptions)
                  .OnSelectionChanged_Lambda(
                      [this](TSharedPtr<FString> NewSelection,
                             ESelectInfo::Type SelectInfo) {
                          if (KeyRippleActor.IsValid() &&
                              NewSelection.IsValid()) {
                              AKeyRippleUnreal* KeyRipple =
                                  KeyRippleActor.Get();
                              KeyRipple->Modify();
                              if (*NewSelection == TEXT("WHITE"))
                                  KeyRipple->LeftHandKeyType = EKeyType::WHITE;
                              else
                                  KeyRipple->LeftHandKeyType = EKeyType::BLACK;
                          }
                      })
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                      return SNew(STextBlock).Text(FText::FromString(*Item));
                  })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                      if (!KeyRippleActor.IsValid())
                          return FText::FromString(TEXT(""));

                      AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
                      FString KeyStr =
                          (KeyRipple->LeftHandKeyType == EKeyType::WHITE)
                              ? TEXT("WHITE")
                              : TEXT("BLACK");
                      return FText::FromString(KeyStr);
                  })]]];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().AutoWidth().Padding(
             5.0f)[SNew(STextBlock)
                       .Text(FText::FromString(TEXT("Left Hand Position:")))] +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(
             5.0f,
             0.0f)[SNew(SComboBox<TSharedPtr<FString>>)
                       .OptionsSource(&PositionTypeOptions)
                       .OnSelectionChanged_Lambda(
                           [this](TSharedPtr<FString> NewSelection,
                                  ESelectInfo::Type SelectInfo) {
                               if (KeyRippleActor.IsValid() &&
                                   NewSelection.IsValid()) {
                                   AKeyRippleUnreal* KeyRipple =
                                       KeyRippleActor.Get();
                                   KeyRipple->Modify();
                                   if (*NewSelection == TEXT("HIGH"))
                                       KeyRipple->LeftHandPositionType =
                                           EPositionType::HIGH;
                                   else if (*NewSelection == TEXT("LOW"))
                                       KeyRipple->LeftHandPositionType =
                                           EPositionType::LOW;
                                   else
                                       KeyRipple->LeftHandPositionType =
                                           EPositionType::MIDDLE;
                               }
                           })
                       .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                           return SNew(STextBlock)
                               .Text(FText::FromString(*Item));
                       })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                           if (!KeyRippleActor.IsValid())
                               return FText::FromString(TEXT(""));

                           AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
                           FString PosStr;
                           switch (KeyRipple->LeftHandPositionType) {
                               case EPositionType::HIGH:
                                   PosStr = TEXT("HIGH");
                                   break;
                               case EPositionType::LOW:
                                   PosStr = TEXT("LOW");
                                   break;
                               default:
                                   PosStr = TEXT("MIDDLE");
                                   break;
                           }
                           return FText::FromString(PosStr);
                       })]]];

    // Right Hand Configuration
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().AutoWidth().Padding(
             5.0f)[SNew(STextBlock)
                       .Text(FText::FromString(TEXT("Right Hand Key Type:")))] +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&KeyTypeOptions)
                  .OnSelectionChanged_Lambda(
                      [this](TSharedPtr<FString> NewSelection,
                             ESelectInfo::Type SelectInfo) {
                          if (KeyRippleActor.IsValid() &&
                              NewSelection.IsValid()) {
                              AKeyRippleUnreal* KeyRipple =
                                  KeyRippleActor.Get();
                              KeyRipple->Modify();
                              if (*NewSelection == TEXT("WHITE"))
                                  KeyRipple->RightHandKeyType = EKeyType::WHITE;
                              else
                                  KeyRipple->RightHandKeyType = EKeyType::BLACK;
                          }
                      })
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                      return SNew(STextBlock).Text(FText::FromString(*Item));
                  })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                      if (!KeyRippleActor.IsValid())
                          return FText::FromString(TEXT(""));

                      AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
                      FString KeyStr =
                          (KeyRipple->RightHandKeyType == EKeyType::WHITE)
                              ? TEXT("WHITE")
                              : TEXT("BLACK");
                      return FText::FromString(KeyStr);
                  })]]];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().AutoWidth().Padding(
             5.0f)[SNew(STextBlock)
                       .Text(FText::FromString(TEXT("Right Hand Position:")))] +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(
             5.0f,
             0.0f)[SNew(SComboBox<TSharedPtr<FString>>)
                       .OptionsSource(&PositionTypeOptions)
                       .OnSelectionChanged_Lambda(
                           [this](TSharedPtr<FString> NewSelection,
                                  ESelectInfo::Type SelectInfo) {
                               if (KeyRippleActor.IsValid() &&
                                   NewSelection.IsValid()) {
                                   AKeyRippleUnreal* KeyRipple =
                                       KeyRippleActor.Get();
                                   KeyRipple->Modify();
                                   if (*NewSelection == TEXT("HIGH"))
                                       KeyRipple->RightHandPositionType =
                                           EPositionType::HIGH;
                                   else if (*NewSelection == TEXT("LOW"))
                                       KeyRipple->RightHandPositionType =
                                           EPositionType::LOW;
                                   else
                                       KeyRipple->RightHandPositionType =
                                           EPositionType::MIDDLE;
                               }
                           })
                       .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                           return SNew(STextBlock)
                               .Text(FText::FromString(*Item));
                       })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                           if (!KeyRippleActor.IsValid())
                               return FText::FromString(TEXT(""));

                           AKeyRippleUnreal* KeyRipple = KeyRippleActor.Get();
                           FString PosStr;
                           switch (KeyRipple->RightHandPositionType) {
                               case EPositionType::HIGH:
                                   PosStr = TEXT("HIGH");
                                   break;
                               case EPositionType::LOW:
                                   PosStr = TEXT("LOW");
                                   break;
                               default:
                                   PosStr = TEXT("MIDDLE");
                                   break;
                           }
                           return FText::FromString(PosStr);
                       })]]];

    // State Management Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("State Management"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SaveStateButton", "Save Current State"))
                  .OnClicked(this,
                             &SKeyRippleModuleOperationsPanel::OnSaveState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("LoadStateButton", "Load Saved State"))
                  .OnClicked(this,
                             &SKeyRippleModuleOperationsPanel::OnLoadState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

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
                      if (KeyRippleActor.IsValid()) {
                          return FText::FromString(
                              KeyRippleActor->AnimationFilePath);
                      }
                      return FText::FromString(TEXT(""));
                  })
                  .OnTextCommitted_Lambda([this](const FText& InText,
                                                 ETextCommit::Type CommitType) {
                      if (CommitType == ETextCommit::OnEnter ||
                          CommitType == ETextCommit::OnUserMovedFocus) {
                          if (KeyRippleActor.IsValid()) {
                              KeyRippleActor->AnimationFilePath =
                                  InText.ToString();
                              KeyRippleActor->Modify();
                          }
                      }
                  })] +
         SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("BrowseButton", "Browse"))
                  .OnClicked_Lambda([this, AnimationFilePathBox]() -> FReply {
                      if (!KeyRippleActor.IsValid()) {
                          return FReply::Handled();
                      }

                      FString FilePath;
                      if (FCommonPanelUtility::BrowseForFile(TEXT(".keyripple"),
                                                             FilePath, false)) {
                          if (AnimationFilePathBox.IsValid()) {
                              AnimationFilePathBox->SetText(
                                  FText::FromString(FilePath));
                              KeyRippleActor->AnimationFilePath = FilePath;
                              KeyRippleActor->Modify();
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
                  .Text(LOCTEXT("GeneratePerformerButton",
                                "Generate Performer Animation"))
                  .OnClicked(this, &SKeyRippleModuleOperationsPanel::
                                       OnGeneratePerformerAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GeneratePianoKeyButton",
                                "Generate Piano Key Animation"))
                  .OnClicked(this, &SKeyRippleModuleOperationsPanel::
                                       OnGeneratePianoKeyAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenerateAllButton", "Generate All Animations"))
                  .OnClicked(
                      this,
                      &SKeyRippleModuleOperationsPanel::OnGenerateAllAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Control Rig Operations Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Control Rig Operations"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("ClearKeyframesButton",
                                "Clear Control Rig Keyframes"))
                  .OnClicked(this, &SKeyRippleModuleOperationsPanel::
                                       OnClearControlRigKeyframes)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("InitPianoButton", "Initialize Piano"))
                  .OnClicked(this,
                             &SKeyRippleModuleOperationsPanel::OnInitPiano)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

FReply SKeyRippleModuleOperationsPanel::OnSaveState() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for save state"));
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::SaveState(KeyRippleActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("KeyRipple: Save State operation triggered"));
    return FReply::Handled();
}

FReply SKeyRippleModuleOperationsPanel::OnLoadState() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for load state"));
        return FReply::Handled();
    }

    UKeyRippleControlRigProcessor::LoadState(KeyRippleActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("KeyRipple: Load State operation triggered"));
    return FReply::Handled();
}

FReply SKeyRippleModuleOperationsPanel::OnClearControlRigKeyframes() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for clear keyframes"));
        return FReply::Handled();
    }

    // 获取Level Sequence和Control Rig实例
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No Level Sequence is currently open"));
        return FReply::Handled();
    }

    if (!GEngine) {
        UE_LOG(LogTemp, Error, TEXT("KeyRipple: GEngine is not available"));
        return FReply::Handled();
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: ControlRig Cache Subsystem is not available"));
        return FReply::Handled();
    }

    UControlRig* ControlRigInstance = CacheSubsystem->GetControlRig(
        KeyRippleActor->SkeletalMeshActor, LevelSequence);

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: Failed to get Control Rig from Subsystem"));
        return FReply::Handled();
    }

    UKeyRippleAnimationProcessor::ClearControlRigKeyframes(
        LevelSequence, ControlRigInstance, KeyRippleActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("KeyRipple: Clear Control Rig Keyframes operation triggered"));
    return FReply::Handled();
}

FReply SKeyRippleModuleOperationsPanel::OnGeneratePerformerAnimation() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for generate performer "
                    "animation"));
        return FReply::Handled();
    }

    UKeyRippleAnimationProcessor::GeneratePerformerAnimation(
        KeyRippleActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("KeyRipple: Generate Performer Animation operation triggered"));
    return FReply::Handled();
}

FReply SKeyRippleModuleOperationsPanel::OnGeneratePianoKeyAnimation() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for generate piano key "
                    "animation"));
        return FReply::Handled();
    }

    FString AnimationPath;
    FString KeyAnimationPath;

    if (!UKeyRippleAnimationProcessor::ParseKeyRippleFile(
            KeyRippleActor.Get(), AnimationPath, KeyAnimationPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: Failed to parse KeyRipple file"));
        return FReply::Handled();
    }

    if (KeyAnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No piano key animation path in file"));
        return FReply::Handled();
    }

    UKeyRippleAnimationProcessor::GeneratePianoKeyAnimation(
        KeyRippleActor.Get(), KeyAnimationPath);
    UE_LOG(LogTemp, Warning,
           TEXT("KeyRipple: Generate Piano Key Animation operation triggered"));
    return FReply::Handled();
}

FReply SKeyRippleModuleOperationsPanel::OnGenerateAllAnimation() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for generate all animation"));
        return FReply::Handled();
    }

    UKeyRippleAnimationProcessor::GenerateAllAnimation(KeyRippleActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("KeyRipple: Generate All Animation operation triggered"));
    return FReply::Handled();
}

FReply SKeyRippleModuleOperationsPanel::OnInitPiano() {
    if (!KeyRippleActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRipple: No actor selected for initialize piano"));
        return FReply::Handled();
    }

    UKeyRipplePianoProcessor::InitPiano(KeyRippleActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("KeyRipple: Initialize Piano operation triggered"));
    return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE