#pragma once

#include "ControlRig/Public/Units/Highlevel/Hierarchy/RigUnit_CCDIK.h"
#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "BlenderStyleIK.generated.h"

/**
 * Blender Style IK
 *
 * 复刻 Blender intern/iksolver 的内嵌雅可比 IK 求解器：
 * - 把整条骨骼链的所有旋转自由度同时放进雅可比矩阵；
 * - 用 SVD + 选择性阻尼最小二乘（SDLS）求伪逆，得到最小范数解；
 * - 角度增量会被均匀分摊到所有能产生该效果的骨骼上，因此弯曲平滑、
 *   每根骨骼的弯曲程度都很平均（这正是 Blender IK 效果平滑的数学根源）。
 *
 * 与 ArcDistributedIK（两阶段：先解位置再重建旋转）不同，本节点是
 * 单阶段迭代求解，姿态演化依赖初始姿态与极点预处理。
 *
 * 末端骨骼（末根）行为：
 * - bPropagateToChildren（"影响子级"）为 true 时，末根骨骼的世界旋转
 *   直接取 EffectorTransform 的旋转；
 * - bPropagateToChildren 为 false 时，末根保持级联所得朝向
 *   （相对父骨骼不旋转）。
 */
USTRUCT(meta = (DisplayName = "Blender Style IK", Category = "Hierarchy",
                Keywords = "N-Bone,IK,Pole,Blender,Smooth", Version = "5.7"))
struct MUSICDOLLCOMMON_API FRigUnit_BlenderStyleIK
    : public FRigUnit_HighlevelBaseMutable {
    GENERATED_BODY()

    /** 链的末端骨骼：从它开始沿父级链向上取 ChainLength 根骨骼构成整条链 */
    UPROPERTY(meta = (Input))
    FRigElementKey EndBone;

    /** 链长度（骨骼数量）：从末端骨骼向上取的骨骼根数（含末端） */
    UPROPERTY(meta = (Input, DisplayName = "Chain Length"))
    int32 ChainLength = 4;

    /** 末端目标（Effector）：世界空间的目标变换 */
    UPROPERTY(meta = (Input))
    FTransform EffectorTransform;

    /** 收敛精度：末端离目标多近算达标 */
    UPROPERTY(meta = (Input, Constant))
    float Precision = 0.05f;

    /** 求解权重：0~1 插值 */
    UPROPERTY(meta = (Input))
    float Weight = 1.f;

    /** 最大迭代次数 */
    UPROPERTY(meta = (Input))
    int32 MaxIterations = 30;

    /** 影响子级：true 时末端关节的世界旋转跟随 Effector */
    UPROPERTY(meta = (Input, Constant))
    bool bPropagateToChildren = true;

    /** 极点目标：引导骨骼链的弯曲平面朝向该方向 */
    UPROPERTY(meta = (Input))
    FVector PoleTarget;

    /** 单次迭代允许的最大角度变化（度），即 SDLS 阻尼上限 */
    UPROPERTY(meta = (Input, DisplayName = "Max Angle Per Step"))
    float MaxAnglePerStep = 45.0f;

    FRigUnit_BlenderStyleIK()
        : EndBone(NAME_None, ERigElementType::Bone),
          EffectorTransform(FTransform::Identity),
          PoleTarget(FVector::ZeroVector) {}

    RIGVM_METHOD()
    virtual void Execute() override;
};
