#include "UI/SingerModuleMainPanel.h"

#include "SingerUnreal.h"
#include "UI/SBoneControlMappingEditPanel.h"
#include "UI/SLipSyncPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SingerModuleMainPanel"

void SSingerModuleMainPanel::Construct(const FArguments& InArgs) {
    // 初始化基础面板（Singer 只需要两个 Tab）
    InitializeModulePanel(TEXT("Singer"), FText(), FText());

    // 创建子面板（仅有 LipSync 和 B/C Mapping，无 Properties/Operations）
    LipSyncPanel = SNew(SLipSyncPanel);
    BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);

    // 注册 Tab（顺序决定 Tab 位置）
    RegisterPanel(LipSyncPanel, LOCTEXT("LipSyncTabLabel", "Lip Sync"));
    RegisterPanel(BoneControlMappingPanel,
                  LOCTEXT("BoneControlMappingTabLabel", "B/C Mapping"));

    // Show first panel after all panels are registered
    ShowFirstPanel();
}

void SSingerModuleMainPanel::SetActor(AActor* InActor) {
    SingerActor = Cast<ASingerUnreal>(InActor);

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
    // Singer 没有 Properties/Operations 面板，无需刷新
}

#undef LOCTEXT_NAMESPACE
