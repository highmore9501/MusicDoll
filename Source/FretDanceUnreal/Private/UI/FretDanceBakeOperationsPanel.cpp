#include "UI/FretDanceBakeOperationsPanel.h"

#include "Baking/BakeTaskManager.h"
#include "InstrumentAnimationUtility.h"
#include "Widgets/Input/STextComboBox.h"
#include "FretDanceUnreal.h"
#include "ControlRigBlueprintLegacy.h"

void SFretDanceBakeOperationsPanel::Construct(const FArguments& InArgs) {
	// Initialize control options
	InitializeControlOptions();

	// Call base class constructor (no parameters)
	SBakeOperationsPanelBase::Construct(SBakeOperationsPanelBase::FArguments());
}

void SFretDanceBakeOperationsPanel::SetActor(AActor* InActor) {
	FretDanceActor = Cast<AFretDanceUnreal>(InActor);

	// Call base class method to set Actor
	SBakeOperationsPanelBase::SetActor(InActor);
}

bool SFretDanceBakeOperationsPanel::CanHandleActor(const AActor* InActor) const {
	return Cast<const AFretDanceUnreal>(InActor) != nullptr;
}

void SFretDanceBakeOperationsPanel::RefreshBakeOperations() {
	RefreshScanResults();
}

TSharedRef<SWidget>
SFretDanceBakeOperationsPanel::CreateControlSelectionWidget() {
	return SNew(SVerticalBox) +
		SVerticalBox::Slot().AutoHeight().Padding(5.0f)
			[ SNew(SHorizontalBox) +
			SHorizontalBox::Slot().AutoWidth().Padding(5.0f)[SNew(STextBlock)
				.Text(FText::FromString(TEXT("Performer Controls:")))] +
			SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
				[ SAssignNew(PerformerControlCombo, STextComboBox)
					.OptionsSource(&PerformerControlOptions)
					.OnSelectionChanged(this, &SFretDanceBakeOperationsPanel::HandlePerformerControlSelectionChanged) ] ] +
		SVerticalBox::Slot().AutoHeight().Padding(5.0f)
			[ SNew(SHorizontalBox) +
			SHorizontalBox::Slot().AutoWidth().Padding(5.0f)[SNew(STextBlock)
				.Text(FText::FromString(TEXT("Guitar Controls:")))] +
			SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
				[ SAssignNew(GuitarControlCombo, STextComboBox)
					.OptionsSource(&GuitarControlOptions)
					.OnSelectionChanged(this, &SFretDanceBakeOperationsPanel::HandleGuitarControlSelectionChanged) ] ];
}

TArray<FString> SFretDanceBakeOperationsPanel::GetSelectedControlNames() const {
	TArray<FString> SelectedControlNames;

	if (SelectedPerformerControl.IsValid() && !SelectedPerformerControl->IsEmpty()) {
		SelectedControlNames.Add(*SelectedPerformerControl);
	}

	if (SelectedGuitarControl.IsValid() && !SelectedGuitarControl->IsEmpty()) {
		SelectedControlNames.Add(*SelectedGuitarControl);
	}

	return SelectedControlNames;
}

void SFretDanceBakeOperationsPanel::InitializeControlOptions() {
	// Initialize option lists
	PerformerControlOptions.Empty();
	GuitarControlOptions.Empty();

	// Add empty option
	PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
	GuitarControlOptions.Add(MakeShareable(new FString(TEXT(""))));

	// Add some common FretDance controls as examples
	TArray<FString> CommonPerformerControls = {
		TEXT("LeftHand_Control"), TEXT("RightHand_Control"),
		TEXT("Head_Control"), TEXT("Spine_Control") };

	TArray<FString> CommonGuitarControls = { TEXT("StringVibration_Control"),
		TEXT("Bridge_Control"),
		TEXT("Fingerboard_Control") };

	for (const FString& Control : CommonPerformerControls) {
		PerformerControlOptions.Add(MakeShareable(new FString(Control)));
	}

	for (const FString& Control : CommonGuitarControls) {
		GuitarControlOptions.Add(MakeShareable(new FString(Control)));
	}

	// Set default selection
	SelectedPerformerControl = PerformerControlOptions[0];
	SelectedGuitarControl = GuitarControlOptions[0];
}

void SFretDanceBakeOperationsPanel::UpdateControlOptionsFromScan() {
	if (!FretDanceActor.IsValid()) {
		return;
	}

	// Clear existing options
	PerformerControlOptions.Empty();
	GuitarControlOptions.Empty();

	// Add empty option
	PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
	GuitarControlOptions.Add(MakeShareable(new FString(TEXT(""))));

	// Extract Control list from scan results
	for (const auto& Pair : ScanResults) {
		const FControlRigScanResult& Result = Pair.Value;

		if (!Result.IsValid()) {
			continue;
		}

		// Classify Controls based on Actor type
		if (Result.BoundActor == FretDanceActor.Get()->SkeletalMeshActor) {
			// Performer Control
			for (const FString& ControlName : Result.AvailableControls) {
				PerformerControlOptions.Add(MakeShareable(new FString(ControlName)));
			}
		} else if (Result.BoundActor == FretDanceActor.Get()->Guitar) {
			// Guitar Control
			for (const FString& ControlName : Result.AvailableControls) {
				GuitarControlOptions.Add(MakeShareable(new FString(ControlName)));
			}
		}
	}

	// Refresh ComboBox - add debug logs
	UE_LOG(LogTemp, Log,
		TEXT("UpdateControlOptionsFromScan - Checking ComboBox validity: Performer=%s, Guitar=%s"),
		PerformerControlCombo.IsValid() ? TEXT("Valid") : TEXT("Invalid"),
		GuitarControlCombo.IsValid() ? TEXT("Valid") : TEXT("Invalid"));

	UE_LOG(LogTemp, Log, TEXT("Control Options Count - Performer: %d, Guitar: %d"),
		PerformerControlOptions.Num(), GuitarControlOptions.Num());

	if (PerformerControlOptions.Num() > 1) {
		UE_LOG(LogTemp, Log, TEXT("First few Performer options: %s, %s, %s"),
			*(*PerformerControlOptions[0]),
			PerformerControlOptions.Num() > 1 ? *(*PerformerControlOptions[1]) : TEXT("N/A"),
			PerformerControlOptions.Num() > 2 ? *(*PerformerControlOptions[2]) : TEXT("N/A"));
	}

	if (PerformerControlCombo.IsValid()) {
		PerformerControlCombo->RefreshOptions();
		UE_LOG(LogTemp, Log, TEXT("Successfully refreshed PerformerControlCombo"));
	} else {
		UE_LOG(LogTemp, Warning, TEXT("PerformerControlCombo is invalid - options may not display"));
	}

	if (GuitarControlCombo.IsValid()) {
		GuitarControlCombo->RefreshOptions();
		UE_LOG(LogTemp, Log, TEXT("Successfully refreshed GuitarControlCombo"));
	} else {
		UE_LOG(LogTemp, Warning, TEXT("GuitarControlCombo is invalid - options may not display"));
	}
}

void SFretDanceBakeOperationsPanel::HandlePerformerControlSelectionChanged(
	TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
	if (NewSelection.IsValid()) {
		SelectedPerformerControl = NewSelection;
	}
}

void SFretDanceBakeOperationsPanel::HandleGuitarControlSelectionChanged(
	TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
	if (NewSelection.IsValid()) {
		SelectedGuitarControl = NewSelection;
	}
}

void SFretDanceBakeOperationsPanel::RefreshScanResults() {
	if (!FretDanceActor.IsValid()) {
		return;
	}

	// Get current LevelSequence
	ULevelSequence* LevelSequence =
		UInstrumentAnimationUtility::GetCurrentLevelSequence();
	if (!LevelSequence) {
		return;
	}

	UMovieScene* MovieScene = LevelSequence->GetMovieScene();
	if (!MovieScene) {
		return;
	}

	// Clear existing options
	PerformerControlOptions.Empty();
	GuitarControlOptions.Empty();

	// Add empty option
	PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
	GuitarControlOptions.Add(MakeShareable(new FString(TEXT(""))));

	// Scan Performer Control Rig
	if (FretDanceActor.Get()->SkeletalMeshActor) {
		UControlRig* PerformerControlRig = nullptr;
		UControlRigBlueprint* PerformerBlueprint = nullptr;

		if (UAnimationBaker::GetControlRigsForActor(
			FretDanceActor.Get()->SkeletalMeshActor, LevelSequence,
			PerformerControlRig, PerformerBlueprint) &&
			PerformerControlRig && PerformerBlueprint) {
			URigHierarchy* Hierarchy = PerformerBlueprint->GetHierarchy();
			if (Hierarchy) {
				TArray<FRigControlElement*> ControlElements = Hierarchy->GetControls(true);
				for (const FRigControlElement* Element : ControlElements) {
					if (Element->Settings.ControlType == ERigControlType::Transform ||
						Element->Settings.ControlType == ERigControlType::EulerTransform) {
						PerformerControlOptions.Add(MakeShareable(new FString(Element->GetDisplayName().ToString())));
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("Found %d performer controls"), PerformerControlOptions.Num() - 1);
		}
	}

	// Scan Guitar Control Rig
	if (FretDanceActor.Get()->Guitar) {
		UControlRig* GuitarControlRig = nullptr;
		UControlRigBlueprint* GuitarBlueprint = nullptr;

		if (UAnimationBaker::GetControlRigsForActor(
			FretDanceActor.Get()->Guitar, LevelSequence, GuitarControlRig,
			GuitarBlueprint) && GuitarControlRig && GuitarBlueprint) {
			URigHierarchy* Hierarchy = GuitarBlueprint->GetHierarchy();
			if (Hierarchy) {
				TArray<FRigControlElement*> ControlElements = Hierarchy->GetControls(true);
				for (const FRigControlElement* Element : ControlElements) {
					if (Element->Settings.ControlType == ERigControlType::Transform ||
						Element->Settings.ControlType == ERigControlType::EulerTransform) {
						GuitarControlOptions.Add(MakeShareable(new FString(Element->GetDisplayName().ToString())));
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("Found %d guitar controls"), GuitarControlOptions.Num() - 1);
		}
	}

	// Refresh ComboBox
	if (PerformerControlCombo.IsValid()) {
		PerformerControlCombo->RefreshOptions();
	}

	if (GuitarControlCombo.IsValid()) {
		GuitarControlCombo->RefreshOptions();
	}

	bHasValidScanResults = true;
}

void SFretDanceBakeOperationsPanel::AddSelectedControl() {
	if (!FretDanceActor.IsValid()) {
		return;
	}

	ULevelSequence* LevelSequence =
		UInstrumentAnimationUtility::GetCurrentLevelSequence();
	if (!LevelSequence) {
		return;
	}

	bool bAddedAny = false;

	// Add Performer Control
	if (SelectedPerformerControl.IsValid() && !SelectedPerformerControl->IsEmpty() && FretDanceActor.Get()->SkeletalMeshActor) {
		UControlRig* PerformerControlRig = nullptr;
		UControlRigBlueprint* PerformerBlueprint = nullptr;

		if (UAnimationBaker::GetControlRigsForActor(
			FretDanceActor.Get()->SkeletalMeshActor, LevelSequence,
			PerformerControlRig, PerformerBlueprint) && PerformerControlRig) {
			FString ControlName = *SelectedPerformerControl;
			FString DisplayName = FString::Printf(TEXT("Performer.%s"), *ControlName);

			if (AddControlToSelection(PerformerControlRig, ControlName, DisplayName)) {
				bAddedAny = true;
			}
		}
	}

	// Add Guitar Control
	if (SelectedGuitarControl.IsValid() && !SelectedGuitarControl->IsEmpty() && FretDanceActor.Get()->Guitar) {
		UControlRig* GuitarControlRig = nullptr;
		UControlRigBlueprint* GuitarBlueprint = nullptr;

		if (UAnimationBaker::GetControlRigsForActor(
			FretDanceActor.Get()->Guitar, LevelSequence, GuitarControlRig,
			GuitarBlueprint) && GuitarControlRig) {
			FString ControlName = *SelectedGuitarControl;
			FString DisplayName = FString::Printf(TEXT("Guitar.%s"), *ControlName);

			if (AddControlToSelection(GuitarControlRig, ControlName, DisplayName)) {
				bAddedAny = true;
			}
		}
	}

	FinalizeAddSelectedControl(bAddedAny);
}

FName SFretDanceBakeOperationsPanel::GetModuleName() const {
	return TEXT("FretDance");
}
