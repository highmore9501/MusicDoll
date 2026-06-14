#pragma once

#include "CoreMinimal.h"
#include "HarpGlideUnreal.h"
#include "UObject/Object.h"
#include "HarpGlideControlRigProcessor.generated.h"

class UControlRig;

/**
 * UHarpGlideControlRigProcessor
 *
 * 竖琴 Control Rig 管理处理器
 * 负责创建控制器、保存/加载状态（RecorderTransforms ↔ Control Rig）
 *
 * 参考：ZhengDriftControlRigProcessor
 */
UCLASS()
class HARPGLIDEUNREAL_API UHarpGlideControlRigProcessor : public UObject {
    GENERATED_BODY()
   public:
    /**
     * 在演奏者 Control Rig Blueprint 中创建所有控制器
     * @return 成功创建的控制器数量
     */
    static int32 SetupControllers(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 检查所有预期控制器是否存在
     * @return 全部存在返回 true
     */
    static bool CheckObjectsStatus(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 完整初始化：注册 CR + 创建控制器 + 验证
     */
    static bool SetupAllObjects(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 同时保存左右手状态到 RecorderTransforms
     */
    static bool SaveState(AHarpGlideUnreal* HarpGlideActor);

    /** 保存左手当前状态到 RecorderTransforms */
    static bool SaveLeftHandState(AHarpGlideUnreal* HarpGlideActor);

    /** 保存右手当前状态到 RecorderTransforms */
    static bool SaveRightHandState(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 从 RecorderTransforms 加载状态到 ControlRig 控制器
     */
    static bool LoadState(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 将 Control Rig 中同类名称控制器按当前选中的两个线性分布
     * @return 成功分布的控制器数量，失败返回 -1
     */
    static int32 LinearDistributeControls(AHarpGlideUnreal* HarpGlideActor);

    /**
     * 将 RecorderTransforms 中的弦位置数据回写到 Control Rig 控制器
     * 仅在 ImportRecorderInfo 之后调用
     */
    static void ApplyStringPositionToControlRig(
        AHarpGlideUnreal* HarpGlideActor, UControlRig* ControlRig);

    /** 保存踏板状态：将脚部控制器位置写入 pedal_{note}_state{n} 记录器 */
    static bool SavePedalState(AHarpGlideUnreal* HarpGlideActor,
                               EHarpGlidePedalNote Note,
                               EHarpGlidePedalState State);

    /** 加载踏板状态：从 pedal_{note}_state{n} 记录器回写脚部控制器 */
    static bool LoadPedalState(AHarpGlideUnreal* HarpGlideActor,
                               EHarpGlidePedalNote Note,
                               EHarpGlidePedalState State);

    /** 保存竖琴倾斜状态：将 harp_pivot 写入 harp_pivot_{state} 记录器 */
    static bool SaveHarpTiltState(AHarpGlideUnreal* HarpGlideActor,
                                  EHarpGlideTiltState State);

    /** 加载竖琴倾斜状态：从 harp_pivot_{state} 记录器回写 harp_pivot */
    static bool LoadHarpTiltState(AHarpGlideUnreal* HarpGlideActor,
                                  EHarpGlideTiltState State);

    /** 保存脚部休息位置：F_L→F_rest_L, F_R→F_rest_R */
    static bool SaveFootRestState(AHarpGlideUnreal* HarpGlideActor);

    /** 加载脚部休息位置：F_rest_L→F_L, F_rest_R→F_R */
    static bool LoadFootRestState(AHarpGlideUnreal* HarpGlideActor);

   private:
    static bool ValidateActor(AHarpGlideUnreal* Actor,
                              const FString& FunctionName);
    static UControlRig* GetPerformerControlRig(AHarpGlideUnreal* Actor);
    static TArray<FString> GetExpectedControllerNames(AHarpGlideUnreal* Actor);

    /** 辅助：通过 FControlRigCreationUtility 创建单个控制器 */
    static bool CreateController(
        UControlRigBlueprint* Blueprint, const FString& ControllerName,
        const FString& ParentName = TEXT(""),
        const FTransform& Transform = FTransform::Identity);

    /** 辅助：从 ControlRig 读取弦位置控制器的当前值并写入 RecorderTransforms */
    static void SaveStringPositionStates(AHarpGlideUnreal* HarpGlideActor,
                                         UControlRig* ControlRig);

    /** 辅助：从 ControlRig 读取脚部控制器的当前值并写入 RecorderTransforms */
    static void SaveFootControllerStates(AHarpGlideUnreal* HarpGlideActor,
                                         UControlRig* ControlRig);

    /** 辅助：从 RecorderTransforms 回写弦位置到 ControlRig */
    static void LoadStringPositionStates(AHarpGlideUnreal* HarpGlideActor,
                                         UControlRig* ControlRig);

    /** 辅助：从 RecorderTransforms 回写脚部到 ControlRig */
    static void LoadFootControllerStates(AHarpGlideUnreal* HarpGlideActor,
                                         UControlRig* ControlRig);
};
