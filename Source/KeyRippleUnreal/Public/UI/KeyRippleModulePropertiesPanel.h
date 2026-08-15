#pragma once

#include "CoreMinimal.h"
#include "UI/ModulePropertiesPanelBase.h"

class AKeyRippleUnreal;

/**
 * Properties panel for KeyRipple module
 * Displays and edits KeyRipple configuration properties
 */
class KEYRIPPLEUNREAL_API SKeyRippleModulePropertiesPanel
    : public SModulePropertiesPanel {
   public:
    SLATE_BEGIN_ARGS(SKeyRippleModulePropertiesPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModulePropertiesPanel interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshProperties() override;

   protected:
    virtual void CreatePropertyWidgets() override;

   private:
    // Currently displayed actor
    TWeakObjectPtr<AKeyRippleUnreal> KeyRippleActor;

    // Property change handlers
    void OnNumericPropertyChanged(const FString& PropertyPath, int32 NewValue);
    void OnStringPropertyChanged(const FString& PropertyPath,
                                 const FString& NewValue);
    void OnEnumPropertyChanged(const FString& PropertyPath, uint8 NewValue);

    // Initialization operation handlers
    FReply OnCheckObjectsStatus();
    FReply OnSetupAllObjects();
    FReply OnExportRecorderInfo();
    FReply OnImportRecorderInfo();
    FReply OnExportToBlender();

    // Blender 格式导出文件路径（在资源管理器中选择）
    FString BlenderExportFilePath;
};