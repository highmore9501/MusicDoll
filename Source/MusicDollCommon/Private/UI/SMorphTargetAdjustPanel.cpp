#include "UI/SMorphTargetAdjustPanel.h"

#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "InstrumentAnimationUtility.h"
#include "Rigs/RigHierarchy.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMorphTargetAdjustPanel"

void SMorphTargetAdjustPanel::Construct(const FArguments& InArgs) {
    PanelTitle = InArgs._Title;

    ChildSlot.Padding(5.0f)[SNew(SVerticalBox)
                            // 标题
                            + SVerticalBox::Slot().AutoHeight().Padding(
                                  0.0f, 0.0f, 0.0f,
                                  5.0f)[SNew(STextBlock)
                                            .Text(FText::FromString(PanelTitle))
                                            .Font(FAppStyle::GetFontStyle(
                                                "DetailsView.CategoryFont"))]

                            // 提示信息或滑动条容器
                            + SVerticalBox::Slot().AutoHeight()[SAssignNew(
                                  SliderContainer, SVerticalBox)]];
}

void SMorphTargetAdjustPanel::SetMorphTargets(
    const TArray<FString>& InNames, USkeletalMeshComponent* InSkelComp,
    UControlRig* InControlRig) {
    MorphTargetNames = InNames;
    SkelComp = InSkelComp;
    ControlRig = InControlRig;

    // 初始化值缓存
    CurrentValues.SetNum(MorphTargetNames.Num());
    for (int32 i = 0; i < CurrentValues.Num(); ++i) {
        CurrentValues[i] = 0.0f;
    }

    // 从 SkeletalMesh 读取当前值
    if (SkelComp.IsValid()) {
        for (int32 i = 0; i < MorphTargetNames.Num(); ++i) {
            float CurVal =
                SkelComp->GetMorphTarget(FName(*MorphTargetNames[i]));
            CurrentValues[i] = CurVal;
        }
    }

    RebuildSliders();
}

void SMorphTargetAdjustPanel::RebuildSliders() {
    if (!SliderContainer.IsValid()) {
        return;
    }

    SliderContainer->ClearChildren();

    if (MorphTargetNames.Num() == 0) {
        SliderContainer->AddSlot().AutoHeight().Padding(
            5.0f, 2.0f)[SNew(STextBlock)
                            .Text(LOCTEXT("NoMorphTargets",
                                          "(No Morph Targets added)"))
                            .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))];
        return;
    }

    for (int32 i = 0; i < MorphTargetNames.Num(); ++i) {
        const int32 Index = i;  // capture for lambdas

        SliderContainer->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
            [SNew(SHorizontalBox)
             // MT 名称（自适应宽度）
             + SHorizontalBox::Slot()
                   .AutoWidth()
                   .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                   .VAlign(VAlign_Center)
                       [SNew(STextBlock)
                            .Text(FText::FromString(MorphTargetNames[i]))
                            .ToolTipText(FText::FromString(MorphTargetNames[i]))
                            .Font(FAppStyle::GetFontStyle(
                                "PropertyWindow.NormalFont"))]

             // 滑动条（填充剩余空间）
             +
             SHorizontalBox::Slot()
                 .FillWidth(1.0f)
                 .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                 .VAlign(VAlign_Center)
                     [SNew(SSlider)
                          .Value_Lambda([this, Index]() {
                              return Index < CurrentValues.Num()
                                         ? CurrentValues[Index]
                                         : 0.0f;
                          })
                          .OnValueChanged_Lambda([this, Index](float NewValue) {
                              OnSliderValueChanged(Index, NewValue);
                          })
                          .OnMouseCaptureEnd_Lambda(
                              [this, Index]() { OnSliderCaptureEnd(Index); })
                          .MinValue(0.0f)
                          .MaxValue(1.0f)
                          .StepSize(0.01f)
                          .SliderBarColor(FLinearColor(0.2f, 0.4f, 0.8f))
                          .SliderHandleColor(FLinearColor(0.4f, 0.6f, 1.0f))]
             // 当前值文本
             +
             SHorizontalBox::Slot()
                 .AutoWidth()
                 .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                 .VAlign(
                     VAlign_Center)[SNew(STextBlock)
                                        .Text_Lambda([this, Index]() -> FText {
                                            if (Index < CurrentValues.Num())
                                                return FText::AsNumber(
                                                    CurrentValues[Index]);
                                            return FText::AsNumber(0.0f);
                                        })
                                        .Font(FAppStyle::GetFontStyle(
                                            "PropertyWindow.NormalFont"))]

             // Reset 按钮
             + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                   [SNew(SButton)
                        .Text(LOCTEXT("ResetMT", "Reset"))
                        .OnClicked_Lambda([this, Index]() -> FReply {
                            OnResetClicked(Index);
                            return FReply::Handled();
                        })
                        .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];
    }
}

void SMorphTargetAdjustPanel::SetFloatChannelValue(const FString& ChannelName,
                                                   float Value) {
    if (!ControlRig.IsValid()) {
        return;
    }

    // 优先通过 Sequencer 的 Float Channel 设置值（避免被 Sequencer 覆盖）
    if (UInstrumentAnimationUtility::SetControlRigFloatChannelValue(
            ControlRig.Get(), ChannelName, Value)) {
        return;  // Sequencer 方式成功，无需回退
    }

    // 回退：没有打开的 Sequence 时，直接修改 CR Hierarchy
    URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
    if (!Hierarchy) {
        return;
    }

    const FRigElementKey ChannelKey(FName(*ChannelName),
                                    ERigElementType::Control);
    if (!Hierarchy->Contains(ChannelKey)) {
        UE_LOG(LogTemp, Warning,
               TEXT("SetFloatChannelValue: control '%s' not found in "
                    "hierarchy"),
               *ChannelName);
        return;
    }

    FRigControlValue ControlValue;
    ControlValue.Set<float>(Value);
    Hierarchy->SetControlValue(ChannelKey, ControlValue,
                               ERigControlValueType::Current);
}

void SMorphTargetAdjustPanel::OnSliderValueChanged(int32 Index,
                                                   float NewValue) {
    if (!MorphTargetNames.IsValidIndex(Index) || !SkelComp.IsValid()) {
        UE_LOG(LogTemp, Warning,
               TEXT("OnSliderValueChanged: skipped (Index=%d, IsValid=%d)"),
               Index, SkelComp.IsValid() ? 1 : 0);
        return;
    }

    // 拖动中仅更新本地缓存，不操作 Sequencer / CR
    // 最终值在 OnSliderCaptureEnd 中一次性应用
    CurrentValues[Index] = NewValue;
}

void SMorphTargetAdjustPanel::OnSliderCaptureEnd(int32 Index) {
    if (!MorphTargetNames.IsValidIndex(Index) || !SkelComp.IsValid()) {
        return;
    }

    // 松开鼠标时，将最终值应用到 Sequencer Float Channel
    SetFloatChannelValue(MorphTargetNames[Index], CurrentValues[Index]);

    // 触发 Control Rig 评估，使变更立即生效
    if (ControlRig.IsValid()) {
        ControlRig->Evaluate_AnyThread();
    }
}

void SMorphTargetAdjustPanel::OnResetClicked(int32 Index) {
    if (!MorphTargetNames.IsValidIndex(Index) || !SkelComp.IsValid()) {
        return;
    }

    CurrentValues[Index] = 0.0f;
    SetFloatChannelValue(MorphTargetNames[Index], 0.0f);

    // 触发 Control Rig 评估，使重置立即生效
    if (ControlRig.IsValid()) {
        ControlRig->Evaluate_AnyThread();
    }
    // 无需 RebuildSliders()，滑动条值已通过 Value_Lambda 动态绑定
}

TArray<float> SMorphTargetAdjustPanel::GetAllValues() const {
    return CurrentValues;
}

void SMorphTargetAdjustPanel::SetAllValues(const TArray<float>& InValues) {
    if (InValues.Num() != MorphTargetNames.Num()) {
        return;
    }

    CurrentValues = InValues;

    for (int32 i = 0; i < MorphTargetNames.Num(); ++i) {
        SetFloatChannelValue(MorphTargetNames[i], CurrentValues[i]);
    }

    // 触发 Control Rig 评估，使加载的值立即生效
    if (ControlRig.IsValid()) {
        ControlRig->Evaluate_AnyThread();
    }

    // 无需 RebuildSliders()，滑动条值已通过 Value_Lambda 动态绑定
}

void SMorphTargetAdjustPanel::ResetAll() {
    for (int32 i = 0; i < MorphTargetNames.Num(); ++i) {
        CurrentValues[i] = 0.0f;
        SetFloatChannelValue(MorphTargetNames[i], 0.0f);
    }

    // 触发 Control Rig 评估，使重置立即生效
    if (ControlRig.IsValid()) {
        ControlRig->Evaluate_AnyThread();
    }

    // 无需 RebuildSliders()，滑动条值已通过 Value_Lambda 动态绑定
}

#undef LOCTEXT_NAMESPACE
