#pragma once

#include "CoreMinimal.h"
#include "UI/ModuleMainPanelBase.h"
#include "UI/SBoneControlMappingEditPanel.h"
#include "ZhengDriftUnreal.h"

class SZhengDriftModulePropertiesPanel;
class SZhengDriftModuleOperationsPanel;
class SLipSyncPanel;

/**
 * 古筝模块主面板
 * 继承 SModuleMainPanelBase，提供 Properties / Operations / Bone Control
 * Mapping / Bake 四个 Tab
 */
class ZHENGDRIFTUNREAL_API SZhengDriftModuleMainPanel
    : public SModuleMainPanelBase {
   public:
    SLATE_BEGIN_ARGS(SZhengDriftModuleMainPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleMainPanelBase interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual FString GetModuleName() const override {
        return TEXT("ZhengDrift");
    }
    virtual void RefreshPanel() override;

   private:
    TWeakObjectPtr<AZhengDriftUnreal> ZhengDriftActor;

    TSharedPtr<SZhengDriftModulePropertiesPanel> PropertiesPanel;
    TSharedPtr<SZhengDriftModuleOperationsPanel> OperationsPanel;
    TSharedPtr<SBoneControlMappingEditPanel> BoneControlMappingPanel;
    TSharedPtr<SLipSyncPanel> LipSyncPanel;
};
