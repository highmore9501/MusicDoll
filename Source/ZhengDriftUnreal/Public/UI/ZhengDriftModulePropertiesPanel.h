#pragma once

#include "CoreMinimal.h"
#include "UI/ModulePropertiesPanelBase.h"
#include "ZhengDriftUnreal.h"

/**
 * 古筝模块属性面板
 */
class ZHENGDRIFTUNREAL_API SZhengDriftModulePropertiesPanel
    : public SModulePropertiesPanel {
public:
    SLATE_BEGIN_ARGS(SZhengDriftModulePropertiesPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshProperties() override;

protected:
    virtual void CreatePropertyWidgets() override;

private:
    TWeakObjectPtr<AZhengDriftUnreal> ZhengDriftActor;

    FReply OnCheckObjectsStatus();
    FReply OnSetupAllObjects();
    FReply OnExportRecorderInfo();
    FReply OnImportRecorderInfo();
    FReply OnInitZhengInstrument();
};
