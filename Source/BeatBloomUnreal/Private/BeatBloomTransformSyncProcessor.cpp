#include "BeatBloomTransformSyncProcessor.h"

#include "Animation/SkeletalMeshActor.h"
#include "BeatBloomControlRigProcessor.h"
#include "ControlRig/Public/ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCreationUtility.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"

#define LOCTEXT_NAMESPACE "BeatBloomTransformSyncProcessor"

// ===== 内部辅助函数（不暴露到 UCLASS） =====

/**
 * 同步单个鼓组的变换
 * 这是 SyncAllInstrumentTransforms 的内部实现
 *
 * @param BeatBloomActor BeatBloom Actor 实例
 * @param bIsRenderingEnvironment 是否为渲染环境
 * @return 同步是否成功
 */
static bool SyncDrumKitTransform(ABeatBloomUnreal* BeatBloomActor,
                                 bool bIsRenderingEnvironment) {
    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncDrumKitTransform: BeatBloomActor is null"));
        return false;
    }

    if (!BeatBloomActor->DrumKit) {
        UE_LOG(LogTemp, Error, TEXT("SyncDrumKitTransform: DrumKit is null"));
        return false;
    }

    // 获取 Performer 的 ControlRig 实例
    UControlRig* PerformerControlRig =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!PerformerControlRig) {
        return false;
    }

    // 每帧更新：使用缓存的相对变换矩阵快速更新
    bool bUpdateResult =
        FInstrumentControlRigUtility::UpdateChildControlFromParent(
            PerformerControlRig, TEXT("controller_root"),
            BeatBloomActor->SkeletalMeshActor, BeatBloomActor->DrumKit,
            TEXT("drumkit_control"),
            BeatBloomActor->CachedDrumKitRelativeTransform);

    if (!bUpdateResult) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncDrumKitTransform: Failed to update child "
                    "control from parent"));
    }

    return bUpdateResult;
}

bool UBeatBloomTransformSyncProcessor::SyncAllInstrumentTransforms(
    ABeatBloomUnreal* BeatBloomActor) {
    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncAllInstrumentTransforms: BeatBloomActor is null"));
        return false;
    }

    if (!BeatBloomActor->bEnableRealtimeSync) {
        return true;
    }

    // 检测是否为渲染环境
    bool bIsRendering = UInstrumentAnimationUtility::IsInRenderingScenario();

    bool bDrumKitSuccess = SyncDrumKitTransform(BeatBloomActor, bIsRendering);

    if (!bDrumKitSuccess) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncAllInstrumentTransforms: Failed to sync "
                    "drumkit transform"));
        BeatBloomActor->bEnableRealtimeSync =
            false;  // 禁用后续帧的同步以节省性能
    }

    return bDrumKitSuccess;
}

bool UBeatBloomTransformSyncProcessor::InitializeDrumKitSync(
    ABeatBloomUnreal* BeatBloomActor) {
    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeDrumKitSync: BeatBloomActor is null"));
        return false;
    }

    if (!BeatBloomActor->DrumKit) {
        UE_LOG(LogTemp, Error, TEXT("InitializeDrumKitSync: DrumKit is null"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeDrumKitSync Started =========="));

    // Step 1: 检查 Performer 的 controller_root 是否存在
    UControlRigBlueprint* PerformerBlueprint =
        BeatBloomActor->GetCachedControlRigBlueprint(TEXT("Performer"));
    if (!PerformerBlueprint || !PerformerBlueprint->Hierarchy ||
        !PerformerBlueprint->Hierarchy->Contains(FRigElementKey(
            TEXT("controller_root"), ERigElementType::Control))) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeDrumKitSync: 'controller_root' does not "
                    "exist in Performer's ControlRig Blueprint. "
                    "Please create it manually."));
        return false;
    }

    // Step 2: 检查 DrumKit 的 drumkit_control 是否存在，不存在则尝试创建
    UControlRigBlueprint* DrumKitBlueprint =
        BeatBloomActor->GetCachedControlRigBlueprint(TEXT("DrumKit"));
    if (DrumKitBlueprint) {
        if (!DrumKitBlueprint->Hierarchy ||
            !DrumKitBlueprint->Hierarchy->Contains(FRigElementKey(
                TEXT("drumkit_control"), ERigElementType::Control))) {
            UE_LOG(LogTemp, Warning,
                   TEXT("InitializeDrumKitSync: 'drumkit_control' not found "
                        "in DrumKit's ControlRig Blueprint. Attempting "
                        "to create it."));
            if (!FControlRigCreationUtility::CreateControl(
                    DrumKitBlueprint, TEXT("drumkit_control"), TEXT(""))) {
                UE_LOG(LogTemp, Error,
                       TEXT("InitializeDrumKitSync: Failed to create "
                            "'drumkit_control' in DrumKit's ControlRig "
                            "Blueprint."));
                return false;
            }
            UE_LOG(LogTemp, Warning,
                   TEXT("InitializeDrumKitSync: 'drumkit_control' created "
                        "successfully."));
        }
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeDrumKitSync: Could not retrieve "
                    "ControlRig Blueprint for DrumKit."));
        return false;
    }

    // Step 3: 使用 FInstrumentControlRigUtility::InitializeControlRelationship
    // 计算并缓存相对变换矩阵
    if (!FInstrumentControlRigUtility::InitializeControlRelationship(
            BeatBloomActor->SkeletalMeshActor, TEXT("controller_root"),
            BeatBloomActor->DrumKit, TEXT("drumkit_control"),
            BeatBloomActor->CachedDrumKitRelativeTransform)) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeDrumKitSync: Failed to initialize "
                    "control relationship"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("InitializeDrumKitSync: Control relationship "
                "initialized and cached successfully"));
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeDrumKitSync Completed =========="));

    return true;
}

#undef LOCTEXT_NAMESPACE
