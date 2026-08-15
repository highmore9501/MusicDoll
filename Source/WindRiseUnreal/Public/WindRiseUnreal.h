#pragma once

#include "ControlRig/Public/ControlRig.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstrumentBase.h"
#include "InstrumentControlRigUtility.h"
#include "Tickable.h"
#include "WindRiseUnreal.generated.h"

// ============================================================
// 辅助结构体
// ============================================================

/**
 * 单个 Morph Target 值（名称 + 数值）
 * 用于 NoteStates 中存储非零的 MT 值
 */
USTRUCT(BlueprintType)
struct FWindRiseMorphTargetValue {
    GENERATED_BODY()

    /** Morph Target 名称 */
    UPROPERTY()
    FString MorphTargetName;

    /** 值（0.0 ~ 1.0） */
    UPROPERTY()
    float Value = 0.0f;

    FWindRiseMorphTargetValue() : MorphTargetName(TEXT("")), Value(0.0f) {}
    FWindRiseMorphTargetValue(const FString& InName, float InValue)
        : MorphTargetName(InName), Value(InValue) {}
};

/**
 * 单个 MIDI 音高的完整状态
 * 包含所有控制器变换 + 人物 MT + 乐器 MT
 */
USTRUCT(BlueprintType)
struct FWindRiseNoteState {
    GENERATED_BODY()

    /** MIDI 音符号 */
    UPROPERTY()
    int32 Note = 0;

    /** 音名（如 "C4"） */
    UPROPERTY()
    FString Name;

    /** 控制器名 → 变换（位置+旋转） */
    UPROPERTY()
    TMap<FString, FTransform> Controllers;

    /** 人物 Morph Target（仅非零值） */
    UPROPERTY()
    TArray<FWindRiseMorphTargetValue> CharacterMT;

    /** 乐器 Morph Target（仅非零值） */
    UPROPERTY()
    TArray<FWindRiseMorphTargetValue> InstrumentMT;
};

// ============================================================
// 核心 Actor
// ============================================================

/**
 * AWindRiseUnreal — 管乐器动画系统核心 Actor
 *
 * 控制器总数：30 个
 *   手部 14 个：H_L, HP_L, T_L, I_L, M_L, R_L, P_L,
 *               H_R, HP_R, T_R, I_R, M_R, R_R, P_R
 *   Pole 10 个：T_L_pole, I_L_pole, M_L_pole, R_L_pole, P_L_pole,
 *               T_R_pole, I_R_pole, M_R_pole, R_R_pole, P_R_pole
 *   脚部  4 个：F_L, FP_L, F_R, FP_R
 *   头部  1 个：Head_Control
 *   Breath 1 个：Breath_Control（挂载人物 Morph Target Float Channel）
 *
 * 不使用 Target 控制器（Middle_Hand、Look_At）和双线性辅助控制器。
 *
 * 数据格式：
 *   - .wind：avatar 配置文件（每音高的控制器+MT 状态）
 *   - .wind_rise：动画汇总文件（指向各 .animation 子文件）
 */
UCLASS(Blueprintable, BlueprintType)
class WINDRISEUNREAL_API AWindRiseUnreal : public AInstrumentBase,
                                           public FTickableGameObject {
    GENERATED_BODY()

   public:
    AWindRiseUnreal();

    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return true; }
    virtual bool IsTickableInEditor() const override { return true; }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override {
        RETURN_QUICK_DECLARE_CYCLE_STAT(AWindRiseUnreal, STATGROUP_Tickables);
    }

   protected:
    virtual void BeginPlay() override;

   public:
    // ========== 乐器引用 ==========

    /** 乐器骨骼网格（用户在 Details 面板中手动设置） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Properties")
    ASkeletalMeshActor* InstrumentMesh;

    // ========== 角色配置 ==========

    /** 乐器类型（如 chinese_dizi / flute） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindRise Config")
    FString InstrumentType;

    /** 音域下限（MIDI 值） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindRise Config")
    int32 MinNote = 60;

    /** 音域上限（MIDI 值） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindRise Config")
    int32 MaxNote = 84;

    /** 乐器说明 / 指法描述 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindRise Config")
    FString Description;

    // ========== Morph Target 选择 ==========

    /** 演奏者脸上需要驱动的 MT 名称列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "WindRise Morph Targets")
    TArray<FString> CharacterMorphTargets;

    /** 乐器上需要驱动的 MT 名称列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "WindRise Morph Targets")
    TArray<FString> InstrumentMorphTargets;

    // ========== 当前音高状态 ==========

    /** 当前编辑的 MIDI 音高 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WindRise State")
    int32 CurrentNote = 60;

    // ========== 控制器映射（构造时硬编码） ==========

    /** 14 个手部控制器 */
    UPROPERTY()
    TMap<FString, FString> HandControllers;

    /** 10 个 Pole Target 控制器（手指弯曲方向） */
    UPROPERTY()
    TMap<FString, FString> PoleControllers;

    /** 4 个脚部控制器 */
    UPROPERTY()
    TMap<FString, FString> FootControllers;

    /** 1 个头部控制器 */
    UPROPERTY()
    TMap<FString, FString> HeadControl;

    /** Breath Control（挂载人物 MT Float Channel，类似 lip_sync） */
    UPROPERTY()
    TMap<FString, FString> BreathControl;

    // ========== 数据存储 ==========

    /** MIDI 音符号 → 完整 NoteState */
    UPROPERTY()
    TMap<int32, FWindRiseNoteState> NoteStates;

    /** 休息状态下 controller_root_offset 的 Transform */
    UPROPERTY()
    FTransform RestOffset;

    // ========== 核心初始化 ==========

    /** 初始化所有控制器映射 */
    void InitializeControllersAndRecorders();

    // ========== 状态管理 ==========

    /**
     * 保存当前音高的所有控制器变换 + MT 值
     * 遍历 HandControllers、FootControllers 的 CR 控件
     * + 读取 CharacterMorphTargets/InstrumentMorphTargets 的当前 MT 值
     * 注：HeadControl 留给用户手动调整，BreathControl 仅用于挂载 MT Float
     * Channel，不做记录
     */
    UFUNCTION(BlueprintCallable, Category = "WindRise State")
    void SaveNoteState(int32 MidiNote);

    /**
     * 从 NoteStates 恢复指定音高的控制器变换 + MT 值
     */
    UFUNCTION(BlueprintCallable, Category = "WindRise State")
    void LoadNoteState(int32 MidiNote);

    // ========== ControlRig 操作 ==========

    /**
     * 初始化演奏者的 Control Rig
     * 创建所有手部、脚部、头部、Pole、Breath Control 控件
     */
    UFUNCTION(BlueprintCallable, Category = "WindRise Control Rig")
    void InitializePerformerControlRig();

    /**
     * 初始化乐器的 Control Rig
     * 创建 wind_root Control + InstrumentMorphTargets 对应的 Float Channel
     */
    UFUNCTION(BlueprintCallable, Category = "WindRise Control Rig")
    void InitializeInstrumentControlRig();

    /** 检查 Control Rig 控件状态 */
    UFUNCTION(BlueprintCallable, Category = "WindRise Control Rig")
    void CheckControlRigStatus();

    // ========== .wind 导入/导出 ==========

    /** 从 .wind 文件恢复全部数据 */
    UFUNCTION(BlueprintCallable, Category = "WindRise File")
    void ImportWindFile(const FString& FilePath);

    /** 导出全部 NoteStates + Config 到 .wind 文件
     *  @param FilePath 目标文件路径
     *  @param bToBlender 为 true 时按 Blender 坐标系导出（位置 Y 取反、旋转 x/z
     * 取反） */
    UFUNCTION(BlueprintCallable, Category = "WindRise File")
    void ExportWindFile(const FString& FilePath, bool bToBlender = false);

    /** 捕获当前 controller_root_offset 的 Transform 到 RestOffset */
    UFUNCTION(BlueprintCallable, Category = "WindRise State")
    void CaptureRestOffset();

    // ========== 动画生成 ==========

    /**
     * 解析 .wind_rise 汇总文件，生成完整动画
     * - 手部动画 → BatchInsertControlRigKeys
     * - 人物 MT → 写入 Breath_Control 的 Float Channel
     * - 乐器 MT → 写入 wind_root 的 Float Channel
     */
    UFUNCTION(BlueprintCallable, Category = "WindRise Animation")
    void GenerateAnimationFromWindRise(const FString& WindRiseFilePath);

    // ========== Morph Target 辅助 ==========

    /** 设置人物单个 MT 值（实时驱动 Performer SkeletalMesh） */
    void SetCharacterMTValue(int32 Index, float Value);

    /** 设置乐器单个 MT 值（实时驱动 Instrument SkeletalMesh） */
    void SetInstrumentMTValue(int32 Index, float Value);

    /** 重置所有人物 MT 为 0 */
    void ResetAllCharacterMT();

    /** 重置所有乐器 MT 为 0 */
    void ResetAllInstrumentMT();

    // ========== ControlRig 缓存 ==========

    UControlRig* GetCachedControlRig(FName ComponentName);
    UControlRigBlueprint* GetCachedControlRigBlueprint(FName ComponentName);
    void TriggerControlRigReregistration(const FString& ErrorMessage);

    /**
     * 根据组件名获取对应的 SkeletalMeshActor
     * "Performer" → SkeletalMeshActor, "Instrument" → InstrumentMesh
     */
    ASkeletalMeshActor* GetSkeletalMeshActorByName(FName ComponentName) const;

    // ========== 静态辅助 ==========

    /** MIDI 音符号 → 音名，如 60 → "C4" */
    static FString NoteNumberToName(int32 NoteNumber);
};
