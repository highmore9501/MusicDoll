#pragma once

#include "CoreMinimal.h"
#include "Animation/SkeletalMeshActor.h"  // 包含SkeletalMeshActor定义
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "KeyRippleUnreal.h"  // 引入原类的定义
#include "LevelSequence.h"
#include "MovieScene.h"
#include "KeyRippleAnimationProcessor.generated.h"

// 前置声明
class UControlRig;
class AActor;

class UMovieSceneTrack;
class UMovieSceneSection;

/**
 * 动画处理器类，用于处理与动画生成相关的操作
 */
UCLASS()

class KEYRIPPLEUNREAL_API UKeyRippleAnimationProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 制作动画
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param AnimationFilePath 动画文件路径
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Animation Processor")
    static void MakeAnimation(AKeyRippleUnreal* KeyRippleActor,
                              const FString& AnimationFilePath);

    /**
     * 批量插入Control Rig关键帧
     * @param LevelSequence Level Sequence实例
     * @param ControlRigInstance Control Rig实例
     * @param ControlKeyframeData 关键帧数据
     */
    static void BatchInsertControlRigKeys(
        ULevelSequence* LevelSequence, UControlRig* ControlRigInstance,
        const TMap<FString, TArray<FControlKeyframe>>& ControlKeyframeData);

    /**
     * 查找Float通道
     * @param Section Section实例
     * @param ChannelName 通道名称
     * @return 找到的Float通道，如果没找到返回nullptr
     */
    static FMovieSceneFloatChannel* FindFloatChannel(
        UMovieSceneSection* Section, const FString& ChannelName);

    /**
     * 从KeyRipple文件中解析动画路径
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param OutAnimationPath 输出的演奏动画路径
     * @param OutKeyAnimationPath 输出的钢琴键动画路径
     * @return 是否成功解析
     */
    static bool ParseKeyRippleFile(AKeyRippleUnreal* KeyRippleActor,
                                   FString& OutAnimationPath,
                                   FString& OutKeyAnimationPath);

    /**
     * 生成演奏动画
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Animation Processor")
    static void GeneratePerformerAnimation(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 生成钢琴键动画
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param PianoKeyAnimationPath 钢琴键动画路径
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Animation Processor")
    static void GeneratePianoKeyAnimation(AKeyRippleUnreal* KeyRippleActor,
                                          const FString& PianoKeyAnimationPath);

    /**
     * 一键生成全部动画（演奏动画和钢琴键动画）
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Animation Processor")
    static void GenerateAllAnimation(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 清空Control Rig轨道上的所有关键帧
     * @param LevelSequence Level Sequence实例
     * @param ControlRigInstance Control Rig实例
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    static void ClearControlRigKeyframes(ULevelSequence* LevelSequence,
                                         UControlRig* ControlRigInstance,
                                         AKeyRippleUnreal* KeyRippleActor);
};