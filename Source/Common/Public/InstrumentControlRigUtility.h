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
     * 从 Control Rig 中获取指定 Control 的世界变换
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param OutTransform 输出的世界变换
     * @return 是否成功获取
     */
    static bool GetControlRigControlTransform(
        ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
        FTransform& OutTransform);

    /**
     * 从 Control Rig Blueprint 中获取指定 Control 的初始化变换（Init Transform）
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param OutInitTransform 输出的初始化变换（相对于父 Control 或 Control Rig）
     * @return 是否成功获取
     */
    static bool GetControlRigControlInitTransform(
        ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
        FTransform& OutInitTransform);

    /**
     * 设置 Control Rig 中指定 Control 的世界变换
     *
     * @param InSkeletalMeshActor 拥有 Control Rig 的骨骼网格 Actor
     * @param ControlName Control 的名称
     * @param NewLocation 新位置（世界坐标）
     * @param NewRotation 新旋转（世界坐标）
     * @return 是否成功设置
     */
    static bool SetControllerTransform(
        ASkeletalMeshActor* InSkeletalMeshActor,
        const FString& ControlName,
        const FVector& NewLocation,
        const FQuat& NewRotation);

    /**
     * 同步两个 Control：将源 Control 的世界变换同步到目标 Control
     * 此方法处理相对坐标系的变换
     *
     * @param SourceSkeletalMeshActor 源乐器 Actor（包含源 Control Rig）
     * @param SourceControlName 源 Control 的名称
     * @param TargetSkeletalMeshActor 目标乐器 Actor（包含目标 Control Rig）
     * @param TargetControlName 目标 Control 的名称
     * @return 是否成功同步
     */
    static bool SyncControlTransform(
        ASkeletalMeshActor* SourceSkeletalMeshActor,
        const FString& SourceControlName,
        ASkeletalMeshActor* TargetSkeletalMeshActor,
        const FString& TargetControlName);

    /**
     * 检查两个 Transform 是否相等（在容差范围内）
     * 用于优化性能：在缓存比较时判断 Transform 是否发生实际变化
     *
     * @param TransformA 第一个 Transform
     * @param TransformB 第二个 Transform
     * @param LocationTolerance 位置容差（单位：厘米，默认 0.1cm）
     * @param RotationTolerance 旋转容差（单位：弧度，默认 0.001rad ≈ 0.057°）
     * @return 如果两个 Transform 在容差范围内相等，返回 true
     */
    static bool AreTransformsEqual(const FTransform& TransformA,
                                   const FTransform& TransformB,
                                   float LocationTolerance = 0.1f,
                                   float RotationTolerance = 0.001f);

    /**
     * 建立父子 Control 关系：使子 Control 跟随父 Control 的偏移运动
     * 
     * 工作原理：
     * 1. 获取父 Control 的初始化变换和当前变换
     * 2. 计算父 Control 的偏移矩阵：offset = Init^(-1) × Current
     * 3. 获取子 Control 的初始化变换
     * 4. 计算子 Control 的新变换：New = ChildInit × ParentOffset
     * 
     * 这样子 Control 就会自动跟随父 Control 的运动，同时保持初始的相对关系
     *
     * @param ParentControlRig 父 Control 所属的 Control Rig 实例（通常是人物的 Control Rig）
     * @param ParentControlName 父 Control 的名称（如 "controller_root"）
     * @param ChildControlRig 子 Control 所属的 Control Rig 实例（通常是乐器的 Control Rig）
     * @param ChildControlName 子 Control 的名称（如 "violin_root"）
     * @return 是否成功建立关系并应用变换
     */
    static bool ParentBetweenControlRig(
        ASkeletalMeshActor* ParentControlRig,
        const FString& ParentControlName,
        ASkeletalMeshActor* ChildControlRig,
        const FString& ChildControlName);
};
