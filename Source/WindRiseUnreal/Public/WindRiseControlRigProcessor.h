#pragma once

#include "CoreMinimal.h"
#include "WindRiseUnreal.h"
#include "WindRiseControlRigProcessor.generated.h"

class UControlRig;
struct FWindRiseNoteState;

/**
 * UWindRiseControlRigProcessor — 演奏者（人物）Control Rig 处理器
 *
 * 管理手部(14)、Pole(10)、脚部(4)、头部(1)、Breath(1) 共 30 个控制器的
 * 初始化、状态检查、数据捕获与恢复，以及人物 Morph Target 操作。
 *
 * ============================================================
 * Control Rig 层级结构
 * ============================================================
 *
 * controller_root
 * ├── controller_root_offset      ← 演奏动作整体偏移
 * │   ├── H_L (左手掌)
 * │   │   ├── T_L / T_L_pole
 * │   │   ├── I_L / I_L_pole
 * │   │   ├── M_L / M_L_pole
 * │   │   ├── R_L / R_L_pole
 * │   │   └── P_L / P_L_pole
 * │   ├── HP_L                    ← 与 H_L 同级
 * │   ├── H_R (右手掌)
 * │   │   ├── T_R / T_R_pole
 * │   │   ├── I_R / I_R_pole
 * │   │   ├── M_R / M_R_pole
 * │   │   ├── R_R / R_R_pole
 * │   │   └── P_R / P_R_pole
 * │   └── HP_R                    ← 与 H_R 同级
 * ├── F_L / FP_L / F_R / FP_R    ← 脚部，与 controller_root_offset 同级
 * ├── Head_Control
 * └── Breath_Control              ← 包含 CharacterMorphTargets 的 float
 * channels
 *
 * ============================================================
 * 调用方式（UI/脚本）：
 * ============================================================
 *
 *   UWindRiseControlRigProcessor::InitializeControllers(Actor);
 *   UWindRiseControlRigProcessor::CaptureControllers(Actor, CR, State);
 *   UWindRiseControlRigProcessor::RestoreControllers(Actor, CR, State);
 */
UCLASS()
class WINDRISEUNREAL_API UWindRiseControlRigProcessor : public UObject {
    GENERATED_BODY()

   public:
    // ========== 控制器映射初始化 ==========

    /**
     * 在 Actor 上初始化所有控制器映射表
     * HandControllers / PoleControllers / FootControllers / HeadControl /
     * BreathControl
     */
    static void InitializeControllers(AWindRiseUnreal* WindRiseActor);

    // ========== ControlRig 初始化与检查 ==========

    /** 在演奏者的 ControlRig Blueprint 上创建所有控制器控件 */
    static void InitializePerformerControlRig(AWindRiseUnreal* WindRiseActor);

    /** 检查演奏者 ControlRig 中各控制器的存在状态 */
    static void CheckControlRigStatus(AWindRiseUnreal* WindRiseActor);

    // ========== 层级辅助 ==========

    /**
     * 确保 Control 存在且 parent 正确
     * - 不存在 → 创建
     * - 存在但 parent 不匹配 → reparent
     * - 存在且 parent 正确 → 跳过
     */
    static bool EnsureControl(UControlRigBlueprint* CRBlueprint,
                              const FString& ControlName,
                              const FString& ExpectedParentName);

    // ========== 状态捕获与恢复 ==========

    /** 捕获当前所有控制器变换到 NoteState */
    static void CaptureControllers(AWindRiseUnreal* WindRiseActor,
                                   UControlRig* CR,
                                   FWindRiseNoteState& OutState);

    /** 从 NoteState 恢复所有控制器变换（通过 ControlRigInstance 的
     * RigHierarchy） */
    static void RestoreControllers(AWindRiseUnreal* WindRiseActor,
                                   UControlRig* CR,
                                   const FWindRiseNoteState& State);

    /** 恢复人物 Morph Target（先归零，再设置非零值） */
    static void RestoreCharacterMorphTargets(AWindRiseUnreal* WindRiseActor,
                                             const FWindRiseNoteState& State);

    // ========== 实时 Morph Target 辅助 ==========

    /** 设置人物单个 MT 值 */
    static void SetCharacterMTValue(AWindRiseUnreal* WindRiseActor, int32 Index,
                                    float Value);

    /** 重置所有人物 MT 为 0 */
    static void ResetAllCharacterMT(AWindRiseUnreal* WindRiseActor);
};
