
#pragma once

#include "CoreMinimal.h"
#include "StringFlowUnreal.h"
#include "UI/BakeOperationsPanelBase.h"

/**
 * StringFlow专用烘焙操作面板
 * 为弦乐器演奏者/琴/弓提供专门的Control选择界面
 */
class STRINGFLOWUNREAL_API SStringFlowBakeOperationsPanel
    : public SBakeOperationsPanelBase {
   public:
    SLATE_BEGIN_ARGS(SStringFlowBakeOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetActor(AActor* InActor);

   protected:
    // 重写基类方法
    virtual TSharedRef<SWidget> CreateControlSelectionWidget() override;
    virtual TArray<FString> GetSelectedControlNames() const override;
    virtual void RefreshScanResults() override;
    virtual void AddSelectedControl() override;

   private:
    // StringFlow特定的UI控件
    TSharedPtr<STextComboBox> PerformerControlCombo;
    TSharedPtr<STextComboBox> InstrumentControlCombo;
    TSharedPtr<STextComboBox> BowControlCombo;

    // 选项列表
    TArray<TSharedPtr<FString>> PerformerControlOptions;
    TArray<TSharedPtr<FString>> InstrumentControlOptions;
    TArray<TSharedPtr<FString>> BowControlOptions;

    // 当前选择
    TSharedPtr<FString> SelectedPerformerControl;
    TSharedPtr<FString> SelectedInstrumentControl;
    TSharedPtr<FString> SelectedBowControl;

    // StringFlow Actor引用
    TWeakObjectPtr<AStringFlowUnreal> StringFlowActor;

    // 初始化选项列表
    void InitializeControlOptions();
    void UpdateControlOptionsFromScan();
        
    // 事件处理
    void HandlePerformerControlSelectionChanged(
        TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void HandleInstrumentControlSelectionChanged(
        TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void HandleBowControlSelectionChanged(TSharedPtr<FString> NewSelection,
                                          ESelectInfo::Type SelectInfo);
};