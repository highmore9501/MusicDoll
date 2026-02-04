#pragma once

#include "ControlRig/Public/Units/Highlevel/Hierarchy/RigUnit_CCDIK.h"
#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "ArcDistributedIK.generated.h"

USTRUCT(meta = (DisplayName = "Arc Distributed IK With Pole Target",
                Category = "Hierarchy", Keywords = "N-Bone,IK,Pole,Arc",
                Version = "5.7"))
struct COMMON_API FRigUnit_ArcDistributedIK : public FRigUnit_CCDIKItemArray {
    GENERATED_BODY()

    UPROPERTY(meta = (Input))
    FVector PoleTarget;

    UPROPERTY(meta = (Input))
    FVector PrimaryAxis = FVector(1.0f, 0.0f, 0.0f);

    UPROPERTY(meta = (Input))
    FVector SecondAxis = FVector(0.0f, 1.0f, 0.0f);

    FRigUnit_ArcDistributedIK()
        : PoleTarget(FVector::ZeroVector),
          SecondAxis(FVector(0.0f, 1.0f, 0.0f)) {}

    RIGVM_METHOD()
    virtual void Execute() override;
};

// ============================================================================
// 用于测试的辅助函数声明
// ============================================================================

namespace FArcDistributedIKHelper {
/**
 * 从两个轴向量构建旋转四元数（用于测试）
 * @param PrimaryDir 世界空间主轴方向
 * @param SecondaryDir 世界空间次轴方向
 * @param LocalPrimaryAxis 本地主轴
 * @param LocalSecondaryAxis 本地次轴
 * @return 构建的旋转四元数
 */
COMMON_API FQuat BuildRotationFromTwoAxes(const FVector& PrimaryDir,
                                          const FVector& SecondaryDir,
                                          const FVector& LocalPrimaryAxis,
                                          const FVector& LocalSecondaryAxis);

/**
 * 计算骨骼长度
 */
COMMON_API float CalculateBoneLength(const FVector& Start, const FVector& End);

/**
 * 计算参考平面法线
 */
COMMON_API FVector CalculateReferencePlaneNormal(
    const FVector& RootPosition, const FVector& EffectorPosition,
    const FVector& PoleTarget);

/**
 * 计算效应器到目标位置的距离
 */
COMMON_API float CalculateEffectorDistance(const TArray<FCCDIKChainLink>& Chain,
                                           const FVector& EffectorPosition);

/**
 * 检查效应器距离是否太远（超过总链长）
 */
COMMON_API bool IsEffectorTooFar(float TotalChainLength,
                                 float EffectorDistance);

/**
 * 检查效应器距离是否太近（小于总链长/(2π)）
 */
COMMON_API bool IsEffectorTooClose(float TotalChainLength,
                                   float EffectorDistance);

/**
 * 根据轴和角度旋转点
 */
COMMON_API FVector RotatePointAroundAxis(const FVector& Position,
                                         const FVector& PivotPoint,
                                         const FVector& RotationAxis,
                                         float Angle);

/**
 * 检查是否已收敛
 */
COMMON_API bool IsConverged(float CurrentDistance, float Precision);

/**
 * 根据关节位置重建旋转信息
 */
COMMON_API void RebuildRotationsForChain(TArray<FCCDIKChainLink>& Chain,
                                         const TArray<float>& BoneLengths,
                                         const FVector& ReferencePlaneNormal,
                                         const FVector& PrimaryAxis,
                                         const FVector& SecondaryAxis);

/**
 * 对骨骼链应用旋转
 * 根据新的旋转逻辑：
 * 1. 所有骨骼相对于根骨骼旋转 +Angle
 * 2. 每个子骨骼及其所有后代相对于该骨骼旋转 -Angle
 */
COMMON_API void ApplyRotationToBoneChain(TArray<FCCDIKChainLink>& Chain,
                                         const TArray<float>& BoneLengths,
                                         const FVector& RotationAxis,
                                         float Angle);

/**
 * 使用牛顿法求解最优旋转角
 * 根据给定的初始角度猜测，使用牛顿法迭代求解最优角度，
 * 使末端效应器尽可能接近目标位置
 */
COMMON_API float SolveOptimalAngleWithNewton(
    const TArray<FCCDIKChainLink>& ChainSnapshot,
    const TArray<float>& BoneLengths, const FVector& RootPosition,
    const FVector& EffectorPosition, const FVector& PlaneNormal,
    float InitialAngleGuess, float Precision, int32 MaxIterations);
}  // namespace FArcDistributedIKHelper
