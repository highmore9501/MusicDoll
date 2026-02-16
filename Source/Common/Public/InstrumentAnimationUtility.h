#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Components/ActorComponent.h"
#include "ControlRig.h"
#include "CoreMinimal.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "Sections/MovieSceneComponentMaterialParameterSection.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "UObject/NoExportTypes.h"
#include "InstrumentAnimationUtility.generated.h"

// ========== 数据结构 ==========

/**
 * 通用的动画关键帧结构体
 * 用于存储单个关键帧的位置和旋转数据
 * 适用于所有乐器类型
 */
USTRUCT(BlueprintType)
struct COMMON_API FAnimationKeyframe
{
    GENERATED_BODY()

    /** 帧编号 */
    UPROPERTY()
    int32 FrameNumber;

    /** 位置（平移） */
    UPROPERTY()
    FVector Translation;

    /** 旋转（四元数） */
    UPROPERTY()
    FQuat Rotation;

    FAnimationKeyframe()
        : FrameNumber(0)
        , Translation(FVector::ZeroVector)
        , Rotation(FQuat::Identity)
    {
    }

    FAnimationKeyframe(int32 InFrameNumber, const FVector& InTranslation, const FQuat& InRotation)
        : FrameNumber(InFrameNumber)
        , Translation(InTranslation)
        , Rotation(InRotation)
    {
    }
};

/**
 * 旋转数据容器（带有效标记）
 */
USTRUCT(BlueprintType)
struct COMMON_API FRotationData
{
    GENERATED_BODY()

    UPROPERTY()
    FQuat Rotation;

    UPROPERTY()
    bool bIsValid;

    FRotationData()
        : Rotation(FQuat::Identity)
        , bIsValid(false)
    {
    }

    FRotationData(const FQuat& InRotation, bool bInValid = true)
        : Rotation(InRotation)
        , bIsValid(bInValid)
    {
    }
};

/**
 * 批量插入关键帧的配置参数
 */
struct COMMON_API FBatchInsertKeyframesSettings
{
    /** 帧范围填充（MaxFrame + N） */
    int32 FramePadding;

    /** 特殊控制器处理规则：控制器名称前缀 -> 是否只插入X轴位置 */
    TMap<FString, bool> SpecialControllerRules;

    /** 是否启用旋转插值优化 */
    bool bUnwrapRotationInterpolation;

    FBatchInsertKeyframesSettings()
        : FramePadding(1)
        , bUnwrapRotationInterpolation(true)
    {
    }
};

/**
 * 材质参数关键帧数据
 * 用于批量写入材质参数动画
 */
USTRUCT(BlueprintType)
struct COMMON_API FMaterialParameterKeyframeData
{
    GENERATED_BODY()

    /** 参数名称（如 "Pressed", "Vibration"） */
    UPROPERTY()
    FString ParameterName;

    /** 关键帧位置 */
    UPROPERTY()
    TArray<FFrameNumber> FrameNumbers;

    /** 关键帧值 */
    UPROPERTY()
    TArray<float> Values;

    FMaterialParameterKeyframeData()
        : ParameterName(TEXT(""))
    {
    }

    FMaterialParameterKeyframeData(const FString& InParameterName)
        : ParameterName(InParameterName)
    {
    }
};

// ========== 通用动画处理工具类 ==========

/**
 * 乐器动画处理工具类
 * 提供与 Sequencer 关键帧处理相关的通用功能
 * 包含旋转处理、通道管理、Component Material Track管理等与乐器类型无关的通用算法
 */
UCLASS()
class COMMON_API UInstrumentAnimationUtility : public UObject
{
    GENERATED_BODY()

public:
    // ===== Sequencer 集成 =====

    /**
     * 获取当前打开的 Level Sequence 和 Sequencer
     * @param OutLevelSequence 输出：Level Sequence
     * @param OutSequencer 输出：Sequencer
     * @return 是否成功获取
     */
    static bool GetActiveLevelSequenceAndSequencer(
        ULevelSequence*& OutLevelSequence,
        TSharedPtr<ISequencer>& OutSequencer);

    // ===== Control Rig 轨道操作 =====

    /**
     * 通用的关键帧批量插入方法
     * @param LevelSequence Level Sequence
     * @param ControlRigInstance Control Rig
     * @param ControlKeyframeData 关键帧数据
     * @param Settings 设置参数
     */
    static void BatchInsertControlRigKeys(
        ULevelSequence* LevelSequence,
        UControlRig* ControlRigInstance,
        const TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
        const FBatchInsertKeyframesSettings& Settings = FBatchInsertKeyframesSettings());

    /**
     * 清空 Control Rig 轨道上的所有关键帧
     * @param LevelSequence Level Sequence
     * @param ControlRigInstance Control Rig
     * @param ControlNamesToClean 要清理的控制器名称集合
     */
    static void ClearControlRigKeyframes(
        ULevelSequence* LevelSequence,
        UControlRig* ControlRigInstance,
        const TSet<FString>& ControlNamesToClean);

    /**
     * 验证是否存在重复的Control Rig参数轨道
     * 检测并自动修复重复的轨道问题
     *
     * @param LevelSequence Level Sequence对象
     * @param ControlRigInstance Control Rig实例
     * @param bAutoFix 是否自动修复（保留第一个，删除重复的）
     * @return 如果检测到重复轨道返回true
     */
    static bool ValidateNoExistingTracks(
        ULevelSequence* LevelSequence,
        UControlRig* ControlRigInstance,
        bool bAutoFix = true);

    // ===== Component Material Track 管理 ===== (新增)

    /**
     * 查找或创建Component Material Track
     * 
     * 在指定的Level Sequence中查找匹配的Component Material Track，
     * 如果不存在则创建新的。
     * 
     * @param LevelSequence Level Sequence
     * @param ObjectBindingID 绑定的对象GUID（通常是SkeletalMeshComponent的绑定）
     * @param MaterialSlotIndex 材质槽索引
     * @param MaterialSlotName 材质槽名称（可选，用于更精确的匹配）
     * @return Component Material Track，失败返回nullptr
     * 
     * @note 优先通过MaterialSlotName匹配，如果未提供则使用MaterialSlotIndex
     * @note 新创建的Track会自动设置DisplayName为 "CM_{SlotIndex}_{SlotName}"
     */
    static UMovieSceneComponentMaterialTrack* FindOrCreateComponentMaterialTrack(
        ULevelSequence* LevelSequence,
        const FGuid& ObjectBindingID,
        int32 MaterialSlotIndex,
        FName MaterialSlotName = NAME_None);

    /**
     * 添加材质参数到Track
     * 
     * 在Track的Section中添加标量材质参数，如果参数已存在则跳过。
     * 
     * @param Track Component Material Track
     * @param ParameterName 参数名称（如 "Pressed", "Vibration"）
     * @param InitialValue 初始值
     * @return 是否成功添加（如果参数已存在也返回true)
     * 
     * @note 如果Track没有Section，会自动创建
     * @note 初始关键帧添加在Frame 0
     */
    static bool AddMaterialParameter(
        UMovieSceneComponentMaterialTrack* Track,
        const FString& ParameterName,
        float InitialValue = 0.0f);

    /**
     * 批量写入材质参数关键帧
     * 
     * 将多个材质参数的关键帧数据批量写入到Section中。
     * 
     * @param Section 材质参数Section
     * @param KeyframeData 关键帧数据数组
     * @return 成功写入的参数数量
     * 
     * @note 每个KeyframeData中的FrameNumbers和Values数组长度必须相同
     * @note 使用AddScalarParameterKey逐个写入关键帧
     * 
     * @warning Section必须是UMovieSceneComponentMaterialParameterSection类型
     */
    static int32 WriteMaterialParameterKeyframes(
        UMovieSceneComponentMaterialParameterSection* Section,
        const TArray<FMaterialParameterKeyframeData>& KeyframeData);

    // ===== 绑定查找与管理 =====

    /**
     * 查找SkeletalMeshActor在Level Sequence中的绑定ID
     * 
     * 遍历MovieScene的所有绑定，使用Sequencer的FindBoundObjects方法
     * 查找指定的SkeletalMeshActor对应的绑定GUID。
     * 
     * @param Sequencer Sequencer实例
     * @param LevelSequence Level Sequence
     * @param SkeletalMeshActor 要查找的SkeletalMeshActor
     * @return 绑定GUID，未找到返回无效GUID
     * 
     * @note 用于定位已在Level Sequence中存在的Actor绑定
     */
    static FGuid FindSkeletalMeshActorBinding(
        TSharedPtr<ISequencer> Sequencer,
        ULevelSequence* LevelSequence,
        ASkeletalMeshActor* SkeletalMeshActor);

    /**
     * 获取或创建组件的Sequencer绑定ID
     * 
     * 使用ISequencer::GetHandleToObject获取或创建组件的绑定GUID。
     * 
     * @param Sequencer Sequencer实例
     * @param Component 组件（如SkeletalMeshComponent）
     * @param bCreateIfNotFound 如果不存在是否创建
     * @return 绑定GUID，失败返回无效GUID
     * 
     * @warning 确保Sequencer有效且Component已添加到Level Sequence中
     */
    static FGuid GetOrCreateComponentBinding(
        TSharedPtr<ISequencer> Sequencer,
        UActorComponent* Component,
        bool bCreateIfNotFound = true);

    // ===== Section 管理 ===== (新增)

    /**
     * 清理Track的所有Section并创建新的空Section
     * 
     * 删除Track上的所有现有Section，然后创建一个新的空Section。
     * 用于在写入新动画数据前清除旧数据。
     * 
     * @param Track 要重置的Track
     * @return 新创建的Section，失败返回nullptr
     * 
     * @note 此操作不可逆，会丢失所有现有的Section数据
     */
    static UMovieSceneSection* ResetTrackSections(UMovieSceneTrack* Track);

    /**
     * 清理SkeletalMeshActor上的所有动画轨道
     * 
     * 清理指定SkeletalMeshActor的Control Rig轨道和材质参数轨道的所有Sections。
     * 这是一个通用方法，适用于所有乐器类型。
     * 
     * @param SkeletalMeshActor 要清理的SkeletalMeshActor
     * @return 是否成功清理（如果没有打开LevelSequence会返回false但不报错）
     * 
     * @note 此方法会：
     *       1. 清理Control Rig Parameter Track上的所有Sections
     *       2. 清理Component Material Track上的所有Sections
     *       3. 自动标记LevelSequence为已修改
     * 
     * @note 需要当前打开LevelSequence才能执行清理操作
     */
    static bool CleanupInstrumentAnimationTracks(ASkeletalMeshActor* SkeletalMeshActor);

    // ===== Float Channel 操作 =====

    /**
     * 在MovieScene Section中查找指定名称的浮点通道
     * 用于快速定位位置和旋转等浮点类型的动画通道
     *
     * @param Section 要搜索的MovieScene Section
     * @param ChannelName 通道名称（如"H_L.Location.X"）
     * @return 找到的通道指针，如果未找到则返回nullptr
     *
     * @note 如果通道未找到，会自动输出可用的所有通道名称用于调试
     */
    static FMovieSceneFloatChannel* FindFloatChannel(
        UMovieSceneSection* Section, const FString& ChannelName);

    /**
     * 输出Section中所有可用的通道信息用于调试
     * 在开发时用于诊断通道查找问题
     *
     * @param Section 要查询的MovieScene Section
     */
    static void LogAvailableChannels(UMovieSceneSection* Section);

    // ===== 旋转处理 =====

    /**
     * 展开旋转序列，解决欧拉角插值跳变问题
     * 通过计算相邻帧间的最短角度差，确保旋转值的连续性
     * 避免从359度到1度这样的跳变
     *
     * @param RotationValues 待处理的旋转值数组
     */
    static void UnwrapRotationSequence(TArray<FMovieSceneFloatValue>& RotationValues);

    /**
     * 批量处理所有旋转通道的展开
     * 同时处理X、Y、Z三个旋转轴的欧拉角连续性问题
     *
     * @param RotationXValues X轴旋转值数组
     * @param RotationYValues Y轴旋转值数组
     * @param RotationZValues Z轴旋转值数组
     */
    static void ProcessRotationChannelsUnwrap(
        TArray<FMovieSceneFloatValue>& RotationXValues,
        TArray<FMovieSceneFloatValue>& RotationYValues,
        TArray<FMovieSceneFloatValue>& RotationZValues);

    // ===== 控制器验证 =====

    /**
     * 验证控制器名称
     * @param ControlName 要验证的控制器名称
     * @param ValidNames 有效名称集合
     * @param ErrorLogPrefix 错误日志前缀
     * @return 如果有效返回原名称，否则返回空字符串
     */
    static FString ValidateControllerName(
        const FString& ControlName,
        const TSet<FString>& ValidNames,
        const FString& ErrorLogPrefix = TEXT(""));

    // ===== JSON 控件容器处理（真正通用的方法）=====

    /**
     * 处理控件数据容器中的所有控件（通用方法）
     * 此方法不假设 JSON 的路径结构，直接处理控件数据容器
     * 
     * @param ControlsContainer 控件数据容器的 JSON 对象（如 hand_infos, instrument_data 等）
     * @param FrameNumber 当前帧编号
     * @param ControlKeyframeData 输出：关键帧数据Map
     * @param ValidControllerNames 有效控制器名称集合
     * @param OutKeyframesAdded 输出：添加的关键帧数
     * 
     * @note 各乐器模块需要自己从 FrameObject 中提取控件容器，然后传入此方法
     */
    static void ProcessControlsContainer(
        TSharedPtr<FJsonObject> ControlsContainer,
        int32 FrameNumber,
        TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
        const TSet<FString>& ValidControllerNames,
        int32& OutKeyframesAdded);

    /**
     * 提前提取旋转数据（通用方法）
     * 从控件容器中提取 H_rotation_L 和 H_rotation_R
     * 
     * @param ControlsContainer 控件数据容器
     * @param OutRotations 输出：旋转数据Map（"H_L" -> FRotationData）
     */
    static void ExtractRotationData(
        TSharedPtr<FJsonObject> ControlsContainer,
        TMap<FString, FRotationData>& OutRotations);
};

// ========== 兼容性别名（向后兼容） ==========

/**
 * FInstrumentAnimationUtility 类
 * 提供静态方法的向后兼容包装
 */
class COMMON_API FInstrumentAnimationUtility
{
public:
    static void UnwrapRotationSequence(TArray<FMovieSceneFloatValue>& RotationValues)
    {
        UInstrumentAnimationUtility::UnwrapRotationSequence(RotationValues);
    }

    static void ProcessRotationChannelsUnwrap(
        TArray<FMovieSceneFloatValue>& RotationXValues,
        TArray<FMovieSceneFloatValue>& RotationYValues,
        TArray<FMovieSceneFloatValue>& RotationZValues)
    {
        UInstrumentAnimationUtility::ProcessRotationChannelsUnwrap(
            RotationXValues, RotationYValues, RotationZValues);
    }

    static FMovieSceneFloatChannel* FindFloatChannel(
        UMovieSceneSection* Section, const FString& ChannelName)
    {
        return UInstrumentAnimationUtility::FindFloatChannel(Section, ChannelName);
    }

    static bool ValidateNoExistingTracks(
        ULevelSequence* LevelSequence,
        UControlRig* ControlRigInstance,
        bool bAutoFix = true)
    {
        return UInstrumentAnimationUtility::ValidateNoExistingTracks(
            LevelSequence, ControlRigInstance, bAutoFix);
    }

    static void LogAvailableChannels(UMovieSceneSection* Section)
    {
        UInstrumentAnimationUtility::LogAvailableChannels(Section);
    }
};
