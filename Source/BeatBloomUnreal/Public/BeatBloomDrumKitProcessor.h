#pragma once

#include "BeatBloomUnreal.h"
#include "CoreMinimal.h"
#include "BeatBloomDrumKitProcessor.generated.h"

class ULevelSequence;
class USkeletalMeshComponent;

/**
 * BeatBloom 鼓组动画处理器
 * 负责解析 _shapekey.animation 文件，将鼓面/镲片的 MorphTarget 动画数据
 * 写入 Unreal 的 MorphTarget 轨道
 *
 * ============================================================
 * 对标参考：
 * ============================================================
 *
 * - UKeyRipplePianoProcessor（钢琴琴键 MorphTarget 动画）
 * - UFretDanceMusicInstrumentProcessor（吉他弦振动动画）
 *
 * ============================================================
 * 与钢琴 MorphTarget 动画的主要差异：
 * ============================================================
 *
 * | 维度             | KeyRipple（钢琴）   | BeatBloom（打击乐）         |
 * |------------------|---------------------|-----------------------------|
 * | MorphTarget 数量 | 88键（固定）        | 动态（由 .drumkit 决定）     |
 * | 每个目标的命名   | Key_{NoteNumber}    | {DrumKitName}_beat           |
 * | 数据格式         | 帧+值对数组         | 帧+值对数组（相同）          |
 * | 目标骨骼         | Piano（单一）       | DrumKit（可能含多个骨骼Mesh）|
 * | 值域             | 0.0~1.0             | 0.0~1.0                      |
 *
 * ============================================================
 * _shapekey.animation 文件格式：
 * ============================================================
 *
 * 顶层为 JSON 数组，每个元素包含：
 * - "drum_kit": 鼓件名称（如 "Kick_drum"）
 * - "animation_data": 帧+值对数组
 *   - "frame": 帧号（浮点数）
 *   - "value": MorphTarget 值（0.0~1.0）
 *
 * MorphTarget 命名规则：{drum_kit}_beat
 * 例如：Kick_drum -> Kick_drum_beat
 *
 * ============================================================
 */
UCLASS()
class BEATBLOOMUNREAL_API UBeatBloomDrumKitProcessor : public UObject {
    GENERATED_BODY()

public:
    /**
     * 初始化鼓组乐器（主入口方法）
     *
     * 流程：
     * 1. 验证 BeatBloomActor 和 DrumKit 引用
     * 2. 触发 ControlRig 注册
     * 3. 清理现有的动画数据
     * 4. 获取所有 MorphTarget 名称
     * 5. 确保 Root Control 存在（drumkit_control）
     * 6. 批量添加动画通道（每个 MorphTarget 对应一个 Float 通道）
     * 7. 刷新 Sequencer 显示
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom DrumKit Processor")
    static void InitializeDrumKit(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 生成鼓组 ShapeKey 动画
     *
     * 流程：
     * 1. 验证 BeatBloomActor 和 DrumKit 引用
     * 2. 获取 LevelSequence 和 Sequencer
     * 3. 从 AnimationFilePath 推导 _shapekey.animation 路径
     * 4. 读取并解析 JSON 文件（顶层为数组）
     * 5. 遍历每个鼓件，构造 MorphTarget 名称并写入关键帧
     * 6. 刷新 Sequencer 显示
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom DrumKit Processor")
    static void GenerateDrumKitAnimation(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 生成鼓组 ShapeKey 动画（指定文件路径）
     *
     * 与 GenerateDrumKitAnimation 相同流程，但使用指定的文件路径
     * 而非从 AnimationFilePath 推导
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     * @param ShapeKeyAnimationPath _shapekey.animation 文件的完整路径
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom DrumKit Processor")
    static void GenerateDrumKitAnimationFromPath(
        ABeatBloomUnreal* BeatBloomActor,
        const FString& ShapeKeyAnimationPath);

private:
    /**
     * 查找鼓组骨骼 Mesh 中包含指定 MorphTarget 的骨骼组件
     *
     * 策略A（简单方案）：直接返回 DrumKit 的主 SkeletalMeshComponent
     * 策略B（遍历方案）：遍历 DrumKit 关联的所有骨骼 Mesh，查找匹配项
     * 当前使用策略A，后续按需扩展
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     * @param MorphTargetName 要查找的 MorphTarget 名称
     * @return 包含该 MorphTarget 的骨骼组件，未找到返回 nullptr
     */
    static USkeletalMeshComponent* FindMorphTargetOwner(
        ABeatBloomUnreal* BeatBloomActor,
        const FString& MorphTargetName);

    /**
     * 为单个鼓件写入 MorphTarget 动画
     *
     * 流程：
     * 1. 在 DrumKit 骨骼 Mesh 上查找指定名称的 MorphTarget
     * 2. 如果不存在则警告并跳过
     * 3. 使用 UInstrumentMorphTargetUtility 写入 MorphTarget 关键帧
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     * @param LevelSequence 当前 LevelSequence
     * @param DrumKitActor 鼓组 SkeletalMeshActor
     * @param MorphTargetName MorphTarget 名称（如 "Kick_drum_beat"）
     * @param AnimationDataArray animation_data JSON 数组
     * @return 写入是否成功
     */
    static bool WriteDrumKitMorphTargetAnimation(
        ABeatBloomUnreal* BeatBloomActor,
        ULevelSequence* LevelSequence,
        ASkeletalMeshActor* DrumKitActor,
        const FString& MorphTargetName,
        const TArray<TSharedPtr<FJsonValue>>& AnimationDataArray);
};
