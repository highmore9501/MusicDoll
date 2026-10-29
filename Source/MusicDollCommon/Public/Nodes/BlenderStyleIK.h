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
 * 单阶段迭代求解。
 *
 * 求解起点：用骨骼的 InitialLocal（本节点从不写入的数据）级联重建出
 * 「未被 IK 解算过的 FK 姿态」，根骨骼挂在父级的实时姿态上。起点因此在一帧内
 * 恒定，不会被本节点自己的写回污染，径向角不再逐帧漂移。
 *
 * 极点（pole）处理：Blender 的 ConstrainPoleVector 在【迭代前】预旋转整条链，
 * 其成立前提是起点末端不在目标上（dir != poledir）。本节点的输入姿态与 IK 输出
 * 是同一批骨骼，起点末端恰好压在目标上（dir == poledir），该约束恒为单位矩阵、
 * 在算法内部结构性失效。因此改为【求解后显式对齐】：把整条链绕「根 -> 末端」轴
 * 刚性旋转，使链的弯曲平面与「根 / 末端 / pole target」确定的平面重合。根与末端
 * 都在旋转轴上，故末端位置、骨长、相对构型都不变，只有链的朝向改变。
 * 退化情形（直链没有弯曲平面、pole 落在链轴上）跳过对齐。
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

    /** 求解权重：0~1 插值 */
    UPROPERTY(meta = (Input))
    float Weight = 1.f;

    /**
     * 最大迭代次数（Constant：值直接取这里的默认值，节点针脚上不留副本）。
     * 非 Constant 的 Input 会把节点首次创建时的默认值烘成针脚字面量，之后改
     * C++ 默认值不生效，很容易误判成"参数没接进代码"。
     * 停止判据与 Blender 一致（角度增量范数 < 1e-3），实际求解远早于上限收敛，
     * 这个值只是发散时的兜底。
     */
    UPROPERTY(meta = (Input, Constant, DisplayName = "Max Iterations"))
    int32 MaxIterations = 100;

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
