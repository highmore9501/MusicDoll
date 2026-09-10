#pragma once

#include "CoreMinimal.h"
#include "UI/ModulePropertiesPanelBase.h"

class ASingerUnreal;

/**
 * Singer 属性面板
 * 提供 Singer 演奏者 Control Rig 的基础初始化操作
 *
 * 布局：
 * - Control Rig 初始化：Add Eye Control
 *   （确保 base_root 存在，并在其下按「なごみ」曲线添加眼睛 Float channel）
 */
class SINGERUNREAL_API SSingerModulePropertiesPanel
    : public SModulePropertiesPanel {
   public:
    SLATE_BEGIN_ARGS(SSingerModulePropertiesPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModulePropertiesPanel interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshProperties() override;

   protected:
    virtual void CreatePropertyWidgets() override;

   private:
    TWeakObjectPtr<ASingerUnreal> SingerActor;

    /** 「Add Eye Control」按钮回调 */
    FReply OnAddEyeControl();
};
