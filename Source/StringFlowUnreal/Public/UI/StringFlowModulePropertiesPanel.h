#pragma once

#include "CoreMinimal.h"
#include "UI/ModulePropertiesPanelBase.h"

class AStringFlowUnreal;

/**
 * Properties panel for StringFlow module
 * Displays and edits StringFlow configuration properties
 */
class STRINGFLOWUNREAL_API SStringFlowModulePropertiesPanel
    : public SModulePropertiesPanel {
   public:
    SLATE_BEGIN_ARGS(SStringFlowModulePropertiesPanel) {}
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
    TWeakObjectPtr<AStringFlowUnreal> StringFlowActor;

    // Instrument type options for combo box
    TArray<TSharedPtr<FString>> InstrumentTypeOptions;

    // Property change handlers
    void OnNumericPropertyChanged(const FString& PropertyPath, int32 NewValue);
    void OnFilePathChanged(const FString& PropertyPath,
                           const FString& NewFilePath);
    void OnInstrumentTypeChanged(int32 SelectedType);

    // Initialization operation handlers
    FReply OnCheckObjectsStatus();
    FReply OnSetupAllObjects();
    FReply OnExportRecorderInfo();
    FReply OnImportRecorderInfo();
    FReply OnExportToBlender();

    // Blender 格式导出文件路径（在资源管理器中选择）
    FString BlenderExportFilePath;
};