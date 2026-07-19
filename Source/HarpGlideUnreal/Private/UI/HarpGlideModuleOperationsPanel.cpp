#include "UI/HarpGlideModuleOperationsPanel.h"

#include "HarpGlideAnimationProcessor.h"
#include "HarpGlideControlRigProcessor.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHarpGlideModuleOperationsPanel"

// ============================================================
// 构造 & 生命周期
// ============================================================

void SHarpGlideModuleOperationsPanel::Construct(const FArguments& InArgs) {
    SModuleOperationsPanel::FArguments BaseArgs;
    SModuleOperationsPanel::Construct(BaseArgs);

    // 初始化手部姿势选项（左右共用同一个状态列表）
    HandPoseOptions = {MakeShareable(new FString(TEXT("FAR"))),
                       MakeShareable(new FString(TEXT("NEAR"))),
                       MakeShareable(new FString(TEXT("ATTACK"))),
                       MakeShareable(new FString(TEXT("REST")))};

    // 手的选择（LEFT / RIGHT）
    HandSelectOptions = {MakeShareable(new FString(TEXT("LEFT"))),
                         MakeShareable(new FString(TEXT("RIGHT")))};

    // 默认选中值
    SelectedHand = HandSelectOptions[0];  // LEFT
    SelectedPose = HandPoseOptions[0];    // FAR

    // 踏板唱名
    PedalNoteOptions = {MakeShareable(new FString(TEXT("D"))),
                        MakeShareable(new FString(TEXT("C"))),
                        MakeShareable(new FString(TEXT("B"))),
                        MakeShareable(new FString(TEXT("E"))),
                        MakeShareable(new FString(TEXT("F"))),
                        MakeShareable(new FString(TEXT("G"))),
                        MakeShareable(new FString(TEXT("A")))};

    // 踏板档位（0=降音 ~ 4=升音）
    PedalStateOptions = {MakeShareable(new FString(TEXT("0"))),
                         MakeShareable(new FString(TEXT("1"))),
                         MakeShareable(new FString(TEXT("2"))),
                         MakeShareable(new FString(TEXT("3"))),
                         MakeShareable(new FString(TEXT("4")))};

    // 竖琴倾斜状态
    TiltStateOptions = {MakeShareable(new FString(TEXT("NEAR"))),
                        MakeShareable(new FString(TEXT("MID"))),
                        MakeShareable(new FString(TEXT("FAR")))};
}

void SHarpGlideModuleOperationsPanel::SetActor(AActor* InActor) {
    HarpGlideActor = Cast<AHarpGlideUnreal>(InActor);
    RefreshOperations();
}

bool SHarpGlideModuleOperationsPanel::CanHandleActor(
    const AActor* InActor) const {
    return InActor && InActor->IsA<AHarpGlideUnreal>();
}

void SHarpGlideModuleOperationsPanel::RefreshOperations() {
    CreateOperationWidgets();
}

// ============================================================
// UI 构建
// ============================================================

void SHarpGlideModuleOperationsPanel::CreateOperationWidgets() {
    auto Container = GetOperationContainer();
    if (!Container.IsValid()) return;

    Container->ClearChildren();

    if (!HarpGlideActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActor", "No HarpGlide Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AHarpGlideUnreal* Actor = HarpGlideActor.Get();

    // ============================================================
    // 1. 手部姿势 (Hand Pose State)
    // ============================================================
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Hand Pose State"))];

    // 手选择 + 状态选择
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox)
         // 手选择
         + SHorizontalBox::Slot().FillWidth(0.5f).Padding(
               0.0f, 0.0f, 5.0f,
               0.0f)[SNew(SComboBox<TSharedPtr<FString>>)
                         .OptionsSource(&HandSelectOptions)
                         .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                             return SNew(STextBlock)
                                 .Text(FText::FromString(*Item));
                         })
                         .OnSelectionChanged_Lambda(
                             [this](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type) {
                                 if (NewSelection.IsValid())
                                     SelectedHand = NewSelection;
                             })
                         .InitiallySelectedItem(SelectedHand)
                             [SNew(STextBlock).Text_Lambda([this]() -> FText {
                                 return FText::FromString(SelectedHand.IsValid()
                                                              ? *SelectedHand
                                                              : TEXT("LEFT"));
                             })]]
         // 状态选择
         + SHorizontalBox::Slot().FillWidth(0.5f).Padding(
               5.0f, 0.0f, 0.0f,
               0.0f)[SNew(SComboBox<TSharedPtr<FString>>)
                         .OptionsSource(&HandPoseOptions)
                         .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                             return SNew(STextBlock)
                                 .Text(FText::FromString(*Item));
                         })
                         .OnSelectionChanged_Lambda(
                             [this](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type) {
                                 if (NewSelection.IsValid())
                                     SelectedPose = NewSelection;
                             })
                         .InitiallySelectedItem(SelectedPose)
                             [SNew(STextBlock).Text_Lambda([this]() -> FText {
                                 return FText::FromString(SelectedPose.IsValid()
                                                              ? *SelectedPose
                                                              : TEXT("FAR"));
                             })]]];

    // Save / Load 按钮
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("SaveHandBtn", "Save Hand Pose"))
                  .OnClicked(this,
                             &SHarpGlideModuleOperationsPanel::OnSaveHandPose)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("LoadHandBtn", "Load Hand Pose"))
                  .OnClicked(this,
                             &SHarpGlideModuleOperationsPanel::OnLoadHandPose)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

    // ============================================================
    // 2. 踏板状态 (Pedal State)
    // ============================================================
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Pedal State (踏板五态位置)"))];

    // 唱名 + 档位选择
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&PedalNoteOptions)
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                      return SNew(STextBlock).Text(FText::FromString(*Item));
                  })
                  .OnSelectionChanged_Lambda(
                      [this, Actor](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type) {
                          if (!NewSelection.IsValid()) return;
                          Actor->Modify();
                          if (*NewSelection == TEXT("D"))
                              Actor->CurrentPedalNote = EHarpGlidePedalNote::D;
                          else if (*NewSelection == TEXT("C"))
                              Actor->CurrentPedalNote = EHarpGlidePedalNote::C;
                          else if (*NewSelection == TEXT("B"))
                              Actor->CurrentPedalNote = EHarpGlidePedalNote::B;
                          else if (*NewSelection == TEXT("E"))
                              Actor->CurrentPedalNote = EHarpGlidePedalNote::E;
                          else if (*NewSelection == TEXT("F"))
                              Actor->CurrentPedalNote = EHarpGlidePedalNote::F;
                          else if (*NewSelection == TEXT("G"))
                              Actor->CurrentPedalNote = EHarpGlidePedalNote::G;
                          else
                              Actor->CurrentPedalNote = EHarpGlidePedalNote::A;
                      })[SNew(STextBlock).Text_Lambda([Actor]() -> FText {
                      return FText::FromString(
                          AHarpGlideUnreal::GetPedalNoteString(
                              Actor->CurrentPedalNote));
                  })]] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(
             5.0f, 0.0f, 0.0f,
             0.0f)[SNew(SComboBox<TSharedPtr<FString>>)
                       .OptionsSource(&PedalStateOptions)
                       .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                           return SNew(STextBlock)
                               .Text(FText::FromString(*Item));
                       })
                       .OnSelectionChanged_Lambda(
                           [this, Actor](TSharedPtr<FString> NewSelection,
                                         ESelectInfo::Type) {
                               if (!NewSelection.IsValid()) return;
                               int32 Val = FCString::Atoi(**NewSelection);
                               Actor->Modify();
                               Actor->CurrentPedalState =
                                   static_cast<EHarpGlidePedalState>(Val);
                           })[SNew(STextBlock).Text_Lambda([Actor]() -> FText {
                           return FText::FromString(
                               AHarpGlideUnreal::GetPedalStateString(
                                   Actor->CurrentPedalState));
                       })]]];

    // Save / Load 按钮
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("SavePedalBtn", "Save Pedal"))
                  .OnClicked(this,
                             &SHarpGlideModuleOperationsPanel::OnSavePedalState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("LoadPedalBtn", "Load Pedal"))
                  .OnClicked(this,
                             &SHarpGlideModuleOperationsPanel::OnLoadPedalState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

    // ============================================================
    // 3. 竖琴倾斜 (Harp Tilt State)
    // ============================================================
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Harp Tilt State (竖琴倾斜状态)"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&TiltStateOptions)
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                      return SNew(STextBlock).Text(FText::FromString(*Item));
                  })
                  .OnSelectionChanged_Lambda(
                      [this, Actor](TSharedPtr<FString> NewSelection,
                                    ESelectInfo::Type) {
                          if (!NewSelection.IsValid()) return;
                          Actor->Modify();
                          if (*NewSelection == TEXT("NEAR"))
                              Actor->CurrentTiltState =
                                  EHarpGlideTiltState::NEAR;
                          else if (*NewSelection == TEXT("MID"))
                              Actor->CurrentTiltState =
                                  EHarpGlideTiltState::MID;
                          else
                              Actor->CurrentTiltState =
                                  EHarpGlideTiltState::FAR;
                      })[SNew(STextBlock).Text_Lambda([Actor]() -> FText {
                      return FText::FromString(
                          AHarpGlideUnreal::GetTiltStateString(
                              Actor->CurrentTiltState));
                  })]];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("SaveTiltBtn", "Save Tilt"))
                  .OnClicked(this,
                             &SHarpGlideModuleOperationsPanel::OnSaveTiltState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("LoadTiltBtn", "Load Tilt"))
                  .OnClicked(this,
                             &SHarpGlideModuleOperationsPanel::OnLoadTiltState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

    // ============================================================
    // 4. 脚部休息位置 (Foot Rest State)
    // ============================================================
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Foot Rest State (脚部休息位置)"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("SaveFootRestBtn", "Save Foot Rest"))
                  .OnClicked(this,
                             &SHarpGlideModuleOperationsPanel::OnSaveFootRest)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("LoadFootRestBtn", "Load Foot Rest"))
                  .OnClicked(this,
                             &SHarpGlideModuleOperationsPanel::OnLoadFootRest)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

    // ============================================================
    // 5. Animation Generation
    // ============================================================
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Animation Generation"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT("Animation File Path"), Actor->AnimationFilePath,
        TEXT("AnimationFilePath"), TEXT(".harpglide"),
        [this](const FString& NewPath) {
            if (HarpGlideActor.IsValid()) {
                HarpGlideActor->Modify();
                HarpGlideActor->AnimationFilePath = NewPath;
            }
        },
        false)];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("GenPerformerBtn", "Generate Performer Animation"))
             .OnClicked(
                 this,
                 &SHarpGlideModuleOperationsPanel::OnGeneratePerformerAnimation)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenInstrumentBtn",
                                "Generate Instrument Animation"))
                  .OnClicked(this, &SHarpGlideModuleOperationsPanel::
                                       OnGenerateInstrumentAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenAllBtn", "Generate All Animation"))
                  .OnClicked(
                      this,
                      &SHarpGlideModuleOperationsPanel::OnGenerateAllAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ============================================================
    // 6. Control Rig Tools
    // ============================================================
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Control Rig Tools"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("ReregisterBtn", "Re-register Control Rigs"))
                  .OnClicked(this, &SHarpGlideModuleOperationsPanel::
                                       OnTriggerControlRigReregistration)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("LinearDistBtn", "Linear Distribute Controls"))
                  .OnClicked(this, &SHarpGlideModuleOperationsPanel::
                                       OnLinearDistributeControls)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

// ============================================================
// 事件处理 — 手部姿势
// ============================================================

FReply SHarpGlideModuleOperationsPanel::OnSaveHandPose() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    if (!SelectedHand.IsValid() || !SelectedPose.IsValid())
        return FReply::Handled();

    AHarpGlideUnreal* Actor = HarpGlideActor.Get();
    const FString& Hand = *SelectedHand;
    const FString& PoseStr = *SelectedPose;

    // 解析手部姿势枚举
    EHarpGlideHandPose Pose = EHarpGlideHandPose::FAR;
    if (PoseStr == TEXT("NEAR"))
        Pose = EHarpGlideHandPose::NEAR;
    else if (PoseStr == TEXT("ATTACK"))
        Pose = EHarpGlideHandPose::ATTACK;
    else if (PoseStr == TEXT("REST"))
        Pose = EHarpGlideHandPose::REST;

    // 更新 Actor 对应手的姿势，然后保存
    if (Hand == TEXT("LEFT")) {
        Actor->CurrentLeftHandPose = Pose;
        UHarpGlideControlRigProcessor::SaveLeftHandState(Actor);
    } else {
        Actor->CurrentRightHandPose = Pose;
        UHarpGlideControlRigProcessor::SaveRightHandState(Actor);
    }
    return FReply::Handled();
}

FReply SHarpGlideModuleOperationsPanel::OnLoadHandPose() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    if (!SelectedHand.IsValid() || !SelectedPose.IsValid())
        return FReply::Handled();

    AHarpGlideUnreal* Actor = HarpGlideActor.Get();
    const FString& Hand = *SelectedHand;
    const FString& PoseStr = *SelectedPose;

    EHarpGlideHandPose Pose = EHarpGlideHandPose::FAR;
    if (PoseStr == TEXT("NEAR"))
        Pose = EHarpGlideHandPose::NEAR;
    else if (PoseStr == TEXT("ATTACK"))
        Pose = EHarpGlideHandPose::ATTACK;
    else if (PoseStr == TEXT("REST"))
        Pose = EHarpGlideHandPose::REST;

    // 更新 Actor 对应手的姿势，调用 LoadState
    if (Hand == TEXT("LEFT")) {
        Actor->CurrentLeftHandPose = Pose;
    } else {
        Actor->CurrentRightHandPose = Pose;
    }
    UHarpGlideControlRigProcessor::LoadState(Actor);
    return FReply::Handled();
}

// ============================================================
// 事件处理 — 踏板状态
// ============================================================

FReply SHarpGlideModuleOperationsPanel::OnSavePedalState() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    AHarpGlideUnreal* Actor = HarpGlideActor.Get();
    UHarpGlideControlRigProcessor::SavePedalState(
        Actor, Actor->CurrentPedalNote, Actor->CurrentPedalState);
    return FReply::Handled();
}

FReply SHarpGlideModuleOperationsPanel::OnLoadPedalState() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    AHarpGlideUnreal* Actor = HarpGlideActor.Get();
    UHarpGlideControlRigProcessor::LoadPedalState(
        Actor, Actor->CurrentPedalNote, Actor->CurrentPedalState);
    return FReply::Handled();
}

// ============================================================
// 事件处理 — 竖琴倾斜
// ============================================================

FReply SHarpGlideModuleOperationsPanel::OnSaveTiltState() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    AHarpGlideUnreal* Actor = HarpGlideActor.Get();
    UHarpGlideControlRigProcessor::SaveHarpTiltState(Actor,
                                                     Actor->CurrentTiltState);
    return FReply::Handled();
}

FReply SHarpGlideModuleOperationsPanel::OnLoadTiltState() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    AHarpGlideUnreal* Actor = HarpGlideActor.Get();
    UHarpGlideControlRigProcessor::LoadHarpTiltState(Actor,
                                                     Actor->CurrentTiltState);
    return FReply::Handled();
}

// ============================================================
// 事件处理 — 脚部休息
// ============================================================

FReply SHarpGlideModuleOperationsPanel::OnSaveFootRest() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    UHarpGlideControlRigProcessor::SaveFootRestState(HarpGlideActor.Get());
    return FReply::Handled();
}

FReply SHarpGlideModuleOperationsPanel::OnLoadFootRest() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    UHarpGlideControlRigProcessor::LoadFootRestState(HarpGlideActor.Get());
    return FReply::Handled();
}

// ============================================================
// 事件处理 — 动画生成
// ============================================================

FReply SHarpGlideModuleOperationsPanel::OnGeneratePerformerAnimation() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    UHarpGlideAnimationProcessor::GeneratePerformerAnimation(
        HarpGlideActor.Get());
    return FReply::Handled();
}

FReply SHarpGlideModuleOperationsPanel::OnGenerateInstrumentAnimation() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    UHarpGlideAnimationProcessor::GenerateInstrumentAnimation(
        HarpGlideActor.Get());
    return FReply::Handled();
}

FReply SHarpGlideModuleOperationsPanel::OnGenerateAllAnimation() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    UHarpGlideAnimationProcessor::GenerateAllAnimation(HarpGlideActor.Get());
    return FReply::Handled();
}

// ============================================================
// 事件处理 — Control Rig
// ============================================================

FReply SHarpGlideModuleOperationsPanel::OnTriggerControlRigReregistration() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    HarpGlideActor->RegisterAllControlRigs();
    return FReply::Handled();
}

FReply SHarpGlideModuleOperationsPanel::OnLinearDistributeControls() {
    if (!HarpGlideActor.IsValid()) return FReply::Handled();
    UHarpGlideControlRigProcessor::LinearDistributeControls(
        HarpGlideActor.Get());
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
