#pragma once

#include "BeatBloomUnreal.h"
#include "CoreMinimal.h"
#include "UI/ModuleMainPanelBase.h"

class SBeatBloomModulePropertiesPanel;
class SBeatBloomModuleOperationsPanel;
class SBoneControlMappingEditPanel;
class SLipSyncPanel;

/**
 * BeatBloom 主面板
 * 继承自 SModuleMainPanelBase，获得内置的 Tab 切换功能
 * 包含属性面板、操作面板和烘焙面板子面板
 *
 * 对标参考：SFretDanceModuleMainPanel
 */
class BEATBLOOMUNREAL_API SBeatBloomModuleMainPanel
    : public SModuleMainPanelBase {
   public:
    SLATE_BEGIN_ARGS(SBeatBloomModuleMainPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleMainPanelBase interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual FString GetModuleName() const override { return TEXT("BeatBloom"); }
    virtual void RefreshPanel() override;

   private:
    TWeakObjectPtr<ABeatBloomUnreal> BeatBloomActor;

    // 子面板
    TSharedPtr<SBeatBloomModulePropertiesPanel> PropertiesPanel;
    TSharedPtr<SBeatBloomModuleOperationsPanel> OperationsPanel;
    TSharedPtr<SBoneControlMappingEditPanel> BoneControlMappingPanel;
    TSharedPtr<SLipSyncPanel> LipSyncPanel;
};
