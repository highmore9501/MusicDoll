#include "UI/BeatBloomModuleMainPanel.h"

#include "BeatBloomUnreal.h"
#include "UI/BeatBloomBakeOperationsPanel.h"
#include "UI/BeatBloomModuleOperationsPanel.h"
#include "UI/BeatBloomModulePropertiesPanel.h"
#include "UI/SBoneControlMappingEditPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BeatBloomModuleMainPanel"

void SBeatBloomModuleMainPanel::Construct(const FArguments& InArgs) {
    // Initialize with basic module panel
    InitializeModulePanel(TEXT("BeatBloom"), FText(), FText());

    // Create sub panels
    PropertiesPanel = SNew(SBeatBloomModulePropertiesPanel);
    OperationsPanel = SNew(SBeatBloomModuleOperationsPanel);
    BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);
    BakeOperationsPanel = SNew(SBeatBloomBakeOperationsPanel);

    // Register panels (order matters for tab positions)
    RegisterPanel(PropertiesPanel, LOCTEXT("PropertiesTabLabel", "Properties"));
    RegisterPanel(OperationsPanel, LOCTEXT("OperationsTabLabel", "Operations"));
    RegisterPanel(BoneControlMappingPanel,
                  LOCTEXT("BoneControlMappingTabLabel", "B/C Mapping"));
    RegisterPanel(BakeOperationsPanel, LOCTEXT("BakeTabLabel", "Bake"));

    // Show first panel after all panels are registered
    ShowFirstPanel();
}

void SBeatBloomModuleMainPanel::SetActor(AActor* InActor) {
    BeatBloomActor = Cast<ABeatBloomUnreal>(InActor);

    if (PropertiesPanel.IsValid()) {
        PropertiesPanel->SetActor(InActor);
    }
    if (OperationsPanel.IsValid()) {
        OperationsPanel->SetActor(InActor);
    }
    if (BoneControlMappingPanel.IsValid()) {
        BoneControlMappingPanel->SetActor(InActor);
    }
    if (BakeOperationsPanel.IsValid()) {
        BakeOperationsPanel->SetActor(InActor);
    }
}

bool SBeatBloomModuleMainPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<ABeatBloomUnreal>();
}

void SBeatBloomModuleMainPanel::RefreshPanel() {
    if (PropertiesPanel.IsValid()) {
        PropertiesPanel->RefreshProperties();
    }
    if (OperationsPanel.IsValid()) {
        OperationsPanel->RefreshOperations();
    }
    if (BakeOperationsPanel.IsValid()) {
        BakeOperationsPanel->RefreshBakeOperations();
    }
}

#undef LOCTEXT_NAMESPACE
