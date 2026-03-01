#pragma once

#include "CoreMinimal.h"
#include "KeyRippleUnreal.h"
#include "UI/BakeOperationsPanelBase.h"

/**
 * KeyRipple专用烘焙操作面板
 * 为钢琴演奏者提供专门的Control选择界面
 */
class KEYRIPPLEUNREAL_API SKeyRippleBakeOperationsPanel
    : public SBakeOperationsPanelBase {
   public:
    SLATE_BEGIN_ARGS(SKeyRippleBakeOperationsPanel) {}
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
    // KeyRipple特定的UI控件
    TSharedPtr<STextComboBox> PerformerControlCombo;
    TSharedPtr<STextComboBox> PianoControlCombo;

    // 选项列表
    TArray<TSharedPtr<FString>> PerformerControlOptions;
    TArray<TSharedPtr<FString>> PianoControlOptions;

    // 当前选择
    TSharedPtr<FString> SelectedPerformerControl;
    TSharedPtr<FString> SelectedPianoControl;

    // KeyRipple Actor引用
    TWeakObjectPtr<AKeyRippleUnreal> KeyRippleActor;

    // 初始化选项列表
    void InitializeControlOptions();
    void UpdateControlOptionsFromScan();
        
    // 事件处理
    void HandlePerformerControlSelectionChanged(
        TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void HandlePianoControlSelectionChanged(TSharedPtr<FString> NewSelection,
                                            ESelectInfo::Type SelectInfo);
};