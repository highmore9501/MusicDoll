#include "UI/FretDanceModuleOperationsPanel.h"

#include "FretDanceAnimationProcessor.h"
#include "FretDanceControlRigProcessor.h"
#include "FretDanceMusicInstrumentProcessor.h"
#include "FretDanceUnreal.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SFretDanceModuleOperationsPanel"

void SFretDanceModuleOperationsPanel::CreateOperationWidgets() {
	auto Container = GetOperationContainer();
	if (!Container.IsValid()) {
		return;
	}

	Container->ClearChildren();

	if (!FretDanceActor.IsValid()) {
		Container->AddSlot().AutoHeight().Padding(
			5.0f)[SNew(STextBlock)
						.Text(LOCTEXT("NoActorSelected",
							"No FretDance Actor Selected"))
						.ColorAndOpacity(FLinearColor::Yellow)];
		return;
	}

	AFretDanceUnreal* FretDance = FretDanceActor.Get();

	// NOTE:
	// The original implementation included UI controls for left/right hand
	// position and fret/string indices copied from other modules. Those
	// enums and pairing logic differ for FretDance and are complex.
	// Per instruction, clear that UI area and leave a placeholder for
	// later rework.

	// Hand State Configuration - placeholder (complex, postponed)
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
		TEXT("Hand State Configuration (TODO)"))];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(STextBlock)
		  .Text(LOCTEXT("HandStatePlaceholder",
					"Hand state UI cleared. Rework required to use EFretDance enums."))
		  .ColorAndOpacity(FLinearColor::Yellow)];

	// Animation File Path Section (moved above generation UI)
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 0.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
		TEXT("Animation File Path"))];

	TSharedPtr<SEditableTextBox> AnimationFilePathBox;
	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SHorizontalBox) +
		 SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
			 [SAssignNew(AnimationFilePathBox, SEditableTextBox)
				  .Text_Lambda([this]() -> FText {
					  if (FretDanceActor.IsValid()) {
						  return FText::FromString(
							  FretDanceActor->AnimationFilePath);
					  }
					  return FText::FromString(TEXT(""));
				  })
				  .OnTextCommitted_Lambda([this](const FText& InText,
						 ETextCommit::Type CommitType) {
					  if (CommitType == ETextCommit::OnEnter ||
						  CommitType == ETextCommit::OnUserMovedFocus) {
						  if (FretDanceActor.IsValid()) {
							  FretDanceActor->AnimationFilePath =
								  InText.ToString();
							  FretDanceActor->Modify();
						  }
					  }
				  })] +
		 SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
			 [SNew(SButton)
				  .Text(LOCTEXT("BrowseButton", "Browse"))
				  .OnClicked_Lambda([this, AnimationFilePathBox]() -> FReply {
					  if (!FretDanceActor.IsValid()) {
						  return FReply::Handled();
					  }

					  FString FilePath;
					  if (FCommonPanelUtility::BrowseForFile(
						  TEXT(".json"), FilePath, false)) {
						  if (AnimationFilePathBox.IsValid()) {
							  AnimationFilePathBox->SetText(
								  FText::FromString(FilePath));
							  FretDanceActor->AnimationFilePath = FilePath;
							  FretDanceActor->Modify();
						  }
					  }
					  return FReply::Handled();
				  })]];

	// State Management Buttons
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
		TEXT("State Management"))];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SHorizontalBox) +
		 SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
			 [SNew(SButton)
				  .Text(LOCTEXT("SaveLeftButton", "Save Left"))
				  .OnClicked(this, &SFretDanceModuleOperationsPanel::OnSaveLeft)
				  .HAlign(HAlign_Center)
				  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
		 SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
			 [SNew(SButton)
				  .Text(LOCTEXT("SaveRightButton", "Save Right"))
				  .OnClicked(this, &SFretDanceModuleOperationsPanel::OnSaveRight)
				  .HAlign(HAlign_Center)
				  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

	Container->AddSlot().AutoHeight().Padding(5.0f)
		[SNew(SButton)
		 .Text(LOCTEXT("LoadStateButton", "Load State"))
		 .OnClicked(this, &SFretDanceModuleOperationsPanel::OnLoadState)
		 .HAlign(HAlign_Center)
		 .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	// Animation Generation Section
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
		TEXT("Animation Generation"))];

	Container->AddSlot().AutoHeight().Padding(
		5.0f)[SNew(SButton)
			  .Text(LOCTEXT("GeneratePerformerAnimationButton",
						"Generate Performer Animation"))
			  .OnClicked(this,
				 &SFretDanceModuleOperationsPanel::
				 OnGeneratePerformerAnimation)
			  .HAlign(HAlign_Center)
			  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	Container->AddSlot().AutoHeight().Padding(
		5.0f)[SNew(SButton)
			  .Text(LOCTEXT("GenerateStringAnimationButton",
						"Generate String Animation"))
			  .OnClicked(this,
				 &SFretDanceModuleOperationsPanel::
				 OnGenerateStringAnimation)
			  .HAlign(HAlign_Center)
			  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	Container->AddSlot().AutoHeight().Padding(
		5.0f)[SNew(SButton)
			  .Text(LOCTEXT("GenerateAllAnimationButton",
						"Generate All Animation"))
			  .OnClicked(
				  this,
				  &SFretDanceModuleOperationsPanel::OnGenerateAllAnimation)
			  .HAlign(HAlign_Center)
			  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	// Control Rig Section
	Container->AddSlot().AutoHeight().Padding(
		5.0f, 15.0f, 5.0f,
		15.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Control Rig"))];

	Container->AddSlot().AutoHeight().Padding(
		5.0f)[SNew(SButton)
			  .Text(LOCTEXT("InitGuitarInstrumentButton",
					"Initialize Guitar Instrument"))
			  .OnClicked(
				  this,
				  &SFretDanceModuleOperationsPanel::OnInitGuitarInstrument)
			  .HAlign(HAlign_Center)
			  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

	Container->AddSlot().AutoHeight().Padding(
		5.0f)[SNew(SButton)
			  .Text(LOCTEXT("TriggerControlRigReregistrationButton",
					"Trigger Control Rig Re-registration"))
			  .OnClicked(
				  this,
				  &SFretDanceModuleOperationsPanel::
					  OnTriggerControlRigReregistration)
			  .HAlign(HAlign_Center)
			  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

void SFretDanceModuleOperationsPanel::UpdateLeftHandStateOptions() {
    // Intentionally left blank as a placeholder for future UI logic.
}

void SFretDanceModuleOperationsPanel::Construct(const FArguments& InArgs) {
	// Call base class constructor with base parameter types
	SModuleOperationsPanel::FArguments BaseArgs;
	SModuleOperationsPanel::Construct(BaseArgs);
	
	// Initialization of left hand state options postponed; placeholder only
}

void SFretDanceModuleOperationsPanel::SetActor(AActor* InActor) {
	FretDanceActor = Cast<AFretDanceUnreal>(InActor);
	RefreshOperations();
}

bool SFretDanceModuleOperationsPanel::CanHandleActor(
	const AActor* InActor) const {
	return Cast<const AFretDanceUnreal>(InActor) != nullptr;
}

void SFretDanceModuleOperationsPanel::RefreshOperations() {
	// Do not attempt to update left-hand state options here; rework later
	CreateOperationWidgets();
}

FReply SFretDanceModuleOperationsPanel::OnSaveLeft() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for save left"));
		return FReply::Handled();
	}

	TMap<FString, FTransform> OutState;
	UFretDanceControlRigProcessor::SaveLeftHandState(FretDanceActor.Get(), OutState);
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Save Left operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnSaveRight() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for save right"));
		return FReply::Handled();
	}

	TMap<FString, FTransform> OutState;
	UFretDanceControlRigProcessor::SaveRightHandState(FretDanceActor.Get(), OutState);
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Save Right operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnLoadState() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for load state"));
		return FReply::Handled();
	}

	// Provide an empty state map for loading; caller can adapt to different sources if needed
	TMap<FString, FTransform> StateData;
	UFretDanceControlRigProcessor::LoadState(FretDanceActor.Get(), StateData);
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Load State operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnGeneratePerformerAnimation() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for generate performer animation"));
		return FReply::Handled();
	}

	UFretDanceAnimationProcessor::GeneratePerformerAnimation(FretDanceActor.Get());
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Generate Performer Animation operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnGenerateStringAnimation() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for generate string animation"));
		return FReply::Handled();
	}

	UFretDanceAnimationProcessor::GenerateInstrumentAnimation(FretDanceActor.Get());
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Generate String Animation operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnGenerateAllAnimation() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for generate all animation"));
		return FReply::Handled();
	}

	UFretDanceAnimationProcessor::GenerateAllAnimation(FretDanceActor.Get());
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Generate All Animation operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnInitGuitarInstrument() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for initialize guitar instrument"));
		return FReply::Handled();
	}

	UFretDanceMusicInstrumentProcessor::InitializeGuitarInstrument(FretDanceActor.Get());
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Initialize Guitar Instrument operation triggered"));
	return FReply::Handled();
}

FReply SFretDanceModuleOperationsPanel::OnTriggerControlRigReregistration() {
	if (!FretDanceActor.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("FretDance: No actor selected for ControlRig re-registration"));
		return FReply::Handled();
	}

	FretDanceActor->TriggerControlRigReregistration(TEXT("Manual trigger from UI panel"));
	UE_LOG(LogTemp, Warning, TEXT("FretDance: Trigger Control Rig Re-registration operation triggered"));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
