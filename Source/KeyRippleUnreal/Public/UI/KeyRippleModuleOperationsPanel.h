#pragma once

#include "CoreMinimal.h"
#include "UI/ModuleOperationsPanelBase.h"

class AKeyRippleUnreal;

/**
 * Operations panel for KeyRipple module
 * Provides buttons to execute various KeyRipple operations
 */
class KEYRIPPLEUNREAL_API SKeyRippleModuleOperationsPanel
    : public SModuleOperationsPanel {
   public:
    SLATE_BEGIN_ARGS(SKeyRippleModuleOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleOperationsPanel interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshOperations() override;

   protected:
    virtual void CreateOperationWidgets() override;

   private:
    // Currently displayed actor
    TWeakObjectPtr<AKeyRippleUnreal> KeyRippleActor;

    // Combo box options
    TArray<TSharedPtr<FString>> KeyTypeOptions;
    TArray<TSharedPtr<FString>> PositionTypeOptions;

    // Operation button handlers
    FReply OnSaveState();
    FReply OnLoadState();
    FReply OnClearControlRigKeyframes();
    FReply OnGeneratePerformerAnimation();
    FReply OnGeneratePianoKeyAnimation();
    FReply OnGenerateAllAnimation();
    FReply OnInitPiano();
};