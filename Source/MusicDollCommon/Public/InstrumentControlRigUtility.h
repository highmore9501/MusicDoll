#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "CoreMinimal.h"

class UControlRig;
class UControlRigBlueprint;

/**
 * Control Rig 工具类
 * 提供与 Control Rig 相关的通用功能
 */
class MUSICDOLLCOMMON_API FInstrumentControlRigUtility {
   public:
    /**
     * 从预获取的 Control Rig 实例中获取指定 Control 的世界变換
     * 高性能版本，避免重复获取ControlRig实例
     *
     * @param ControlRigInstance 已获取的 Control Rig 实例
     * @param ControlName Control 的名称
     * @param InSkeletalMeshActor 对应的骨骼网格 Actor（用于获取世界变换）
     * @param OutTransform 输出的世界变换
     * @return 是否成功获取
     */
    static bool GetControlRigControlWorldTransform(
        UControlRig* ControlRigInstance, const FString& ControlName,
        ASkeletalMeshActor* InSkeletalMeshActor, FTransform& OutTransform);

    /**
     * 直接设置 Control Rig 中指定 Control 的局部变换
     *
     * 该方法不进行任何坐标系转换，直接将输入的位置和旋转应用为 Control Rig
     * 内部的局部变换。 调用者需要自行确保输入的变换是正确的局部变换。
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param NewLocation 新的局部位置（相对于 Control 的父级）
     * @param NewRotation 新的局部旋转（相对于 Control 的父级）
     * @return 是否成功设置
     *
     * @see SetControlRigWorldTransform 用于设置世界坐标
     */
    static bool SetControlRigLocalTransform(
        ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
        const FVector& NewLocation, const FQuat& NewRotation);

    /**
     * 设置 Control Rig 中指定 Control 的世界变换
     *
     * 该方法会自动将输入的世界坐标转换为 Control Rig 内部的局部变换，
     * 考虑 SkeletalMeshActor 的位置和旋转（但忽略缩放）。
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param NewWorldLocation 新的世界位置（世界坐标系）
     * @param NewWorldRotation 新的世界旋转（世界坐标系）
     * @return 是否成功设置
     *
     * @note 该方法会忽略 SkeletalMeshActor 的缩放，以避免缩放被错误地应用到
     * Control。 如果需要考虑 Actor 的缩放，请改用 SetControlRigLocalTransform。
     *
     * @see SetControlRigLocalTransform 用于设置局部变换
     */
    static bool SetControlRigWorldTransform(
        ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
        const FVector& NewWorldLocation, const FQuat& NewWorldRotation);

    

    

   private:
    /**
     * 获取 Control Rig 实例和对应 Control 的索引
     * 这是一个内部辅助方法，用于提取验证和索引查找的公共逻辑
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param OutControlRigInstance 输出参数：Control Rig 实例指针
     * @param OutControlIndex 输出参数：Control 在 Hierarchy 中的索引
     * @return 是否成功获取实例和索引
     */
    static bool GetControlRigAndIndex(ASkeletalMeshActor* InSkeletalMeshActor,
                                      const FString& ControlName,
                                      UControlRig*& OutControlRigInstance,
                                      int32& OutControlIndex);

    /**
     * 从 Control Rig Blueprint 中获取指定 Control 的全局初始化变换（相对于
     * Control Rig 根）
     *
     * 该方法会递归计算 Control 及其所有父级的初始化变换，以获得相对于 Control
     * Rig 根的完整变换。 这与 GetControlRigControlInitTransform
     * 不同，后者仅返回相对于直接父级的变换。
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param OutGlobalInitTransform 输出的全局初始化变换（相对于 Control Rig
     * 根）
     * @return 是否成功获取
     */
    static bool GetControlRigControlGlobalInitTransform(
        ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
        FTransform& OutGlobalInitTransform);
};
