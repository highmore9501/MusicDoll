#pragma once

#include "CoreMinimal.h"
#include "LevelSequence.h"
#include "StringFlowUnreal.h"
#include "Templates/Tuple.h"
#include "UObject/NoExportTypes.h"
#include "StringFlowMusicInstrumentProcessor.generated.h"

// 前置声明
class AStringFlowUnreal;
class UControlRigBlueprint;
class USkeletalMeshComponent;
class UMaterialInterface;
struct FMovieSceneFloatValue;
struct FFrameNumber;

/**
 * 弦乐器处理器类，用于处理与弦乐器相关的动画和材质操作
 */
UCLASS()
class STRINGFLOWUNREAL_API UStringFlowMusicInstrumentProcessor
    : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 初始化弦乐器
     * 完整的乐器初始化流程，包括材质、Control Rig通道和动画轨道的初始化
     * @param StringFlowActor StringFlowUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Music Processor")
    static void InitializeStringInstrument(AStringFlowUnreal* StringFlowActor);

    /**
     * 初始化弦乐器弦的材质
     * 为每根弦创建独立的材质实例，用于后续morph target动画驱动
     * @param StringFlowActor StringFlowUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Music Processor")
    static void InitializeStringMaterials(AStringFlowUnreal* StringFlowActor);

    /**
     * 初始化弦材质参数动画轨道
     * 在Level Sequencer中为每根弦创建材质参数轨道，用于记录演奏强度等参数
     * @param StringFlowActor StringFlowUnreal 实例
     * @return 成功初始化的材质参数轨道数量
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Music Processor")
    static int32 InitializeStringMaterialAnimationTracks(
        AStringFlowUnreal* StringFlowActor);

    /**
     * 初始化弦振动动画轨道（Morph Target）
     * 在Control Rig中为每根弦创建振动方向的动画通道
     * @param StringFlowActor StringFlowUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Music Processor")
    static void InitializeStringVibrationAnimationChannels(
        AStringFlowUnreal* StringFlowActor);

    /**
     * 生成乐器动画
     * 使用新的Morph Target生成方法替代旧的弦振动动画方法
     * @param StringFlowActor StringFlowUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Music Processor")
    static void GenerateInstrumentAnimation(AStringFlowUnreal* StringFlowActor);

    /**
     * 生成乐器材质动画
     * @param StringFlowActor StringFlowUnreal 实例
     * @param InstrumentAnimationDataPath 乐器动画数据JSON文件路径
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Music Processor")
    static void GenerateInstrumentMaterialAnimation(
        AStringFlowUnreal* StringFlowActor,
        const FString& InstrumentAnimationDataPath);

    /**
     * 从JSON加载弦振动数据并生成动画
     * 解析弦振动数据文件，将数据写入Control Rig的动画通道
     * 内部方法，不暴露给蓝图（因为返回值类型过于复杂）
     * @param StringFlowActor StringFlowUnreal 实例
     * @param StringVibrationDataPath 弦振动数据JSON文件路径
     * @param OutVibrationKeyframeData 输出的弦振动关键帧数据
     * @return 操作是否成功
     */
    static bool LoadAndGenerateStringVibrationAnimation(
        AStringFlowUnreal* StringFlowActor,
        const FString& StringVibrationDataPath,
        TMap<FString, TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
            OutVibrationKeyframeData);

    /**
     * 将振动数据同步写入材质动画轨道
     * 根据振动数据生成材质参数动画，用于可视化演奏效果
     * 内部方法，不暴露给蓝图
     * @param StringFlowActor StringFlowUnreal 实例
     * @param LevelSequence Level Sequence
     * @param VibrationKeyframeData 弦振动关键帧数据
     * @param MinFrame 最小帧数
     * @param MaxFrame 最大帧数
     * @return 成功写入的材质参数轨道数量
     */
    static int32 SyncVibrationToMaterialAnimation(
        AStringFlowUnreal* StringFlowActor, ULevelSequence* LevelSequence,
        const TMap<FString,
                   TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
            VibrationKeyframeData,
        FFrameNumber MinFrame, FFrameNumber MaxFrame);

   private:
    /**
     * 清理现有的弦乐器动画数据
     * 在初始化前清除Control Rig轨道和材质轨道上的旧数据
     * @param StringFlowActor StringFlowUnreal 实例
     */
    static void CleanupExistingStringAnimations(AStringFlowUnreal* StringFlowActor);
};
