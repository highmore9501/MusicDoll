#pragma once

#include "BeatBloomUnreal.h"
#include "CoreMinimal.h"
#include "InstrumentAnimationUtility.h"
#include "BeatBloomAnimationProcessor.generated.h"

class ULevelSequence;

/**
 * BeatBloom 动画处理器
 * 负责从 .animation 文件生成人物四肢动画（手/脚/目标控制器）
 *
 * ============================================================
 * 与 FretDance 的差异：
 * ============================================================
 *
 * 1. 动画文件结构不同：
 *    - FretDance: 配置文件指向两个独立的左/右手 JSON
 *    - BeatBloom: 单个 JSON 包含 5 个动画数组
 *      (left_hand, right_hand, left_foot, right_foot, target_animation)
 *
 * 2. 帧数据结构不同：
 *    - FretDance: { "frame": N, "fingerInfos": { "H_L": [...], ... } }
 *    - BeatBloom: { "frame": N, "position": [...], "rotation": [...],
 *                   "pivot_position": [...], "state": "Beat" }
 *
 * 3. 控制器集合不同：
 *    - FretDance: 手掌 + 手指（H/HP/H_rotation + I/M/R/P/T/TP）
 *    - BeatBloom: 手掌 + 脚部 + 目标
 *      (H/HP/H_rotation + F/F_rotation + Tar_Body/Chest/Head)
 *
 * 4. 目标控制器仅写入 Z 轴（X/Y 由 Driver 自动计算）
 *
 * ============================================================
 * 控制器列表：
 * ============================================================
 *
 * 手部控制器：
 *   - H_L / H_R               手掌主控制器（位置）
 *   - HP_L / HP_R              手掌 IK 轴点（位置）
 *   - H_rotation_L / H_rotation_R  手掌旋转（四元数）
 *
 * 脚部控制器：
 *   - F_L / F_R                脚部主控制器（位置）
 *   - F_rotation_L / F_rotation_R  脚部旋转（四元数）
 *
 * 目标控制器：
 *   - Tar_Body                 身体目标（仅 Z 轴位置）
 *   - Tar_Chest                胸部目标（仅 Z 轴位置）
 *   - Tar_Head                 头部目标（仅 Z 轴位置）
 *
 * ============================================================
 */
UCLASS()
class BEATBLOOMUNREAL_API UBeatBloomAnimationProcessor : public UObject {
    GENERATED_BODY()

public:
    /**
     * 生成人物演奏动画（四肢 + 目标控制器）
     *
     * 流程：
     * 1. 读取 AnimationFilePath 指向的 .animation 文件
     * 2. 解析 JSON，提取五个动画数组
     * 3. 分别处理手部/脚部/目标动画数据
     * 4. 批量写入 Control Rig 关键帧
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom Animation Processor")
    static void GeneratePerformerAnimation(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 生成鼓组 ShapeKey 动画
     * 委托给 UBeatBloomDrumKitProcessor 处理
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom Animation Processor")
    static void GenerateDrumKitAnimation(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 生成所有动画（人物演奏 + 鼓组 ShapeKey）
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom Animation Processor")
    static void GenerateAllAnimation(ABeatBloomUnreal* BeatBloomActor);

private:
    /**
     * 处理手部动画数据（左手/右手共用逻辑）
     *
     * 对每一帧提取 position -> H_{Suffix}, rotation -> H_rotation_{Suffix},
     * pivot_position -> HP_{Suffix}
     *
     * @param AnimationArray 手部帧数据 JSON 数组
     * @param HandSuffix "L" 或 "R"
     * @param ControlKeyframeData 输出的控制器关键帧数据
     * @param OutProcessedFrames 已处理帧计数
     * @param OutKeyframesAdded 已添加关键帧计数
     */
    static void ProcessHandAnimation(
        const TArray<TSharedPtr<FJsonValue>>& AnimationArray,
        const FString& HandSuffix,
        TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
        int32& OutProcessedFrames, int32& OutKeyframesAdded);

    /**
     * 处理脚部动画数据（左脚/右脚共用逻辑）
     *
     * 对每一帧提取 position -> F_{Suffix}, rotation -> F_rotation_{Suffix}
     * 注意：脚部不使用 pivot_position
     *
     * @param AnimationArray 脚部帧数据 JSON 数组
     * @param FootSuffix "L" 或 "R"
     * @param ControlKeyframeData 输出的控制器关键帧数据
     * @param OutProcessedFrames 已处理帧计数
     * @param OutKeyframesAdded 已添加关键帧计数
     */
    static void ProcessFootAnimation(
        const TArray<TSharedPtr<FJsonValue>>& AnimationArray,
        const FString& FootSuffix,
        TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
        int32& OutProcessedFrames, int32& OutKeyframesAdded);

    /**
     * 处理目标控制器动画数据
     *
     * 遍历 Tar_Body/Tar_Chest/Tar_Head 三个目标控制器，
     * 每个控制器只写入 Z 轴位置关键帧
     *
     * @param TargetAnimationObject target_animation JSON 对象
     * @param ControlKeyframeData 输出的控制器关键帧数据
     * @param OutProcessedFrames 已处理帧计数
     * @param OutKeyframesAdded 已添加关键帧计数
     */
    static void ProcessTargetAnimation(
        TSharedPtr<FJsonObject> TargetAnimationObject,
        TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
        int32& OutProcessedFrames, int32& OutKeyframesAdded);

    /**
     * 获取有效的 BeatBloom 控制器名称集合
     *
     * @return 包含所有 13 个控制器名称的集合
     */
    static const TSet<FString>& GetValidBeatBloomControllerNames();
};
