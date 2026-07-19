#pragma once

#include "CoreMinimal.h"
#include "UI/ModuleMainPanelBase.h"

class AWindRiseUnreal;
class SWindRiseModulePropertiesPanel;
class SWindRiseModuleOperationsPanel;
class SBoneControlMappingEditPanel;

/**
 * WindRise 主面板
 * 继承自 SModuleMainPanelBase，内置 Tab 切换功能
 *
 * Tab 布局：
 * - Properties：Config 编辑 + MT 选择 + CR 初始化 + .wind 文件
 * - Operations：音高选择 + MT 调整 + Save/Load + 动画生成
 * - B/C Mapping：复用 SBoneControlMappingEditPanel
 * - 无 LipSync Tab（管乐器嘴已用于演奏）
 */
class WINDRISEUNREAL_API SWindRiseModuleMainPanel
    : public SModuleMainPanelBase {
   public:
    SLATE_BEGIN_ARGS(SWindRiseModuleMainPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModuleMainPanelBase interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual FString GetModuleName() const override { return TEXT("WindRise"); }
    virtual void RefreshPanel() override;

   private:
    TWeakObjectPtr<AWindRiseUnreal> WindRiseActor;

    // 子面板
    TSharedPtr<SWindRiseModulePropertiesPanel> PropertiesPanel;
    TSharedPtr<SWindRiseModuleOperationsPanel> OperationsPanel;
    TSharedPtr<SBoneControlMappingEditPanel> BoneControlMappingPanel;
};
