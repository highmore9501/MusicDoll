#pragma once

#include "CoreMinimal.h"
#include "UI/ModuleMainPanelBase.h"
#include "UI/SBoneControlMappingEditPanel.h"

class ASingerUnreal;
class SLipSyncPanel;
class SSingerModulePropertiesPanel;

/**
 * Singer 模块主面板
 * 继承自 SModuleMainPanelBase，内置 Tab 切换功能
 *
 * Singer 不需要乐器演奏，仅通过 Lip Sync 驱动角色口型动画。
 * 因此面板包含以下 Tab：
 * - Properties：Control Rig 初始化（Add Eye Control）
 * - Lip Sync：口型映射编辑 + Lip Sync 动画生成
 * - B/C Mapping：骨骼控制映射编辑
 */
class SINGERUNREAL_API SSingerModuleMainPanel : public SModuleMainPanelBase {
   public:
    SLATE_BEGIN_ARGS(SSingerModuleMainPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleMainPanelBase interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual FString GetModuleName() const override { return TEXT("Singer"); }
    virtual void RefreshPanel() override;

   private:
    TWeakObjectPtr<ASingerUnreal> SingerActor;

    // 子面板（Properties + LipSync + B/C Mapping，无 Operations）
    TSharedPtr<SSingerModulePropertiesPanel> PropertiesPanel;
    TSharedPtr<SLipSyncPanel> LipSyncPanel;
    TSharedPtr<SBoneControlMappingEditPanel> BoneControlMappingPanel;
};
