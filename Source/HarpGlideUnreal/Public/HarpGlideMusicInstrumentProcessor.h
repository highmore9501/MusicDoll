#pragma once

#include "Channels/MovieSceneFloatChannel.h"
#include "CoreMinimal.h"
#include "HarpGlideUnreal.h"
#include "MovieScene.h"
#include "UObject/Object.h"
#include "HarpGlideMusicInstrumentProcessor.generated.h"

/**
 * UHarpGlideMusicInstrumentProcessor
 *
 * 竖琴乐器动画处理器
 * 负责生成弦振动 Morph Target 动画和踏板 Morph Target 动画
 *
 * 竖琴 Morph Target（已经在骨骼网格上预先创建，仅驱动）：
 *   - 弦振动：string_{n}_inner 或 string_{n}_outer（n = 0..46，is_thumb=true
 * 时用 outer，否则用 inner）
 *   - 单排弦：名字不带方向后缀（string12_inner）
 *   - 双排弦：左右两排各有自己的 Morph Target，按**拨弦的手**加方向后缀
 *     —— 左手弹左排 _L、右手弹右排 _R（string12_inner_L / string12_outer_R）；
 *     数据里没有 hand 字段时按单排弦处理
 *   - 踏板：pedal_{note}（note = D/C/B/E/F/G/A）
 *
 * Root Control：harp_root
 *
 * 参考：ZhengDriftMusicInstrumentProcessor
 */
UCLASS()
class HARPGLIDEUNREAL_API UHarpGlideMusicInstrumentProcessor : public UObject {
    GENERATED_BODY()
   public:
    /**
     * 竖琴乐器初始化：注册竖琴 CR、清理、初始化通道
     */
    UFUNCTION(BlueprintCallable, Category = "HarpGlide Music Processor")
    static void InitializeHarpInstrument(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 初始化弦材质（为每根弦创建独立材质实例）
     */
    UFUNCTION(BlueprintCallable, Category = "HarpGlide Music Processor")
    static void InitializeStringMaterials(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 创建弦振动 Morph Target 动画通道
     * 使用 UInstrumentMorphTargetUtility 从骨骼网格获取已有 MT 名称并创建通道
     */
    UFUNCTION(BlueprintCallable, Category = "HarpGlide Music Processor")
    static void InitializeStringVibrationAnimationChannels(
        AHarpGlideUnreal* HarpGlideActor);

    /**
     * 从 JSON 生成乐器 Morph Target 动画（弦振动 + 踏板 Shape Key）
     *
     * 将两份 JSON 数据合并为一份 KeyframeData，一次性写入 Control Rig Section，
     * 避免多次调用 WriteMorphTargetAnimationToControlRig 导致后一次覆盖前一次。
     *
     * 弦振动 JSON 每项格式：
     *   { "string_index": 10, "frame": 24, "value": 0.8, "is_thumb": false }
     * 双排弦时额外带拨弦的手（hand = "left" / "right"），Morph Target 名随之
     * 加 _L / _R 后缀；单排弦数据不带该字段：
     *   { "string_index": 10, "frame": 24, "value": 0.8, "is_thumb": false,
     *     "hand": "left" }
     * 踏板 Shape Key JSON 每项格式（PedalShapeKeyEvent）：
     *   {
     *     "pedal_state": "pedal_A_state0",
     *     "data": { "frame": 0.0, "value": 0.0 }
     *   }
     * 其中 "pedal_state" 为完整的 Morph Target 名称（7 踏板 × 5 状态 = 35 个）
     */
    UFUNCTION(BlueprintCallable, Category = "HarpGlide Music Processor")
    static void GenerateInstrumentAnimation(
        AHarpGlideUnreal* HarpGlideActor,
        const FString& StringAnimationDataPath,
        const FString& PedalShapeAnimationDataPath);

   private:
    static void CleanupExistingHarpAnimations(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 弦振动事件 → Morph Target 名（弦索引 + 方向 + 双排弦方向后缀）
     *
     * @param StringIndex 弦索引（string_index）
     * @param bIsThumb 是否拇指拨弦（true = outer，false = inner）
     * @param HandSide 拨弦的手："left" / "right"；空串 = 单排弦（不加后缀）
     * @return 如 string12_inner / string12_inner_L / string12_outer_R
     */
    static FString BuildStringMorphName(int32 StringIndex, bool bIsThumb,
                                        const FString& HandSide);
};
