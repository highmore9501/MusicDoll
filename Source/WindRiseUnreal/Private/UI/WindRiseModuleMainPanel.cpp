#include "UI/WindRiseModuleMainPanel.h"

#include "UI/SBoneControlMappingEditPanel.h"
#include "UI/WindRiseModuleOperationsPanel.h"
#include "UI/WindRiseModulePropertiesPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "WindRiseUnreal.h"

#define LOCTEXT_NAMESPACE "WindRiseModuleMainPanel"

void SWindRiseModuleMainPanel::Construct(const FArguments& InArgs) {
    // Initialize with basic module panel (no LipSync, so no fourth tab)
    InitializeModulePanel(TEXT("WindRise"),
                          LOCTEXT("PropertiesTabLabel", "Properties"),
                          LOCTEXT("OperationsTabLabel", "Operations"),
                          LOCTEXT("BoneControlMappingTabLabel", "B/C Mapping"));

    // Create sub panels
    PropertiesPanel = SNew(SWindRiseModulePropertiesPanel);
    OperationsPanel = SNew(SWindRiseModuleOperationsPanel);
    BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);

    // Register panels (order matters for tab positions)
    RegisterPanel(PropertiesPanel, LOCTEXT("PropertiesTabLabel", "Properties"));
    RegisterPanel(OperationsPanel, LOCTEXT("OperationsTabLabel", "Operations"));
    RegisterPanel(BoneControlMappingPanel,
                  LOCTEXT("BoneControlMappingTabLabel", "B/C Mapping"));

    // Show first panel after all panels are registered
    ShowFirstPanel();
}

void SWindRiseModuleMainPanel::SetActor(AActor* InActor) {
    WindRiseActor = Cast<AWindRiseUnreal>(InActor);

    if (PropertiesPanel.IsValid()) {
        PropertiesPanel->SetActor(InActor);
    }
    if (OperationsPanel.IsValid()) {
        OperationsPanel->SetActor(InActor);
    }
    if (BoneControlMappingPanel.IsValid()) {
        BoneControlMappingPanel->SetActor(InActor);
    }
}

bool SWindRiseModuleMainPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AWindRiseUnreal>();
}

void SWindRiseModuleMainPanel::RefreshPanel() {
    if (PropertiesPanel.IsValid()) {
        PropertiesPanel->RefreshProperties();
    }
    if (OperationsPanel.IsValid()) {
        OperationsPanel->RefreshOperations();
    }
}

#undef LOCTEXT_NAMESPACE
