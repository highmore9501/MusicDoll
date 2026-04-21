#pragma once

#include "CoreMinimal.h"
#include "UI/ModuleOperationsPanelBase.h"

class ABeatBloomUnreal;

/**
 * BeatBloom 操作面板
 * 提供鼓手状态选择、状态保存/加载、动画生成等交互操作
 *
 * 布局区域：
 * - Hand State Configuration：左/右手鼓件 + 状态下拉菜单
 * - Foot State Configuration：左/右脚鼓件 + 状态下拉菜单
 * - State Management: SaveHandState（同时保存 Head_Control）/SaveFootState/LoadState 按钮
 * - Animation Generation：生成演奏动画/鼓组动画/全部动画 按钮
 * - Control Rig：初始化鼓组、重新注册 ControlRig 按钮
 *
 * 自动同步逻辑：
 * - 手部（DrumKit 或 State）变化 → 同时同步 Target 的 DrumKit 和 State
 * - 脚部变化 → 不影响 Target，保持独立控制
 *
 * 对标参考：SFretDanceModuleOperationsPanel
 */
class BEATBLOOMUNREAL_API SBeatBloomModuleOperationsPanel
    : public SModuleOperationsPanel {
public:
    SLATE_BEGIN_ARGS(SBeatBloomModuleOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleOperationsPanel interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshOperations() override;

protected:
    virtual void CreateOperationWidgets() override;

private:
    TWeakObjectPtr<ABeatBloomUnreal> BeatBloomActor;

    // 更新鼓件选项（当加载 drumkit 配置后刷新下拉菜单）
    void UpdateDrumKitOptions();

    // 下拉菜单选项
    TArray<TSharedPtr<FString>> LeftHandKitOptions;
    TArray<TSharedPtr<FString>> RightHandKitOptions;
    TArray<TSharedPtr<FString>> LeftFootKitOptions;
    TArray<TSharedPtr<FString>> RightFootKitOptions;
    TArray<TSharedPtr<FString>> StateOptions;

    // 双线性映射辅助记录器状态选择
    TSharedPtr<FString> SelectedBilinearState;
    TArray<TSharedPtr<FString>> BilinearStateOptions;

    // ===== 按钮回调 =====

    // 状态管理
    FReply OnSaveHand();
    FReply OnSaveFoot();
    FReply OnSaveTarget();
    FReply OnSaveAll();
    FReply OnLoadState();
    
    // 双线性映射辅助记录器
    TSharedRef<SWidget> OnGenerateBilinearStateWidget(TSharedPtr<FString> InItem);
    void OnBilinearStateChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
    FReply OnSaveBilinearHelperState();
    FReply OnLoadBilinearHelperState();    

    // 动画生成
    FReply OnGeneratePerformerAnimation();
    FReply OnGenerateDrumKitAnimation();
    FReply OnGenerateAllAnimation();

    // ControlRig 操作
    FReply OnInitDrumKit();
    FReply OnTriggerControlRigReregistration();
};
