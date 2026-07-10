#pragma once

#include "CoreMinimal.h"
#include "InstrumentBase.h"
#include "LipSyncTypes.h"
#include "SingerUnreal.generated.h"

class UControlRigBlueprint;
class ULevelSequence;

/**
 * ASingerUnreal - 歌手 Actor
 *
 * 不演奏任何乐器，仅通过 Lip Sync 驱动角色口型动画。
 * 核心能力（Lip Sync 解析/CR通道创建/关键帧写入）由 MusicDollCommon 中的
 * ULipSyncUtility 提供，本类只负责配置入口和流程触发。
 */
UCLASS(Blueprintable, BlueprintType)
class SINGERUNREAL_API ASingerUnreal : public AInstrumentBase {
    GENERATED_BODY()

   public:
    ASingerUnreal();

    // ===== Lip Sync 配置 =====

    /** Lip Sync JSON 文件路径（由开源 Lip Sync 项目生成） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
    FString LipSyncJsonPath;

    /** 口型映射表（Phoneme → Morph Target 名称） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync")
    TArray<FLipSyncMappingPair> LipSyncMapping;

    // ===== 便捷方法 =====

    /**
     * 从 JSON 文件一键生成 Lip Sync 动画并写入 Sequencer
     *
     * 内部委托给 ULipSyncUtility::GenerateLipSyncFromJson。
     * 映射表（LipSyncMapping）需要在调用前已设置到 ControlRigBlueprint 变量中，
     * 可通过 ULipSyncUtility::SetLipSyncMapping 预先设置。
     *
     * @param Performer 歌手 SkeletalMeshActor（演奏者）
     * @param ControlRigBlueprint 演奏者的
     * ControlRigBlueprint（用于读取映射表变量）
     * @param LevelSequence 目标 LevelSequence
     * @return 成功写入的 Morph Target 数量，失败返回 -1
     */
    UFUNCTION(BlueprintCallable, Category = "Lip Sync")
    int32 GenerateLipSyncAnimation(ASkeletalMeshActor* Performer,
                                   UControlRigBlueprint* ControlRigBlueprint,
                                   ULevelSequence* LevelSequence);
};
