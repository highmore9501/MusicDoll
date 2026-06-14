#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ZhengDriftUnreal.h"
#include "ZhengDriftControlRigProcessor.generated.h"

class UControlRig;

/**
 * UZhengDriftControlRigProcessor
 *
 * 古筝 Control Rig 管理处理器
 * 负责创建控制器、保存/加载状态
 *
 * 参考：FretDanceControlRigProcessor
 */
UCLASS()
class ZHENGDRIFTUNREAL_API UZhengDriftControlRigProcessor : public UObject {
    GENERATED_BODY()
public:
    /**
     * 在 Control Rig Blueprint 中创建所有必要的控制器
     * @return 成功创建的控制器数量
     */
    static int32 SetupControllers(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 检查所有预期控制器是否存在
     * @return 全部存在返回 true
     */
    static bool CheckObjectsStatus(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 完整初始化：创建控制器 + 验证
     */
    static bool SetupAllObjects(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 同时保存左右手状态到 RecorderTransforms
     */
    static bool SaveState(AZhengDriftUnreal* ZhengDriftActor,
                          TMap<FString, FTransform>& OutStateData);

    /** 保存左手当前状态到 RecorderTransforms */
    static bool SaveLeftHandState(AZhengDriftUnreal* ZhengDriftActor);

    /** 保存右手当前状态到 RecorderTransforms */
    static bool SaveRightHandState(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 从 RecorderTransforms 加载状态到 ControlRig 控制器
     */
    static bool LoadState(AZhengDriftUnreal* ZhengDriftActor,
                          const TMap<FString, FTransform>& StateData);

    /**
     * 将 Control Rig 中同类名称的控制器按当前选中的两个控制器线性分布
     * 需先在 Sequencer 中选中恰好两个同类控制器（如 s0end 和 s20end）
     * @return 成功分布的控制器数量，失败返回 -1
     */
    static int32 LinearDistributeControls(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 将 RecorderTransforms 中的弦位置数据回写到 Control Rig 控制器
     * 仅在 ImportRecorderInfo 之后调用
     */
    static void ApplyStringPositionToControlRig(AZhengDriftUnreal* ZhengDriftActor,
                                                 UControlRig* ControlRig);

private:
    static bool ValidateZhengDriftActor(AZhengDriftUnreal* Actor,
                                         const FString& FunctionName);
    static UControlRig* GetControlRig(AZhengDriftUnreal* Actor);
    static TArray<FString> GetExpectedControllerNames(
        AZhengDriftUnreal* Actor);

    /** 辅助：通过 FControlRigCreationUtility 创建单个控制器 */
    static bool CreateController(UControlRigBlueprint* Blueprint,
                                  const FString& ControllerName,
                                  const FString& ParentName = TEXT(""),
                                  const FTransform& Transform = FTransform::Identity);

    /** 辅助：从 ControlRig 中读取所有弦位置控制器的当前值并写入 RecorderTransforms */
    static void SaveStringPositionStates(AZhengDriftUnreal* ZhengDriftActor,
                                          UControlRig* ControlRig);

    /** 辅助：从 ControlRig 中读取所有脚部控制器的当前值并写入 RecorderTransforms */
    static void SaveFootControllerStates(AZhengDriftUnreal* ZhengDriftActor,
                                          UControlRig* ControlRig);

    /**
     * 辅助：检测当前是否为 A/B/C/D 四态之一，若是则自动把
     * Middle_Hand 和 Head_Control 的当前位置写入对应的双线性辅助记录器
     */
    static void CheckAndSaveBilinearHelpers(AZhengDriftUnreal* ZhengDriftActor,
                                             UControlRig* ControlRig);

    /**
     * 辅助：检测当前是否为 A/B/C/D 四态之一，若是则自动把
     * 对应双线性辅助记录器中的位置应用到 Middle_Hand 和 Head_Control 控制器
     */
    static void CheckAndLoadBilinearHelpers(AZhengDriftUnreal* ZhengDriftActor,
                                             UControlRig* ControlRig);

    /** 辅助：从 RecorderTransforms 加载弦位置数据到 ControlRig 控制器 */
    static void LoadStringPositionStates(AZhengDriftUnreal* ZhengDriftActor,
                                          UControlRig* ControlRig);

    /** 辅助：从 RecorderTransforms 加载脚部控制器数据到 ControlRig 控制器 */
    static void LoadFootControllerStates(AZhengDriftUnreal* ZhengDriftActor,
                                          UControlRig* ControlRig);
};
