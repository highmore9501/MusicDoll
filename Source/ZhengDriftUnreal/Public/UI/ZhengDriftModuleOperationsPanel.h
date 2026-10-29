#pragma once

#include "CoreMinimal.h"
#include "UI/ModuleOperationsPanelBase.h"
#include "ZhengDriftUnreal.h"

/**
 * 古筝模块操作面板
 */
class ZHENGDRIFTUNREAL_API SZhengDriftModuleOperationsPanel
    : public SModuleOperationsPanel {
public:
    SLATE_BEGIN_ARGS(SZhengDriftModuleOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshOperations() override;

protected:
    virtual void CreateOperationWidgets() override;

private:
    TWeakObjectPtr<AZhengDriftUnreal> ZhengDriftActor;

    // 手部状态下拉菜单选项
    TArray<TSharedPtr<FString>> LeftHandPositionOptions;
    TArray<TSharedPtr<FString>> LeftHandActionOptions;
    TArray<TSharedPtr<FString>> RightHandPositionOptions;
    TArray<TSharedPtr<FString>> RightHandActionOptions;

    // 头部采样：头部原点（各手部状态的样本跟着 Save Left / Save Right 一起采）
    FReply OnSaveHeadOrigin();
    FReply OnLoadHeadOrigin();

    // 按钮事件
    FReply OnSaveLeft();
    FReply OnSaveRight();
    FReply OnLoadState();
    FReply OnGeneratePerformerAnimation();
    FReply OnGenerateInstrumentAnimation();
    FReply OnGenerateAllAnimation();
    FReply OnTriggerControlRigReregistration();
    FReply OnLinearDistributeControls();
};
