#include "UI/HarpGlideModuleMainPanel.h"

#include "UI/HarpGlideModuleOperationsPanel.h"
#include "UI/HarpGlideModulePropertiesPanel.h"
#include "UI/SLipSyncPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHarpGlideModuleMainPanel"

void SHarpGlideModuleMainPanel::Construct(const FArguments& InArgs) {
    // Initialize with basic module panel
    InitializeModulePanel(TEXT("HarpGlide"), FText(), FText());

    // 创建子面板
    PropertiesPanel = SNew(SHarpGlideModulePropertiesPanel);
    OperationsPanel = SNew(SHarpGlideModuleOperationsPanel);
    BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);
    LipSyncPanel = SNew(SLipSyncPanel);

    // 注册 Tab（顺序决定 Tab 位置）
    RegisterPanel(PropertiesPanel, LOCTEXT("PropertiesTabLabel", "Properties"));
    RegisterPanel(OperationsPanel, LOCTEXT("OperationsTabLabel", "Operations"));
    RegisterPanel(BoneControlMappingPanel,
                  LOCTEXT("BoneControlMappingTabLabel", "B/C Mapping"));
    RegisterPanel(LipSyncPanel, LOCTEXT("LipSyncTabLabel", "Lip Sync"));

    // Show first panel after all panels are registered
    ShowFirstPanel();
}

void SHarpGlideModuleMainPanel::SetActor(AActor* InActor) {
    HarpGlideActor = Cast<AHarpGlideUnreal>(InActor);

    if (PropertiesPanel.IsValid()) PropertiesPanel->SetActor(InActor);
    if (OperationsPanel.IsValid()) OperationsPanel->SetActor(InActor);
    if (BoneControlMappingPanel.IsValid())
        BoneControlMappingPanel->SetActor(InActor);
    if (LipSyncPanel.IsValid()) LipSyncPanel->SetActor(InActor);

    RefreshPanel();
}

bool SHarpGlideModuleMainPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AHarpGlideUnreal>();
}

void SHarpGlideModuleMainPanel::RefreshPanel() {
    if (PropertiesPanel.IsValid()) PropertiesPanel->RefreshProperties();
    if (OperationsPanel.IsValid()) OperationsPanel->RefreshOperations();
    // Bake tab removed — replaced by Lip Sync
}

#undef LOCTEXT_NAMESPACE
