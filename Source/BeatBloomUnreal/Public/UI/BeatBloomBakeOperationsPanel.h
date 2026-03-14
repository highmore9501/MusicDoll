#pragma once

#include "CoreMinimal.h"
#include "UI/BakeOperationsPanelBase.h"

class ABeatBloomUnreal;

/**
 * BeatBloom 烘焙操作面板
 * 处理鼓手演奏者和鼓组的烘焙操作
 *
 * 对标参考：SFretDanceBakeOperationsPanel
 */
class BEATBLOOMUNREAL_API SBeatBloomBakeOperationsPanel
    : public SBakeOperationsPanelBase {
public:
    SLATE_BEGIN_ARGS(SBeatBloomBakeOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SBakeOperationsPanelBase interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const;
    virtual void RefreshBakeOperations();

private:
    TWeakObjectPtr<ABeatBloomUnreal> BeatBloomActor;

    // Control 选择下拉框
    TSharedPtr<STextComboBox> PerformerControlCombo;
    TSharedPtr<STextComboBox> DrumKitControlCombo;

    // 已选择的 Control
    TSharedPtr<FString> SelectedPerformerControl;
    TSharedPtr<FString> SelectedDrumKitControl;

    // Control 选项数组
    TArray<TSharedPtr<FString>> PerformerControlOptions;
    TArray<TSharedPtr<FString>> DrumKitControlOptions;

    // 初始化 Control 选项
    void InitializeControlOptions();

    // 从扫描结果更新 Control 选项
    void UpdateControlOptionsFromScan();

    // 选择变更处理
    void HandlePerformerControlSelectionChanged(
        TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void HandleDrumKitControlSelectionChanged(
        TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

    // 创建 Control 选择控件
    virtual TSharedRef<SWidget> CreateControlSelectionWidget() override;

    // 获取已选择的 Control 名称
    virtual TArray<FString> GetSelectedControlNames() const override;

    // 刷新扫描结果并更新 Control 选项
    virtual void RefreshScanResults() override;

    // 添加已选择的 Control 到烘焙队列
    virtual void AddSelectedControl() override;

    // 获取模块名
    virtual FName GetModuleName() const override;
};
