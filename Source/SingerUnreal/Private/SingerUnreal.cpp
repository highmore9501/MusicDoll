#include "SingerUnreal.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "LevelSequence.h"
#include "LipSyncUtility.h"

ASingerUnreal::ASingerUnreal() {
    // Singer 不需要每帧 Tick，口型动画由 LipSyncUtility 批量生成
    PrimaryActorTick.bCanEverTick = false;
}

bool ASingerUnreal::InitializeControlRig() {
    if (!SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("ASingerUnreal::InitializeControlRig: SkeletalMeshActor "
                    "is null"));
        return false;
    }

    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("ASingerUnreal::InitializeControlRig: GEngine is null"));
        return false;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("ASingerUnreal::InitializeControlRig: CacheSubsystem not "
                    "found"));
        return false;
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("ASingerUnreal::InitializeControlRig: No active "
                    "LevelSequence"));
        return false;
    }

    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(SkeletalMeshActor,
                                               LevelSequence);
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("ASingerUnreal::InitializeControlRig: Failed to get "
                    "ControlRigBlueprint"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== Singer InitializeControlRig Started =========="));

    // 1. base_root（最上层根 Control；已存在时 CreateControl 直接返回 true）
    if (!FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, TEXT("base_root"), TEXT(""))) {
        UE_LOG(LogTemp, Error,
               TEXT("ASingerUnreal::InitializeControlRig: Failed to create "
                    "base_root"));
        return false;
    }

    // 2. 在 base_root 下添加眼睛睁开 Float channel（无「なごみ」曲线时跳过）
    if (!FControlRigCreationUtility::AddEyeControl(ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("ASingerUnreal::InitializeControlRig: Failed to add eye "
                    "control under base_root"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== Singer InitializeControlRig Completed =========="));
    return true;
}

int32 ASingerUnreal::GenerateLipSyncAnimation(
    ASkeletalMeshActor* Performer, UControlRigBlueprint* ControlRigBlueprint,
    ULevelSequence* LevelSequence) {
    if (!Performer || !ControlRigBlueprint || !LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("ASingerUnreal::GenerateLipSyncAnimation: 无效的参数"));
        return -1;
    }

    if (LipSyncJsonPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("ASingerUnreal::GenerateLipSyncAnimation: LipSyncJsonPath "
                    "为空"));
        return -1;
    }

    // 先确保映射表已设置到 ControlRigBlueprint 变量中
    TArray<FLipSyncMappingPair> ExistingMapping;
    if (ULipSyncUtility::GetLipSyncMapping(ControlRigBlueprint,
                                           ExistingMapping)) {
        if (ExistingMapping.Num() > 0) {
            // 已有映射表，使用已有的
        } else if (LipSyncMapping.Num() > 0) {
            // Actor 上有映射表配置但 CR 中还没有，写入 CR
            ULipSyncUtility::SetLipSyncMapping(ControlRigBlueprint,
                                               LipSyncMapping);
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("ASingerUnreal::GenerateLipSyncAnimation: 未配置 "
                        "LipSyncMapping"));
            return -1;
        }
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("ASingerUnreal::GenerateLipSyncAnimation: 无法读取 CR 中的 "
                    "LipSyncMapping 变量"));
        return -1;
    }

    // 委托给 ULipSyncUtility 完成核心工作
    int32 Result = ULipSyncUtility::GenerateLipSyncFromJson(
        Performer, ControlRigBlueprint, LipSyncJsonPath);

    UE_LOG(LogTemp, Log,
           TEXT("ASingerUnreal::GenerateLipSyncAnimation: 完成，写入 %d 个 "
                "Morph Target"),
           Result);

    return Result;
}
