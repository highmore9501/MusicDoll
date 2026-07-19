#pragma once

#include "CoreMinimal.h"
#include "UI/ModuleOperationsPanelBase.h"
#include "UI/SMorphTargetAdjustPanel.h"

class AWindRiseUnreal;

/**
 * WindRise 操作面板
 * 提供音高状态保存/加载、MT 调整、CR 初始化、动画生成等操作
 *
 * 布局区域：
 * - 音高状态录制：音高选择 + 人物 MT 调整 + 乐器 MT 调整 + Save/Load
 * - 乐器初始化：Initialize Instrument CR
 * - 动画生成：.wind_rise 文件浏览 + Generate Animation
 *
 * 核心交互流程：
 * 1. 选择音高 → 调整控制器（在场景中）
 * 2. 微调 MT 滑动条（实时驱动 SkeletalMesh）
 * 3. 点击 Save State 保存当前音高的完整状态
 */
class WINDRISEUNREAL_API SWindRiseModuleOperationsPanel
    : public SModuleOperationsPanel {
   public:
    SLATE_BEGIN_ARGS(SWindRiseModuleOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleOperationsPanel interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshOperations() override;

   protected:
    virtual void CreateOperationWidgets() override;

   private:
    TWeakObjectPtr<AWindRiseUnreal> WindRiseActor;

    // 音高选项
    TArray<TSharedPtr<FString>> NoteOptions;
    TSharedPtr<FString> SelectedNote;

    // MT 调整面板
    TSharedPtr<SMorphTargetAdjustPanel> CharacterMTPanel;
    TSharedPtr<SMorphTargetAdjustPanel> InstrumentMTPanel;

    // 按钮回调
    FReply OnSaveState();
    FReply OnLoadState();
    FReply OnCaptureRestOffset();
    FReply OnInitializeInstrumentCR();
    FReply OnGenerateAnimation();

    // 音高下拉
    void OnNoteSelectionChanged(TSharedPtr<FString> NewValue,
                                ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> OnGenerateNoteWidget(TSharedPtr<FString> InItem);

    // 更新音高选项
    void UpdateNoteOptions();
};
