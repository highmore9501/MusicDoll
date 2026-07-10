#pragma once

#include "CoreMinimal.h"
#include "HarpGlideUnreal.h"
#include "UI/ModuleMainPanelBase.h"
#include "UI/SBoneControlMappingEditPanel.h"

class SHarpGlideModulePropertiesPanel;
class SHarpGlideModuleOperationsPanel;
class SLipSyncPanel;

/**
 * 竖琴模块主面板
 * 提供 Properties / Operations / Bone Control Mapping / Bake 四个 Tab
 */
class HARPGLIDEUNREAL_API SHarpGlideModuleMainPanel
    : public SModuleMainPanelBase {
   public:
    SLATE_BEGIN_ARGS(SHarpGlideModuleMainPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual FString GetModuleName() const override { return TEXT("HarpGlide"); }
    virtual void RefreshPanel() override;

   private:
    TWeakObjectPtr<AHarpGlideUnreal> HarpGlideActor;

    TSharedPtr<SHarpGlideModulePropertiesPanel> PropertiesPanel;
    TSharedPtr<SHarpGlideModuleOperationsPanel> OperationsPanel;
    TSharedPtr<SBoneControlMappingEditPanel> BoneControlMappingPanel;
    TSharedPtr<SLipSyncPanel> LipSyncPanel;
};
