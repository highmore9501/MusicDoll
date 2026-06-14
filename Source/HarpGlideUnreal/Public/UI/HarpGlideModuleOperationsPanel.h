#pragma once

#include "CoreMinimal.h"
#include "HarpGlideUnreal.h"
#include "UI/ModuleOperationsPanelBase.h"

/**
 * HarpGlide 模块操作面板
 *
 * 对标 Blender 插件 harp_blender_addon/__init__.py 中的 HARP_PT_main_panel
 * 提供完整的状态选择 → Save/Load 交互：
 *   - 手部姿势：选择手(LEFT/RIGHT) + 状态(FAR/NEAR/ATTACK/REST) → Save/Load
 *   - 踏板状态：选择唱名(D/C/B/E/F/G/A) + 档位(0~4) → Save/Load
 *   - 竖琴倾斜：选择状态(NEAR/MID/FAR) → Save/Load
 *   - 脚部休息：Save/Load
 *   - 动画生成 / Control Rig 工具
 */
class HARPGLIDEUNREAL_API SHarpGlideModuleOperationsPanel
    : public SModuleOperationsPanel {
   public:
    SLATE_BEGIN_ARGS(SHarpGlideModuleOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshOperations() override;

   protected:
    virtual void CreateOperationWidgets() override;

   private:
    TWeakObjectPtr<AHarpGlideUnreal> HarpGlideActor;

    // ===== 下拉选项数据 =====

    TArray<TSharedPtr<FString>> HandPoseOptions;  // FAR / NEAR / ATTACK / REST
    TArray<TSharedPtr<FString>> HandSelectOptions;  // LEFT / RIGHT
    TArray<TSharedPtr<FString>> PedalNoteOptions;   // D / C / B / E / F / G / A
    TArray<TSharedPtr<FString>> PedalStateOptions;  // 0(Flat) ~ 4(Sharp)
    TArray<TSharedPtr<FString>> TiltStateOptions;   // NEAR / MID / FAR
    // 当前选中的值
    TSharedPtr<FString> SelectedHand;  // LEFT / RIGHT
    TSharedPtr<FString> SelectedPose;  // FAR / NEAR / ATTACK / REST
    // ===== 手部姿势 =====
    FReply OnSaveHandPose();
    FReply OnLoadHandPose();

    // ===== 踏板状态 =====
    FReply OnSavePedalState();
    FReply OnLoadPedalState();

    // ===== 竖琴倾斜 =====
    FReply OnSaveTiltState();
    FReply OnLoadTiltState();

    // ===== 脚部休息 =====
    FReply OnSaveFootRest();
    FReply OnLoadFootRest();

    // ===== 动画生成 =====
    FReply OnGeneratePerformerAnimation();
    FReply OnGenerateInstrumentAnimation();
    FReply OnGenerateAllAnimation();

    // ===== Control Rig =====
    FReply OnTriggerControlRigReregistration();
    FReply OnLinearDistributeControls();
};
