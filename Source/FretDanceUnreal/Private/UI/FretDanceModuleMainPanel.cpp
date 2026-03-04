#include "UI/FretDanceModuleMainPanel.h"

#include "UI/FretDanceModulePropertiesPanel.h"
#include "UI/FretDanceModuleOperationsPanel.h"
#include "UI/FretDanceBakeOperationsPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "SFretDanceModuleMainPanel"

void SFretDanceModuleMainPanel::Construct(const FArguments& InArgs)
{
	// Initialize with basic module panel
	InitializeModulePanel(TEXT("FretDance"), FText(), FText());
	
	// Create sub panels
	PropertiesPanel = SNew(SFretDanceModulePropertiesPanel);
	OperationsPanel = SNew(SFretDanceModuleOperationsPanel);
	BoneControlMappingPanel = SNew(SBoneControlMappingEditPanel);
	BakeOperationsPanel = SNew(SFretDanceBakeOperationsPanel);
	
	// Register tabs (order matters for tab positions)
	RegisterPanel(PropertiesPanel, LOCTEXT("PropertiesTabLabel", "Properties"));
	RegisterPanel(OperationsPanel, LOCTEXT("OperationsTabLabel", "Operations"));
	RegisterPanel(BoneControlMappingPanel, LOCTEXT("BoneControlMappingTabLabel", "Bone Control Mapping"));
	RegisterPanel(BakeOperationsPanel, LOCTEXT("BakeTabLabel", "Bake"));
	
	// Show first panel after all panels are registered
	ShowFirstPanel();
}

void SFretDanceModuleMainPanel::SetActor(AActor* InActor)
{
	FretDanceActor = Cast<AFretDanceUnreal>(InActor);
	
	// Set actor for all panels
	if (PropertiesPanel.IsValid())
	{
		PropertiesPanel->SetActor(InActor);
	}
	
	if (OperationsPanel.IsValid())
	{
		OperationsPanel->SetActor(InActor);
	}
	
	if (BoneControlMappingPanel.IsValid())
	{
		BoneControlMappingPanel->SetActor(InActor);
	}
	
	if (BakeOperationsPanel.IsValid())
	{
		BakeOperationsPanel->SetActor(InActor);
	}
	
	RefreshPanel();
}

bool SFretDanceModuleMainPanel::CanHandleActor(const AActor* InActor) const
{
	return InActor && InActor->IsA<AFretDanceUnreal>();
}

void SFretDanceModuleMainPanel::RefreshPanel()
{
	if (PropertiesPanel.IsValid())
	{
		PropertiesPanel->RefreshProperties();
	}
	
	if (OperationsPanel.IsValid())
	{
		OperationsPanel->RefreshOperations();
	}
}

#undef LOCTEXT_NAMESPACE
