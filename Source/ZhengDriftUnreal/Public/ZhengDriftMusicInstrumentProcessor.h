#pragma once

#include "CoreMinimal.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieScene.h"
#include "UObject/Object.h"
#include "ZhengDriftUnreal.h"
#include "ZhengDriftMusicInstrumentProcessor.generated.h"

/**
 * UZhengDriftMusicInstrumentProcessor
 *
 * 古筝乐器动画处理器
 * 负责创建弦振动 Morph Target 通道并从 JSON 写入关键帧
 *
 * 通道命名：string{N}_press / string{N}_vib（N = 0-20，共 42 个）
 * Root Control：zheng_root
 *
 * 参考：FretDanceMusicInstrumentProcessor（结构完全一致）
 */
UCLASS()
class ZHENGDRIFTUNREAL_API UZhengDriftMusicInstrumentProcessor : public UObject {
    GENERATED_BODY()
public:
    /**
     * 古筝乐器初始化主入口：注册 ControlRig、清理、初始化材质和通道
     */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Music Processor")
    static void InitializeZhengInstrument(AZhengDriftUnreal* ZhengDriftActor);

    /** 初始化弦材质（为每根弦创建独立材质实例） */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Music Processor")
    static void InitializeStringMaterials(AZhengDriftUnreal* ZhengDriftActor);

    /** 初始化弦材质参数动画轨道，@return 成功数量 */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Music Processor")
    static int32 InitializeStringMaterialAnimationTracks(
        AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 创建 42 个弦振动 Morph Target 动画通道
     * 命名：string0_press, string0_vib, ..., string20_press, string20_vib
     * Root Control：zheng_root
     */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Music Processor")
    static void InitializeStringVibrationAnimationChannels(
        AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 从 JSON 生成弦振动动画（公开入口）
     *
     * JSON 数组每项格式：
     * {
     *   "string_index": 10,
     *   "frame": 24,
     *   "value": 0.8,
     *   "shape_key_type": "Press"  // "Press" 或 "Vib"
     * }
     */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Music Processor")
    static void GenerateInstrumentAnimation(
        AZhengDriftUnreal* ZhengDriftActor,
        const FString& StringAnimationDataPath);

    /** 材质动画生成（Phase 7 预留） */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Music Processor")
    static void GenerateInstrumentMaterialAnimation(
        AZhengDriftUnreal* ZhengDriftActor,
        const FString& InstrumentAnimationDataPath);

    /** 内部实现，返回关键帧数据供外部使用 */
    static bool LoadAndGenerateStringVibrationAnimation(
        AZhengDriftUnreal* ZhengDriftActor,
        const FString& StringAnimationDataPath,
        TMap<FString,
             TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
            OutVibrationKeyframeData);

private:
    static void CleanupExistingZhengAnimations(
        AZhengDriftUnreal* ZhengDriftActor);
};
