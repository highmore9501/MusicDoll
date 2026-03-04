#pragma once

#include "CoreMinimal.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "FretDanceUnreal.h"
#include "FretDanceMusicInstrumentProcessor.generated.h"

/**
 * FretDance 乐器处理器
 * 用于处理吉他弦的材质、Morph Target 动画通道和振动动画生成
 *
 * ============================================================
 * Morph Target 命名规则（从 Blender Python 脚本分析）：
 * ============================================================
 *
 * Shape Key 名称格式：s{弦编号}fret{品格编号}{方向}
 *
 * 例如：
 *   - s0fret0up    - 第 0 弦，第 0 品格，向上振动
 *   - s0fret0down  - 第 0 弦，第 0 品格，向下振动
 *   - s1fret5up    - 第 1 弦，第 5 品格，向上振动
 *   - s2fret12down - 第 2 弦，第 12 品格，向下振动
 *
 * 参数说明：
 *   - 弦编号：0-5（6 根弦）
 *   - 品格编号：0-20（21 个品格）
 *   - 方向：up / down
 *
 * 总 Shape Key 数量：
 *   - 每根弦：21 品格 × 2 方向 = 42 个
 *   - 6 根弦：6 × 42 = 252 个
 *   - 加上 Basis：253 个
 *
 * ============================================================
 * Control Rig 通道命名（在 violin_root 下创建）：
 * ============================================================
 *
 * 通道格式：s{弦编号}fret{品格编号}{方向}
 * （与 Shape Key 名称完全一致，因为上下振动是两个独立的 Morph Target）
 *
 * 例如：
 *   - s0fret0up, s0fret0down    (第 0 弦，第 0 品格，上/下)
 *   - s0fret1up, s0fret1down    (第 0 弦，第 1 品格，上/下)
 *   - ...
 *   - s5fret20up, s5fret20down  (第 5 弦，第 20 品格，上/下)
 *
 * 总通道数：6 弦 × 21 品格 × 2 方向 = 252 个通道
 *
 * ============================================================
 */
UCLASS()
class FRETDANCEUNREAL_API UFretDanceMusicInstrumentProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 初始化吉他乐器
     * 完整的乐器初始化流程，包括材质、Control Rig 通道和动画轨道的初始化
     * @param FretDanceActor FretDanceUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Music Processor")
    static void InitializeGuitarInstrument(AFretDanceUnreal* FretDanceActor);

    /**
     * 初始化吉他弦的材质
     * 为每根弦创建独立的材质实例，用于后续 morph target 动画驱动
     * @param FretDanceActor FretDanceUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Music Processor")
    static void InitializeStringMaterials(AFretDanceUnreal* FretDanceActor);

    /**
     * 初始化弦材质参数动画轨道
     * 在 Level Sequencer 中为每根弦创建材质参数轨道，用于记录演奏强度等参数
     * @param FretDanceActor FretDanceUnreal 实例
     * @return 成功初始化的材质参数轨道数量
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Music Processor")
    static int32 InitializeStringMaterialAnimationTracks(
        AFretDanceUnreal* FretDanceActor);

    /**
     * 初始化弦振动动画轨道（Morph Target）
     * 在 Control Rig 中为每根弦创建振动方向的动画通道
     * @param FretDanceActor FretDanceUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Music Processor")
    static void InitializeStringVibrationAnimationChannels(
        AFretDanceUnreal* FretDanceActor);

    /**
     * 生成乐器动画
     * 使用新的 Morph Target 生成方法替代旧的弦振动动画方法
     * @param FretDanceActor FretDanceUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Music Processor")
    static void GenerateInstrumentAnimation(AFretDanceUnreal* FretDanceActor);

    /**
     * 从 JSON 加载弦振动数据并生成动画
     * 解析弦振动数据文件，将数据写入 Control Rig 的 Morph Target 通道
     * 内部方法，不暴露给蓝图（因为返回值类型过于复杂）
     * @param FretDanceActor FretDanceUnreal 实例
     * @param StringVibrationDataPath 弦振动数据 JSON 文件路径
     * @param OutVibrationKeyframeData 输出的弦振动关键帧数据
     * @return 操作是否成功
     */
    static bool LoadAndGenerateStringVibrationAnimation(
        AFretDanceUnreal* FretDanceActor,
        const FString& StringVibrationDataPath,
        TMap<FString,
             TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
            OutVibrationKeyframeData);

    /**
     * 生成乐器材质动画
     * @param FretDanceActor FretDanceUnreal 实例
     * @param InstrumentAnimationDataPath 乐器动画数据 JSON 文件路径
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Music Processor")
    static void GenerateInstrumentMaterialAnimation(
        AFretDanceUnreal* FretDanceActor,
        const FString& InstrumentAnimationDataPath);

   private:
    /**
     * 清理现有的吉他动画数据
     * 在初始化前清除 Control Rig 轨道和材质轨道上的旧数据
     * @param FretDanceActor FretDanceUnreal 实例
     */
    static void CleanupExistingGuitarAnimations(
        AFretDanceUnreal* FretDanceActor);
};
