#pragma once

#include "CoreMinimal.h"
#include "KeyRippleUnreal.h"
#include "KeyRipplePianoProcessor.generated.h"

UCLASS() class KEYRIPPLEUNREAL_API UKeyRipplePianoProcessor : public UObject {
    GENERATED_BODY()

   public:
    /** 更新钢琴材质 */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple|Piano")
    static void UpdatePianoMaterials(AKeyRippleUnreal* KeyRippleActor);

    /** 初始化钢琴系统 */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple|Piano")
    static void InitPiano(AKeyRippleUnreal* KeyRippleActor);

    /** 生成乐器动画 */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple|Piano")
    static void GenerateInstrumentAnimation(
        AKeyRippleUnreal* KeyRippleActor, const FString& PianoKeyAnimationPath);

    /** 获取钢琴变形目标名称 */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple|Piano")
    static bool GetPianoMorphTargetNames(AKeyRippleUnreal* KeyRippleActor,
                                         TArray<FString>& OutMorphTargetNames);

    /** 初始化钢琴键 Control Rig */
    static void InitPianoKeyControlRig(AKeyRippleUnreal* KeyRippleActor);

    /** 初始化钢琴材质参数轨道 */
    static int32 InitPianoMaterialParameterTracks(
        AKeyRippleUnreal* KeyRippleActor);

    static int32 GenerateInstrumentMaterialAnimation(
        AKeyRippleUnreal* KeyRippleActor, ULevelSequence* LevelSequence,
        const TMap<FString,
                   TPair<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
            MorphTargetKeyframeData,
        FFrameNumber MinFrame, FFrameNumber MaxFrame);



#if WITH_EDITOR
   private:
    /**
     * 清理现有的钢琴动画数据
     */
    static void CleanupExistingPianoAnimations(
        AKeyRippleUnreal* KeyRippleActor);

#endif
};
