#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "MovieSceneSection.h"
#include "Rigs/RigHierarchy.h"
#include "UObject/NoExportTypes.h"
#include "InstrumentMorphTargetUtility.generated.h"

// 前置声明
class USkeletalMeshComponent;
class UControlRigBlueprint;
class UMovieSceneSection;
struct FRigElementKey;

/**
 * Morph Target关键帧数据
 * 用于批量写入Morph Target动画
 */
USTRUCT(BlueprintType)
struct COMMON_API FMorphTargetKeyframeData {
    GENERATED_BODY()

    /** Morph Target名称（如 "key_75_pressed"） */
    UPROPERTY()
    FString MorphTargetName;

    /** 关键帧位置 */
    UPROPERTY()
    TArray<FFrameNumber> FrameNumbers;

    /** 关键帧值（范围通常0-1） */
    UPROPERTY()
    TArray<float> Values;

    FMorphTargetKeyframeData() : MorphTargetName(TEXT("")) {}

    FMorphTargetKeyframeData(const FString& InMorphTargetName)
        : MorphTargetName(InMorphTargetName) {}
};

/**
 * 乐器Morph Target工具类
 * 处理Morph Target名称获取、Animation Channel管理、JSON解析等
 *
 * 主要功能:
 * - 从骨骼网格获取Morph Target名称
 * - 确保Root Control存在并创建
 * - 批量添加Animation Channels
 * - 解析JSON中的Morph Target数据
 * - 批量写入Morph Target关键帧
 *
 * 使用示例:
 * @code
 * // 获取Morph Target名称
 * TArray<FString> MorphTargetNames;
 * UInstrumentMorphTargetUtility::GetMorphTargetNames(SkeletalMeshComp,
 * MorphTargetNames);
 *
 * // 确保Root Control存在
 * UInstrumentMorphTargetUtility::EnsureRootControlExists(
 *     ControlRigBlueprint, TEXT("piano_key_root")
 * );
 *
 * // 添加Animation Channels
 * FRigElementKey ParentKey(TEXT("piano_key_root"), ERigElementType::Control);
 * UInstrumentMorphTargetUtility::AddAnimationChannels(
 *     ControlRigBlueprint, ParentKey, MorphTargetNames
 * );
 * @endcode
 */
UCLASS()
class COMMON_API UInstrumentMorphTargetUtility : public UObject {
    GENERATED_BODY()

   public:
    // ========== Morph Target信息获取 ==========

    /**
     * 从骨骼网格获取所有Morph Target名称
     *
     * @param SkeletalMeshComp 骨骼网格组件
     * @param OutNames 输出：Morph Target名称数组
     * @return 是否成功获取（至少有一个Morph Target）
     *
     * @note 如果骨骼网格没有Morph Target，返回false
     */
    static bool GetMorphTargetNames(USkeletalMeshComponent* SkeletalMeshComp,
                                    TArray<FString>& OutNames);

    // ========== Control Rig管理 ==========

    /**
     * 确保Root Control存在
     *
     * 检查指定名称的Control是否存在，如果不存在则创建。
     *
     * @param ControlRigBlueprint Control Rig蓝图
     * @param RootControlName Root Control名称（如 "piano_key_root"）
     * @param ControlType Control类型（默认Transform）
     * @return 是否成功（已存在或创建成功）
     *
     * @note 新创建的Control会使用Cube形状
     * @note Initial Transform为Identity
     */
    static bool EnsureRootControlExists(
        UControlRigBlueprint* ControlRigBlueprint,
        const FString& RootControlName,
        ERigControlType ControlType = ERigControlType::Transform);

    /**
     * 批量添加Animation Channels
     *
     * 在指定的父Control下批量创建Animation Channels。
     * 如果通道已存在则跳过。
     *
     * @param ControlRigBlueprint Control Rig蓝图
     * @param ParentControl 父Control元素Key
     * @param ChannelNames 要添加的通道名称数组
     * @param ChannelType 通道类型（默认Float）
     * @return 成功添加的通道数量（已存在的也计入）
     *
     * @note 通道的DisplayName与通道名称相同
     * @note 如果通道已存在且是Animation Channel，会被计入成功数
     */
    static int32 AddAnimationChannels(
        UControlRigBlueprint* ControlRigBlueprint,
        const FRigElementKey& ParentControl,
        const TArray<FString>& ChannelNames,
        ERigControlType ChannelType = ERigControlType::Float);

    // ========== JSON数据解析 ==========

    /**
     * 从已解析的KeyDataArray处理Morph Target关键帧数据
     *
     * 这是通用方法，用于处理JSON解析后的数据。
     * 不同的乐器需要各自实现JSON读取和初步解析的逻辑。
     *
     * @param KeyDataArray 已解析的JSON数据数组，每项为：{shape_key_name,
     * keyframes: [{frame, shape_key_value}]}
     * @param OutKeyframeData 输出：关键帧数据数组
     * @param TickResolution Tick分辨率（用于帧数转换）
     * @param DisplayRate 显示帧率（用于帧数转换）
     * @return 是否成功处理（至少有一个Morph Target数据）
     *
     * @note 帧数转换公式: ScaledFrame = Frame * TickRes.Num * DisplayRate.Den /
     * (TickRes.Den * DisplayRate.Num)
     * @note 如果同一个Morph Target在数据中出现多次，关键帧会被追加（不会覆盖）
     */
    static bool ProcessMorphTargetKeyframeData(
        const TArray<TSharedPtr<FJsonValue>>& KeyDataArray,
        TArray<FMorphTargetKeyframeData>& OutKeyframeData,
        FFrameRate TickResolution, FFrameRate DisplayRate);

    /**
     * 批量写入Morph Target关键帧到Control Rig Section
     *
     * 将关键帧数据写入到Control Rig的Float Channels中。
     *
     * @param Section Control Rig Section
     * @param KeyframeData 关键帧数据数组
     * @return 成功写入的Morph Target数量
     *
     * @note 每个KeyframeData中的FrameNumbers和Values数组长度必须相同
     * @note 使用FloatChannel->AddKeys批量写入关键帧
     * @note 如果找不到对应的通道，会输出警告并跳过
     *
     * @warning Section必须属于Control Rig Parameter Track
     */
    static int32 WriteMorphTargetKeyframes(
        UMovieSceneSection* Section,
        const TArray<FMorphTargetKeyframeData>& KeyframeData);

    /**
     * 通用的Morph Target动画写入完整流程
     *
     * 包含以下步骤：
     * 1. 获取ControlRig信息（Instance, Blueprint, Hierarchy）
     * 2. 检查Root Control是否存在
     * 3. 查找或创建Control Rig轨道和Section
     * 4. 计算帧数范围
     * 5. 写入关键帧数据
     * 6. 更新Section范围
     * 7. 标记为修改并刷新
     *
     * @param Instrument 乐器骨骼网格组件
     * @param KeyframeData 关键帧数据数组
     * @param LevelSequence 关卡序列
     * @param RootControlName Root Control名称 (如 "piano_key_root" 或
     * "violin_root")
     * @return 成功写入的Morph Target数量，失败返回0
     */
    static int32 WriteMorphTargetAnimationToControlRig(
        class ASkeletalMeshActor* Instrument,
        const TArray<FMorphTargetKeyframeData>& KeyframeData,
        class ULevelSequence* LevelSequence,
        const FString& RootControlName = TEXT("piano_key_root"));
};
