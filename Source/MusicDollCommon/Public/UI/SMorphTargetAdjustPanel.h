#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USkeletalMeshComponent;
class UControlRig;

/**
 * 通用 Morph Target 调整面板
 * 显示 MT 名称列表 + 滑动条 + Reset 按钮
 *
 * 通过 Control Rig Hierarchy 设置 Float Channel 的值来驱动 Morph Target，
 * 而非直接调用 SkelComp->SetMorphTarget()。
 *
 * 用法：
 *   auto Panel = SNew(SMorphTargetAdjustPanel).Title(TEXT("Character MT"));
 *   Panel->SetMorphTargets(Names, SkelComp, ControlRig);
 *   Panel->GetAllValues();   // Save State
 *   Panel->SetAllValues(v);  // Load State
 */
class MUSICDOLLCOMMON_API SMorphTargetAdjustPanel : public SCompoundWidget {
   public:
    SLATE_BEGIN_ARGS(SMorphTargetAdjustPanel) : _Title(TEXT("Morph Targets")) {}
    SLATE_ARGUMENT(FString, Title)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** 设置 MT 名称列表 + 绑定的 SkeletalMeshComponent + ControlRig */
    void SetMorphTargets(const TArray<FString>& InNames,
                         USkeletalMeshComponent* InSkelComp,
                         UControlRig* InControlRig = nullptr);

    /** 获取所有 MT 的当前值（按名称列表顺序，用于 Save State） */
    TArray<float> GetAllValues() const;

    /** 设置所有 MT 的值（按名称列表顺序，用于 Load State） */
    void SetAllValues(const TArray<float>& InValues);

    /** 重置所有 MT 为 0 */
    void ResetAll();

    /** 检查当前是否已绑定 Mesh */
    bool IsValid() const { return SkelComp.IsValid(); }

   private:
    /** 通过 Control Rig Hierarchy 设置单个 Float Channel 的值 */
    void SetFloatChannelValue(const FString& ChannelName, float Value);

    /** 重建所有滑动条 */
    void RebuildSliders();

    /** 单个 MT 值变更回调 */
    void OnSliderValueChanged(int32 Index, float NewValue);

    /** 单个 MT Reset 按钮 */
    void OnResetClicked(int32 Index);

    /** 名称列表 */
    TArray<FString> MorphTargetNames;

    /** 绑定的 SkeletalMesh 组件 */
    TWeakObjectPtr<USkeletalMeshComponent> SkelComp;

    /** Control Rig（用于写入 Float Channel 以驱动 Morph Target） */
    TWeakObjectPtr<UControlRig> ControlRig;

    /** 面板标题 */
    FString PanelTitle;

    /** 滑动条容器 */
    TSharedPtr<SVerticalBox> SliderContainer;

    /** 当前值缓存 */
    TArray<float> CurrentValues;
};
