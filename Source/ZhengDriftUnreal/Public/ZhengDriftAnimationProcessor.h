#pragma once

#include "CoreMinimal.h"
#include "InstrumentAnimationUtility.h"
#include "UObject/Object.h"
#include "ZhengDriftUnreal.h"
#include "ZhengDriftAnimationProcessor.generated.h"

class ULevelSequence;

/**
 * UZhengDriftAnimationProcessor
 *
 * 古筝演奏者动画处理器
 * 负责解析 .zhengdrift 配置文件并生成手部 / Target 控制器动画
 *
 * JSON 文件格式：
 *   .zhengdrift 配置文件 → 指向三个路径：
 *     "performance_animation" → 手部帧数据
 *     "target_animation"      → 身体朝向帧数据
 *     "string_animation"      → 弦振动数据（由 MusicInstrumentProcessor 处理）
 *
 * 参考：BeatBloomAnimationProcessor（有 Target 动画）
 *       FretDanceAnimationProcessor（有手指动画）
 */
UCLASS()
class ZHENGDRIFTUNREAL_API UZhengDriftAnimationProcessor : public UObject {
    GENERATED_BODY()
   public:
    /**
     * 生成演奏者手部动画（左右手控制器）
     * 从 .zhengdrift 中读取 performance_animation 路径
     */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Animation Processor")
    static void GeneratePerformerAnimation(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 生成弦振动动画（委托给 MusicInstrumentProcessor）
     * 从 .zhengdrift 中读取 string_animation 路径
     */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Animation Processor")
    static void GenerateInstrumentAnimation(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 生成全部动画（演奏者 + Target + 弦振动）
     */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Animation Processor")
    static void GenerateAllAnimation(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 解析 .zhengdrift 配置文件，输出三个动画文件路径
     * @return 至少有一个路径有效时返回 true
     */
    static bool ParseZhengDriftConfigFile(AZhengDriftUnreal* ZhengDriftActor,
                                          FString& OutPerformanceAnimationPath,
                                          FString& OutTargetAnimationPath,
                                          FString& OutStringAnimationPath,
                                          FString& OutActivityCurvePath);

   private:
    /**
     * 从 performance JSON 生成左右手控制器关键帧
     *
     * JSON 每帧格式（新格式 hand_infos）：
     * {
     *   "frame": 0,
     *   "hand_infos": {
     *     "H_L": [x, y, z, w, i, j, k],   → H_L / H_R（位置前 3 + 旋转四元数后
     * 4，WXYZ） "HP_L": [x, y, z],              → HP_L / HP_R "T_L": [x, y, z],
     * → T_L / T_R "I_L": [x, y, z],               → I_L / I_R "M_L": [x, y, z],
     * → M_L / M_R "R_L": [x, y, z],               → R_L / R_R "P_L": [x, y, z]
     * → P_L / P_R
     *   },
     *   "state": "ready" | "attack" | "hold" | "release" | "transition"
     * }
     *
     * 注：手掌 H_{L/R} 单个控件同时承载位置与旋转（7 元素），不再使用
     * H_rotation_{L/R} 字段。旧格式（hand_position / hand_rotation 顶层字段）
     * 仍被 CollectPerformerKeyframes 兼容解析。
     */
    static void MakePerformerAnimation(AZhengDriftUnreal* ZhengDriftActor,
                                       const FString& AnimationFilePath,
                                       ULevelSequence* LevelSequence);

    /**
     * 从 target JSON 生成 Head_Control 位置关键帧
     * Middle_Hand 由 Control Rig 内部自动计算（H_L + H_R 的中点）
     * Look_At 通过父子关系跟随 Middle_Hand，Head_Control 朝向由 Track To
     * 约束驱动
     *
     * JSON 每帧格式：
     * {
     *   "frame": 0,
     *   "head_control_position": [x, y, z]   → Head_Control
     * }
     *
     * 注意：只写入位置 XYZ，不写入旋转
     */
    static void MakeTargetAnimation(AZhengDriftUnreal* ZhengDriftActor,
                                    const FString& TargetAnimationPath,
                                    ULevelSequence* LevelSequence);

    // ==================== 收集器（数据收集 + 写入分离） ====================

    /**
     * 从 performance JSON 收集关键帧数据到指定 map（不写入 Control Rig）
     * 与 MakePerformerAnimation 功能相同，但不调用 BatchInsertControlRigKeys
     */
    static void CollectPerformerKeyframes(
        const FString& AnimationFilePath,
        TMap<FString, TArray<FAnimationKeyframe>>& OutControlKeyframeData);

    /**
     * 从 target JSON 收集 keyframe 数据到指定 map（不写入 Control Rig）
     * 与 MakeTargetAnimation 功能相同，但不调用 BatchInsertControlRigKeys
     */
    static void CollectTargetKeyframes(
        const FString& TargetAnimationPath,
        TMap<FString, TArray<FAnimationKeyframe>>& OutControlKeyframeData);
};
