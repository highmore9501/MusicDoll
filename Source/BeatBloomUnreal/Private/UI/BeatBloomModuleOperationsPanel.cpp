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

    // 初始化双线性状态选项
    BilinearStateOptions = {MakeShareable(new FString(TEXT("A"))),
                            MakeShareable(new FString(TEXT("B"))),
                            MakeShareable(new FString(TEXT("C"))),
                            MakeShareable(new FString(TEXT("D")))};
    SelectedBilinearState = BilinearStateOptions[0];  // 默认选择 A
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

                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: LeftHandDrumKit "
                                                 "changed to %s"),
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
              + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
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

                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: LeftHandState "
                                                 "changed to %s"),
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

                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: RightHandDrumKit "
                                                 "changed to %s"),
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
              + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
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

                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: RightHandState "
                                                 "changed to %s"),
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

                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: LeftFootDrumKit "
                                                 "changed to %s"),
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
              + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
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

                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: LeftFootState "
                                                 "changed to %s"),
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

                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: RightFootDrumKit "
                                                 "changed to %s"),
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
              + SVerticalBox::Slot().AutoHeight().Padding(5.0f, 2.0f)
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

                                     UE_LOG(LogTemp, Warning,
                                            TEXT("BeatBloom: RightFootState "
                                                 "changed to %s."),
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

    // Bilinear Mapping Helpers Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Bilinear Mapping Helpers"))];

    // 状态选择下拉菜单和保存/加载按钮
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().AutoWidth().Padding(2.0f).VAlign(VAlign_Center)
             [SNew(STextBlock).Text(LOCTEXT("BilinearStateLabel", "State:"))] +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&BilinearStateOptions)  // 使用成员变量
                  .OnGenerateWidget(this, &SBeatBloomModuleOperationsPanel::
                                              OnGenerateBilinearStateWidget)
                  .OnSelectionChanged(
                      this,
                      &SBeatBloomModuleOperationsPanel::OnBilinearStateChanged)
                  .InitiallySelectedItem(SelectedBilinearState)
                  .Content()[SNew(STextBlock).Text_Lambda([this]() {
                      return FText::FromString(*SelectedBilinearState.Get());
                  })]] +
         SHorizontalBox::Slot().AutoWidth().Padding(
             2.0f)[SNew(SButton)
                       .Text(LOCTEXT("SaveBilinearState", "Save"))
                       .ToolTipText(
                           LOCTEXT("SaveBilinearStateTooltip",
                                   "Save current Middle_Hand and Head_Control "
                                   "positions to selected state"))
                       .OnClicked(this, &SBeatBloomModuleOperationsPanel::
                                            OnSaveBilinearHelperState)] +
         SHorizontalBox::Slot().AutoWidth().Padding(
             2.0f)[SNew(SButton)
                       .Text(LOCTEXT("LoadBilinearState", "Load"))
                       .ToolTipText(
                           LOCTEXT("LoadBilinearStateTooltip",
                                   "Auto-detect current Middle_Hand position "
                                   "and load matching Head_Control"))
                       .OnClicked(this, &SBeatBloomModuleOperationsPanel::
                                            OnLoadBilinearHelperState)]];

    // Animation Generation Section
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Animation Generation"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT("Animation File Path"), BeatBloomActor->AnimationFilePath,
        TEXT("AnimationFilePath"), TEXT(".beatbloom"),
        [this](const FString& NewPath) {
            if (BeatBloomActor.IsValid()) {
                BeatBloomActor->Modify();
                BeatBloomActor->AnimationFilePath = NewPath;
            }
        },
        false)];

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

    // 添加"休息"选项到所有肢体类型
    TSharedPtr<FString> RestOption = MakeShareable(new FString(TEXT("Rest")));
    LeftHandKitOptions.Add(RestOption);
    RightHandKitOptions.Add(RestOption);
    LeftFootKitOptions.Add(RestOption);
    RightFootKitOptions.Add(RestOption);
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: UpdateDrumKitOptions() - Added 'Rest' option to "
                "all limbs"));

    // 用于防止同一个名称在同一个列表里重复添加
    TSet<FString> AddedToLH, AddedToRH, AddedToLF, AddedToRF;

    // 辅助函数：为组件或特殊动作按肢体类型添加到对应下拉列表（每个列表去重）
    auto AddOptionForLimbs = [&](const FString& Name,
                                 const TArray<FString>& Limbs) {
        TSharedPtr<FString> OptionName = MakeShareable(new FString(Name));
        for (const FString& Limb : Limbs) {
            if (Limb == TEXT("left_hand")) {
                if (!AddedToLH.Contains(Name)) {
                    LeftHandKitOptions.Add(OptionName);
                    AddedToLH.Add(Name);
                }
            } else if (Limb == TEXT("right_hand")) {
                if (!AddedToRH.Contains(Name)) {
                    RightHandKitOptions.Add(OptionName);
                    AddedToRH.Add(Name);
                }
            } else if (Limb == TEXT("left_foot")) {
                if (!AddedToLF.Contains(Name)) {
                    LeftFootKitOptions.Add(OptionName);
                    AddedToLF.Add(Name);
                }
            } else if (Limb == TEXT("right_foot")) {
                if (!AddedToRF.Contains(Name)) {
                    RightFootKitOptions.Add(OptionName);
                    AddedToRF.Add(Name);
                }
            } else {
                UE_LOG(LogTemp, Warning,
                       TEXT("BeatBloom: '%s' has unrecognized limb '%s'"),
                       *Name, *Limb);
            }
        }
    };

    // 遍历 DrumKitConfig 中的所有组件
    int32 ComponentIndex = 0;
    for (const FBeatBloomDrumComponent& Component :
         BeatBloom->DrumKitConfig.Components) {
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Processing Component[%d] - Name: %s, "
                    "DrivableLimbs.Count: %d"),
               ComponentIndex, *Component.Name, Component.DrivableLimbs.Num());

        TArray<FString> LimbNames;
        for (const FBeatBloomDrivableLimb& DL : Component.DrivableLimbs) {
            LimbNames.Add(DL.Limb);
        }
        AddOptionForLimbs(Component.Name, LimbNames);
        ComponentIndex++;
    }

    // 遍历 DrumKitConfig 中的所有特殊动作（如 Sticks）
    int32 ActionIndex = 0;
    for (const FBeatBloomSpecialAction& Action :
         BeatBloom->DrumKitConfig.SpecialActions) {
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Processing SpecialAction[%d] - Name: %s, "
                    "Limbs.Count: %d"),
               ActionIndex, *Action.Name, Action.Limbs.Num());

        AddOptionForLimbs(Action.Name, Action.Limbs);
        ActionIndex++;
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
                "RH:%d, LF:%d, RF:%d"),
           LeftHandKitOptions.Num(), RightHandKitOptions.Num(),
           LeftFootKitOptions.Num(), RightFootKitOptions.Num());
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

    // 同时保存 Head_Control 状态
    UBeatBloomControlRigProcessor::SaveHeadControlState(BeatBloomActor.Get());

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Save Hand State + Head_Control State operation "
                "triggered"));
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

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Save Foot State operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModuleOperationsPanel::OnSaveTarget() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(
            LogTemp, Error,
            TEXT("BeatBloom: No actor selected for save head control state"));
        return FReply::Handled();
    }

    UBeatBloomControlRigProcessor::SaveHeadControlState(BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Save Head_Control State operation triggered"));
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
        UpdateDrumKitOptions();
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

// ===== 双线性映射辅助记录器回调函数 =====

TSharedRef<SWidget>
SBeatBloomModuleOperationsPanel::OnGenerateBilinearStateWidget(
    TSharedPtr<FString> InItem) {
    return SNew(STextBlock).Text(FText::FromString(*InItem));
}

void SBeatBloomModuleOperationsPanel::OnBilinearStateChanged(
    TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo) {
    if (NewValue.IsValid()) {
        SelectedBilinearState = NewValue;
    }
}

FReply SBeatBloomModuleOperationsPanel::OnSaveBilinearHelperState() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor selected"));
        return FReply::Handled();
    }

    if (!SelectedBilinearState.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No state selected"));
        return FReply::Handled();
    }

    FString StateSuffix = *SelectedBilinearState;  // "A", "B", "C", 或 "D"

    ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();
    UBeatBloomControlRigProcessor::SaveBilinearHelperState(BeatBloom,
                                                           StateSuffix);

    UE_LOG(LogTemp, Warning, TEXT("BeatBloom: Saved bilinear helper state %s"),
           *StateSuffix);

    return FReply::Handled();
}

FReply SBeatBloomModuleOperationsPanel::OnLoadBilinearHelperState() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor selected"));
        return FReply::Handled();
    }

    ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();
    UBeatBloomControlRigProcessor::LoadBilinearHelperState(
        BeatBloom, *SelectedBilinearState);

    UE_LOG(LogTemp, Warning, TEXT("BeatBloom: Loaded bilinear helper state %s"),
           *(*SelectedBilinearState));

    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
