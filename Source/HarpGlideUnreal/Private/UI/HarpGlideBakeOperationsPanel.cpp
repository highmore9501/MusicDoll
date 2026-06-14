#include "UI/HarpGlideBakeOperationsPanel.h"

#include "Baking/BakeTaskManager.h"
#include "ControlRigBlueprintLegacy.h"
#include "InstrumentAnimationUtility.h"

#define LOCTEXT_NAMESPACE "SHarpGlideBakeOperationsPanel"

void SHarpGlideBakeOperationsPanel::Construct(const FArguments& InArgs) {
    SBakeOperationsPanelBase::FArguments BaseArgs;
    SBakeOperationsPanelBase::Construct(BaseArgs);

    // 初始化 CR 选择下拉选项
    InitializeControlOptions();
}

void SHarpGlideBakeOperationsPanel::SetActor(AActor* InActor) {
    HarpGlideActor = Cast<AHarpGlideUnreal>(InActor);
    RefreshBakeOperations();
}

void SHarpGlideBakeOperationsPanel::RefreshBakeOperations() {
    UpdateControlOptionsFromScan();
}

void SHarpGlideBakeOperationsPanel::InitializeControlOptions() {
    PerformerControlOptions.Empty();
    HarpControlOptions.Empty();
}

void SHarpGlideBakeOperationsPanel::RefreshScanResults() {
    if (!HarpGlideActor.IsValid()) return;

    ScanResults.Empty();

    // 扫描演奏者 CR
    if (HarpGlideActor->SkeletalMeshActor) {
        UControlRig* PerformerCR =
            HarpGlideActor->GetCachedControlRig(TEXT("Performer"));
        UControlRigBlueprint* PerformerBP =
            HarpGlideActor->GetCachedControlRigBlueprint(TEXT("Performer"));
        if (PerformerCR && PerformerBP) {
            FControlRigScanResult Result;
            Result.BoundActor = HarpGlideActor->SkeletalMeshActor;
            Result.ControlRigInstance = PerformerCR;
            Result.ControlRigBlueprint = PerformerBP;
            Result.TrackName = TEXT("Performer");
            ScanResults.Add(TEXT("Performer"), Result);
        }
    }

    // 扫描竖琴 CR
    if (HarpGlideActor->Harp) {
        UControlRig* HarpCR = HarpGlideActor->GetCachedControlRig(TEXT("Harp"));
        UControlRigBlueprint* HarpBP =
            HarpGlideActor->GetCachedControlRigBlueprint(TEXT("Harp"));
        if (HarpCR && HarpBP) {
            FControlRigScanResult Result;
            Result.BoundActor = HarpGlideActor->Harp;
            Result.ControlRigInstance = HarpCR;
            Result.ControlRigBlueprint = HarpBP;
            Result.TrackName = TEXT("Harp");
            ScanResults.Add(TEXT("Harp"), Result);
        }
    }

    bHasValidScanResults = ScanResults.Num() > 0;
}

TSharedRef<SWidget>
SHarpGlideBakeOperationsPanel::CreateControlSelectionWidget() {
    return SNew(STextBlock)
        .Text(LOCTEXT("ControlSelection",
                      "HarpGlide: Select controls in Sequencer"))
        .ColorAndOpacity(FLinearColor::White);
}

TArray<FString> SHarpGlideBakeOperationsPanel::GetSelectedControlNames() const {
    TArray<FString> Names;
    if (SelectedPerformerControl.IsValid())
        Names.Add(*SelectedPerformerControl);
    if (SelectedHarpControl.IsValid()) Names.Add(*SelectedHarpControl);
    return Names;
}

void SHarpGlideBakeOperationsPanel::UpdateControlOptionsFromScan() {
    if (!HarpGlideActor.IsValid()) return;

    RefreshScanResults();

    // 更新下拉选项
    PerformerControlOptions.Empty();
    if (HarpGlideActor->SkeletalMeshActor) {
        PerformerControlOptions.Add(MakeShareable(
            new FString(HarpGlideActor->SkeletalMeshActor->GetName())));
    }

    HarpControlOptions.Empty();
    if (HarpGlideActor->Harp) {
        HarpControlOptions.Add(
            MakeShareable(new FString(HarpGlideActor->Harp->GetName())));
    }
}

void SHarpGlideBakeOperationsPanel::HandlePerformerControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
    SelectedPerformerControl = NewSelection;
}

void SHarpGlideBakeOperationsPanel::HandleHarpControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
    SelectedHarpControl = NewSelection;
}

#undef LOCTEXT_NAMESPACE
