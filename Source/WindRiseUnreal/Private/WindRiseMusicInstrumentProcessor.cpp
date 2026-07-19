#include "WindRiseMusicInstrumentProcessor.h"

#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCreationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "Rigs/RigHierarchyDefines.h"
#include "WindRiseUnreal.h"

// ============================================================
// ControlRig 初始化
// ============================================================

void UWindRiseMusicInstrumentProcessor::InitializeInstrumentControlRig(
    AWindRiseUnreal* WindRiseActor) {
    if (!WindRiseActor) {
        UE_LOG(LogTemp, Error,
               TEXT("UWindRiseInstProcessor: WindRiseActor is null"));
        return;
    }

    if (!WindRiseActor->InstrumentMesh) {
        UE_LOG(LogTemp, Error,
               TEXT("UWindRiseInstProcessor: No Instrument SkeletalMeshActor "
                    "set"));
        return;
    }

    UControlRigBlueprint* CRBlueprint =
        WindRiseActor->GetCachedControlRigBlueprint(TEXT("Instrument"));
    if (!CRBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("UWindRiseInstProcessor: Cannot get ControlRigBlueprint "
                    "for Instrument"));
        WindRiseActor->TriggerControlRigReregistration(
            TEXT("Instrument ControlRig not found. Please ensure it is set up "
                 "in the Instrument SkeletalMeshActor."));
        return;
    }

    // 创建 wind_root Control
    static const FString WindRootControlName = TEXT("wind_root");
    if (!FControlRigCreationUtility::CreateControl(
            CRBlueprint, WindRootControlName, TEXT(""))) {
        UE_LOG(LogTemp, Warning,
               TEXT("UWindRiseInstProcessor: Failed to create wind_root "
                    "control (may already exist)"));
    }

    // 在 wind_root 下创建 MT Float Channels
    FRigElementKey ParentKey(*WindRootControlName, ERigElementType::Control);
    int32 ChannelsAdded = UInstrumentMorphTargetUtility::AddAnimationChannels(
        CRBlueprint, ParentKey, WindRiseActor->InstrumentMorphTargets);

    UE_LOG(LogTemp, Log,
           TEXT("UWindRiseInstProcessor: Instrument ControlRig initialized, "
                "%d channels added"),
           ChannelsAdded);
}

// ============================================================
// 状态恢复
// ============================================================

void UWindRiseMusicInstrumentProcessor::RestoreInstrumentMorphTargets(
    AWindRiseUnreal* WindRiseActor, const FWindRiseNoteState& State) {
    if (!WindRiseActor || !WindRiseActor->InstrumentMesh ||
        !WindRiseActor->InstrumentMesh->GetSkeletalMeshComponent())
        return;

    USkeletalMeshComponent* SkelComp =
        WindRiseActor->InstrumentMesh->GetSkeletalMeshComponent();

    // 先全部归零
    for (const FString& Name : WindRiseActor->InstrumentMorphTargets) {
        SkelComp->SetMorphTarget(FName(*Name), 0.0f);
    }
    // 再恢复非零值
    for (const FWindRiseMorphTargetValue& MT : State.InstrumentMT) {
        SkelComp->SetMorphTarget(FName(*MT.MorphTargetName), MT.Value);
    }
}

// ============================================================
// 实时 Morph Target 辅助
// ============================================================

void UWindRiseMusicInstrumentProcessor::SetInstrumentMTValue(
    AWindRiseUnreal* WindRiseActor, int32 Index, float Value) {
    if (!WindRiseActor || !WindRiseActor->InstrumentMesh ||
        !WindRiseActor->InstrumentMesh->GetSkeletalMeshComponent())
        return;
    if (!WindRiseActor->InstrumentMorphTargets.IsValidIndex(Index)) return;

    WindRiseActor->InstrumentMesh->GetSkeletalMeshComponent()->SetMorphTarget(
        FName(*WindRiseActor->InstrumentMorphTargets[Index]), Value);
}

void UWindRiseMusicInstrumentProcessor::ResetAllInstrumentMT(
    AWindRiseUnreal* WindRiseActor) {
    if (!WindRiseActor || !WindRiseActor->InstrumentMesh ||
        !WindRiseActor->InstrumentMesh->GetSkeletalMeshComponent())
        return;

    USkeletalMeshComponent* SkelComp =
        WindRiseActor->InstrumentMesh->GetSkeletalMeshComponent();
    for (const FString& Name : WindRiseActor->InstrumentMorphTargets) {
        SkelComp->SetMorphTarget(FName(*Name), 0.0f);
    }
}
