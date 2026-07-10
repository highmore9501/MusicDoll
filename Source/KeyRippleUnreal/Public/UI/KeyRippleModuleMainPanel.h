#pragma once

#include "CoreMinimal.h"
#include "KeyRippleUnreal.h"
#include "UI/ModuleMainPanelBase.h"
#include "UI/SBoneControlMappingEditPanel.h"

class SKeyRippleModulePropertiesPanel;
class SKeyRippleModuleOperationsPanel;
class SLipSyncPanel;

/**
 * Main panel for KeyRipple module
 * Inherits from SModuleMainPanelBase to get built-in tab functionality
 */
class KEYRIPPLEUNREAL_API SKeyRippleModuleMainPanel
    : public SModuleMainPanelBase {
   public:
    SLATE_BEGIN_ARGS(SKeyRippleModuleMainPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleMainPanelBase interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual FString GetModuleName() const override { return TEXT("KeyRipple"); }
    virtual void RefreshPanel() override;

   private:
    // Currently displayed actor
    TWeakObjectPtr<AKeyRippleUnreal> KeyRippleActor;

    // Sub panels
    TSharedPtr<SKeyRippleModulePropertiesPanel> PropertiesPanel;
    TSharedPtr<SKeyRippleModuleOperationsPanel> OperationsPanel;
    TSharedPtr<SBoneControlMappingEditPanel> BoneControlMappingPanel;
    TSharedPtr<SLipSyncPanel> LipSyncPanel;
};