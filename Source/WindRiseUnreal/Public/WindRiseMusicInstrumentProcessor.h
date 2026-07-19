#pragma once

#include "CoreMinimal.h"
#include "WindRiseUnreal.h"
#include "WindRiseMusicInstrumentProcessor.generated.h"

struct FWindRiseNoteState;

/**
 * UWindRiseMusicInstrumentProcessor — 乐器 Control Rig 处理器
 *
 * 管理乐器的 ControlRig（wind_root）初始化、Morph Target 恢复、
 * 以及乐器 Morph Target 的实时操作。
 *
 * ============================================================
 * 调用方式（UI/脚本）：
 * ============================================================
 *
 *   UWindRiseMusicInstrumentProcessor::InitializeInstrumentControlRig(Actor);
 *   UWindRiseMusicInstrumentProcessor::RestoreInstrumentMorphTargets(Actor,
 * State);
 */
UCLASS()
class WINDRISEUNREAL_API UWindRiseMusicInstrumentProcessor : public UObject {
    GENERATED_BODY()

   public:
    // ========== ControlRig 初始化 ==========

    /** 初始化乐器的 ControlRig（创建 wind_root + MT Float Channels） */
    static void InitializeInstrumentControlRig(AWindRiseUnreal* WindRiseActor);

    // ========== 状态恢复 ==========

    /** 恢复乐器 Morph Target（先归零，再设置非零值） */
    static void RestoreInstrumentMorphTargets(AWindRiseUnreal* WindRiseActor,
                                              const FWindRiseNoteState& State);

    // ========== 实时 Morph Target 辅助 ==========

    /** 设置乐器单个 MT 值 */
    static void SetInstrumentMTValue(AWindRiseUnreal* WindRiseActor,
                                     int32 Index, float Value);

    /** 重置所有乐器 MT 为 0 */
    static void ResetAllInstrumentMT(AWindRiseUnreal* WindRiseActor);
};
