#pragma once

#include "CoreMinimal.h"
#include "UI/BakeOperationsPanelBase.h"

class AFretDanceUnreal;

/**
 * Bake operations panel for FretDance module
 * Handles baking operations for guitar performer and instrument
 */
class FRETDANCEUNREAL_API SFretDanceBakeOperationsPanel
    : public SBakeOperationsPanelBase {
   public:
    SLATE_BEGIN_ARGS(SFretDanceBakeOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SBakeOperationsPanelBase interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const;
    virtual void RefreshBakeOperations();

   private:
    // Currently displayed actor
    TWeakObjectPtr<AFretDanceUnreal> FretDanceActor;

    // Control selection combo boxes
    TSharedPtr<STextComboBox> PerformerControlCombo;
    TSharedPtr<STextComboBox> GuitarControlCombo;

    // Selected controls
    TSharedPtr<FString> SelectedPerformerControl;
    TSharedPtr<FString> SelectedGuitarControl;

    // Control options arrays
    TArray<TSharedPtr<FString>> PerformerControlOptions;
    TArray<TSharedPtr<FString>> GuitarControlOptions;

    // Initialize control options
    void InitializeControlOptions();

    // Update control options from scan results
    void UpdateControlOptionsFromScan();

    // Selection change handlers
    void HandlePerformerControlSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void HandleGuitarControlSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

    // Create control selection widgets
    virtual TSharedRef<SWidget> CreateControlSelectionWidget() override;

    // Get selected control names
    virtual TArray<FString> GetSelectedControlNames() const override;

    // Refresh scan results and update control options
    virtual void RefreshScanResults() override;

    // Add selected controls to bake queue
    virtual void AddSelectedControl() override;

    // Get module name
    virtual FName GetModuleName() const override;
};