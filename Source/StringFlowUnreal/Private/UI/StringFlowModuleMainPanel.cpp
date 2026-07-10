#include "UI/StringFlowModuleMainPanel.h"

#include "UI/SBoneControlMappingEditPanel.h"
#include "UI/SLipSyncPanel.h"
#include "UI/StringFlowModuleOperationsPanel.h"
#include "UI/StringFlowModulePropertiesPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SStringFlowModuleMainPanel"

void SStringFlowModuleMainPanel::Construct(const FArguments& InArgs) {
    // Initialize with basic module panel
    InitializeModulePanel(TEXT("StringFlow"), FText(), FText());

    // Create sub panels
    PropertiesPanel = SNew(SStringFlowModulePropertiesPanel);
    OperationsPanel = SNew(SStringFlowModuleOperationsPanel);
    BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);
    LipSyncPanel = SNew(SLipSyncPanel);

    // Register panels (order matters for tab positions)
    RegisterPanel(PropertiesPanel, LOCTEXT("PropertiesTabLabel", "Properties"));
    RegisterPanel(OperationsPanel, LOCTEXT("OperationsTabLabel", "Operations"));
    RegisterPanel(BoneControlMappingPanel,
                  LOCTEXT("BoneControlMappingTabLabel", "B/C Mapping"));
    RegisterPanel(LipSyncPanel, LOCTEXT("LipSyncTabLabel", "Lip Sync"));

    // Show first panel after all panels are registered
    ShowFirstPanel();
}

void SStringFlowModuleMainPanel::SetActor(AActor* InActor) {
    StringFlowActor = Cast<AStringFlowUnreal>(InActor);

    // Set actor for all panels
    if (PropertiesPanel.IsValid()) {
        PropertiesPanel->SetActor(InActor);
    }

    if (OperationsPanel.IsValid()) {
        OperationsPanel->SetActor(InActor);
    }

    if (BoneControlMappingPanel.IsValid()) {
        BoneControlMappingPanel->SetActor(InActor);
    }

    if (LipSyncPanel.IsValid()) {
        LipSyncPanel->SetActor(InActor);
    }

    RefreshPanel();
}

bool SStringFlowModuleMainPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AStringFlowUnreal>();
}

void SStringFlowModuleMainPanel::RefreshPanel() {
    if (PropertiesPanel.IsValid()) {
        PropertiesPanel->RefreshProperties();
    }

    if (OperationsPanel.IsValid()) {
        OperationsPanel->RefreshOperations();
    }
}

#undef LOCTEXT_NAMESPACE