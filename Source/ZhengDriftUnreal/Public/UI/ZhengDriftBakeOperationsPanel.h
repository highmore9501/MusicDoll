#pragma once

#include "CoreMinimal.h"
#include "UI/BakeOperationsPanelBase.h"
#include "ZhengDriftUnreal.h"

/**
 * 古筝烘焙操作面板
 */
class ZHENGDRIFTUNREAL_API SZhengDriftBakeOperationsPanel
    : public SBakeOperationsPanelBase {
public:
    SLATE_BEGIN_ARGS(SZhengDriftBakeOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const;
    virtual void RefreshBakeOperations();

private:
    TWeakObjectPtr<AZhengDriftUnreal> ZhengDriftActor;

    TSharedPtr<STextComboBox> PerformerControlCombo;
    TSharedPtr<STextComboBox> ZhengControlCombo;

    TSharedPtr<FString> SelectedPerformerControl;
    TSharedPtr<FString> SelectedZhengControl;

    TArray<TSharedPtr<FString>> PerformerControlOptions;
    TArray<TSharedPtr<FString>> ZhengControlOptions;

    void InitializeControlOptions();
    void UpdateControlOptionsFromScan();

    void HandlePerformerControlSelectionChanged(
        TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void HandleZhengControlSelectionChanged(
        TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

    virtual TSharedRef<SWidget> CreateControlSelectionWidget() override;
    virtual TArray<FString> GetSelectedControlNames() const override;
    virtual void RefreshScanResults() override;
    virtual void AddSelectedControl() override;
    virtual FName GetModuleName() const override;
};
