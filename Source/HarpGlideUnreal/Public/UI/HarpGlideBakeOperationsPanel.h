#pragma once

#include "CoreMinimal.h"
#include "HarpGlideUnreal.h"
#include "UI/BakeOperationsPanelBase.h"

/**
 * 竖琴烘焙操作面板
 */
class HARPGLIDEUNREAL_API SHarpGlideBakeOperationsPanel
    : public SBakeOperationsPanelBase {
   public:
    SLATE_BEGIN_ARGS(SHarpGlideBakeOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void SetActor(AActor* InActor) override;
    virtual void RefreshScanResults() override;
    virtual void RefreshBakeOperations();

   protected:
    virtual TSharedRef<SWidget> CreateControlSelectionWidget() override;
    virtual TArray<FString> GetSelectedControlNames() const override;

   private:
    TWeakObjectPtr<AHarpGlideUnreal> HarpGlideActor;

    TSharedPtr<STextComboBox> PerformerControlCombo;
    TSharedPtr<STextComboBox> HarpControlCombo;

    TSharedPtr<FString> SelectedPerformerControl;
    TSharedPtr<FString> SelectedHarpControl;

    TArray<TSharedPtr<FString>> PerformerControlOptions;
    TArray<TSharedPtr<FString>> HarpControlOptions;

    void InitializeControlOptions();
    void UpdateControlOptionsFromScan();
    void HandlePerformerControlSelectionChanged(
        TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void HandleHarpControlSelectionChanged(TSharedPtr<FString> NewSelection,
                                           ESelectInfo::Type SelectInfo);
};
