#include "InstrumentControlRigUtility.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "ISequencer.h"
#include "InstrumentAnimationUtility.h"
#include "LevelEditor.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneSequence.h"

bool FInstrumentControlRigUtility::GetControlRigControlWorldTransform(
    UControlRig* ControlRigInstance, const FString& ControlName,
    ASkeletalMeshActor* InSkeletalMeshActor, FTransform& OutTransform) {
    // 参数验证
    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlWorldTransform: "
                    "ControlRigInstance is null"));
        return false;
    }

    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlWorldTransform: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    if (ControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlWorldTransform: "
                    "ControlName is empty"));
        return false;
    }

    // 获取 Control 在 Hierarchy 中的全局变换（Control Rig 内部坐标系）
    URigHierarchy* Hierarchy = ControlRigInstance->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlWorldTransform: "
                    "Hierarchy is null"));
        return false;
    }

    int32 ControlIndex = Hierarchy->GetIndex(
        FRigElementKey(*ControlName, ERigElementType::Control));

    if (ControlIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlWorldTransform: "
                    "Control '%s' not found in Hierarchy"),
               *ControlName);
        return false;
    }

    // 在读取变换之前重新评估 Control Rig，确保蓝图中的约束（如 LookAt）已被计算
    ControlRigInstance->Evaluate_AnyThread();

    FTransform ControlGlobalTransform =
        Hierarchy->GetGlobalTransform(ControlIndex);

    // SkeletalMeshActor 的世界变换
    FTransform ActorWorldTransform = InSkeletalMeshActor->GetActorTransform();

    // 真实世界坐标 = Control Rig 内部全局变换 × Actor 的世界变换
    OutTransform = ControlGlobalTransform * ActorWorldTransform;

    return true;
}

bool FInstrumentControlRigUtility::SetControlRigLocalTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    const FVector& NewLocation, const FQuat& NewRotation) {
    // 获取 Control Rig 实例和 Control 索引
    UControlRig* ControlRigInstance = nullptr;
    int32 ControlIndex = INDEX_NONE;

    if (!GetControlRigAndIndex(InSkeletalMeshActor, ControlName,
                               ControlRigInstance, ControlIndex)) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("FInstrumentControlRigUtility::SetControlRigLocalTransform: "
                 "Failed to get Control Rig instance or Control index"));
        return false;
    }

    URigHierarchy* Hierarchy = ControlRigInstance->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(
            LogTemp, Error,
            TEXT("FInstrumentControlRigUtility::SetControlRigLocalTransform: "
                 "Hierarchy is null"));
        return false;
    }

    // 直接构建局部变换，无需任何坐标系转换
    FTransform NewLocalTransform(NewRotation, NewLocation,
                                 FVector(1.0f, 1.0f, 1.0f));

    // 设置 Control 的局部变换
    Hierarchy->SetLocalTransform(ControlIndex, NewLocalTransform);

    // 通知 Control Rig 系统更新
    // 1. 重新评估 Control Rig 以应用变换更改
    ControlRigInstance->Evaluate_AnyThread();

    // 2. 强制更新骨骼网格组件
    if (USkeletalMeshComponent* SkelMeshComp =
            InSkeletalMeshActor->GetSkeletalMeshComponent()) {
        SkelMeshComp->RefreshBoneTransforms();
        SkelMeshComp->MarkRenderTransformDirty();
        SkelMeshComp->MarkRenderStateDirty();
    }

    return true;
}

bool FInstrumentControlRigUtility::SetControlRigWorldTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    const FVector& NewWorldLocation, const FQuat& NewWorldRotation) {
    // 构建期望的世界变换
    FTransform DesiredWorldTransform(NewWorldRotation, NewWorldLocation,
                                     FVector(1.0f, 1.0f, 1.0f));

    if (!InSkeletalMeshActor) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("FInstrumentControlRigUtility::SetControlRigWorldTransform: "
                 "InSkeletalMeshActor is null"));
        return false;
    }

    // 获取 SkeletalMeshActor 的世界变换
    FTransform RootWorldTransform = InSkeletalMeshActor->GetActorTransform();

    // 计算相对于 Actor 的局部变换
    // 局部变换 = 根元素的世界变换的逆 × 世界变换
    FTransform LocalTransform =
        DesiredWorldTransform.GetRelativeTransform(RootWorldTransform);

    // 调用低级的局部变换应用方法
    return SetControlRigLocalTransform(InSkeletalMeshActor, ControlName,
                                       LocalTransform.GetLocation(),
                                       LocalTransform.Rotator().Quaternion());
}

// ========== 私有辅助方法实现 =========

bool FInstrumentControlRigUtility::GetControlRigControlGlobalInitTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    FTransform& OutGlobalInitTransform) {
    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    // 通过Subsystem获取 Control Rig 蓝图
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "GEngine is not available"));
        return false;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "ControlRig Cache Subsystem is not available"));
        return false;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "No Level Sequence is currently open"));
        return false;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(InSkeletalMeshActor, LevelSequence);
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(InSkeletalMeshActor,
                                               LevelSequence);

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "Failed to get ControlRig from SkeletalMeshActor"));
        return false;
    }

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "ControlRigBlueprint is null"));
        return false;
    }

    URigHierarchy* BlueprintHierarchy = ControlRigBlueprint->Hierarchy;
    if (!BlueprintHierarchy) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "Blueprint Hierarchy is null"));
        return false;
    }

    // 获取 Control 的索引
    int32 ControlIndex = BlueprintHierarchy->GetIndex(
        FRigElementKey(*ControlName, ERigElementType::Control));

    if (ControlIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "Control '%s' not found in Blueprint Hierarchy"),
               *ControlName);
        return false;
    }

    // 使用 URigHierarchy::GetInitialGlobalTransform 获取相对于 Control Rig
    // 根的全局初始化变换 这个方法已经在 URigHierarchy
    // 中实现，能高效地计算完整的初始化全局变换
    OutGlobalInitTransform =
        BlueprintHierarchy->GetInitialGlobalTransform(ControlIndex);

    return true;
}

bool FInstrumentControlRigUtility::GetControlRigAndIndex(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    UControlRig*& OutControlRigInstance, int32& OutControlIndex) {
    OutControlRigInstance = nullptr;
    OutControlIndex = INDEX_NONE;

    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    if (ControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "ControlName is empty"));
        return false;
    }

    // 通过Subsystem获取 Control Rig 实例
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "GEngine is not available"));
        return false;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "ControlRig Cache Subsystem is not available"));
        return false;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "No Level Sequence is currently open"));
        return false;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(InSkeletalMeshActor, LevelSequence);
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(InSkeletalMeshActor,
                                               LevelSequence);

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "Failed to get ControlRig from SkeletalMeshActor"));
        return false;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "ControlRigInstance is null"));
        return false;
    }

    // 获取 Control 索引
    URigHierarchy* Hierarchy = ControlRigInstance->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "Hierarchy is null"));
        return false;
    }

    OutControlIndex = Hierarchy->GetIndex(
        FRigElementKey(*ControlName, ERigElementType::Control));

    if (OutControlIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "Control '%s' not found in Hierarchy"),
               *ControlName);
        return false;
    }

    OutControlRigInstance = ControlRigInstance;
    return true;
}




