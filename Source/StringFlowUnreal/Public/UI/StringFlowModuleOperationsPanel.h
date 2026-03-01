#pragma once

#include "CoreMinimal.h"
#include "UI/ModuleOperationsPanelBase.h"

class AStringFlowUnreal;

/**
 * Operations panel for StringFlow module
 * Provides buttons to execute various StringFlow operations
 */
class STRINGFLOWUNREAL_API SStringFlowModuleOperationsPanel
    : public SModuleOperationsPanel {
   public:
    SLATE_BEGIN_ARGS(SStringFlowModuleOperationsPanel) {}
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
    TWeakObjectPtr<AStringFlowUnreal> StringFlowActor;

    // Combo box options
    TArray<TSharedPtr<FString>> LeftHandPositionOptions;
    TArray<TSharedPtr<FString>> RightHandPositionOptions;
    TArray<TSharedPtr<FString>> LeftHandFretIndexOptions;
    TArray<TSharedPtr<FString>> RightHandStringIndexOptions;

    // Operation button handlers
    FReply OnSaveState();
    FReply OnLoadState();
    FReply OnGeneratePerformerAnimation();
    FReply OnGenerateStringAnimation();
    FReply OnGenerateAllAnimation();
    FReply OnInitStringInstrument();
    FReply OnTriggerControlRigReregistration();
};