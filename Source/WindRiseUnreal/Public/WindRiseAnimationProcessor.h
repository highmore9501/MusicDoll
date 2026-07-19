#pragma once

#include "CoreMinimal.h"
#include "WindRiseUnreal.h"
#include "WindRiseAnimationProcessor.generated.h"

class ULevelSequence;

/**
 * WindRise 动画处理器
 * 负责解析 .wind_rise 配置文件和生成管乐器完整动画
 *
 * ============================================================
 * 功能概述：
 * ============================================================
 *
 * 1. 解析 .wind_rise 配置文件，获取各子动画文件路径
 * 2. 生成手部动画（左/右手关键帧 → BatchInsertControlRigKeys）
 * 3. 生成人物 Morph Target 动画（写入 Breath_Control Float Channel）
 * 4. 生成乐器 Morph Target 动画（写入 wind_root Float Channel）
 *
 * ============================================================
 * 调用方式（UI/脚本）：
 * ============================================================
 *
 *   UWindRiseAnimationProcessor::GenerateAnimationFromWindRise(Actor,
 * FilePath);
 *
 * ============================================================
 * JSON 数据格式 (.wind_rise)：
 * ============================================================
 *
 * {
 *   "left_hand_animation_file":   "path/to/left_hand.json",
 *   "right_hand_animation_file":  "path/to/right_hand.json",
 *   "character_sk_animation_file": "path/to/char_sk.json",
 *   "instrument_sk_animation_file": "path/to/inst_sk.json",
 *   "activity_curve_file":        "path/to/activity.json"
 * }
 *
 * 手部动画 JSON 格式：
 * [
 *   {
 *     "frame": 0.0,
 *     "hand_infos": {
 *       "H_L": [x, y, z, rw, rx, ry, rz],
 *       ...
 *     }
 *   }
 * ]
 *
 * Morph Target 动画 JSON 格式：
 * [
 *   {
 *     "shape_key_name": "SK_Name",
 *     "value": 0.5,
 *     "frame": 30.0
 *   }
 * ]
 */
UCLASS()
class WINDRISEUNREAL_API UWindRiseAnimationProcessor : public UObject {
    GENERATED_BODY()

   public:
    // ============================================================
    // 公开接口
    // ============================================================

    /**
     * 生成完整动画（入口函数）
     * 解析 .wind_rise 汇总文件 → 依次生成手部/人物MT/乐器MT动画
     *
     * @param WindRiseActor 管乐器 Actor 实例
     * @param WindRiseFilePath .wind_rise 汇总文件路径
     */
    UFUNCTION(BlueprintCallable, Category = "WindRise Animation Processor")
    static void GenerateAnimationFromWindRise(AWindRiseUnreal* WindRiseActor,
                                              const FString& WindRiseFilePath);

    /**
     * 仅生成手部动画（左/右手关键帧）
     *
     * @param WindRiseActor 管乐器 Actor 实例
     * @param LeftHandPath  左手动画 JSON 路径
     * @param RightHandPath 右手动画 JSON 路径
     * @param LevelSequence 目标 Level Sequence
     * @return 是否成功
     */
    static bool GenerateHandAnimation(AWindRiseUnreal* WindRiseActor,
                                      const FString& LeftHandPath,
                                      const FString& RightHandPath,
                                      ULevelSequence* LevelSequence);

    /**
     * 仅生成人物 Morph Target 动画（写入 Breath_Control）
     *
     * @param WindRiseActor 管乐器 Actor 实例
     * @param CharSKPath    人物 MT 动画 JSON 路径
     * @param LevelSequence 目标 Level Sequence
     * @return 是否成功
     */
    static bool GenerateCharacterMorphTargetAnimation(
        AWindRiseUnreal* WindRiseActor, const FString& CharSKPath,
        ULevelSequence* LevelSequence);

    /**
     * 仅生成乐器 Morph Target 动画（写入 wind_root）
     *
     * @param WindRiseActor 管乐器 Actor 实例
     * @param InstSKPath    乐器 MT 动画 JSON 路径
     * @param LevelSequence 目标 Level Sequence
     * @return 是否成功
     */
    static bool GenerateInstrumentMorphTargetAnimation(
        AWindRiseUnreal* WindRiseActor, const FString& InstSKPath,
        ULevelSequence* LevelSequence);
};
