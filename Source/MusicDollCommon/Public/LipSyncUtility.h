#pragma once

#include "CoreMinimal.h"
#include "InstrumentMorphTargetUtility.h"
#include "LipSyncTypes.h"
#include "UObject/NoExportTypes.h"
#include "LipSyncUtility.generated.h"

class UControlRigBlueprint;
class ASkeletalMeshActor;
class ULevelSequence;

/**
 * Lip Sync 工具类
 * 提供口型映射管理、JSON解析、Control Rig Channel创建、关键帧写入等通用功能
 *
 * 参照：BoneControlMappingUtility（映射变量管理）、InstrumentMorphTargetUtility（CR操作）
 */
UCLASS()
class MUSICDOLLCOMMON_API ULipSyncUtility : public UObject {
    GENERATED_BODY()

   public:
    // ===== 映射表管理（参照 BoneControlMappingUtility）=====

    /**
     * 为 ControlRigBlueprint 添加 LipSyncMapping 数组变量
     * @param ControlRigBlueprint 目标 ControlRigBlueprint
     * @return 是否成功添加
     */
    static bool AddLipSyncMappingVariable(
        UControlRigBlueprint* ControlRigBlueprint);

    /**
     * 从 ControlRigBlueprint 读取 LipSyncMapping 变量
     * @param ControlRigBlueprint 目标 ControlRigBlueprint
     * @param OutMapping 输出的映射数组
     * @return 是否成功读取
     */
    static bool GetLipSyncMapping(UControlRigBlueprint* ControlRigBlueprint,
                                  TArray<FLipSyncMappingPair>& OutMapping);

    /**
     * 保存 LipSyncMapping 到 ControlRigBlueprint 变量
     * @param ControlRigBlueprint 目标 ControlRigBlueprint
     * @param InMapping 要保存的映射数组
     * @return 是否成功保存
     */
    static bool SetLipSyncMapping(UControlRigBlueprint* ControlRigBlueprint,
                                  const TArray<FLipSyncMappingPair>& InMapping);

    // ===== Control Rig 操作 =====

    /**
     * 初始化 Lip Sync 模块（两层递进逻辑）
     *
     * 第一层：如果 LipSyncMapping 变量不存在 → 创建变量，提示用户编译后返回。
     * 第二层：如果变量已存在但 lip_sync Control 不存在 → 创建 Control。
     * 如果两者都已存在，则不做任何操作（提示使用 ApplyMappingToRig）。
     *
     * 内部调用：
     *   - AddLipSyncMappingVariable（第一层）
     *   - FControlRigCreationUtility::CreateControl（第二层）
     *
     * @param ControlRigBlueprint 目标 ControlRigBlueprint
     * @return 是否成功
     */
    static bool InitializeLipSyncControl(
        UControlRigBlueprint* ControlRigBlueprint);

    /**
     * 将已保存的 Mapping 应用到 Control Rig
     *
     * 读取 LipSyncMapping 变量中的口型映射数据，
     * 在 lip_sync Control 下创建对应的 Float Animation
     * Channel（已存在的跳过）。 用户在编辑并保存映射表后调用此方法。
     *
     * @param ControlRigBlueprint 目标 ControlRigBlueprint
     * @return 成功创建的 Channel 数量
     */
    static int32 ApplyMappingToRig(UControlRigBlueprint* ControlRigBlueprint);

    // ===== 文件解析 =====

    /**
     * 自动检测文件类型并解析 Lip Sync 文件
     * 支持：.json（Lisa 格式）和 .tsv（Cherry 格式）
     * @param FilePath 文件路径
     * @param OutCues 输出的 mouthCues 数组
     * @param OutDuration 输出的音频总时长
     * @return 是否成功解析
     */
    static bool ParseLipSyncFile(const FString& FilePath,
                                 TArray<FLipSyncMouthCue>& OutCues,
                                 float& OutDuration);

    /**
     * 读取并解析 Lip Sync JSON 文件（Lisa 格式）
     * @param FilePath JSON 文件路径
     * @param OutCues 输出的 mouthCues 数组
     * @param OutDuration 输出的音频总时长
     * @return 是否成功解析
     */
    static bool ParseLipSyncJson(const FString& FilePath,
                                 TArray<FLipSyncMouthCue>& OutCues,
                                 float& OutDuration);

    /**
     * 读取并解析 Lip Sync TSV 文件（Cherry 格式）
     *
     * 格式说明：
     *   - 每行一个制表符分隔的口型切换点：<时间>\t<口型字母>
     *   - 每个 cue 的结束时间由下一行的开始时间决定
     *   - 最后一行使用短暂默认值
     *
     * @param FilePath TSV 文件路径
     * @param OutCues 输出的 mouthCues 数组
     * @param OutDuration 输出的音频总时长
     * @return 是否成功解析
     */
    static bool ParseLipSyncTsv(const FString& FilePath,
                                TArray<FLipSyncMouthCue>& OutCues,
                                float& OutDuration);

    // ===== 关键帧转换 =====

    /**
     * 将 MouthCues 转换为 FMorphTargetKeyframeData
     *
     * 转换策略（详见文档第七节 Q1）：
     *   对于非 X 的 cue {start, end, value}：
     *     - 在映射表中查找 Phoneme → MorphTargetName
     *     - change_duration = (end-start)>=1.0 ? 0.5 : (end-start)/3
     *     - next_change_duration 由下一个非 X cue 的 (end-start) 决定
     *     - 写入四帧：(start-cd):0.0 → start:1.0 → end:1.0 → (end+ncd):0.0
     *   X cue 直接跳过。
     *
     * @param Cues 口型 cue 数组
     * @param Mapping 口型映射表
     * @param TickResolution Tick 分辨率
     * @param DisplayRate 显示帧率
     * @param OutKeyframeData 输出的关键帧数据（按 MorphTarget 分组）
     * @return 是否成功转换
     */
    static bool ConvertCuesToKeyframeData(
        const TArray<FLipSyncMouthCue>& Cues,
        const TArray<FLipSyncMappingPair>& Mapping, FFrameRate TickResolution,
        FFrameRate DisplayRate,
        TArray<FMorphTargetKeyframeData>& OutKeyframeData);

    // ===== 写入 Sequencer（定向清理，不破坏其他轨道）=====

    /**
     * 将 Morph Target 关键帧数据写入 Sequencer 的指定 Control 下。
     *
     * 与 UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig
     * 不同， 此方法不会删除整个 ControlRig Track 的所有 Section，而是：
     *   1. 找到（或创建）ControlRigParameterSection
     *   2. 只清理 RootControlName 下指定名称的 Float Channel 的关键帧
     *   3. 只写入这些 Channel 的新关键帧
     *   4. 更新 Section Range
     *
     * 这样可以保证 Control Rig 上其他轨道的数据（如手部位置、旋转等）不受影响。
     *
     * @param Performer 演奏者 SkeletalMeshActor
     * @param KeyframeData 关键帧数据数组
     * @param LevelSequence 关卡序列
     * @param RootControlName 目标 Control 名称（如 "lip_sync"）
     * @param FramePadding 帧范围填充（显示帧数，默认600帧）
     * @return 成功写入的 Morph Target 数量
     */
    static int32 WriteLipSyncToControlRig(
        ASkeletalMeshActor* Performer,
        const TArray<FMorphTargetKeyframeData>& KeyframeData,
        ULevelSequence* LevelSequence,
        const FString& RootControlName = TEXT("lip_sync"),
        int32 FramePadding = 600);

    // ===== 完整写入流程 =====

    /**
     * 一站式：从口型文件（.json / .tsv）生成 Lip Sync 动画并写入 Sequencer
     *
     * 步骤：
     *   1. ParseLipSyncFile（自动检测 JSON 或 TSV 格式）
     *   2. GetLipSyncMapping
     *   3. ConvertCuesToKeyframeData
     *   4. 调用 WriteLipSyncToControlRig（定向写入，不破坏其他轨道）
     *
     * @param Performer 演奏者 SkeletalMeshActor
     * @param ControlRigBlueprint 演奏者 ControlRigBlueprint（用于读取映射表）
     * @param FilePath 口型文件路径（支持 .json 或 .tsv）
     * @return 成功写入的 Morph Target 数量
     */
    static int32 GenerateLipSyncFromJson(
        ASkeletalMeshActor* Performer,
        UControlRigBlueprint* ControlRigBlueprint, const FString& FilePath);

   private:
    /** Lip Sync 映射变量名常量 */
    static const FName LipSyncMappingVariableName;

    /** Lip Sync Control 名称常量 */
    static const FString LipSyncControlName;

    /**
     * 计算一个 cue 的 change_duration
     * 规则：duration >= 1.0 → 0.5，否则 duration / 3
     */
    static float ComputeChangeDuration(float CueDuration);

    /**
     * 将秒转换为 FrameNumber
     */
    static FFrameNumber SecondsToFrameNumber(float Seconds,
                                             FFrameRate TickResolution,
                                             FFrameRate DisplayRate);
};
