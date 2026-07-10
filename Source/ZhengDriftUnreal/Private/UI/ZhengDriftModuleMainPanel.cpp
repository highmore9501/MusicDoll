#include "UI/ZhengDriftModuleMainPanel.h"

#include "UI/SBoneControlMappingEditPanel.h"
#include "UI/SLipSyncPanel.h"
#include "UI/ZhengDriftModuleOperationsPanel.h"
#include "UI/ZhengDriftModulePropertiesPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SZhengDriftModuleMainPanel"

void SZhengDriftModuleMainPanel::Construct(const FArguments& InArgs) {
    // 初始化基础面板
    InitializeModulePanel(TEXT("ZhengDrift"), FText(), FText());

    // 创建子面板
    PropertiesPanel = SNew(SZhengDriftModulePropertiesPanel);
    OperationsPanel = SNew(SZhengDriftModuleOperationsPanel);
    BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);
    LipSyncPanel = SNew(SLipSyncPanel);

    // 注册 Tab（顺序决定 Tab 位置）
    RegisterPanel(PropertiesPanel, LOCTEXT("PropertiesTabLabel", "Properties"));
    RegisterPanel(OperationsPanel, LOCTEXT("OperationsTabLabel", "Operations"));
    RegisterPanel(BoneControlMappingPanel,
                  LOCTEXT("BoneControlMappingTabLabel", "B/C Mapping"));
    RegisterPanel(LipSyncPanel, LOCTEXT("LipSyncTabLabel", "Lip Sync"));

    ShowFirstPanel();
}

void SZhengDriftModuleMainPanel::SetActor(AActor* InActor) {
    ZhengDriftActor = Cast<AZhengDriftUnreal>(InActor);

    if (PropertiesPanel.IsValid()) PropertiesPanel->SetActor(InActor);
    if (OperationsPanel.IsValid()) OperationsPanel->SetActor(InActor);
    if (BoneControlMappingPanel.IsValid())
        BoneControlMappingPanel->SetActor(InActor);
    if (LipSyncPanel.IsValid()) LipSyncPanel->SetActor(InActor);

    RefreshPanel();
}

bool SZhengDriftModuleMainPanel::CanHandleActor(const AActor* InActor) const {
    return Cast<const AZhengDriftUnreal>(InActor) != nullptr;
}

void SZhengDriftModuleMainPanel::RefreshPanel() {
    if (PropertiesPanel.IsValid()) PropertiesPanel->RefreshProperties();
    if (OperationsPanel.IsValid()) OperationsPanel->RefreshOperations();
}

#undef LOCTEXT_NAMESPACE
