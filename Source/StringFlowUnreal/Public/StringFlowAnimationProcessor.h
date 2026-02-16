#pragma once

#include "Channels/MovieSceneFloatChannel.h"
#include "CoreMinimal.h"
#include "LevelSequence.h"
#include "StringFlowUnreal.h"
#include "StringFlowAnimationProcessor.generated.h"

/**
 * 弦乐器控制器关键帧结构体
 * 用于存储单个关键帧的位置和旋转数据
 */
USTRUCT(BlueprintType)
struct FStringControlKeyframe {
    GENERATED_BODY()

   public:
    /** 帧编号 */
    UPROPERTY()
    int32 FrameNumber = 0;

    /** 位置向量 */
    UPROPERTY()
    FVector Translation = FVector::ZeroVector;

    /** 旋转四元数 */
    UPROPERTY()
    FQuat Rotation = FQuat::Identity;
};

/**
 * 弦乐器动画处理器
 * 用于处理弦乐器（小提琴）的表演动画和弦振动动画
 *
 * ============================================================
 * 功能概述：
 * ============================================================
 *
 * 1. 从JSON文件加载弦乐器表演动画数据
 * 2. 解析手指控制器、弓控制器等的位置和旋转信息
 * 3. 将数据写入Sequencer的Control Rig轨道
 * 4. 处理旋转数据的欧拉角展开以解决插值跳变问题
 * 5. 清除现有动画关键帧以支持重新生成
 *
 * ============================================================
 * JSON数据格式说明：
 * ============================================================
 *
 * 表演动画格式 (left_hand.json, right_hand.json):
 * {
 *   "frame": 0,
 *   "hand_infos": {
 *     "controller_name": [x, y, z],              // 3维位置
 *     "H_rotation_R": [w, x, y, z],              // 4维四元数旋转
 *     "H_L": [x, y, z],
 *     "H_rotation_L": [w, x, y, z],
 *     "1_L": [x, y, z],                          // 左手1指
 *     "2_L": [x, y, z],                          // 左手2指
 *     "3_L": [x, y, z],                          // 左手3指
 *     "4_L": [x, y, z],                          // 左手4指
 *     "1_R": [x, y, z],                          // 右手1指
 *     "2_R": [x, y, z],                          // 右手2指
 *     "3_R": [x, y, z],                          // 右手3指
 *     "4_R": [x, y, z],                          // 右手4指
 *     "String_Touch_Point": [x, y, z],           // 触弦点
 *     "Bow_Controller": [x, y, z]                // 琴弓控制器
 *   }
 * }
 *
 * 弦振动动画格式 (string_vibration.json):
 * {
 *   "strings": [
 *     {
 *       "shape_key_name": "s0fret2",
 *       "keyframes": [
 *         {"frame": 0, "shape_key_value": 0.5},
 *         {"frame": 10, "shape_key_value": 0.7},
 *         ...
 *       ]
 *     },
 *     ...
 *   ]
 * }
 *
 * ============================================================
 * 控制器列表：
 * ============================================================
 *
 * 左手控制器：
 *   - H_L              右手手掌主控制器
 *   - HP_L             右手手掌轴点控制器
 *   - H_rotation_L     右手手掌旋转控制器
 *   - T_L              右手拇指控制器
 *   - TP_L             右手拇指轴点控制器
 *   - 1_L              右手食指
 *   - 2_L              右手中指
 *   - 3_L              右手无名指
 *   - 4_L              右手小指
 *
 * 右手控制器（弓手）：
 *   - H_R              左手手掌主控制器
 *   - HP_R             左手手掌轴点控制器
 *   - H_rotation_R     左手手掌旋转控制器
 *   - T_R              左手拇指控制器
 *   - TP_R             左手拇指轴点控制器
 *   - 1_R              左手食指
 *   - 2_R              左手中指
 *   - 3_R              左手无名指
 *   - 4_R              左手小指
 *
 * 其他控制器：
 *   - String_Touch_Point  触弦点（弦的接触位置）
 *   - Bow_Controller      琴弓主控制器
 *
 * ============================================================
 * 工作流程：
 * ============================================================
 *
 * 1. 初始化：
 *    - 调用 GenerateAllAnimation(StringFlowActor)
 *    - 该方法会自动找到配置文件并解析所有动画路径
 *
 * 2. 生成演奏动画：
 *    - 调用 MakeStringAnimation(StringFlowActor, AnimationFilePath)
 *    - 读取JSON文件 -> 解析帧数据 -> 收集关键帧 -> 批量写入Sequencer
 *
 * 3. 生成弦振动动画：
 *    - 调用 GenerateStringVibrationAnimation(StringFlowActor,
 * VibrationFilePath)
 *    - 读取弦振动JSON -> 为每根弦生成Shape Key动画
 *
 * 4. 清除动画：
 *    - 调用 ClearStringControlRigKeyframes(LevelSequence, ControlRig,
 * StringFlowActor)
 *    - 清除Control Rig轨道上的所有关键帧
 *
 * ============================================================
 * 关键技术点：
 * ============================================================
 *
 * 1. 四元数到欧拉角转换：
 *    - 使用 FRotator = FQuat.Rotator() 进行转换
 *    - 将四元数 (w, x, y, z) 转换为欧拉角 (Pitch, Yaw, Roll)
 *    - 注意：X->Roll, Y->Pitch, Z->Yaw 的映射
 *
 * 2. 欧拉角展开（Unwrapping）：
 *    - 解决旋转通道插值时的360度跳变问题
 *    - 对每个旋转轴分别进行展开处理
 *    - 确保角度序列的连续性
 *
 * 3. 帧率转换：
 *    - JSON中的帧数基于源文件的帧率
 *    - Sequencer中的帧数基于 TickResolution
 *    - 转换公式：ScaledFrame = Frame * TickResolution.Num * DisplayRate.Den /
 * (TickResolution.Den * DisplayRate.Num)
 *
 * 4. 关键帧批量写入：
 *    - 使用 AddKeys() 而不是逐个 SetKeyTime/SetKeyValue
 *    - 一次性提交所有关键帧以提高性能
 *
 * ============================================================
 */
UCLASS()
class STRINGFLOWUNREAL_API UStringFlowAnimationProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 生成弦乐器表演动画
     *
     * 流程：
     * 1. 从 StringFlowActor 的配置文件中提取动画路径
     * 2. 调用 MakeStringAnimation() 生成动画
     *
     * @param StringFlowActor 弦乐器Actor实例
     * @return 无
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Animation Processor")
    static void GeneratePerformerAnimation(AStringFlowUnreal* StringFlowActor);

    /**
     * 生成乐器动画
     *
     * 流程：
     * 1. 从 StringFlowActor 的配置文件中提取动画路径
     * 2. 调用 MakeInstrumentAnimation() 生成动画
     *
     * @param StringFlowActor 弦乐器Actor实例
     * @return 无
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Animation Processor")
    static void GenerateInstrumentAnimation(AStringFlowUnreal* StringFlowActor);

    /**
     * 生成乐器材质动画
     *
     * 流程：
     * 1. 解析乐器动画数据
     * 2. 为材质参数创建动画轨道
     * 3. 写入材质参数关键帧
     *
     * @param StringFlowActor 弦乐器Actor实例
     * @param InstrumentAnimationDataPath 乐器动画数据JSON文件路径
     * @return 无
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Animation Processor")
    static void GenerateInstrumentMaterialAnimation(
        AStringFlowUnreal* StringFlowActor,
        const FString& InstrumentAnimationDataPath);

    /**
     * 生成所有动画
     *
     * 流程：
     * 1. 解析配置文件获取所有动画路径
     * 2. 依次调用各个动画生成函数
     * 3. 输出完整的动画生成报告
     *
     * @param StringFlowActor 弦乐器Actor实例
     * @return 无
     *
     * @note 会调用 GeneratePerformerAnimation 和 GenerateInstrumentAnimation
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Animation Processor")
    static void GenerateAllAnimation(AStringFlowUnreal* StringFlowActor);

    /**
     * 解析StringFlow配置文件
     *
     * 流程：
     * 1. 读取StringFlowActor的AnimationFilePath指向的JSON配置文件
     * 2. 解析JSON对象获取各种动画路径
     * 3. 返回解析结果
     *
     * @param StringFlowActor 弦乐器Actor实例
     * @param OutLeftHandAnimationPath [out] 左手动画文件路径
     * @param OutRightHandAnimationPath [out] 右手动画文件路径
     * @param OutStringVibrationPath [out] 弦振动动画文件路径
     * @return 解析是否成功
     *
     * @note 如果某个路径不存在，对应的输出参数将为空字符串
     */
    static bool ParseStringFlowConfigFile(AStringFlowUnreal* StringFlowActor,
                                          FString& OutLeftHandAnimationPath,
                                          FString& OutRightHandAnimationPath,
                                          FString& OutStringVibrationPath);

   private:
    /**
     * 从JSON文件生成弦乐器表演动画
     *
     * 详细流程：
     * 1. 读取JSON动画文件
     * 2. 解析JSON数组，获取每帧的hand_infos数据
     * 3. 验证并收集所有控制器的关键帧数据
     * 4. 清除现有的动画关键帧
     * 5. 批量写入新的关键帧到Control Rig轨道
     * 6. 刷新Sequencer显示
     *
     * @param StringFlowActor 弦乐器Actor实例
     * @param AnimationFilePath 动画JSON文件路径（left_hand.json 或
     * right_hand.json）
     * @param LevelSequence Level Sequence实例
     * @return 无
     *
     * @note 会自动处理四元数旋转和欧拉角展开
     * @note 支持自动帧率转换
     */
    static void MakeStringAnimation(AStringFlowUnreal* StringFlowActor,
                                    const FString& AnimationFilePath,
                                    ULevelSequence* LevelSequence);

    /**
     * 从JSON文件生成演奏者动画
     *
     * 详细流程：
     * 1. 读取JSON动画文件
     * 2. 解析JSON数组，获取每帧的hand_infos数据
     * 3. 验证并收集所有控制器的关键帧数据
     * 4. 清除现有的动画关键帧
     * 5. 批量写入新的关键帧到Control Rig轨道
     * 6. 刷新Sequencer显示
     *
     * @param StringFlowActor 弦乐器Actor实例
     * @param AnimationFilePath 动画JSON文件路径（left_hand.json 或
     * right_hand.json）
     * @param LevelSequence Level Sequence实例
     * @return 无
     *
     * @note 操作的是演奏者模型(SkeletalMeshActor)的Control Rig
     * @note 会自动处理四元数旋转和欧拉角展开
     * @note 支持自动帧率转换
     */
    static void MakePerformerAnimation(AStringFlowUnreal* StringFlowActor,
                                       const FString& AnimationFilePath,
                                       ULevelSequence* LevelSequence);
};
