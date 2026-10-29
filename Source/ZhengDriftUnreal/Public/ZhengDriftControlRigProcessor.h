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
     * 头部采样：采集默认坐姿下的头部位置（头部原点）
     *
     * 头部原点只有一个用途——判断当前哪只手离头更远（更远的那只手主导头部位置）。
     * 各手部状态的样本跟着 SaveLeftHandState / SaveRightHandState 一起采集。
     *
     * @return 是否成功写入（Head_Control 控件缺失时返回 false）
     */
    static bool SaveHeadOrigin(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 头部采样：把头部原点还原到 Head_Control
     * @return 是否成功还原（原点从未采集时返回 false）
     */
    static bool LoadHeadOrigin(AZhengDriftUnreal* ZhengDriftActor);

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
     * 头部采样的内部实现（需要调用方已取好 ControlRig）：
     * 采集头部原点 / 还原头部原点。
     */
    static bool SaveHeadOriginInternal(AZhengDriftUnreal* ZhengDriftActor,
                                       UControlRig* ControlRig);
    static bool LoadHeadOriginInternal(AZhengDriftUnreal* ZhengDriftActor,
                                       UControlRig* ControlRig);

    /**
     * 辅助：把某个手部状态下的"这只手在哪 + 头在哪"写进 HeadSamples。
     * `SideKey` 取 "left" / "right"，`StateKey` 形如 "Normal_far"。
     */
    static void CaptureHeadSample(AZhengDriftUnreal* ZhengDriftActor,
                                  UControlRig* ControlRig,
                                  const FString& SideKey,
                                  const FString& StateKey,
                                  const FVector& HandLocation);

    /** 辅助：从 RecorderTransforms 加载弦位置数据到 ControlRig 控制器 */
    static void LoadStringPositionStates(AZhengDriftUnreal* ZhengDriftActor,
                                          UControlRig* ControlRig);

    /** 辅助：从 RecorderTransforms 加载脚部控制器数据到 ControlRig 控制器 */
    static void LoadFootControllerStates(AZhengDriftUnreal* ZhengDriftActor,
                                          UControlRig* ControlRig);
};
