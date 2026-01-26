#pragma once

#include "CoreMinimal.h"
#include "KeyRippleUnreal.h"
#include "LevelSequence.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "UObject/NoExportTypes.h"
#include "KeyRipplePianoProcessor.generated.h"

// 前置声明
class AKeyRippleUnreal;
class UControlRigBlueprint;
class USkeletalMeshComponent;
class UMaterialInterface;
struct FMovieSceneFloatValue;
struct FFrameNumber;

/**
 * 钢琴处理器类，用于处理与钢琴相关的操作
 */
UCLASS()
class KEYRIPPLEUNREAL_API UKeyRipplePianoProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 初始化钢琴
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Piano Processor")
    static void InitPiano(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 更新钢琴材质
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Piano Processor")
    static void UpdatePianoMaterials(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 在 Level Sequencer 中为钢琴生成 Morph Target 动画
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param PianoKeyAnimationPath 钢琴键动画路径
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Piano Processor")
    static void GenerateMorphTargetAnimationInLevelSequencer(
        AKeyRippleUnreal* KeyRippleActor, const FString& PianoKeyAnimationPath);

    /**
     * 初始化钢琴键 Control Rig
     * 从钢琴的 Morph Target 中提取名称，在 Control Rig 的 Root Control
     * 上创建动画通道
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Piano Processor")
    static void InitPianoKeyControlRig(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 初始化钢琴材质参数轨道
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @return 成功初始化的材质参数轨道数量
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Piano Processor")
    static int32 InitPianoMaterialParameterTracks(
        AKeyRippleUnreal* KeyRippleActor);

    /**
     * 获取钢琴骨骼网格中的 Morph Target 名称
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param OutMorphTargetNames 输出的 Morph Target 名称数组
     * @return 是否成功获取
     */
    static bool GetPianoMorphTargetNames(AKeyRippleUnreal* KeyRippleActor,
                                         TArray<FString>& OutMorphTargetNames);

    /**
     * 在 Control Rig 中创建或获取 Root Control
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param ControlRigBlueprint Control Rig Blueprint
     * @return Root Control 是否存在或成功创建
     */
    static bool EnsureRootControlExists(
        AKeyRippleUnreal* KeyRippleActor,
        UControlRigBlueprint* ControlRigBlueprint);

    /**
     * 为 Root Control 添加动画通道
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param ControlRigBlueprint Control Rig Blueprint
     * @param AnimationChannelNames 要添加的动画通道名称数组
     * @return 成功添加的通道数量
     */
    static int32 AddAnimationChannelsToRootControl(
        AKeyRippleUnreal* KeyRippleActor,
        UControlRigBlueprint* ControlRigBlueprint,
        const TArray<FString>& AnimationChannelNames);

    /**
     * 检查材质是否具有 Pressed 参数
     * @param Material 要检查的材质接口
     * @return 如果材质有 Pressed 参数则返回 true
     */
    static bool MaterialHasPressedParameter(UMaterialInterface* Material);

    /**
     * 为指定材质槽添加材质参数轨道到 Control Rig
     * @param LevelSequence Level Sequence
     * @param SkeletalMeshComponent 目标骨骼网格组件
     * @param MaterialSlotIndex 材质槽索引
     * @param ParameterName 参数名称
     * @param ObjectBindingID Object Binding ID (来自辅助方法)
     * @return 是否成功创建轨道
     */
    static bool AddMaterialParameterTrackToControlRig(
        ULevelSequence* LevelSequence,
        USkeletalMeshComponent* SkeletalMeshComponent, int32 MaterialSlotIndex,
        const FString& ParameterName, const FGuid& ObjectBindingID);

    /**
     * 在 Level Sequencer 中生成材质参数动画
     * 将 Morph Target 动画数据直接写入对应的材质参数轨道
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param LevelSequence Level Sequence
     * @param MorphTargetKeyframeData Morph Target 关键帧数据 (MorphTargetName -> (FrameNumbers, FloatValues))
     * @param MinFrame 最小帧数
     * @param MaxFrame 最大帧数
     * @return 成功写入的材质参数轨道数量
     */
    static int32 GenerateMaterialAnimationInLevelSequencer(
        AKeyRippleUnreal* KeyRippleActor,
        ULevelSequence* LevelSequence,
        const TMap<FString, TPair<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>& 
            MorphTargetKeyframeData,
        FFrameNumber MinFrame,
        FFrameNumber MaxFrame);
};
