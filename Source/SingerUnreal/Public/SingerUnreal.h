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

    /** Lip Sync 文件路径（支持 .json 的 Lisa 格式或 .tsv 的 Cherry 格式） */
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

    // ===== Control Rig 初始化 =====

    /**
     * 初始化演奏者的 Control Rig（Singer 专用最小控制体系）
     *
     * Singer 不演奏乐器，不需要手指/脚部等控制器，控制体系只包含：
     * 1. 创建 base_root（最上层根 Control，已存在则跳过）
     * 2. 在 base_root 下按「なごみ」曲线创建眼睛睁开 Float channel
     *    （委托给 FControlRigCreationUtility::AddEyeControl）
     *
     * ControlRigBlueprint 通过当前 LevelSequence 的
     * UControlRigCacheSubsystem 从 SkeletalMeshActor 获取。
     *
     * @return 是否成功（无「なごみ」曲线时视为正常跳过并返回 true）
     */
    UFUNCTION(BlueprintCallable, Category = "Control Rig")
    bool InitializeControlRig();
};
