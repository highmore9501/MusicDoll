#include "UI/ZhengDriftBakeOperationsPanel.h"

#include "Baking/BakeTaskManager.h"
#include "ControlRigBlueprintLegacy.h"
#include "InstrumentAnimationUtility.h"
#include "Widgets/Input/STextComboBox.h"
#include "ZhengDriftUnreal.h"

void SZhengDriftBakeOperationsPanel::Construct(const FArguments& InArgs) {
    InitializeControlOptions();
    SBakeOperationsPanelBase::Construct(
        SBakeOperationsPanelBase::FArguments());
}

void SZhengDriftBakeOperationsPanel::SetActor(AActor* InActor) {
    ZhengDriftActor = Cast<AZhengDriftUnreal>(InActor);
    SBakeOperationsPanelBase::SetActor(InActor);
}

bool SZhengDriftBakeOperationsPanel::CanHandleActor(
    const AActor* InActor) const {
    return Cast<const AZhengDriftUnreal>(InActor) != nullptr;
}

void SZhengDriftBakeOperationsPanel::RefreshBakeOperations() {
    RefreshScanResults();
}

TSharedRef<SWidget>
SZhengDriftBakeOperationsPanel::CreateControlSelectionWidget() {
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
                             &SZhengDriftBakeOperationsPanel::
                                 HandlePerformerControlSelectionChanged)]] +
           SVerticalBox::Slot().AutoHeight().Padding(5.0f)
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().AutoWidth().Padding(
                    5.0f)[SNew(STextBlock)
                              .Text(FText::FromString(
                                  TEXT("Zheng Controls:")))] +
                SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
                    [SAssignNew(ZhengControlCombo, STextComboBox)
                         .OptionsSource(&ZhengControlOptions)
                         .OnSelectionChanged(
                             this, &SZhengDriftBakeOperationsPanel::
                                       HandleZhengControlSelectionChanged)]];
}

TArray<FString>
SZhengDriftBakeOperationsPanel::GetSelectedControlNames() const {
    TArray<FString> Names;
    if (SelectedPerformerControl.IsValid() &&
        !SelectedPerformerControl->IsEmpty())
        Names.Add(*SelectedPerformerControl);
    if (SelectedZhengControl.IsValid() && !SelectedZhengControl->IsEmpty())
        Names.Add(*SelectedZhengControl);
    return Names;
}

void SZhengDriftBakeOperationsPanel::InitializeControlOptions() {
    PerformerControlOptions.Empty();
    ZhengControlOptions.Empty();

    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    ZhengControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    SelectedPerformerControl = PerformerControlOptions[0];
    SelectedZhengControl     = ZhengControlOptions[0];
}

void SZhengDriftBakeOperationsPanel::UpdateControlOptionsFromScan() {
    if (!ZhengDriftActor.IsValid()) return;

    PerformerControlOptions.Empty();
    ZhengControlOptions.Empty();

    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    ZhengControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    for (const auto& Pair : ScanResults) {
        const FControlRigScanResult& Result = Pair.Value;
        if (!Result.IsValid()) continue;

        if (Result.BoundActor == ZhengDriftActor.Get()->SkeletalMeshActor) {
            for (const FString& Ctrl : Result.AvailableControls)
                PerformerControlOptions.Add(MakeShareable(new FString(Ctrl)));
        } else if (Result.BoundActor == ZhengDriftActor.Get()->Zheng) {
            for (const FString& Ctrl : Result.AvailableControls)
                ZhengControlOptions.Add(MakeShareable(new FString(Ctrl)));
        }
    }

    if (PerformerControlCombo.IsValid())
        PerformerControlCombo->RefreshOptions();
    if (ZhengControlCombo.IsValid())
        ZhengControlCombo->RefreshOptions();
}

void SZhengDriftBakeOperationsPanel::HandlePerformerControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type) {
    if (NewSelection.IsValid())
        SelectedPerformerControl = NewSelection;
}

void SZhengDriftBakeOperationsPanel::HandleZhengControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type) {
    if (NewSelection.IsValid())
        SelectedZhengControl = NewSelection;
}

void SZhengDriftBakeOperationsPanel::RefreshScanResults() {
    if (!ZhengDriftActor.IsValid()) return;

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return;

    PerformerControlOptions.Empty();
    ZhengControlOptions.Empty();
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    ZhengControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // 扫描演奏者 ControlRig
    if (ZhengDriftActor.Get()->SkeletalMeshActor) {
        UControlRig*          CtrlRig  = nullptr;
        UControlRigBlueprint* Blueprint = nullptr;
        if (UAnimationBaker::GetControlRigsForActor(
                ZhengDriftActor.Get()->SkeletalMeshActor, LevelSequence,
                CtrlRig, Blueprint) && CtrlRig && Blueprint) {
            URigHierarchy* H = Blueprint->GetHierarchy();
            if (H) {
                for (FRigControlElement* E : H->GetControls(true)) {
                    if (E->Settings.ControlType ==
                            ERigControlType::Transform ||
                        E->Settings.ControlType ==
                            ERigControlType::EulerTransform) {
                        PerformerControlOptions.Add(MakeShareable(
                            new FString(E->GetDisplayName().ToString())));
                    }
                }
            }
        }
    }

    // 扫描古筝 ControlRig
    if (ZhengDriftActor.Get()->Zheng) {
        UControlRig*          CtrlRig  = nullptr;
        UControlRigBlueprint* Blueprint = nullptr;
        if (UAnimationBaker::GetControlRigsForActor(
                ZhengDriftActor.Get()->Zheng, LevelSequence, CtrlRig,
                Blueprint) && CtrlRig && Blueprint) {
            URigHierarchy* H = Blueprint->GetHierarchy();
            if (H) {
                for (FRigControlElement* E : H->GetControls(true)) {
                    if (E->Settings.ControlType ==
                            ERigControlType::Transform ||
                        E->Settings.ControlType ==
                            ERigControlType::EulerTransform) {
                        ZhengControlOptions.Add(MakeShareable(
                            new FString(E->GetDisplayName().ToString())));
                    }
                }
            }
        }
    }

    if (PerformerControlCombo.IsValid())
        PerformerControlCombo->RefreshOptions();
    if (ZhengControlCombo.IsValid())
        ZhengControlCombo->RefreshOptions();

    bHasValidScanResults = true;
}

void SZhengDriftBakeOperationsPanel::AddSelectedControl() {
    if (!ZhengDriftActor.IsValid()) return;

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return;

    bool bAddedAny = false;

    if (SelectedPerformerControl.IsValid() &&
        !SelectedPerformerControl->IsEmpty() &&
        ZhengDriftActor.Get()->SkeletalMeshActor) {
        UControlRig*          CtrlRig  = nullptr;
        UControlRigBlueprint* Blueprint = nullptr;
        if (UAnimationBaker::GetControlRigsForActor(
                ZhengDriftActor.Get()->SkeletalMeshActor, LevelSequence,
                CtrlRig, Blueprint) && CtrlRig) {
            FString Name  = *SelectedPerformerControl;
            FString Disp  = FString::Printf(TEXT("Performer.%s"), *Name);
            if (AddControlToSelection(CtrlRig, Name, Disp))
                bAddedAny = true;
        }
    }

    if (SelectedZhengControl.IsValid() &&
        !SelectedZhengControl->IsEmpty() &&
        ZhengDriftActor.Get()->Zheng) {
        UControlRig*          CtrlRig  = nullptr;
        UControlRigBlueprint* Blueprint = nullptr;
        if (UAnimationBaker::GetControlRigsForActor(
                ZhengDriftActor.Get()->Zheng, LevelSequence, CtrlRig,
                Blueprint) && CtrlRig) {
            FString Name  = *SelectedZhengControl;
            FString Disp  = FString::Printf(TEXT("Zheng.%s"), *Name);
            if (AddControlToSelection(CtrlRig, Name, Disp))
                bAddedAny = true;
        }
    }

    FinalizeAddSelectedControl(bAddedAny);
}

FName SZhengDriftBakeOperationsPanel::GetModuleName() const {
    return TEXT("ZhengDrift");
}
