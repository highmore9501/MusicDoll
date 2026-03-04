#pragma once

#include "CoreMinimal.h"
#include "FretDanceUnreal.h"
#include "UI/ModuleMainPanelBase.h"
#include "UI/SBoneControlMappingEditPanel.h"
#include "UI/FretDanceBakeOperationsPanel.h"

class SFretDanceModulePropertiesPanel;
class SFretDanceModuleOperationsPanel;

/**
 * Main panel for FretDance module
 * Inherits from SModuleMainPanelBase to get built-in tab functionality
 */
class FRETDANCEUNREAL_API SFretDanceModuleMainPanel
    : public SModuleMainPanelBase {
   public:
    SLATE_BEGIN_ARGS(SFretDanceModuleMainPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleMainPanelBase interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual FString GetModuleName() const override {
        return TEXT("FretDance");
    }
    virtual void RefreshPanel() override;

   private:
    // Currently displayed actor
    TWeakObjectPtr<AFretDanceUnreal> FretDanceActor;

    // Sub panels
    TSharedPtr<SFretDanceModulePropertiesPanel> PropertiesPanel;
    TSharedPtr<SFretDanceModuleOperationsPanel> OperationsPanel;
    TSharedPtr<SBoneControlMappingEditPanel> BoneControlMappingPanel;
    TSharedPtr<SFretDanceBakeOperationsPanel> BakeOperationsPanel;
};