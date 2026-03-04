#include "FretDanceTransformSyncProcessor.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig/Public/ControlRig.h"
#include "Engine/Engine.h"
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

    // 计算并缓存吉他相对于 SkeletalMeshActor 的初始变换
    USkeletalMeshComponent* GuitarComponent =
        FretDanceActor->Guitar->GetSkeletalMeshComponent();
    USkeletalMeshComponent* PerformerComponent =
        FretDanceActor->SkeletalMeshActor->GetSkeletalMeshComponent();

    if (!GuitarComponent || !PerformerComponent) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeGuitarSync: Failed to get skeletal mesh components"));
        return false;
    }

    // 获取吉他的 root 骨骼变换（在吉他 Actor 的组件空间中）
    int32 RootBoneIndex = GuitarComponent->GetBoneIndex(TEXT("root"));
    if (RootBoneIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeGuitarSync: 'root' bone not found in Guitar"));
        return false;
    }

    // 获取 root 骨骼在吉他组件空间中的变换
    const FTransform& RootBoneComponentSpaceTransform =
        GuitarComponent->GetComponentSpaceTransforms()[RootBoneIndex];

    // 转换为世界空间变换
    FTransform RootBoneWorldTransform =
        RootBoneComponentSpaceTransform * FretDanceActor->Guitar->GetActorTransform();

    // 转换为相对于 Performer 的变换
    FTransform PerformerWorldTransform = PerformerComponent->GetComponentToWorld();
    FretDanceActor->CachedGuitarRelativeTransform =
        RootBoneWorldTransform.Inverse() * PerformerWorldTransform;

    UE_LOG(LogTemp, Warning,
           TEXT("InitializeGuitarSync: Successfully cached guitar relative transform"));

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
        UE_LOG(LogTemp, Error,
               TEXT("SyncGuitarTransform: Guitar is null"));
        return false;
    }

    // 获取 Performer 的 ControlRig 实例
    UControlRig* PerformerControlRig =
        FretDanceActor->GetCachedControlRig(TEXT("Performer"));
    if (!PerformerControlRig) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncGuitarTransform: Failed to get cached "
                    "Performer ControlRig"));
        return false;
    }

    // 每帧更新：使用缓存的相对变换矩阵快速更新
    bool bUpdateResult =
        FInstrumentControlRigUtility::UpdateChildControlFromParent(
            PerformerControlRig, TEXT("controller_root"),
            FretDanceActor->SkeletalMeshActor,
            FretDanceActor->Guitar, TEXT("root"),
            FretDanceActor->CachedGuitarRelativeTransform);

    if (!bUpdateResult) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncGuitarTransform: Failed to update child "
                    "control from parent"));
    }

    return bUpdateResult;
}
