#pragma once

#include "CoreMinimal.h"
#include "HarpGlideUnreal.h"
#include "UI/ModulePropertiesPanelBase.h"

/**
 * 竖琴模块属性面板
 */
class HARPGLIDEUNREAL_API SHarpGlideModulePropertiesPanel
    : public SModulePropertiesPanel {
   public:
    SLATE_BEGIN_ARGS(SHarpGlideModulePropertiesPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshProperties() override;

   protected:
    virtual void CreatePropertyWidgets() override;

   private:
    TWeakObjectPtr<AHarpGlideUnreal> HarpGlideActor;

    void OnNumericPropertyChanged(const FString& PropertyPath, int32 NewValue);
    FReply OnCheckObjectsStatus();
    FReply OnSetupAllObjects();
    FReply OnExportRecorderInfo();
    FReply OnImportRecorderInfo();
    FReply OnInitHarpInstrument();
    FReply OnExportToBlender();

    // Blender 格式导出文件路径（在资源管理器中选择）
    FString BlenderExportFilePath;
};
