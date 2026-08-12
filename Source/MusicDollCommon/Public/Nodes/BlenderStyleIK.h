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
 * - bPropagateToChildren（"影响子级"，继承自基类）为 true 时，末根骨骼的旋转
 *   与前一根骨骼保持一致（不独立旋转）；
 * - bPropagateToChildren 为 false 时，末根旋转由 "Use Orientation Task" 决定：
 *   开启则朝向 EffectorTransform，关闭则保持初始朝向。
 */
USTRUCT(meta = (DisplayName = "Blender Style IK", Category = "Hierarchy",
                Keywords = "N-Bone,IK,Pole,Blender,Smooth", Version = "5.7"))
struct MUSICDOLLCOMMON_API FRigUnit_BlenderStyleIK
    : public FRigUnit_CCDIKItemArray {
    GENERATED_BODY()

    /** 极点目标：引导骨骼链的弯曲平面朝向该方向 */
    UPROPERTY(meta = (Input))
    FVector PoleTarget;

    /**
     * 主轴（在根骨骼局部空间内）：定义极点约束的旋转轴方向，
     * 对应 Blender 中骨骼沿局部 Y 的主方向。
     */
    UPROPERTY(meta = (Input))
    FVector PrimaryAxis = FVector(1.0f, 0.0f, 0.0f);

    /**
     * 副轴（在根骨骼局部空间内）：在垂直主轴的平面内定义极点方向，
     * 等价于 Blender 的 poleangle。翻译规则：
     *   SecondAxis 投影到垂直 PrimaryAxis 的平面后归一化，即 pole 的 up 方向；
     *   该方向相对平面内参考基的角度即 angle。
     * 例：主轴 = Y 轴时，副轴 X=0°、-X=180°、Z=90°、-Z=-90°，X/Z 组合成任意角。
     */
    UPROPERTY(meta = (Input))
    FVector SecondAxis = FVector(0.0f, 1.0f, 0.0f);

    /** 是否反转极点方向 */
    UPROPERTY(meta = (Input))
    bool bNegativePole = false;

    /**
     * 是否将末端朝向作为目标参与求解（对应 Blender 的 Orientation Task）。
     * 默认关闭：EffectorTransform 仅作为位置目标（最常用）。
     * 注意：开启时，若 EffectorTransform 的旋转与链当前朝向不一致，朝向残差
     * 会主导求解并把位置带偏（典型症状：位置残差为 0 时仍被推飞、NaN）。
     * 仅在确实需要末端朝向约束时开启，并保证 EffectorTransform 旋转与链一致。
     * 仅在 bPropagateToChildren（影响子级）为 false 时生效：
     *   开启 -> 末根朝向 EffectorTransform；关闭 -> 末根保持初始朝向。
     */
    UPROPERTY(meta = (Input, DisplayName = "Use Orientation Task"))
    bool bUseOrientationTask = false;

    /** 单次迭代允许的最大角度变化（度），即 SDLS 阻尼上限 */
    UPROPERTY(meta = (Input, DisplayName = "Max Angle Per Step"))
    float MaxAnglePerStep = 45.0f;

    UPROPERTY(meta = (Input))
    bool bUseDebug = false;

    FRigUnit_BlenderStyleIK()
        : PoleTarget(FVector::ZeroVector),
          SecondAxis(FVector(0.0f, 1.0f, 0.0f)) {}

    RIGVM_METHOD()
    virtual void Execute() override;
};
