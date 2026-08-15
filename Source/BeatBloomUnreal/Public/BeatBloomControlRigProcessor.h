#pragma once

#include "BeatBloomUnreal.h"
#include "CoreMinimal.h"
#include "BeatBloomControlRigProcessor.generated.h"

// 前向声明
class URigHierarchy;

/**
 * BeatBloom ControlRig 状态处理器
 * 负责在 ControlRig 控制器与 ABeatBloomUnreal 的 RecorderTransforms
 * 之间传输数据
 *
 * ============================================================
 * 对标参考：
 * ============================================================
 *
 * - UFretDanceControlRigProcessor（吉他 ControlRig 状态管理）
 *
 * ============================================================
 * 与 FretDance 的主要差异：
 * ============================================================
 *
 * | 维度         | FretDance                          | BeatBloom |
 * |--------------|------------------------------------|----------------------------------------|
 * | 保存操作     | SaveLeft / SaveRight               | SaveHand / SaveFoot /
 * SaveHeadControl  | | 加载操作     | LoadState                          |
 * LoadState                              | | 状态维度     | Position x State |
 * 鼓件名 + 状态                           | | 朝向控制     | 无 |
 * Middle_Hand/Look_At/Head_Control       | | 手脚分离     | 只有手部 | 手部 +
 * 脚部分开处理                      |
 *
 * ============================================================
 * 数据传输类型：
 * ============================================================
 *
 * | 控制器名                         | 传输数据              |
 * |----------------------------------|-----------------------|
 * | H_L / H_R                        | location + rotation   |
 * | HP_L / HP_R                      | location (3D位置)     |
 * | F_L / F_R                        | location + rotation   |
 * | Middle_Hand                      | location (3D位置)     |
 * | Head_Control                     | location (3D位置)     |
 *
 * ============================================================
 */
UCLASS()
class BEATBLOOMUNREAL_API UBeatBloomControlRigProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 保存手部状态（左手 + 右手）
     * 根据当前界面选择的鼓件和状态，将 ControlRig 中对应控制器的值
     * 保存到 RecorderTransforms
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom ControlRig Processor")
    static void SaveHandState(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 保存脚部状态（左脚 + 右脚）
     * 根据当前界面选择的鼓件和状态，将 ControlRig 中对应控制器的值
     * 保存到 RecorderTransforms
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom ControlRig Processor")
    static void SaveFootState(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 保存双线性映射辅助记录器状态
     * @param StateSuffix 状态后缀 "A", "B", "C", 或 "D"
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom ControlRig Processor")
    static void SaveBilinearHelperState(ABeatBloomUnreal* BeatBloomActor,
                                        const FString& StateSuffix);

    /**
     * 加载双线性映射辅助记录器指定状态
     * 直接按 StateSuffix 指定的状态将 H_L、H_R 和 Head_Control 还原到对应控制器
     * @param StateSuffix 状态后缀 "A", "B", "C", 或 "D"
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom ControlRig Processor")
    static void LoadBilinearHelperState(ABeatBloomUnreal* BeatBloomActor,
                                        const FString& StateSuffix);

    /**
     * 保存 Head_Control 状态
     * 将当前 Head_Control 控制器的位置保存到对应的 Head_Control 记录器
     * 记录器名基于当前左手/右手的鼓件和状态
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom ControlRig Processor")
    static void SaveHeadControlState(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 保存所有状态（手部 + 脚部 + Head_Control）
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom ControlRig Processor")
    static void SaveAllState(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 加载状态
     * 根据当前界面选择，将 RecorderTransforms 中对应的值写入 ControlRig 控制器
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom ControlRig Processor")
    static void LoadState(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 检查所有对象状态
     * 验证 ControlRig Blueprint 中的控制器是否完整存在
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom ControlRig Processor")
    static void CheckObjectsStatus(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 一键初始化所有对象
     * 创建控制器、添加 Bone Control Mapping、验证完整性
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom ControlRig Processor")
    static void SetupAllObjects(ABeatBloomUnreal* BeatBloomActor);

   private:
    /**
     * 获取当前状态下所有控制器到记录器的映射
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     * @return 控制器名 -> 记录器名 映射
     */
    static TMap<FString, FString> GetCurrentControllerToRecorderMapping(
        ABeatBloomUnreal* BeatBloomActor);

    /**
     * 从 ControlRig 读取指定控制器的 Transform
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     * @param ControllerName 控制器名称
     * @param OutLocation 输出位置
     * @param OutRotation 输出旋转
     * @return 读取是否成功
     */
    static bool ReadControllerTransform(ABeatBloomUnreal* BeatBloomActor,
                                        const FString& ControllerName,
                                        FVector& OutLocation,
                                        FQuat& OutRotation);

    /**
     * 将 Transform 写入 ControlRig 指定控制器
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     * @param ControllerName 控制器名称
     * @param Location 位置
     * @param Rotation 旋转
     * @param bLocationOnly 仅写入位置
     * @param bRotationOnly 仅写入旋转
     * @param bZOnly 仅写入 Z 轴位置（用于目标控制器）
     * @return 写入是否成功
     */
    static bool WriteControllerTransform(ABeatBloomUnreal* BeatBloomActor,
                                         const FString& ControllerName,
                                         const FVector& Location,
                                         const FQuat& Rotation,
                                         bool bLocationOnly = false,
                                         bool bRotationOnly = false,
                                         bool bZOnly = false);

    /**
     * 设置所有控制器（创建层级结构）
     * 层级：base_root -> controller_root -> 其他控制器
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     * @param ControlRigBlueprint ControlRig Blueprint
     * @return 创建的控制器数量
     */
    static int32 SetupControllers(ABeatBloomUnreal* BeatBloomActor,
                                  UControlRigBlueprint* ControlRigBlueprint);

    /**
     * 检查控制器是否存在
     *
     * @param RigHierarchy Rig 层次结构
     * @param ControlName 控制器名称
     * @return 是否存在
     */
    static bool ControlExists(URigHierarchy* RigHierarchy,
                              const FString& ControlName);
};
