#pragma once

#include "CoreMinimal.h"
#include "HarpGlideUnreal.h"
#include "UObject/Object.h"
#include "HarpGlideAnimationProcessor.generated.h"

class ULevelSequence;

/**
 * UHarpGlideAnimationProcessor
 *
 * 竖琴演奏者动画处理器
 * 负责解析 .harpglide 配置文件并生成手部/脚部/头部/竖琴倾斜动画
 *
 * .harpglide 报告文件指向四个路径：
 *   "performance_animation" → 双手 + 双脚 + 头部帧数据
 *   "harp_animation"        → 竖琴倾斜 (harp_pivot) 帧数据
 *   "string_animation"      → 弦振动数据（由 MusicInstrumentProcessor 处理）
 *   "pedal_shape_animation" → 踏板 Morph Target 数据
 */
UCLASS()
class HARPGLIDEUNREAL_API UHarpGlideAnimationProcessor : public UObject {
    GENERATED_BODY()
   public:
    /**
     * 生成演奏者动画（双手 + 双脚 + 头部 + 竖琴倾斜）
     */
    UFUNCTION(BlueprintCallable, Category = "HarpGlide Animation Processor")
    static void GeneratePerformerAnimation(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 生成乐器动画（弦振动 + 踏板 Shape Key，委托给 MusicInstrumentProcessor）
     */
    UFUNCTION(BlueprintCallable, Category = "HarpGlide Animation Processor")
    static void GenerateInstrumentAnimation(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 生成全部动画（演奏者 + 乐器）
     */
    UFUNCTION(BlueprintCallable, Category = "HarpGlide Animation Processor")
    static void GenerateAllAnimation(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 解析 .harpglide 配置文件
     * @return 至少有一个路径有效时返回 true
     */
    static bool ParseHarpGlideConfigFile(AHarpGlideUnreal* HarpGlideActor,
                                         FString& OutPerformanceAnimationPath,
                                         FString& OutHarpAnimationPath,
                                         FString& OutStringAnimationPath,
                                         FString& OutPedalShapeAnimationPath);

   private:
    /**
     * 从 performance JSON 生成左右手控制器关键帧
     *
     * JSON 每帧格式（Rust 端 AnimationOutput 序列化）：
     * {
     *   "frame": 0,
     *   "hand_position": [x, y, z],
     *   "hand_rotation": [w, x, y, z],
     *   "finger_positions": { "thumb": [...], "index": [...], "middle": [...],
     * "ring": [...], "pinky": [...] }, "hand_pole_target": [x, y, z]
     * }
     */
    static void MakePerformanceAnimation(
        AHarpGlideUnreal* HarpGlideActor, const FString& AnimationFilePath,
        ULevelSequence* LevelSequence,
        const FString& HarpAnimationPath = TEXT(""));
};
