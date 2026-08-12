#pragma once

#include "CoreMinimal.h"
#include "FretDanceUnreal.h"
#include "FretDanceAnimationProcessor.generated.h"

class ULevelSequence;

/**
 * FretDance 动画处理器
 * 用于处理吉他演奏动画的生成
 *
 * ============================================================
 * 功能概述：
 * ============================================================
 *
 * 1. 从 JSON 配置文件读取动画数据路径
 * 2. 解析左手/右手动画 JSON 文件
 * 3. 生成演奏者动画到 Control Rig 轨道
 * 4. 处理旋转数据的欧拉角展开
 * 5. 清除现有动画关键帧以支持重新生成
 *
 * ============================================================
 * JSON 数据格式说明：
 * ============================================================
 *
 * 配置文件格式 (AnimationFilePath):
 * {
 *   "guitar_string_recorder_file": "path/to/string_recorder.json",
 *   "left_hand_animation_file": "path/to/left_hand.json",
 *   "right_hand_animation_file": "path/to/right_hand.json"
 * }
 *
 * 演奏动画格式 (left_hand.json / right_hand.json):
 * [
 *   {
 *     "frame": 0.0,
 *     "fingerInfos": {
 *       "H_L": [x, y, z],              // 3 维位置
 *       "H_rotation_L": [w, x, y, z],  // 4 维四元数
 *       "HP_L": [x, y, z],
 *       ...
 *     }
 *   }
 * ]
 *
 * ============================================================
 * 控制器列表：
 * ============================================================
 *
 * 左手控制器：
 *   - H_L              左手手掌主控制器（位置 + 旋转）
 *   - HP_L             左手手掌轴点控制器（位置）
 *   - T_L              左手拇指控制器（位置）
 *   - I_L              左手食指（位置）
 *   - M_L              左手中指（位置）
 *   - R_L              左手无名指（位置）
 *   - P_L              左手小指（位置）
 *
 * 右手控制器：
 *   - H_R              右手手掌主控制器（位置 + 旋转）
 *   - HP_R             右手手掌轴点控制器（位置）
 *   - T_R              右手拇指控制器（位置）
 *   - I_R              右手食指 (仅指弹/Bass，位置)
 *   - M_R              右手中指 (仅指弹/Bass，位置)
 *   - R_R              右手无名指 (仅指弹/Bass，位置)
 *   - P_R              右手小指 (仅指弹/Bass)
 *
 * ============================================================
 * 工作流程：
 * ============================================================
 *
 * 1. 初始化：
 *    - 调用 GenerateAllAnimation(FretDanceActor)
 *    - 该方法会自动找到配置文件并解析所有动画路径
 *
 * 2. 生成演奏动画：
 *    - 调用 MakePerformerAnimation(FretDanceActor, AnimationFilePath)
 *    - 读取 JSON 文件 -> 解析帧数据 -> 收集关键帧 -> 批量写入 Sequencer
 *
 * 3. 生成弦动画:
 *    - 调用 GenerateInstrumentAnimation(FretDanceActor)
 *    - 需要从 string_recorder 文件转换数据格式
 *
 * ============================================================
 */
UCLASS()
class FRETDANCEUNREAL_API UFretDanceAnimationProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 生成演奏者动画（双手）
     *
     * 流程：
     * 1. 从 FretDanceActor 的配置文件中提取动画路径
     * 2. 分别调用 MakePerformerAnimation 生成左手和右手动画
     *
     * @param FretDanceActor 吉他 Actor 实例
     * @return 无
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Animation Processor")
    static void GeneratePerformerAnimation(AFretDanceUnreal* FretDanceActor);

    /**
     * 生成乐器动画（弦振动）
     *
     * @param FretDanceActor 吉他 Actor 实例
     * @return 无
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Animation Processor")
    static void GenerateInstrumentAnimation(AFretDanceUnreal* FretDanceActor);

    /**
     * 生成所有动画
     *
     * 流程：
     * 1. 解析配置文件获取所有动画路径
     * 2. 依次调用各个动画生成函数
     * 3. 输出完整的动画生成报告
     *
     * @param FretDanceActor 吉他 Actor 实例
     * @return 无
     *
     * @note 会调用 GeneratePerformerAnimation 和 GenerateInstrumentAnimation
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Animation Processor")
    static void GenerateAllAnimation(AFretDanceUnreal* FretDanceActor);

   private:
    /**
     * 解析 FretDance 配置文件
     *
     * 流程：
     * 1. 读取 FretDanceActor 的 AnimationFilePath 指向的 JSON 配置文件
     * 2. 解析 JSON 对象获取各种动画路径
     * 3. 返回解析结果
     *
     * @param FretDanceActor 吉他 Actor 实例
     * @param OutLeftHandAnimationPath [out] 左手动画文件路径
     * @param OutRightHandAnimationPath [out] 右手动画文件路径
     * @param OutStringRecorderPath [out] 弦记录器文件路径
     * @param OutControllerRootAnimationPath [out] Controller Root 动画文件路径
     * @param OutActivityCurvePath [out] 活动曲线文件路径
     * @param OutVibratoShapeKeyPath [out] 摇把 shape key 文件路径
     * @return 解析是否成功
     *
     * @note 如果某个路径不存在，对应的输出参数将为空字符串
     */
    static bool ParseFretDanceConfigFile(
        AFretDanceUnreal* FretDanceActor, FString& OutLeftHandAnimationPath,
        FString& OutRightHandAnimationPath, FString& OutStringRecorderPath,
        FString& OutControllerRootAnimationPath, FString& OutActivityCurvePath,
        FString& OutVibratoShapeKeyPath);

    /**
     * 从 JSON 文件生成 controller_root 动画

    /**
     * 从 JSON 文件生成 controller_root 动画
     *
     * @param FretDanceActor 吉他 Actor 实例
     * @param AnimationFilePath controller_root 动画 JSON 文件路径
     * @param LevelSequence Level Sequence 实例
     * @return 无
     */
    static void MakeControllerRootAnimation(AFretDanceUnreal* FretDanceActor,
                                            const FString& AnimationFilePath,
                                            ULevelSequence* LevelSequence);

    /**
     * 从 JSON 文件生成演奏者动画
     *
     * 详细流程：
     * 1. 读取 JSON 动画文件
     * 2. 解析 JSON 数组，获取每帧的 fingerInfos 数据
     * 3. 验证并收集所有控制器的关键帧数据
     * 4. 清除现有的动画关键帧
     * 5. 批量写入新的关键帧到 Control Rig 轨道
     * 6. 刷新 Sequencer 显示
     *
     * @param FretDanceActor 吉他 Actor 实例
     * @param AnimationFilePath 动画 JSON 文件路径
     * @param LevelSequence Level Sequence 实例
     * @return 无
     *
     * @note 会自动处理四元数旋转和欧拉角展开
     * @note 支持自动帧率转换
     */
    static void MakePerformerAnimation(AFretDanceUnreal* FretDanceActor,
                                       const FString& AnimationFilePath,
                                       ULevelSequence* LevelSequence);
};
