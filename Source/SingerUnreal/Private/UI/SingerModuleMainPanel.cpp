#include "UI/SingerModuleMainPanel.h"

#include "SingerUnreal.h"
#include "UI/SBoneControlMappingEditPanel.h"
#include "UI/SingerModulePropertiesPanel.h"
#include "UI/SLipSyncPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SingerModuleMainPanel"

void SSingerModuleMainPanel::Construct(const FArguments& InArgs) {
    // 初始化基础面板（Singer 无 Operations Tab）
    InitializeModulePanel(TEXT("Singer"), FText(), FText());

    // 创建子面板（Properties + LipSync + B/C Mapping）
    PropertiesPanel = SNew(SSingerModulePropertiesPanel);
    LipSyncPanel = SNew(SLipSyncPanel);
    BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);

    // 注册 Tab（顺序决定 Tab 位置）
    RegisterPanel(PropertiesPanel, LOCTEXT("PropertiesTabLabel", "Properties"));
    RegisterPanel(BoneControlMappingPanel,
                  LOCTEXT("BoneControlMappingTabLabel", "B/C Mapping"));
    RegisterPanel(LipSyncPanel, LOCTEXT("LipSyncTabLabel", "Lip Sync"));

    // Show first panel after all panels are registered
    ShowFirstPanel();
}

void SSingerModuleMainPanel::SetActor(AActor* InActor) {
    SingerActor = Cast<ASingerUnreal>(InActor);

    if (PropertiesPanel.IsValid()) {
        PropertiesPanel->SetActor(InActor);
    }
    if (LipSyncPanel.IsValid()) {
        LipSyncPanel->SetActor(InActor);
    }
    if (BoneControlMappingPanel.IsValid()) {
        BoneControlMappingPanel->SetActor(InActor);
    }
}

bool SSingerModuleMainPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<ASingerUnreal>();
}

void SSingerModuleMainPanel::RefreshPanel() {
    if (PropertiesPanel.IsValid()) {
        PropertiesPanel->RefreshProperties();
    }
    // Singer 没有 Operations 面板，无需刷新
}

#undef LOCTEXT_NAMESPACE
