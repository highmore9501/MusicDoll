#pragma once

#include "CoreMinimal.h"
#include "UI/ModuleOperationsPanelBase.h"

class AFretDanceUnreal;

/**
 * Operations panel for FretDance module
 * Provides buttons to execute various FretDance operations
 */
class FRETDANCEUNREAL_API SFretDanceModuleOperationsPanel
    : public SModuleOperationsPanel {
   public:
    SLATE_BEGIN_ARGS(SFretDanceModuleOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleOperationsPanel interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshOperations() override;

   protected:
    virtual void CreateOperationWidgets() override;

    // Placeholder for updating left hand state options (implemented as empty for now)
    void UpdateLeftHandStateOptions();

   private:
    // Currently displayed actor
    TWeakObjectPtr<AFretDanceUnreal> FretDanceActor;

    // Combo box options
    TArray<TSharedPtr<FString>> BasePositionOptions;
    TArray<TSharedPtr<FString>> LeftHandStateOptions;
    TArray<TSharedPtr<FString>> RightHandStateOptions;

    // Operation button handlers
    FReply OnSaveState();
    FReply OnLoadState();
    FReply OnSaveLeft();
    FReply OnSaveRight();
    FReply OnGeneratePerformerAnimation();
    FReply OnGenerateStringAnimation();
    FReply OnGenerateAllAnimation();
    FReply OnInitGuitarInstrument();
    FReply OnTriggerControlRigReregistration();
};