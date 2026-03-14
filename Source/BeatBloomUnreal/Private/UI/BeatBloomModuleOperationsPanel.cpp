#include "UI/BeatBloomModuleOperationsPanel.h"

#include "BeatBloomAnimationProcessor.h"
#include "BeatBloomControlRigProcessor.h"
#include "BeatBloomDrumKitProcessor.h"
#include "BeatBloomUnreal.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BeatBloomModuleOperationsPanel"

void SBeatBloomModuleOperationsPanel::Construct(const FArguments& InArgs) {
    // 调用基类构造函数，使用基类的参数类型
    SModuleOperationsPanel::FArguments BaseArgs;
    SModuleOperationsPanel::Construct(BaseArgs);
}

void SBeatBloomModuleOperationsPanel::SetActor(AActor* InActor) {
    BeatBloomActor = Cast<ABeatBloomUnreal>(InActor);
    if (BeatBloomActor.IsValid()) {
        UpdateDrumKitOptions();
    }
}

bool SBeatBloomModuleOperationsPanel::CanHandleActor(
    const AActor* InActor) const {
    return Cast<const ABeatBloomUnreal>(InActor) != nullptr;
}

void SBeatBloomModuleOperationsPanel::CreateOperationWidgets() {
    TSharedPtr<SVerticalBox> Container = GetOperationContainer();

    // Hand State Configuration Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Hand State Configuration"))];

    // 左手和右手并排布局
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox)
         // Left Hand Column
         +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f, 0.0f)[SNew(STextBlock)
                                  .Text(LOCTEXT("LeftHandLabel", "Left Hand"))
                                  .Font(FAppStyle::GetFontStyle(
                                      "DetailsView.CategoryFont"))]
              // Left Hand Drum Kit
              + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
                    [SNew(SComboBox<TSharedPtr<FString>>)
                         .OptionsSource(&LeftHandKitOptions)
                         .OnSelectionChanged_Lambda(
                             [this](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type SelectInfo) {
                                 if (BeatBloomActor.IsValid() &&
                                     NewSelection.IsValid()) {
                                     ABeatBloomUnreal* BeatBloom =
                                         BeatBloomActor.Get();
                                     BeatBloom->Modify();
                                     BeatBloom->CurrentLeftHandDrumKit =
                                         *NewSelection;

                                     // 同步到 Target
                                     BeatBloom->CurrentTargetDrumKit =
                                         *NewSelection;
                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: LeftHandDrumKit "
                                                 "changed to %s, TargetDrumKit "
                                                 "synced"),
                                            **NewSelection);
                                 }
                             })
                         .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                             return SNew(STextBlock)
                                 .Text(FText::FromString(*Item));
                         })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                             if (!BeatBloomActor.IsValid())
                                 return FText::FromString(TEXT(""));
                             return FText::FromString(
                                 BeatBloomActor->CurrentLeftHandDrumKit);
                         })]]
              // Left Hand State
              +
              SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
                  [SNew(SComboBox<TSharedPtr<FString>>)
                       .OptionsSource(&StateOptions)
                       .OnSelectionChanged_Lambda(
                           [this](TSharedPtr<FString> NewSelection,
                                  ESelectInfo::Type SelectInfo) {
                               if (BeatBloomActor.IsValid() &&
                                   NewSelection.IsValid()) {
                                   ABeatBloomUnreal* BeatBloom =
                                       BeatBloomActor.Get();
                                   BeatBloom->Modify();
                                   if (*NewSelection == TEXT("beat"))
                                       BeatBloom->CurrentLeftHandState =
                                           EBeatBloomState::BEAT;
                                   else if (*NewSelection == TEXT("ready"))
                                       BeatBloom->CurrentLeftHandState =
                                           EBeatBloomState::READY;
                                   else
                                       BeatBloom->CurrentLeftHandState =
                                           EBeatBloomState::REST;

                                   // 同步到 Target
                                   BeatBloom->CurrentTargetState =
                                       BeatBloom->CurrentLeftHandState;
                                   UE_LOG(
                                       LogTemp, Warning,
                                       TEXT(
                                           "BeatBloom: LeftHandState "
                                           "changed to %s, TargetState synced"),
                                       **NewSelection);
                               }
                           })
                       .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                           return SNew(STextBlock)
                               .Text(FText::FromString(*Item));
                       })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                           if (!BeatBloomActor.IsValid())
                               return FText::FromString(TEXT(""));
                           return FText::FromString(
                               ABeatBloomUnreal::GetStateString(
                                   BeatBloomActor->CurrentLeftHandState));
                       })]]]

         // Right Hand Column
         +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f, 0.0f)[SNew(STextBlock)
                                  .Text(LOCTEXT("RightHandLabel", "Right Hand"))
                                  .Font(FAppStyle::GetFontStyle(
                                      "DetailsView.CategoryFont"))]
              // Right Hand Drum Kit
              + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
                    [SNew(SComboBox<TSharedPtr<FString>>)
                         .OptionsSource(&RightHandKitOptions)
                         .OnSelectionChanged_Lambda(
                             [this](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type SelectInfo) {
                                 if (BeatBloomActor.IsValid() &&
                                     NewSelection.IsValid()) {
                                     ABeatBloomUnreal* BeatBloom =
                                         BeatBloomActor.Get();
                                     BeatBloom->Modify();
                                     BeatBloom->CurrentRightHandDrumKit =
                                         *NewSelection;

                                     // 同步到 Target
                                     BeatBloom->CurrentTargetDrumKit =
                                         *NewSelection;
                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: RightHandDrumKit "
                                                 "changed to %s, TargetDrumKit "
                                                 "synced"),
                                            **NewSelection);
                                 }
                             })
                         .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                             return SNew(STextBlock)
                                 .Text(FText::FromString(*Item));
                         })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                             if (!BeatBloomActor.IsValid())
                                 return FText::FromString(TEXT(""));
                             return FText::FromString(
                                 BeatBloomActor->CurrentRightHandDrumKit);
                         })]]
              // Right Hand State
              +
              SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
                  [SNew(SComboBox<TSharedPtr<FString>>)
                       .OptionsSource(&StateOptions)
                       .OnSelectionChanged_Lambda(
                           [this](TSharedPtr<FString> NewSelection,
                                  ESelectInfo::Type SelectInfo) {
                               if (BeatBloomActor.IsValid() &&
                                   NewSelection.IsValid()) {
                                   ABeatBloomUnreal* BeatBloom =
                                       BeatBloomActor.Get();
                                   BeatBloom->Modify();
                                   if (*NewSelection == TEXT("beat"))
                                       BeatBloom->CurrentRightHandState =
                                           EBeatBloomState::BEAT;
                                   else if (*NewSelection == TEXT("ready"))
                                       BeatBloom->CurrentRightHandState =
                                           EBeatBloomState::READY;
                                   else
                                       BeatBloom->CurrentRightHandState =
                                           EBeatBloomState::REST;

                                   // 同步到 Target
                                   BeatBloom->CurrentTargetState =
                                       BeatBloom->CurrentRightHandState;
                                   UE_LOG(
                                       LogTemp, Warning,
                                       TEXT(
                                           "BeatBloom: RightHandState "
                                           "changed to %s, TargetState synced"),
                                       **NewSelection);
                               }
                           })
                       .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                           return SNew(STextBlock)
                               .Text(FText::FromString(*Item));
                       })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                           if (!BeatBloomActor.IsValid())
                               return FText::FromString(TEXT(""));
                           return FText::FromString(
                               ABeatBloomUnreal::GetStateString(
                                   BeatBloomActor->CurrentRightHandState));
                       })]]]];

    // Foot State Configuration Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Foot State Configuration"))];

    // 左脚和右脚并排布局
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox)
         // Left Foot Column
         +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f, 0.0f)[SNew(STextBlock)
                                  .Text(LOCTEXT("LeftFootLabel", "Left Foot"))
                                  .Font(FAppStyle::GetFontStyle(
                                      "DetailsView.CategoryFont"))]
              // Left Foot Drum Kit
              + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
                    [SNew(SComboBox<TSharedPtr<FString>>)
                         .OptionsSource(&LeftFootKitOptions)
                         .OnSelectionChanged_Lambda(
                             [this](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type SelectInfo) {
                                 if (BeatBloomActor.IsValid() &&
                                     NewSelection.IsValid()) {
                                     ABeatBloomUnreal* BeatBloom =
                                         BeatBloomActor.Get();
                                     BeatBloom->Modify();
                                     BeatBloom->CurrentLeftFootDrumKit =
                                         *NewSelection;

                                     // 同步到 Target
                                     BeatBloom->CurrentTargetDrumKit =
                                         *NewSelection;
                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: LeftFootDrumKit "
                                                 "changed to %s, TargetDrumKit "
                                                 "synced"),
                                            **NewSelection);
                                 }
                             })
                         .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                             return SNew(STextBlock)
                                 .Text(FText::FromString(*Item));
                         })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                             if (!BeatBloomActor.IsValid())
                                 return FText::FromString(TEXT(""));
                             return FText::FromString(
                                 BeatBloomActor->CurrentLeftFootDrumKit);
                         })]]
              // Left Foot State
              +
              SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
                  [SNew(SComboBox<TSharedPtr<FString>>)
                       .OptionsSource(&StateOptions)
                       .OnSelectionChanged_Lambda(
                           [this](TSharedPtr<FString> NewSelection,
                                  ESelectInfo::Type SelectInfo) {
                               if (BeatBloomActor.IsValid() &&
                                   NewSelection.IsValid()) {
                                   ABeatBloomUnreal* BeatBloom =
                                       BeatBloomActor.Get();
                                   BeatBloom->Modify();
                                   if (*NewSelection == TEXT("beat"))
                                       BeatBloom->CurrentLeftFootState =
                                           EBeatBloomState::BEAT;
                                   else if (*NewSelection == TEXT("ready"))
                                       BeatBloom->CurrentLeftFootState =
                                           EBeatBloomState::READY;
                                   else
                                       BeatBloom->CurrentLeftFootState =
                                           EBeatBloomState::REST;

                                   // 同步到 Target
                                   BeatBloom->CurrentTargetState =
                                       BeatBloom->CurrentLeftFootState;
                                   UE_LOG(
                                       LogTemp, Warning,
                                       TEXT(
                                           "BeatBloom: LeftFootState "
                                           "changed to %s, TargetState synced"),
                                       **NewSelection);
                               }
                           })
                       .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                           return SNew(STextBlock)
                               .Text(FText::FromString(*Item));
                       })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                           if (!BeatBloomActor.IsValid())
                               return FText::FromString(TEXT(""));
                           return FText::FromString(
                               ABeatBloomUnreal::GetStateString(
                                   BeatBloomActor->CurrentLeftFootState));
                       })]]]

         // Right Foot Column
         +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f, 0.0f)[SNew(STextBlock)
                                  .Text(LOCTEXT("RightFootLabel", "Right Foot"))
                                  .Font(FAppStyle::GetFontStyle(
                                      "DetailsView.CategoryFont"))]
              // Right Foot Drum Kit
              + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
                    [SNew(SComboBox<TSharedPtr<FString>>)
                         .OptionsSource(&RightFootKitOptions)
                         .OnSelectionChanged_Lambda(
                             [this](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type SelectInfo) {
                                 if (BeatBloomActor.IsValid() &&
                                     NewSelection.IsValid()) {
                                     ABeatBloomUnreal* BeatBloom =
                                         BeatBloomActor.Get();
                                     BeatBloom->Modify();
                                     BeatBloom->CurrentRightFootDrumKit =
                                         *NewSelection;

                                     // 同步到 Target
                                     BeatBloom->CurrentTargetDrumKit =
                                         *NewSelection;
                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: RightFootDrumKit "
                                                 "changed to %s, TargetDrumKit "
                                                 "synced"),
                                            **NewSelection);
                                 }
                             })
                         .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                             return SNew(STextBlock)
                                 .Text(FText::FromString(*Item));
                         })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                             if (!BeatBloomActor.IsValid())
                                 return FText::FromString(TEXT(""));
                             return FText::FromString(
                                 BeatBloomActor->CurrentRightFootDrumKit);
                         })]]
              // Right Foot State
              +
              SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
                  [SNew(SComboBox<TSharedPtr<FString>>)
                       .OptionsSource(&StateOptions)
                       .OnSelectionChanged_Lambda(
                           [this](TSharedPtr<FString> NewSelection,
                                  ESelectInfo::Type SelectInfo) {
                               if (BeatBloomActor.IsValid() &&
                                   NewSelection.IsValid()) {
                                   ABeatBloomUnreal* BeatBloom =
                                       BeatBloomActor.Get();
                                   BeatBloom->Modify();
                                   if (*NewSelection == TEXT("beat"))
                                       BeatBloom->CurrentRightFootState =
                                           EBeatBloomState::BEAT;
                                   else if (*NewSelection == TEXT("ready"))
                                       BeatBloom->CurrentRightFootState =
                                           EBeatBloomState::READY;
                                   else
                                       BeatBloom->CurrentRightFootState =
                                           EBeatBloomState::REST;

                                   // 同步到 Target
                                   BeatBloom->CurrentTargetState =
                                       BeatBloom->CurrentRightFootState;
                                   UE_LOG(
                                       LogTemp, Warning,
                                       TEXT(
                                           "BeatBloom: RightFootState "
                                           "changed to %s, TargetState synced"),
                                       **NewSelection);
                               }
                           })
                       .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                           return SNew(STextBlock)
                               .Text(FText::FromString(*Item));
                       })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                           if (!BeatBloomActor.IsValid())
                               return FText::FromString(TEXT(""));
                           return FText::FromString(
                               ABeatBloomUnreal::GetStateString(
                                   BeatBloomActor->CurrentRightFootState));
                       })]]]];

    // State Management Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("State Management"))];

    // Save Hand State Button - saves hand state + target state
    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SaveHandButton", "Save Hand State"))
                  .OnClicked(this, &SBeatBloomModuleOperationsPanel::OnSaveHand)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Save Foot State Button - saves foot state + target state
    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SaveFootButton", "Save Foot State"))
                  .OnClicked(this, &SBeatBloomModuleOperationsPanel::OnSaveFoot)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("LoadStateButton", "Load State"))
                  .OnClicked(this,
                             &SBeatBloomModuleOperationsPanel::OnLoadState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Animation Generation Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Animation Generation"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GeneratePerformerAnimationButton",
                                "Generate Performer Animation"))
                  .OnClicked(this, &SBeatBloomModuleOperationsPanel::
                                       OnGeneratePerformerAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenerateDrumKitAnimationButton",
                                "Generate DrumKit Animation"))
                  .OnClicked(this, &SBeatBloomModuleOperationsPanel::
                                       OnGenerateDrumKitAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenerateAllAnimationButton",
                                "Generate All Animation"))
                  .OnClicked(
                      this,
                      &SBeatBloomModuleOperationsPanel::OnGenerateAllAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // Control Rig Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        15.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Control Rig"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("InitDrumKitButton", "Initialize DrumKit"))
                  .OnClicked(this,
                             &SBeatBloomModuleOperationsPanel::OnInitDrumKit)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("TriggerControlRigReregistrationButton",
                                "Trigger Control Rig Re-registration"))
                  .OnClicked(this, &SBeatBloomModuleOperationsPanel::
                                       OnTriggerControlRigReregistration)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

void SBeatBloomModuleOperationsPanel::UpdateDrumKitOptions() {
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: UpdateDrumKitOptions() - Starting update"));

    // 从 BeatBloomActor->DrumKitConfig 动态获取鼓件选项
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: UpdateDrumKitOptions() - BeatBloomActor is not "
                    "valid"));
        return;
    }

    ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: UpdateDrumKitOptions() - Actor: %s, "
                "DrumKitConfig.Components.Count: %d"),
           *BeatBloom->GetActorLabel(),
           BeatBloom->DrumKitConfig.Components.Num());

    // 清空现有选项
    LeftHandKitOptions.Empty();
    RightHandKitOptions.Empty();
    LeftFootKitOptions.Empty();
    RightFootKitOptions.Empty();
    TargetKitOptions.Empty();

    // 添加"休息"选项到所有肢体类型
    TSharedPtr<FString> RestOption = MakeShareable(new FString(TEXT("Rest")));
    LeftHandKitOptions.Add(RestOption);
    RightHandKitOptions.Add(RestOption);
    LeftFootKitOptions.Add(RestOption);
    RightFootKitOptions.Add(RestOption);
    TargetKitOptions.Add(RestOption);
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: UpdateDrumKitOptions() - Added 'Rest' option to "
                "all limbs"));

    // 遍历 DrumKitConfig 中的所有组件
    int32 ComponentIndex = 0;
    for (const FBeatBloomDrumComponent& Component :
         BeatBloom->DrumKitConfig.Components) {
        TSharedPtr<FString> ComponentName =
            MakeShareable(new FString(Component.Name));
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Processing Component[%d] - Name: %s, "
                    "DrivableLimbs.Count: %d"),
               ComponentIndex, *Component.Name, Component.DrivableLimbs.Num());

        // Target 鼓组项目：添加所有组件（参考 Blender插件逻辑）
        TargetKitOptions.Add(ComponentName);
        UE_LOG(
            LogTemp, Verbose,
            TEXT("BeatBloom:   -> Added to TargetKitOptions (all components)"));

        // 根据 DrivableLimbs 分配到对应的肢体选项
        for (const FBeatBloomDrivableLimb& DrivableLimb :
             Component.DrivableLimbs) {
            const FString& Limb = DrivableLimb.Limb;
            UE_LOG(LogTemp, Verbose,
                   TEXT("BeatBloom:   - DrivableLimb: %s, Coefficient: %f"),
                   *Limb, DrivableLimb.Coefficient);

            // 根据肢体名称分配到对应的下拉菜单
            if (Limb.Contains(TEXT("left_hand"), ESearchCase::IgnoreCase)) {
                LeftHandKitOptions.Add(ComponentName);
                UE_LOG(LogTemp, Verbose,
                       TEXT("BeatBloom:     -> Added to LeftHandKitOptions"));
            } else if (Limb.Contains(TEXT("right_hand"),
                                     ESearchCase::IgnoreCase)) {
                RightHandKitOptions.Add(ComponentName);
                UE_LOG(LogTemp, Verbose,
                       TEXT("BeatBloom:     -> Added to RightHandKitOptions"));
            } else if (Limb.Contains(TEXT("left_foot"),
                                     ESearchCase::IgnoreCase)) {
                LeftFootKitOptions.Add(ComponentName);
                UE_LOG(LogTemp, Verbose,
                       TEXT("BeatBloom:     -> Added to LeftFootKitOptions"));
            } else if (Limb.Contains(TEXT("right_foot"),
                                     ESearchCase::IgnoreCase)) {
                RightFootKitOptions.Add(ComponentName);
                UE_LOG(LogTemp, Verbose,
                       TEXT("BeatBloom:     -> Added to RightFootKitOptions"));
            } else {
                UE_LOG(LogTemp, Warning,
                       TEXT("BeatBloom:     -> WARNING: Limb '%s' does not "
                            "match any category"),
                       *Limb);
            }
        }
        ComponentIndex++;
    }

    // 添加特殊动作到 Target 鼓组项目（参考 Blender插件逻辑）
    for (const auto& SpecialAction : BeatBloom->DrumKitConfig.SpecialActions) {
        TSharedPtr<FString> ActionName =
            MakeShareable(new FString(SpecialAction.Name));
        TargetKitOptions.Add(ActionName);
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Added SpecialAction '%s' to TargetKitOptions"),
               *SpecialAction.Name);
    }

    // 初始化 StateOptions: beat/ready/rest
    StateOptions.Empty();
    StateOptions.Add(MakeShareable(new FString(TEXT("beat"))));
    StateOptions.Add(MakeShareable(new FString(TEXT("ready"))));
    StateOptions.Add(MakeShareable(new FString(TEXT("rest"))));
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: UpdateDrumKitOptions() - StateOptions initialized "
                "with 3 states"));

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: UpdateDrumKitOptions() - Final counts - LH:%d, "
                "RH:%d, LF:%d, RF:%d, Target:%d"),
           LeftHandKitOptions.Num(), RightHandKitOptions.Num(),
           LeftFootKitOptions.Num(), RightFootKitOptions.Num(),
           TargetKitOptions.Num());
}

// ===== 状态管理 =====

FReply SBeatBloomModuleOperationsPanel::OnSaveHand() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for save hand state"));
        return FReply::Handled();
    }

    // 保存手部状态
    UBeatBloomControlRigProcessor::SaveHandState(BeatBloomActor.Get());

    // 同时保存目标状态
    UBeatBloomControlRigProcessor::SaveTargetState(BeatBloomActor.Get());

    UE_LOG(
        LogTemp, Warning,
        TEXT("BeatBloom: Save Hand State + Target State operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModuleOperationsPanel::OnSaveFoot() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for save foot state"));
        return FReply::Handled();
    }

    // 保存脚部状态
    UBeatBloomControlRigProcessor::SaveFootState(BeatBloomActor.Get());

    // 同时保存目标状态
    UBeatBloomControlRigProcessor::SaveTargetState(BeatBloomActor.Get());

    UE_LOG(
        LogTemp, Warning,
        TEXT("BeatBloom: Save Foot State + Target State operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModuleOperationsPanel::OnSaveTarget() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for save target state"));
        return FReply::Handled();
    }

    UBeatBloomControlRigProcessor::SaveTargetState(BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Save Target State operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModuleOperationsPanel::OnSaveAll() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for save all states"));
        return FReply::Handled();
    }

    UBeatBloomControlRigProcessor::SaveAllState(BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Save All States operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModuleOperationsPanel::OnLoadState() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for load state"));
        return FReply::Handled();
    }

    UBeatBloomControlRigProcessor::LoadState(BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("BeatBloom: Load State operation triggered"));
    return FReply::Handled();
}

// ===== IO 操作 =====

void SBeatBloomModuleOperationsPanel::RefreshOperations() {
    // 此功能已移至属性面板，操作面板只负责使用已加载的配置
    // 用户应先在属性面板中加载 .drumkit 配置文件
    if (!BeatBloomActor.IsValid()) {
        return;
    }

    ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();

    // 检查是否已加载 drumkit 配置
    if (BeatBloom->DrumKitConfig.Components.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: No drumkit config loaded. Please load a "
                    ".drumkit file in Properties panel first."));
    } else {
        // 刷新下拉菜单选项
        RefreshOperations();
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Drumkit options refreshed from loaded config"));
    }
}

// ===== 动画生成 =====

FReply SBeatBloomModuleOperationsPanel::OnGeneratePerformerAnimation() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for generate performer "
                    "animation"));
        return FReply::Handled();
    }

    UBeatBloomAnimationProcessor::GeneratePerformerAnimation(
        BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Generate Performer Animation operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModuleOperationsPanel::OnGenerateDrumKitAnimation() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "BeatBloom: No actor selected for generate drumkit animation"));
        return FReply::Handled();
    }

    UBeatBloomDrumKitProcessor::GenerateDrumKitAnimation(BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Generate DrumKit Animation operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModuleOperationsPanel::OnGenerateAllAnimation() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for generate all animation"));
        return FReply::Handled();
    }

    UBeatBloomAnimationProcessor::GenerateAllAnimation(BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Generate All Animation operation triggered"));
    return FReply::Handled();
}

// ===== ControlRig 操作 =====

FReply SBeatBloomModuleOperationsPanel::OnInitDrumKit() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for initialize drumkit"));
        return FReply::Handled();
    }

    UBeatBloomDrumKitProcessor::InitializeDrumKit(BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Initialize DrumKit operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModuleOperationsPanel::OnTriggerControlRigReregistration() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for trigger control rig "
                    "reregistration"));
        return FReply::Handled();
    }

    BeatBloomActor.Get()->TriggerControlRigReregistration(
        TEXT("Triggered from Operations Panel"));
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Trigger Control Rig Re-registration operation "
                "triggered"));
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
