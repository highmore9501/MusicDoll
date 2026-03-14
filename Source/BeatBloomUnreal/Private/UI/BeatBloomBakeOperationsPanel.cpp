#include "UI/BeatBloomBakeOperationsPanel.h"

#include "Baking/BakeTaskManager.h"
#include "BeatBloomUnreal.h"
#include "ControlRigBlueprintLegacy.h"
#include "InstrumentAnimationUtility.h"
#include "Widgets/Input/STextComboBox.h"

#define LOCTEXT_NAMESPACE "BeatBloomBakeOperationsPanel"

void SBeatBloomBakeOperationsPanel::Construct(const FArguments& InArgs) {
    // Initialize control options
    InitializeControlOptions();

    // Call base class constructor (no parameters)
    SBakeOperationsPanelBase::Construct(SBakeOperationsPanelBase::FArguments());
}

void SBeatBloomBakeOperationsPanel::SetActor(AActor* InActor) {
    BeatBloomActor = Cast<ABeatBloomUnreal>(InActor);

    // Call base class method to set Actor
    SBakeOperationsPanelBase::SetActor(InActor);

    if (BeatBloomActor.IsValid()) {
        InitializeControlOptions();
    }
}

bool SBeatBloomBakeOperationsPanel::CanHandleActor(
    const AActor* InActor) const {
    return Cast<const ABeatBloomUnreal>(InActor) != nullptr;
}

void SBeatBloomBakeOperationsPanel::RefreshBakeOperations() {
    RefreshScanResults();
}

void SBeatBloomBakeOperationsPanel::InitializeControlOptions() {
    // Initialize option lists
    PerformerControlOptions.Empty();
    DrumKitControlOptions.Empty();

    // Add empty option
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    DrumKitControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // Add some common BeatBloom controls as examples
    TArray<FString> CommonPerformerControls = {
        TEXT("LeftHand_Control"), TEXT("RightHand_Control"),
        TEXT("Head_Control"), TEXT("Spine_Control")};

    TArray<FString> CommonDrumKitControls = {
        TEXT("drumkit_control"), TEXT("Kick_Drum"), TEXT("Snare_Drum"),
        TEXT("HiHat_Control")};

    for (const FString& Control : CommonPerformerControls) {
        PerformerControlOptions.Add(MakeShareable(new FString(Control)));
    }

    for (const FString& Control : CommonDrumKitControls) {
        DrumKitControlOptions.Add(MakeShareable(new FString(Control)));
    }

    // Set default selection
    SelectedPerformerControl = PerformerControlOptions[0];
    SelectedDrumKitControl = DrumKitControlOptions[0];
}

void SBeatBloomBakeOperationsPanel::UpdateControlOptionsFromScan() {
    if (!BeatBloomActor.IsValid()) {
        return;
    }

    // Clear existing options
    PerformerControlOptions.Empty();
    DrumKitControlOptions.Empty();

    // Add empty option
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    DrumKitControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // Extract Control list from scan results
    for (const auto& Pair : ScanResults) {
        const FControlRigScanResult& Result = Pair.Value;

        if (!Result.IsValid()) {
            continue;
        }

        // Classify Controls based on Actor type
        if (Result.BoundActor == BeatBloomActor.Get()->SkeletalMeshActor) {
            // Performer Control
            for (const FString& ControlName : Result.AvailableControls) {
                PerformerControlOptions.Add(
                    MakeShareable(new FString(ControlName)));
            }
        } else if (Result.BoundActor == BeatBloomActor.Get()->DrumKit) {
            // DrumKit Control
            for (const FString& ControlName : Result.AvailableControls) {
                DrumKitControlOptions.Add(
                    MakeShareable(new FString(ControlName)));
            }
        }
    }

    // Refresh ComboBox - add debug logs
    UE_LOG(LogTemp, Log,
           TEXT("UpdateControlOptionsFromScan - Checking ComboBox validity: "
                "Performer=%s, DrumKit=%s"),
           PerformerControlCombo.IsValid() ? TEXT("Valid") : TEXT("Invalid"),
           DrumKitControlCombo.IsValid() ? TEXT("Valid") : TEXT("Invalid"));

    UE_LOG(LogTemp, Log,
           TEXT("Control Options Count - Performer: %d, DrumKit: %d"),
           PerformerControlOptions.Num(), DrumKitControlOptions.Num());

    if (PerformerControlOptions.Num() > 1) {
        UE_LOG(
            LogTemp, Log, TEXT("First few Performer options: %s, %s, %s"),
            *(*PerformerControlOptions[0]),
            PerformerControlOptions.Num() > 1 ? *(*PerformerControlOptions[1])
                                              : TEXT("N/A"),
            PerformerControlOptions.Num() > 2 ? *(*PerformerControlOptions[2])
                                              : TEXT("N/A"));
    }

    if (PerformerControlCombo.IsValid()) {
        PerformerControlCombo->RefreshOptions();
        UE_LOG(LogTemp, Log,
               TEXT("Successfully refreshed PerformerControlCombo"));
    } else {
        UE_LOG(
            LogTemp, Warning,
            TEXT("PerformerControlCombo is invalid - options may not display"));
    }

    if (DrumKitControlCombo.IsValid()) {
        DrumKitControlCombo->RefreshOptions();
        UE_LOG(LogTemp, Log,
               TEXT("Successfully refreshed DrumKitControlCombo"));
    } else {
        UE_LOG(
            LogTemp, Warning,
            TEXT("DrumKitControlCombo is invalid - options may not display"));
    }
}

void SBeatBloomBakeOperationsPanel::HandlePerformerControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
    SelectedPerformerControl = NewSelection;
}

void SBeatBloomBakeOperationsPanel::HandleDrumKitControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
    SelectedDrumKitControl = NewSelection;
}

TSharedRef<SWidget>
SBeatBloomBakeOperationsPanel::CreateControlSelectionWidget() {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(5.0f)
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().AutoWidth().Padding(
                    5.0f)[SNew(STextBlock)
                              .Text(FText::FromString(
                                  TEXT("Performer Controls:")))] +
                SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
                    [SAssignNew(PerformerControlCombo, STextComboBox)
                         .OptionsSource(&PerformerControlOptions)
                         .OnSelectionChanged(
                             this,
                             &SBeatBloomBakeOperationsPanel::
                                 HandlePerformerControlSelectionChanged)]] +
           SVerticalBox::Slot().AutoHeight().Padding(5.0f)
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                    [SNew(STextBlock)
                         .Text(FText::FromString(TEXT("DrumKit Controls:")))] +
                SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                    5.0f)[SAssignNew(DrumKitControlCombo, STextComboBox)
                              .OptionsSource(&DrumKitControlOptions)
                              .OnSelectionChanged(
                                  this,
                                  &SBeatBloomBakeOperationsPanel::
                                      HandleDrumKitControlSelectionChanged)]];
}

TArray<FString> SBeatBloomBakeOperationsPanel::GetSelectedControlNames() const {
    TArray<FString> Names;
    if (SelectedPerformerControl.IsValid()) {
        Names.Add(*SelectedPerformerControl);
    }
    if (SelectedDrumKitControl.IsValid()) {
        Names.Add(*SelectedDrumKitControl);
    }
    return Names;
}

void SBeatBloomBakeOperationsPanel::RefreshScanResults() {
    if (!BeatBloomActor.IsValid()) {
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
    DrumKitControlOptions.Empty();

    // Add empty option
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    DrumKitControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // Scan Performer Control Rig
    if (BeatBloomActor.Get()->SkeletalMeshActor) {
        UControlRig* PerformerControlRig = nullptr;
        UControlRigBlueprint* PerformerBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                BeatBloomActor.Get()->SkeletalMeshActor, LevelSequence,
                PerformerControlRig, PerformerBlueprint) &&
            PerformerControlRig && PerformerBlueprint) {
            URigHierarchy* Hierarchy = PerformerBlueprint->GetHierarchy();
            if (Hierarchy) {
                TArray<FRigControlElement*> ControlElements =
                    Hierarchy->GetControls(true);
                for (const FRigControlElement* Element : ControlElements) {
                    if (Element->Settings.ControlType ==
                            ERigControlType::Transform ||
                        Element->Settings.ControlType ==
                            ERigControlType::EulerTransform) {
                        PerformerControlOptions.Add(MakeShareable(
                            new FString(Element->GetDisplayName().ToString())));
                    }
                }
            }

            UE_LOG(LogTemp, Log, TEXT("Found %d performer controls"),
                   PerformerControlOptions.Num() - 1);
        }
    }

    // Scan DrumKit Control Rig
    if (BeatBloomActor.Get()->DrumKit) {
        UControlRig* DrumKitControlRig = nullptr;
        UControlRigBlueprint* DrumKitBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                BeatBloomActor.Get()->DrumKit, LevelSequence, DrumKitControlRig,
                DrumKitBlueprint) &&
            DrumKitControlRig && DrumKitBlueprint) {
            URigHierarchy* Hierarchy = DrumKitBlueprint->GetHierarchy();
            if (Hierarchy) {
                TArray<FRigControlElement*> ControlElements =
                    Hierarchy->GetControls(true);
                for (const FRigControlElement* Element : ControlElements) {
                    if (Element->Settings.ControlType ==
                            ERigControlType::Transform ||
                        Element->Settings.ControlType ==
                            ERigControlType::EulerTransform) {
                        DrumKitControlOptions.Add(MakeShareable(
                            new FString(Element->GetDisplayName().ToString())));
                    }
                }
            }

            UE_LOG(LogTemp, Log, TEXT("Found %d drumkit controls"),
                   DrumKitControlOptions.Num() - 1);
        }
    }

    // Refresh ComboBox
    if (PerformerControlCombo.IsValid()) {
        PerformerControlCombo->RefreshOptions();
    }

    if (DrumKitControlCombo.IsValid()) {
        DrumKitControlCombo->RefreshOptions();
    }

    bHasValidScanResults = true;
}

void SBeatBloomBakeOperationsPanel::AddSelectedControl() {
    if (!BeatBloomActor.IsValid()) {
        return;
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        return;
    }

    bool bAddedAny = false;

    // Add Performer Control
    if (SelectedPerformerControl.IsValid() &&
        !SelectedPerformerControl->IsEmpty() &&
        BeatBloomActor.Get()->SkeletalMeshActor) {
        UControlRig* PerformerControlRig = nullptr;
        UControlRigBlueprint* PerformerBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                BeatBloomActor.Get()->SkeletalMeshActor, LevelSequence,
                PerformerControlRig, PerformerBlueprint) &&
            PerformerControlRig) {
            FString ControlName = *SelectedPerformerControl;
            FString DisplayName =
                FString::Printf(TEXT("Performer.%s"), *ControlName);

            if (AddControlToSelection(PerformerControlRig, ControlName,
                                      DisplayName)) {
                bAddedAny = true;
            }
        }
    }

    // Add DrumKit Control
    if (SelectedDrumKitControl.IsValid() &&
        !SelectedDrumKitControl->IsEmpty() && BeatBloomActor.Get()->DrumKit) {
        UControlRig* DrumKitControlRig = nullptr;
        UControlRigBlueprint* DrumKitBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                BeatBloomActor.Get()->DrumKit, LevelSequence, DrumKitControlRig,
                DrumKitBlueprint) &&
            DrumKitControlRig) {
            FString ControlName = *SelectedDrumKitControl;
            FString DisplayName =
                FString::Printf(TEXT("DrumKit.%s"), *ControlName);

            if (AddControlToSelection(DrumKitControlRig, ControlName,
                                      DisplayName)) {
                bAddedAny = true;
            }
        }
    }

    FinalizeAddSelectedControl(bAddedAny);
}

FName SBeatBloomBakeOperationsPanel::GetModuleName() const {
    return FName(TEXT("BeatBloom"));
}

#undef LOCTEXT_NAMESPACE
