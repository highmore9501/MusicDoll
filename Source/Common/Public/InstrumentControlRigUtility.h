#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "CoreMinimal.h"

class UControlRig;
class UControlRigBlueprint;

/**
 * Control Rig 工具类
 * 提供与 Control Rig 相关的通用功能
 */
class COMMON_API FInstrumentControlRigUtility {
   public:
    /**
     * 从 SkeletalMeshActor 获取绑定的 Control Rig 实例和蓝图
     * 这是 KeyRippleControlRigProcessor::GetControlRigFromSkeletalMeshActor
     * 和 StringFlowControlRigProcessor::GetControlRigFromStringInstrument
     * 的统一实现
     *
     * @param InSkeletalMeshActor 要查询的骨骼网格Actor
     * @param OutControlRigInstance 输出参数：Control Rig 实例指针
     * @param OutControlRigBlueprint 输出参数：Control Rig 蓝图指针
     * @return 是否成功获取 Control Rig
     *
     * @note 该方法会搜索当前打开的 Level Sequence 中的所有 Control Rig 绑定，
     *       并找出绑定到指定 SkeletalMeshActor 的第一个 Control Rig。
     */
    static bool GetControlRigFromSkeletalMeshActor(
        ASkeletalMeshActor* InSkeletalMeshActor,
        UControlRig*& OutControlRigInstance,
        UControlRigBlueprint*& OutControlRigBlueprint);

    /**
     * 从 Control Rig 中获取指定 Control 的世界变換
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param OutTransform 输出的世界变换
     * @return 是否成功获取
     */
    static bool GetControlRigControlWorldTransform(
        ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
        FTransform& OutTransform);

    /**
     * 直接设置 Control Rig 中指定 Control 的局部变换
     * 
     * 该方法不进行任何坐标系转换，直接将输入的位置和旋转应用为 Control Rig 内部的局部变换。
     * 调用者需要自行确保输入的变换是正确的局部变换。
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
        ASkeletalMeshActor* InSkeletalMeshActor,
        const FString& ControlName,
        const FVector& NewLocation,
        const FQuat& NewRotation);

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
     * @note 该方法会忽略 SkeletalMeshActor 的缩放，以避免缩放被错误地应用到 Control。
     *       如果需要考虑 Actor 的缩放，请改用 SetControlRigLocalTransform。
     * 
     * @see SetControlRigLocalTransform 用于设置局部变换
     */
    static bool SetControlRigWorldTransform(
        ASkeletalMeshActor* InSkeletalMeshActor,
        const FString& ControlName,
        const FVector& NewWorldLocation,
        const FQuat& NewWorldRotation);

    /**
     * 初始化父子 Control 关系（仅第一次调用时使用）
     * 计算并缓存初始的相对变换矩阵，用于后续每帧快速更新
     * 
     * 相对变换矩阵的计算方式：
     * 1. 将父、子 Control 的初始变换从 Control Rig 坐标系转换到世界坐标系
     * 2. 计算 Child 相对于 Parent 的相对位置和旋转
     * 3. 这个相对关系在整个生命周期中保持不变
     *
     * @param ParentControlRig 父 Control 所属的 Control Rig 实例
     * @param ParentControlName 父 Control 的名称
     * @param ChildControlRig 子 Control 所属的 Control Rig 实例
     * @param ChildControlName 子 Control 的名称
     * @param OutRelativeTransform 输出的相对变换矩阵
     * @return 是否成功初始化
     * 
     * @note 该方法应该在应用启动或 Child 第一次绑定到 Parent 时调用一次
     *       之后使用 UpdateChildControlFromParent 每帧更新即可
     * 
     * @see UpdateChildControlFromParent
     */
    static bool InitializeControlRelationship(
        ASkeletalMeshActor* ParentControlRig,
        const FString& ParentControlName,
        ASkeletalMeshActor* ChildControlRig,
        const FString& ChildControlName,
        FTransform& OutRelativeTransform);

    /**
     * 根据缓存的相对变换矩阵更新子 Control 的位置（每帧调用）
     * 使用预先计算的相对变换矩阵，快速将 Child 更新到相对于当前 Parent 的位置
     * 
     * 工作原理：
     * - ChildNewWorldTransform = RelativeTransform * ParentCurrentWorldTransform
     * - 然后将世界坐标转换回 Child 的 Control Rig 坐标系并应用
     *
     * @param ParentControlRig 父 Control 所属的 Control Rig 实例
     * @param ParentControlName 父 Control 的名称
     * @param ChildControlRig 子 Control 所属的 Control Rig 实例
     * @param ChildControlName 子 Control 的名称
     * @param RelativeTransform 预先计算好的相对变换矩阵（来自 InitializeControlRelationship）
     * @return 是否成功更新
     * 
     * @note 该方法设计用于每帧调用，性能开销较小
     * @note RelativeTransform 必须由 InitializeControlRelationship 提供
     * 
     * @see InitializeControlRelationship
     */
    static bool UpdateChildControlFromParent(
        ASkeletalMeshActor* ParentControlRig,
        const FString& ParentControlName,
        ASkeletalMeshActor* ChildControlRig,
        const FString& ChildControlName,
        const FTransform& RelativeTransform);

    /**
     * 检测初始化值是否发生了变化
     * 用于判断是否需要重新初始化相对变换矩阵
     * 
     * 检查的值包括：
     * 1. 父 Control 的初始化全局变换
     * 2. 子 Control 的初始化全局变换
     * 3. 父 Control Rig Actor 的世界变换
     * 4. 子 Control Rig Actor 的世界变换
     *
     * @param ParentControlRig 父 Control 所属的 Control Rig 实例
     * @param ParentControlName 父 Control 的名称
     * @param ChildControlRig 子 Control 所属的 Control Rig 实例
     * @param ChildControlName 子 Control 的名称
     * @param CachedValues 上一次缓存的初始化值数组（大小应为4）
     * @param OutNewValues 输出当前的初始化值数组（大小为4）
     * @return 如果任何值发生了变化返回 true，否则返回 false
     * 
     * @note 返回 true 表示需要重新初始化相对变换矩阵
     */
    static bool HasInitializationValuesChanged(
        ASkeletalMeshActor* ParentControlRig,
        const FString& ParentControlName,
        ASkeletalMeshActor* ChildControlRig,
        const FString& ChildControlName,
        const TArray<FTransform>& CachedValues,
        TArray<FTransform>& OutNewValues);

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
    static bool GetControlRigAndIndex(
        ASkeletalMeshActor* InSkeletalMeshActor,
        const FString& ControlName,
        UControlRig*& OutControlRigInstance,
        int32& OutControlIndex);

    /**
     * 从 Control Rig Blueprint 中获取指定 Control 的全局初始化变换（相对于 Control Rig 根）
     * 
     * 该方法会递归计算 Control 及其所有父级的初始化变换，以获得相对于 Control Rig 根的完整变换。
     * 这与 GetControlRigControlInitTransform 不同，后者仅返回相对于直接父级的变换。
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param OutGlobalInitTransform 输出的全局初始化变换（相对于 Control Rig 根）
     * @return 是否成功获取
     */
    static bool GetControlRigControlGlobalInitTransform(
        ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
        FTransform& OutGlobalInitTransform);

    /**
     * 从 Control Rig 中获取指定 Control 的当前全局局部变换
     * 
     * 该方法返回 Control 在 Control Rig 内部坐标系中的全局变换，
     * 等同于调用 GetGlobalTransform，而不是 GetLocalTransform。
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param OutGlobalTransform 输出的 Control Rig 内部全局变换（不包含 Actor 世界变换）
     * @return 是否成功获取
     */
    static bool GetControlRigControlCurrentGlobalTransform(
        ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
        FTransform& OutGlobalTransform);
};
