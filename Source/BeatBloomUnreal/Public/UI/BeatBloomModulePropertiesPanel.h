#pragma once

#include "CoreMinimal.h"
#include "UI/ModulePropertiesPanelBase.h"

class ABeatBloomUnreal;

/**
 * BeatBloom 属性面板
 * 显示和编辑 BeatBloom Actor 的基础属性
 *
 * 布局：
 * - Basic Properties：Performer / DrumKit 骨骼引用、实时同步开关
 * - IO Configuration：Settings 文件路径、Animation 文件路径、DrumKit Config 路径
 * - DrumKit Info（只读）：配置名称、肢体列表、鼓件数量、特殊动作数量
 *
 * 对标参考：SFretDanceModulePropertiesPanel
 */
class BEATBLOOMUNREAL_API SBeatBloomModulePropertiesPanel
    : public SModulePropertiesPanel {
public:
    SLATE_BEGIN_ARGS(SBeatBloomModulePropertiesPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModulePropertiesPanel interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshProperties() override;

protected:
    virtual void CreatePropertyWidgets() override;

private:
    TWeakObjectPtr<ABeatBloomUnreal> BeatBloomActor;

    // 属性变更处理
    void OnFilePathChanged(const FString& PropertyPath,
                           const FString& NewFilePath);

    // 初始化操作处理
    FReply OnLoadDrumKitConfig();
    FReply OnSetupAllObjects();
    FReply OnCheckObjectsStatus();
    FReply OnExportRecorderInfo();
    FReply OnImportRecorderInfo();
};
