#include "FretDanceTransformSyncProcessor.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig/Public/ControlRig.h"
#include "Engine/Engine.h"
#include "FretDanceControlRigProcessor.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"

bool UFretDanceTransformSyncProcessor::SyncAllInstrumentTransforms(
    AFretDanceUnreal* FretDanceActor) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncAllInstrumentTransforms: FretDanceActor is null"));
        return false;
    }

    if (!FretDanceActor->bEnableRealtimeSync) {
        return true;
    }

    // 检测是否为渲染环境
    bool bIsRendering = UInstrumentAnimationUtility::IsInRenderingScenario();

    bool bGuitarSuccess = SyncGuitarTransform(FretDanceActor, bIsRendering);

    if (!bGuitarSuccess) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncAllInstrumentTransforms: Failed to sync "
                    "guitar transform"));
        FretDanceActor->bEnableRealtimeSync =
            false;  // 禁用后续帧的同步以节省性能
    }

    return bGuitarSuccess;
}

bool UFretDanceTransformSyncProcessor::InitializeGuitarSync(
    AFretDanceUnreal* FretDanceActor) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeGuitarSync: FretDanceActor is null"));
        return false;
    }

    if (!FretDanceActor->Guitar) {
        UE_LOG(LogTemp, Error, TEXT("InitializeGuitarSync: Guitar is null"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeGuitarSync Started =========="));

    // Step 1: 检查 Performer 的 controller_root 是否存在
    UControlRigBlueprint* PerformerBlueprint =
        FretDanceActor->GetCachedControlRigBlueprint(TEXT("Performer"));
    if (!PerformerBlueprint || !PerformerBlueprint->Hierarchy ||
        !PerformerBlueprint->Hierarchy->Contains(FRigElementKey(
            TEXT("controller_root"), ERigElementType::Control))) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeGuitarSync: 'controller_root' does not "
                    "exist in Performer's ControlRig Blueprint. "
                    "Please create it manually."));
        return false;
    }

    // Step 2: 检查 Guitar 的 guitar_root 是否存在，不存在则尝试创建
    UControlRigBlueprint* GuitarBlueprint =
        FretDanceActor->GetCachedControlRigBlueprint(TEXT("Guitar"));
    if (GuitarBlueprint) {
        if (!GuitarBlueprint->Hierarchy ||
            !GuitarBlueprint->Hierarchy->Contains(FRigElementKey(
                TEXT("guitar_root"), ERigElementType::Control))) {
            UE_LOG(LogTemp, Warning,
                   TEXT("InitializeGuitarSync: 'guitar_root' not found "
                        "in Guitar's ControlRig Blueprint. Attempting "
                        "to create it."));
            if (!FControlRigCreationUtility::CreateControl(
                    GuitarBlueprint, TEXT("guitar_root"), TEXT(""))) {
                UE_LOG(LogTemp, Error,
                       TEXT("InitializeGuitarSync: Failed to create "
                            "'guitar_root' in Guitar's ControlRig "
                            "Blueprint."));
                return false;
            }
            UE_LOG(LogTemp, Warning,
                   TEXT("InitializeGuitarSync: 'guitar_root' created "
                        "successfully."));
        }
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeGuitarSync: Could not retrieve "
                    "ControlRig Blueprint for Guitar."));
        return false;
    }

    // Step 3: 使用 FInstrumentControlRigUtility::InitializeControlRelationship
    // 计算并缓存相对变换矩阵
    if (!FInstrumentControlRigUtility::InitializeControlRelationship(
            FretDanceActor->SkeletalMeshActor, TEXT("controller_root"),
            FretDanceActor->Guitar, TEXT("guitar_root"),
            FretDanceActor->CachedGuitarRelativeTransform)) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeGuitarSync: Failed to initialize "
                    "control relationship"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("InitializeGuitarSync: Control relationship "
                "initialized and cached successfully"));
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeGuitarSync Completed =========="));

    return true;
}

bool UFretDanceTransformSyncProcessor::SyncGuitarTransform(
    AFretDanceUnreal* FretDanceActor, bool bIsRenderingEnvironment) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncGuitarTransform: FretDanceActor is null"));
        return false;
    }

    if (!FretDanceActor->Guitar) {
        UE_LOG(LogTemp, Error, TEXT("SyncGuitarTransform: Guitar is null"));
        return false;
    }

    // 获取 Performer 的 ControlRig 实例
    UControlRig* PerformerControlRig =
        FretDanceActor->GetCachedControlRig(TEXT("Performer"));
    if (!PerformerControlRig) {
        return false;
    }

    // 每帧更新：使用缓存的相对变换矩阵快速更新
    bool bUpdateResult =
        FInstrumentControlRigUtility::UpdateChildControlFromParent(
            PerformerControlRig, TEXT("controller_root"),
            FretDanceActor->SkeletalMeshActor, FretDanceActor->Guitar,
            TEXT("guitar_root"), FretDanceActor->CachedGuitarRelativeTransform);

    if (!bUpdateResult) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncGuitarTransform: Failed to update child "
                    "control from parent"));
    }

    return bUpdateResult;
}
