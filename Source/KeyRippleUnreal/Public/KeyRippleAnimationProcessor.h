#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "CoreMinimal.h"
#include "KeyRippleUnreal.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "KeyRippleAnimationProcessor.generated.h"

/**
 * KeyRipple 动画处理器
 * 用于处理 KeyRipple 的动画生成
 *
 * 通过继承 Common 模块的通用方法，大幅降低代码重复
 * 仅保留 KeyRipple 特定的逻辑（配置文件解析、特殊控制器处理）
 */
UCLASS()
class KEYRIPPLEUNREAL_API UKeyRippleAnimationProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 生成演奏动画（通过解析配置文件）
     * @param KeyRippleActor KeyRippleUnreal 实例
     */
    UFUNCTION(BlueprintCallable, Category = "KeyRipple Animation Processor")
    static void GeneratePerformerAnimation(AKeyRippleUnreal* KeyRippleActor);

    /**
     * 生成演奏动画（直接处理动画文件）
     * @param KeyRippleActor KeyRippleUnreal 实例
     * @param AnimationFilePath 动画文件路径
     */
    static void GeneratePerformerAnimationDirect(AKeyRippleUnreal* KeyRippleActor,
                                             const FString& AnimationFilePath);

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

    // ===== 向后兼容性包装方法 =====
    // 以下方法现在直接调用 Common 模块的通用方法，仅用于向后兼容

    /**
     * 批量插入Control Rig关键帧
     * 已弃用：请使用 UInstrumentAnimationUtility::BatchInsertControlRigKeys()
     */
    static void BatchInsertControlRigKeys(
        ULevelSequence* LevelSequence, UControlRig* ControlRigInstance,
        const TMap<FString, TArray<FControlKeyframe>>& ControlKeyframeData);
};