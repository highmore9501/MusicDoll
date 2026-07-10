#include "SingerUnreal.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRigBlueprintLegacy.h"
#include "InstrumentMorphTargetUtility.h"
#include "LevelSequence.h"
#include "LipSyncUtility.h"

ASingerUnreal::ASingerUnreal() {
    // Singer 不需要每帧 Tick，口型动画由 LipSyncUtility 批量生成
    PrimaryActorTick.bCanEverTick = false;
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
