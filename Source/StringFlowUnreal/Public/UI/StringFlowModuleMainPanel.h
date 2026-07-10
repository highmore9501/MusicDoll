#pragma once

#include "CoreMinimal.h"
#include "StringFlowUnreal.h"
#include "UI/ModuleMainPanelBase.h"
#include "UI/SBoneControlMappingEditPanel.h"

class SStringFlowModulePropertiesPanel;
class SStringFlowModuleOperationsPanel;
class SLipSyncPanel;

/**
 * Main panel for StringFlow module
 * Inherits from SModuleMainPanelBase to get built-in tab functionality
 */
class STRINGFLOWUNREAL_API SStringFlowModuleMainPanel
    : public SModuleMainPanelBase {
   public:
    SLATE_BEGIN_ARGS(SStringFlowModuleMainPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleMainPanelBase interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual FString GetModuleName() const override {
        return TEXT("StringFlow");
    }
    virtual void RefreshPanel() override;

   private:
    // Currently displayed actor
    TWeakObjectPtr<AStringFlowUnreal> StringFlowActor;

    // Sub panels
    TSharedPtr<SStringFlowModulePropertiesPanel> PropertiesPanel;
    TSharedPtr<SStringFlowModuleOperationsPanel> OperationsPanel;
    TSharedPtr<SBoneControlMappingEditPanel> BoneControlMappingPanel;
    TSharedPtr<SLipSyncPanel> LipSyncPanel;
};