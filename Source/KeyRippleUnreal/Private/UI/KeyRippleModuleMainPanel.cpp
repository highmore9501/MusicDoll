#include "UI/KeyRippleModuleMainPanel.h"

#include "UI/KeyRippleBakeOperationsPanel.h"
#include "UI/KeyRippleModuleOperationsPanel.h"
#include "UI/KeyRippleModulePropertiesPanel.h"
#include "UI/SBoneControlMappingEditPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SKeyRippleModuleMainPanel"

void SKeyRippleModuleMainPanel::Construct(const FArguments& InArgs) {
    // Initialize with basic module panel
    InitializeModulePanel(TEXT("KeyRipple"), FText(), FText());
    
    // Create sub panels
    PropertiesPanel = SNew(SKeyRippleModulePropertiesPanel);
    OperationsPanel = SNew(SKeyRippleModuleOperationsPanel);
    BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);
    BakeOperationsPanel = SNew(SKeyRippleBakeOperationsPanel);
    
    // Register panels (order matters for tab positions)
    RegisterPanel(PropertiesPanel, LOCTEXT("PropertiesTabLabel", "Properties"));
    RegisterPanel(OperationsPanel, LOCTEXT("OperationsTabLabel", "Operations"));
    RegisterPanel(BoneControlMappingPanel, LOCTEXT("BoneControlMappingTabLabel", "Bone Control Mapping"));
    RegisterPanel(BakeOperationsPanel, LOCTEXT("BakeTabLabel", "Bake"));
    	
    // Show first panel after all panels are registered
    ShowFirstPanel();
}

void SKeyRippleModuleMainPanel::SetActor(AActor* InActor) {
    KeyRippleActor = Cast<AKeyRippleUnreal>(InActor);
    
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
    
    if (BakeOperationsPanel.IsValid()) {
        BakeOperationsPanel->SetActor(InActor);
    }
    
    RefreshPanel();
}

bool SKeyRippleModuleMainPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AKeyRippleUnreal>();
}

void SKeyRippleModuleMainPanel::RefreshPanel() {
    if (PropertiesPanel.IsValid()) {
        PropertiesPanel->RefreshProperties();
    }
    
    if (OperationsPanel.IsValid()) {
        OperationsPanel->RefreshOperations();
    }
}

#undef LOCTEXT_NAMESPACE