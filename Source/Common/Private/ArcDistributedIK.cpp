// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcDistributedIK.h"

#include "AnimationCore.h"
#include "ControlRig.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "RigVMDeveloper/Public/RigVMModel/RigVMFunctionLibrary.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcDistributedIK)

struct FArcDistributedIKData {
    TArray<float> BoneLengths;        // 每个骨骼的长度
    float TotalChainLength;           // 总链长
    FVector ReferencePlaneNormal;     // 参考平面法线
    FVector RootPosition;             // 根位置
    FVector InitialEffectorDistance;  // 初始effector距离
};

static FArcDistributedIKData GatherChainData(
    const TArray<FCCDIKChainLink>& Chain, const FVector& EffectorPosition,
    const FVector& PoleTarget);

static float CalculateBoneLength(const FVector& Start, const FVector& End);

static FVector CalculateReferencePlaneNormal(const FVector& RootPosition,
                                             const FVector& EffectorPosition,
                                             const FVector& PoleTarget);

static float CalculateEffectorDistance(const TArray<FCCDIKChainLink>& Chain,
                                       const FVector& EffectorPosition);

static FArcDistributedIKData GatherChainData(
    const TArray<FCCDIKChainLink>& Chain, const FVector& EffectorPosition,
    const FVector& PoleTarget) {
    FArcDistributedIKData Data;

    if (Chain.Num() < 2) {
        return Data;
    }

    // 收集骨骼长度
    Data.BoneLengths.SetNum(Chain.Num());
    Data.TotalChainLength = 0.0f;

    for (int32 i = 0; i < Chain.Num() - 1; ++i) {
        float Length =
            CalculateBoneLength(Chain[i].Transform.GetLocation(),
                                Chain[i + 1].Transform.GetLocation());
        Data.BoneLengths[i] = Length;
        Data.TotalChainLength += Length;
    }

    // 最后一个骨骼的长度设为0（它是末端effector点）
    Data.BoneLengths[Chain.Num() - 1] = 0.0f;

    // 获取根位置
    Data.RootPosition = Chain[0].Transform.GetLocation();

    // 计算参考平面法线
    Data.ReferencePlaneNormal = CalculateReferencePlaneNormal(
        Data.RootPosition, EffectorPosition, PoleTarget);

    return Data;
}

static float CalculateBoneLength(const FVector& Start, const FVector& End) {
    return FVector::Dist(Start, End);
}

static FVector CalculateReferencePlaneNormal(const FVector& RootPosition,
                                             const FVector& EffectorPosition,
                                             const FVector& PoleTarget) {
    // 计算参考平面法线：(Effector - Root) × (PoleTarget - Root)
    FVector RootToEffector = (EffectorPosition - RootPosition).GetSafeNormal();
    FVector RootToPole = (PoleTarget - RootPosition).GetSafeNormal();

    FVector PlaneNormal = FVector::CrossProduct(RootToEffector, RootToPole);

    // 如果叉积结果太小（共线或接近共线），使用备用方案
    if (PlaneNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
        // 尝试与上向量叉积
        PlaneNormal = FVector::CrossProduct(RootToEffector, FVector::UpVector);

        // 如果还是太小，尝试与前向量叉积
        if (PlaneNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
            PlaneNormal =
                FVector::CrossProduct(RootToEffector, FVector::ForwardVector);
        }
    }

    return PlaneNormal.GetSafeNormal();
}

static bool DetermineAlgorithmBranch(float TotalChainLength,
                                     float EffectorDistance,
                                     int32& OutAlgorithmType);

static bool IsEffectorTooClose(float TotalChainLength, float EffectorDistance);

static bool IsEffectorTooFar(float TotalChainLength, float EffectorDistance);

static bool DetermineAlgorithmBranch(float TotalChainLength,
                                     float EffectorDistance,
                                     int32& OutAlgorithmType) {
    // 检查effector是否太远
    if (IsEffectorTooFar(TotalChainLength, EffectorDistance)) {
        OutAlgorithmType = 1;  // 拉直成直线
        return true;
    }

    // 检查effector是否太近
    if (IsEffectorTooClose(TotalChainLength, EffectorDistance)) {
        OutAlgorithmType = 0;  // 保持原状
        return false;
    }

    // 中间距离：使用圆弧算法
    OutAlgorithmType = 2;
    return true;
}

static bool IsEffectorTooClose(float TotalChainLength, float EffectorDistance) {
    // 这个函数现在只作为备用检查
    // 真正的"过近"判断在Execute()函数中进行，基于：
    // effectorDistance < (FirstBoneLength - OtherBonesLength)
    //
    // 这里保留一个极端的检查：effector几乎在根骨骼处
    float MinDistance = KINDA_SMALL_NUMBER;
    return EffectorDistance < MinDistance;
}

static bool IsEffectorTooFar(float TotalChainLength, float EffectorDistance) {
    // 太远：effector距离 > 总链长
    return EffectorDistance > TotalChainLength;
}

static float PreparePhaseStretchChain(TArray<FCCDIKChainLink>& Chain,
                                      const TArray<float>& BoneLengths,
                                      const FVector& EffectorPosition);

static void StretchChainAlongDirection(TArray<FCCDIKChainLink>& Chain,
                                       const TArray<float>& BoneLengths,
                                       const FVector& Direction);

static float PreparePhaseStretchChain(TArray<FCCDIKChainLink>& Chain,
                                      const TArray<float>& BoneLengths,
                                      const FVector& EffectorPosition) {
    if (Chain.Num() < 1) {
        return 0.0f;
    }

    FVector RootPosition = Chain[0].Transform.GetLocation();
    FVector DirectionToEffector =
        (EffectorPosition - RootPosition).GetSafeNormal();

    StretchChainAlongDirection(Chain, BoneLengths, DirectionToEffector);

    return CalculateEffectorDistance(Chain, EffectorPosition);
}

static void StretchChainAlongDirection(TArray<FCCDIKChainLink>& Chain,
                                       const TArray<float>& BoneLengths,
                                       const FVector& Direction) {
    if (Chain.Num() < 1) {
        return;
    }

    FVector RootPosition = Chain[0].Transform.GetLocation();
    FVector CurrentPosition = RootPosition;

    for (int32 i = 1; i < Chain.Num(); ++i) {
        CurrentPosition = CurrentPosition + Direction * BoneLengths[i - 1];
        Chain[i].Transform.SetLocation(CurrentPosition);
    }

    // 不在这里更新 LocalTransform，等待 RebuildRotationsForChain 最终重建
}

static void IterativePhase(TArray<FCCDIKChainLink>& Chain,
                           const TArray<float>& BoneLengths,
                           const FVector& EffectorPosition,
                           const FVector& PoleTarget,
                           const FVector& ReferencePlaneNormal,
                           float InitialDistance, float Precision,
                           int32 MaxIterations);

static float CalculateRotationAngle(const FVector& RootPosition,
                                    const FVector& EffectorPosition,
                                    const FVector& PoleTarget,
                                    const FVector& PlaneNormal);

static void ApplyRotationToBoneChain(TArray<FCCDIKChainLink>& Chain,
                                     const TArray<float>& BoneLengths,
                                     const FVector& RotationAxis, float Angle,
                                     bool bApplyReverseRotation = true);

static FVector RotatePointAroundAxis(const FVector& Position,
                                     const FVector& PivotPoint,
                                     const FVector& RotationAxis, float Angle);

static float CalculateEffectorDistance(const TArray<FCCDIKChainLink>& Chain,
                                       const FVector& EffectorPosition);

static bool IsConverged(float CurrentDistance, float Precision);

static void HandleTooFarCase(TArray<FCCDIKChainLink>& Chain,
                             const TArray<float>& BoneLengths,
                             const FVector& EffectorPosition);

static void RebuildRotationsForChain(
    TArray<FCCDIKChainLink>& Chain, const TArray<float>& BoneLengths,
    const FVector& ReferencePlaneNormal, const FVector& PrimaryAxis,
    const FVector& SecondaryAxis, const FVector& PoleTarget,
    const FTransform& RootParentTransform, int32 AlgorithmType);

static FVector FindSecondaryAxisPointOnPlane(const FVector& CurrentPosition,
                                             const FVector& PlaneNormal,
                                             const FVector& PrimaryDirection,
                                             const FVector& MiddlePosition,
                                             const FVector& PoleTarget,
                                             bool bUseMiddlePosition = false,
                                             float Distance = 50.0f);

static void WriteChainToHierarchy(FControlRigExecuteContext& ExecuteContext,
                                  const TArray<FCachedRigElement>& CachedItems,
                                  const TArray<FCCDIKChainLink>& Chain);

// ============================================================================
// 阶段3.2: 迭代阶段 - 基础几何计算方法实现
// ============================================================================

static FVector RotatePointAroundAxis(const FVector& Position,
                                     const FVector& PivotPoint,
                                     const FVector& RotationAxis, float Angle) {
    // 使用罗德里格旋转公式
    FVector RelativePos = Position - PivotPoint;
    FQuat RotationQuat = FQuat(RotationAxis.GetSafeNormal(), Angle);
    FVector RotatedRelativePos = RotationQuat.RotateVector(RelativePos);
    return PivotPoint + RotatedRelativePos;
}

static float CalculateEffectorDistance(const TArray<FCCDIKChainLink>& Chain,
                                       const FVector& EffectorPosition) {
    if (Chain.Num() < 1) {
        return 0.0f;
    }

    FVector EndEffectorPosition =
        Chain[Chain.Num() - 1].Transform.GetLocation();
    return FVector::Dist(EndEffectorPosition, EffectorPosition);
}

static bool IsConverged(float CurrentDistance, float Precision) {
    return CurrentDistance < Precision;
}

static float CalculateRotationAngle(const FVector& RootPosition,
                                    const FVector& EffectorPosition,
                                    const FVector& PoleTarget,
                                    const FVector& PlaneNormal) {
    FVector RootToEffector = (EffectorPosition - RootPosition).GetSafeNormal();
    FVector RootToPole = (PoleTarget - RootPosition).GetSafeNormal();

    // 计算两个向量在垂直于PlaneNormal的平面上的夹角
    // 首先投影到平面上
    FVector EffectorProjected =
        RootToEffector -
        FVector::DotProduct(RootToEffector, PlaneNormal) * PlaneNormal;
    FVector PoleProjected =
        RootToPole - FVector::DotProduct(RootToPole, PlaneNormal) * PlaneNormal;

    // 验证：检查投影后的向量与原向量的偏差
    // 如果链条已拉直，原向量应该已在参考平面上，偏差应该很小
    float EffectorProjectionError =
        FVector::Dist(EffectorProjected, RootToEffector);
    float PoleProjectionError = FVector::Dist(PoleProjected, RootToPole);
    const float ProjectionErrorThreshold = 0.1f;  // 允许的投影误差阈值

    if (EffectorProjectionError > ProjectionErrorThreshold) {
        UE_LOG(LogControlRig, Warning,
               TEXT("CalculateRotationAngle: Effector vector projection error "
                    "%.4f exceeds threshold. ")
                   TEXT("Effector may not be on reference plane. PlaneNormal: "
                        "(%.4f, %.4f, %.4f)"),
               EffectorProjectionError, PlaneNormal.X, PlaneNormal.Y,
               PlaneNormal.Z);
    }

    if (PoleProjectionError > ProjectionErrorThreshold) {
        UE_LOG(LogControlRig, Warning,
               TEXT("CalculateRotationAngle: PoleTarget vector projection "
                    "error %.4f exceeds threshold. ")
                   TEXT("PoleTarget may not be on reference plane. "
                        "PlaneNormal: (%.4f, %.4f, %.4f)"),
               PoleProjectionError, PlaneNormal.X, PlaneNormal.Y,
               PlaneNormal.Z);
    }

    // 规范化投影向量，检查是否为零向量
    if (EffectorProjected.IsNearlyZero() || PoleProjected.IsNearlyZero()) {
        return 0.0f;  // 如果投影为零，无法计算有效的旋转角
    }

    EffectorProjected.Normalize();
    PoleProjected.Normalize();

    // 计算夹角
    float CosAngle = FVector::DotProduct(PoleProjected, EffectorProjected);
    CosAngle = FMath::Clamp(CosAngle, -1.0f, 1.0f);

    float Angle = FMath::Acos(CosAngle);

    // 确定符号：使用叉积和平面法线的方向
    FVector Cross = FVector::CrossProduct(PoleProjected, EffectorProjected);
    if (FVector::DotProduct(Cross, PlaneNormal) < 0.0f) {
        Angle = -Angle;
    }

    return Angle;
}

static void ApplyRotationToBoneChain(TArray<FCCDIKChainLink>& Chain,
                                     const TArray<float>& BoneLengths,
                                     const FVector& RotationAxis, float Angle,
                                     bool bApplyReverseRotation) {
    if (Chain.Num() < 1) {
        return;
    }

    // 根骨骼不动，以它为原点，所有其他骨骼绕根骼旋转
    FVector RootPosition = Chain[0].Transform.GetLocation();

    // 第一步：所有骨骼相对于根骼旋转 -Angle
    for (int32 i = 1; i < Chain.Num(); ++i) {
        FVector OldPos = Chain[i].Transform.GetLocation();
        FVector NewPos =
            RotatePointAroundAxis(OldPos, RootPosition, RotationAxis, -Angle);
        Chain[i].Transform.SetLocation(NewPos);
    }

    // 第二步：从第一个非根骨骼开始，每个骨骼以及它的所有子骨骼绕该骨骼旋转
    for (int32 i = 1; i < Chain.Num(); ++i) {
        FVector CurrentBonePosition = Chain[i].Transform.GetLocation();

        // 旋转当前骨骼的所有子骨骼
        for (int32 j = i + 1; j < Chain.Num(); ++j) {
            FVector ChildOldPos = Chain[j].Transform.GetLocation();
            FVector ChildNewPos = RotatePointAroundAxis(
                ChildOldPos, CurrentBonePosition, RotationAxis, Angle);
            Chain[j].Transform.SetLocation(ChildNewPos);
        }
    }
}

// 使用梯度下降和线搜索求解最优旋转角
//
// 改进策略：
// 1. 使用更宽松的线搜索条件，允许小的改进
// 2. 自适应学习率，基于梯度大小调整步长
// 3. 额外的多点采样检测，避免陷入局部最小值
static float SolveOptimalAngleWithNewton(
    const TArray<FCCDIKChainLink>& ChainSnapshot,
    const TArray<float>& BoneLengths, const FVector& RootPosition,
    const FVector& EffectorPosition, const FVector& PlaneNormal,
    float InitialAngleGuess, float Precision, int32 MaxIterations) {
    // 定义目标函数：计算末端effector到目标的距离
    auto EvaluateChainState = [&](float Theta) -> float {
        TArray<FCCDIKChainLink> TestChain = ChainSnapshot;
        ApplyRotationToBoneChain(TestChain, BoneLengths, PlaneNormal, Theta,
                                 true);

        FVector EndEffectorPos =
            TestChain[TestChain.Num() - 1].Transform.GetLocation();
        float Distance = FVector::Dist(EndEffectorPos, EffectorPosition);
        return Distance;
    };
    // 计算初始猜测角度值，基本原则就是链越长，初始角度越大
    float DirectLength = CalculateBoneLength(RootPosition, EffectorPosition);
    float TotalChainLength = 0;
    for (int32 i = 0; i < ChainSnapshot.Num() - 1; ++i) {
        TotalChainLength +=
            CalculateBoneLength(ChainSnapshot[i].Transform.GetLocation(),
                                ChainSnapshot[i + 1].Transform.GetLocation());
    }
    float AngelRation = TotalChainLength / (PI * DirectLength);

    // 标准化初始角度到 [-π, π]
    float CurrentTheta = InitialAngleGuess * AngelRation;
    while (CurrentTheta > PI) CurrentTheta -= 2.0f * PI;
    while (CurrentTheta < -PI) CurrentTheta += 2.0f * PI;

    const float GradientTolerance = 1e-9f;
    const float ThetaTolerance = 1e-7f;
    const float DeltaTheta = 5e-5f;  // 用于数值导数的步长

    float CurrentDistance = EvaluateChainState(CurrentTheta);
    float BestTheta = CurrentTheta;
    float BestDistance = CurrentDistance;

    for (int32 Iter = 0; Iter < MaxIterations; ++Iter) {
        // 检查是否已收敛
        if (CurrentDistance < Precision) {
            return CurrentTheta;
        }

        // 计算数值梯度（中心差分）
        float DistancePlus = EvaluateChainState(CurrentTheta + DeltaTheta);
        float DistanceMinus = EvaluateChainState(CurrentTheta - DeltaTheta);
        float Gradient = (DistancePlus - DistanceMinus) / (2.0f * DeltaTheta);

        // 如果梯度太小，已到达稳定点
        if (FMath::Abs(Gradient) < GradientTolerance) {
            break;
        }

        // 自适应学习率：根据梯度大小调整初始步长
        float BaseStepSize = 0.1f / FMath::Max(FMath::Abs(Gradient), 0.01f);
        BaseStepSize = FMath::Clamp(BaseStepSize, 0.01f, 1.0f);

        // 使用线搜索找到最佳步长
        float Alpha = BaseStepSize;
        const float Beta = 0.5f;  // 退火参数
        const int32 MaxLineSearch = 15;

        float BestAlpha = 0.0f;
        float BestLineSearchDistance = CurrentDistance;

        for (int32 LineSearchIter = 0; LineSearchIter < MaxLineSearch;
             ++LineSearchIter) {
            float CandidateTheta = CurrentTheta - Alpha * Gradient;

            // 标准化角度到 [-π, π]
            while (CandidateTheta > PI) CandidateTheta -= 2.0f * PI;
            while (CandidateTheta < -PI) CandidateTheta += 2.0f * PI;

            float CandidateDistance = EvaluateChainState(CandidateTheta);

            // 使用宽松的Armijo条件：允许任何改进（即使很小）
            // 或者接受相等的情况（可能在平坦区域）
            if (CandidateDistance <= CurrentDistance + 1e-6f) {
                BestAlpha = Alpha;
                BestLineSearchDistance = CandidateDistance;
                break;
            }

            // 减小步长并重试
            Alpha *= Beta;
        }

        // 如果没有找到任何改进，尝试相反方向（梯度上升）
        if (BestAlpha < 1e-10f) {
            Alpha = 0.05f;
            for (int32 LineSearchIter = 0; LineSearchIter < 5;
                 ++LineSearchIter) {
                float CandidateTheta = CurrentTheta + Alpha * Gradient;

                // 标准化角度
                while (CandidateTheta > PI) CandidateTheta -= 2.0f * PI;
                while (CandidateTheta < -PI) CandidateTheta += 2.0f * PI;

                float CandidateDistance = EvaluateChainState(CandidateTheta);

                if (CandidateDistance < BestLineSearchDistance) {
                    BestAlpha = -Alpha;  // 负号表示相反方向
                    BestLineSearchDistance = CandidateDistance;
                    break;
                }

                Alpha *= Beta;
            }
        }

        // 如果仍然没有改进，停止
        if (BestAlpha == 0.0f && BestLineSearchDistance >= CurrentDistance) {
            break;
        }

        // 应用最佳步长
        float NewTheta = CurrentTheta - BestAlpha * Gradient;

        // 标准化新角度
        while (NewTheta > PI) NewTheta -= 2.0f * PI;
        while (NewTheta < -PI) NewTheta += 2.0f * PI;

        float NewDistance = EvaluateChainState(NewTheta);

        // 更新全局最佳解
        if (NewDistance < BestDistance) {
            BestDistance = NewDistance;
            BestTheta = NewTheta;
        }

        // 检查收敛条件（角度变化）
        float ThetaDelta = NewTheta - CurrentTheta;
        // 考虑角度的周期性
        if (FMath::Abs(ThetaDelta) > PI) {
            ThetaDelta = 2.0f * PI - FMath::Abs(ThetaDelta);
        }

        if (FMath::Abs(ThetaDelta) < ThetaTolerance) {
            CurrentTheta = NewTheta;
            CurrentDistance = NewDistance;
            break;
        }

        // 更新状态
        CurrentTheta = NewTheta;
        CurrentDistance = NewDistance;
    }

    // 返回最佳解
    return BestTheta;
}

static void IterativePhase(TArray<FCCDIKChainLink>& Chain,
                           const TArray<float>& BoneLengths,
                           const FVector& EffectorPosition,
                           const FVector& PoleTarget,
                           const FVector& ReferencePlaneNormal,
                           float InitialDistance, float Precision,
                           int32 MaxIterations) {
    if (Chain.Num() < 1) {
        return;
    }

    float CurrentDistance = InitialDistance;
    float PreviousDistance = CurrentDistance;
    int32 IterationCount = 0;
    FVector RootPosition = Chain[0].Transform.GetLocation();

    while (IterationCount < MaxIterations &&
           !IsConverged(CurrentDistance, Precision)) {
        // 计算当前的rotation angle alpha
        float Alpha = CalculateRotationAngle(RootPosition, EffectorPosition,
                                             PoleTarget, ReferencePlaneNormal);

        // 如果Alpha太小，跳过这次迭代
        if (FMath::Abs(Alpha) < 1e-6f) {
            break;
        }

        // 保存当前状态以便测试
        TArray<FCCDIKChainLink> ChainSnapshot = Chain;

        // 使用牛顿法求解最优角度（替代二分搜索）
        float OptimalAngle = SolveOptimalAngleWithNewton(
            ChainSnapshot, BoneLengths, RootPosition, EffectorPosition,
            ReferencePlaneNormal, Alpha, Precision, MaxIterations);

        // 应用最优角度
        if (FMath::Abs(OptimalAngle) > 1e-8f) {
            ApplyRotationToBoneChain(Chain, BoneLengths, ReferencePlaneNormal,
                                     OptimalAngle, true);
            CurrentDistance =
                CalculateEffectorDistance(Chain, EffectorPosition);
        }

        // 更新根位置（因为可能发生了变化）
        RootPosition = Chain[0].Transform.GetLocation();

        ++IterationCount;
    }
}

static void HandleTooFarCase(TArray<FCCDIKChainLink>& Chain,
                             const TArray<float>& BoneLengths,
                             const FVector& EffectorPosition) {
    if (Chain.Num() < 1) {
        return;
    }

    FVector RootPosition = Chain[0].Transform.GetLocation();
    FVector DirectionToEffector =
        (EffectorPosition - RootPosition).GetSafeNormal();

    StretchChainAlongDirection(Chain, BoneLengths, DirectionToEffector);
}

static FVector FindSecondaryAxisPointOnPlane(
    const FVector& CurrentPosition, const FVector& PlaneNormal,
    const FVector& PrimaryDirection, const FVector& MiddlePosition,
    const FVector& PoleTarget, bool bUseMiddlePosition, float Distance) {
    // 在平面内计算垂直于primary direction的方向
    FVector PerpendicularInPlane =
        FVector::CrossProduct(PlaneNormal, PrimaryDirection.GetSafeNormal());

    // 检查垂直方向是否为零
    if (PerpendicularInPlane.IsNearlyZero()) {
        // 备用方案：使用一个正交向量
        if (FMath::Abs(PrimaryDirection.X) < 0.9f) {
            PerpendicularInPlane =
                FVector::CrossProduct(PrimaryDirection, FVector::RightVector);
        } else {
            PerpendicularInPlane =
                FVector::CrossProduct(PrimaryDirection, FVector::ForwardVector);
        }
    }

    PerpendicularInPlane.Normalize();

    // 根据 bUseMiddlePosition 选择方向参考点
    FVector ReferencePosition =
        bUseMiddlePosition ? MiddlePosition : PoleTarget;

    // 检查参考位置是否有效且不为零向量
    if (!ReferencePosition.IsNearlyZero()) {
        // 计算到参考位置的向量
        FVector ToReference = ReferencePosition - CurrentPosition;

        // 只有当距离足够大时才使用参考位置来确定方向
        // 这样可以避免当 CurrentPosition 接近参考位置时的数值不稳定性
        if (ToReference.Length() > KINDA_SMALL_NUMBER) {
            ToReference = ToReference.GetSafeNormal();
            float DotWithReference =
                FVector::DotProduct(PerpendicularInPlane, ToReference);

            // 如果点积为负，说明方向相反，需要翻转
            if (DotWithReference < 0) {
                PerpendicularInPlane = -PerpendicularInPlane;
            }
        }
    }

    return CurrentPosition + PerpendicularInPlane * Distance;
}

static FQuat BuildRotationFromTwoAxes(const FVector& PrimaryDir,
                                      const FVector& SecondaryDir,
                                      const FVector& LocalPrimaryAxis,
                                      const FVector& LocalSecondaryAxis,
                                      const FVector& PlaneNormal) {
    // 直接从三个正交的轴构建旋转
    // PrimaryDir 和 SecondaryDir 应该已经正交，PlaneNormal 是第三个轴
    FVector WorldX = PrimaryDir.GetSafeNormal();
    FVector WorldY = SecondaryDir.GetSafeNormal();
    FVector WorldZ = PlaneNormal.GetSafeNormal();

    FVector LocalX = LocalPrimaryAxis.GetSafeNormal();
    FVector LocalY = LocalSecondaryAxis.GetSafeNormal();
    FVector LocalZ = FVector::CrossProduct(LocalX, LocalY).GetSafeNormal();

    // 构建从本地空间到世界空间的旋转矩阵
    FMatrix LocalToWorldMatrix(FPlane(WorldX.X, WorldX.Y, WorldX.Z, 0.0f),
                               FPlane(WorldY.X, WorldY.Y, WorldY.Z, 0.0f),
                               FPlane(WorldZ.X, WorldZ.Y, WorldZ.Z, 0.0f),
                               FPlane(0.0f, 0.0f, 0.0f, 1.0f));

    // 构建本地轴定义的坐标系到标准坐标系的矩阵
    FMatrix LocalAxisMatrix(FPlane(LocalX.X, LocalX.Y, LocalX.Z, 0.0f),
                            FPlane(LocalY.X, LocalY.Y, LocalY.Z, 0.0f),
                            FPlane(LocalZ.X, LocalZ.Y, LocalZ.Z, 0.0f),
                            FPlane(0.0f, 0.0f, 0.0f, 1.0f));

    // 所需旋转 = LocalAxisMatrix^(-1) * LocalToWorldMatrix
    FMatrix RotationMatrix = LocalAxisMatrix.Inverse() * LocalToWorldMatrix;

    FQuat ResultQuat(RotationMatrix);
    return ResultQuat.GetNormalized();
}

// 在命名空间中定义公开可测试的版本
namespace FArcDistributedIKHelper {
FQuat BuildRotationFromTwoAxes(const FVector& PrimaryDir,
                               const FVector& SecondaryDir,
                               const FVector& LocalPrimaryAxis,
                               const FVector& LocalSecondaryAxis) {
    return ::BuildRotationFromTwoAxes(PrimaryDir, SecondaryDir,
                                      LocalPrimaryAxis, LocalSecondaryAxis,
                                      FVector::ZeroVector);
}

float CalculateBoneLength(const FVector& Start, const FVector& End) {
    return ::CalculateBoneLength(Start, End);
}

FVector CalculateReferencePlaneNormal(const FVector& RootPosition,
                                      const FVector& EffectorPosition,
                                      const FVector& PoleTarget) {
    return ::CalculateReferencePlaneNormal(RootPosition, EffectorPosition,
                                           PoleTarget);
}

float CalculateEffectorDistance(const TArray<FCCDIKChainLink>& Chain,
                                const FVector& EffectorPosition) {
    return ::CalculateEffectorDistance(Chain, EffectorPosition);
}

bool IsEffectorTooFar(float TotalChainLength, float EffectorDistance) {
    return ::IsEffectorTooFar(TotalChainLength, EffectorDistance);
}

bool IsEffectorTooClose(float TotalChainLength, float EffectorDistance) {
    return ::IsEffectorTooClose(TotalChainLength, EffectorDistance);
}

FVector RotatePointAroundAxis(const FVector& Position,
                              const FVector& PivotPoint,
                              const FVector& RotationAxis, float Angle) {
    return ::RotatePointAroundAxis(Position, PivotPoint, RotationAxis, Angle);
}

bool IsConverged(float CurrentDistance, float Precision) {
    return ::IsConverged(CurrentDistance, Precision);
}

void RebuildRotationsForChain(TArray<FCCDIKChainLink>& Chain,
                              const TArray<float>& BoneLengths,
                              const FVector& ReferencePlaneNormal,
                              const FVector& PrimaryAxis,
                              const FVector& SecondaryAxis) {
    return ::RebuildRotationsForChain(
        Chain, BoneLengths, ReferencePlaneNormal, PrimaryAxis, SecondaryAxis,
        FVector::ZeroVector, FTransform::Identity, 1);
}

void ApplyRotationToBoneChain(TArray<FCCDIKChainLink>& Chain,
                              const TArray<float>& BoneLengths,
                              const FVector& RotationAxis, float Angle) {
    return ::ApplyRotationToBoneChain(Chain, BoneLengths, RotationAxis, Angle,
                                      true);
}

float SolveOptimalAngleWithNewton(const TArray<FCCDIKChainLink>& ChainSnapshot,
                                  const TArray<float>& BoneLengths,
                                  const FVector& RootPosition,
                                  const FVector& EffectorPosition,
                                  const FVector& PlaneNormal,
                                  float InitialAngleGuess, float Precision,
                                  int32 MaxIterations) {
    return ::SolveOptimalAngleWithNewton(
        ChainSnapshot, BoneLengths, RootPosition, EffectorPosition, PlaneNormal,
        InitialAngleGuess, Precision, MaxIterations);
}
}  // namespace FArcDistributedIKHelper

static FQuat CalculateBoneRotation(
    const FVector& CurrentPosition, const FVector& NextPosition,
    const FVector& PlaneNormal, const FVector& PrimaryAxis,
    const FVector& SecondaryAxis, const FVector& MiddlePosition,
    const FVector& PoleTarget, int32 AlgorithmType, bool bIsLastBone) {
    // 计算世界空间的主轴方向（指向下一个关节）
    FVector WorldPrimaryDir = (NextPosition - CurrentPosition).GetSafeNormal();

    // 在平面上找到次轴方向
    FVector SecondaryAxisPoint = FindSecondaryAxisPointOnPlane(
        CurrentPosition, PlaneNormal, WorldPrimaryDir, MiddlePosition,
        PoleTarget, true, 50.0f);  // 始终使用 bUseMiddlePosition=true

    // 统一的方向计算方式
    FVector WorldSecondaryDir =
        (CurrentPosition - SecondaryAxisPoint).GetSafeNormal();

    // 使用两轴构建旋转
    FQuat Rotation =
        BuildRotationFromTwoAxes(WorldPrimaryDir, WorldSecondaryDir,
                                 PrimaryAxis, SecondaryAxis, PlaneNormal);

    return Rotation;
}

static void RebuildRotationsForChain(
    TArray<FCCDIKChainLink>& Chain, const TArray<float>& BoneLengths,
    const FVector& ReferencePlaneNormal, const FVector& PrimaryAxis,
    const FVector& SecondaryAxis, const FVector& PoleTarget,
    const FTransform& RootParentTransform, int32 AlgorithmType) {
    if (Chain.Num() < 1) {
        return;
    }

    // 计算骨骼链根和末端的中点位置
    FVector RootPosition = Chain[0].Transform.GetLocation();
    FVector EffectorPosition = Chain[Chain.Num() - 1].Transform.GetLocation();
    FVector MiddlePosition = (RootPosition + EffectorPosition) * 0.5f;
    float TotalChainLength =
        CalculateBoneLength(RootPosition, EffectorPosition);

    // 在中点的基础上，沿着 Middle->PoleTarget 的反方向微小偏移
    // 这样可以确保参考点不在链的直线上（对AlgorithmType==1尤其重要）
    FVector DirectionToPole = (PoleTarget - MiddlePosition).GetSafeNormal();
    float PoleOffset = 0.1 * TotalChainLength;

    bool bUseOriginalMiddlePosition = AlgorithmType == 2;
    FVector AdjustedMiddlePosition =
        bUseOriginalMiddlePosition
            ? MiddlePosition - 0.5f * PoleOffset * DirectionToPole
            : MiddlePosition - DirectionToPole * PoleOffset;

    for (int32 i = 0; i < Chain.Num(); ++i) {
        FVector CurrentPosition = Chain[i].Transform.GetLocation();
        FVector NextPosition;
        bool bIsLastBone = (i == Chain.Num() - 1);

        if (i < Chain.Num() - 1) {
            // 对于所有非末端骨骼，指向下一个骨骼
            NextPosition = Chain[i + 1].Transform.GetLocation();
        } else {
            // 对于末端骨骼，使用前一个骨骼的方向
            FVector PrevToCurrent =
                (CurrentPosition - Chain[i - 1].Transform.GetLocation())
                    .GetSafeNormal();
            NextPosition = CurrentPosition + PrevToCurrent * 50.0f;
        }

        // 现在统一使用 AdjustedMiddlePosition
        FQuat NewRotation = CalculateBoneRotation(
            CurrentPosition, NextPosition, ReferencePlaneNormal, PrimaryAxis,
            SecondaryAxis, AdjustedMiddlePosition, PoleTarget, AlgorithmType,
            bIsLastBone);

        Chain[i].Transform.SetRotation(NewRotation);
    }
}

static void WriteChainToHierarchy(FControlRigExecuteContext& ExecuteContext,
                                  const TArray<FCachedRigElement>& CachedItems,
                                  const TArray<FCCDIKChainLink>& Chain) {
    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy || CachedItems.Num() != Chain.Num()) {
        return;
    }

    for (int32 i = 0; i < CachedItems.Num(); ++i) {
        const FCachedRigElement& CachedBone = CachedItems[i];
        if (CachedBone.IsValid()) {
            Hierarchy->SetGlobalTransform(
                CachedBone.GetKey(), Chain[i].Transform, false, false, false);
        }
    }
}

FRigUnit_ArcDistributedIK_Execute() {
    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy) {
        return;
    }

    // 骨骼链准备
    WorkData.CachedItems.Empty();
    if (Items.Num() < 2) {
        return;
    }

    WorkData.CachedItems.Reserve(Items.Num());
    for (int32 i = 0; i < Items.Num(); ++i) {
        const FRigElementKey& Key = Items[i];
        FRigBoneElement* Bone = Hierarchy->Find<FRigBoneElement>(Key);
        if (!Bone) {
            return;
        }
        FCachedRigElement CachedBone(Key, Hierarchy, true);
        WorkData.CachedItems.Add(CachedBone);
    }

    int32 NumChainLinks = WorkData.CachedItems.Num();
    if (NumChainLinks < 2) {
        return;
    }

    // 获取根骨骼的实际父骨骼变换（可能不在IK链中）
    FTransform RootParentTransform = FTransform::Identity;
    FRigElementKey RootBoneKey = WorkData.CachedItems[0].GetKey();
    FRigBoneElement* RootBone = Hierarchy->Find<FRigBoneElement>(RootBoneKey);

    if (RootBone && RootBone->ParentElement) {
        // 根骨骼有父骨骼，获取其全局变换
        RootParentTransform = RootBone->ParentElement->GetTransform().Get(
            ERigTransformType::CurrentGlobal);
    }

    // 构造CCDIK链
    TArray<FCCDIKChainLink> Chain;
    Chain.SetNum(NumChainLinks);

    for (int32 i = 0; i < NumChainLinks; ++i) {
        const FCachedRigElement& CachedBone = WorkData.CachedItems[i];
        FTransform BoneTransform =
            Hierarchy->GetGlobalTransform(CachedBone.GetIndex());
        Chain[i].Transform = BoneTransform;

        if (i > 0) {
            const FCachedRigElement& ParentBone = WorkData.CachedItems[i - 1];
            FTransform ParentTransform =
                Hierarchy->GetGlobalTransform(ParentBone.GetIndex());
            Chain[i].LocalTransform =
                BoneTransform.GetRelativeTransform(ParentTransform);
        } else {
            // 根骨骼：LocalTransform相对于它的实际父骨骼（可能在链外）
            Chain[i].LocalTransform =
                BoneTransform.GetRelativeTransform(RootParentTransform);
        }
        Chain[i].CurrentAngleDelta = 0.0;
    }

    // 阶段1: 数据收集
    FArcDistributedIKData Data =
        GatherChainData(Chain, EffectorTransform.GetLocation(), PoleTarget);

    if (Data.ReferencePlaneNormal.IsNearlyZero()) {
        return;
    }

    // 阶段2: 判断算法分支
    int32 AlgorithmType = 0;
    float EffectorDistance =
        FVector::Dist(Data.RootPosition, EffectorTransform.GetLocation());

    // 检查effector是否过近：当effector距离 < (最长骨骼长度 -
    // 其他所有骨骼长度之和)
    // 这表示链无法形成闭环，即使所有骨骼都伸直也无法从根骨骼连接到effector
    // 因为最长的骨骼是能够达到最远距离的关键
    bool bEffectorTooClose = false;
    if (Chain.Num() >= 2 && Data.BoneLengths.Num() >= 2) {
        // 找出最长的骨骼
        float MaxBoneLength = 0.0f;
        for (int32 i = 0; i < Data.BoneLengths.Num() - 1;
             ++i) {  // -1 因为最后一个骨骼长度为0
            MaxBoneLength = FMath::Max(MaxBoneLength, Data.BoneLengths[i]);
        }

        float OtherBonesLength = Data.TotalChainLength - MaxBoneLength;
        float MinReachableDistance = MaxBoneLength - OtherBonesLength;
        bEffectorTooClose = (EffectorDistance < MinReachableDistance);
    }

    if (bEffectorTooClose) {
        // 太近，保持原状
        return;
    }

    if (!DetermineAlgorithmBranch(Data.TotalChainLength, EffectorDistance,
                                  AlgorithmType)) {
        // 根据DetermineAlgorithmBranch的判断，太近，保持原状
        return;
    }

    // 阶段3: 位置计算
    if (AlgorithmType == 1) {
        // 太远，拉直成直线
        HandleTooFarCase(Chain, Data.BoneLengths,
                         EffectorTransform.GetLocation());
    } else if (AlgorithmType == 2) {
        // 中间距离，使用圆弧算法
        // 阶段3.1: 准备阶段
        float InitialDistance = PreparePhaseStretchChain(
            Chain, Data.BoneLengths, EffectorTransform.GetLocation());

        // 阶段3.2: 迭代阶段
        IterativePhase(Chain, Data.BoneLengths, EffectorTransform.GetLocation(),
                       PoleTarget, Data.ReferencePlaneNormal, InitialDistance,
                       Precision > 0.f ? Precision : 0.001f,
                       MaxIterations > 0 ? MaxIterations : 10);
    }

    // 阶段4: 旋转重建
    RebuildRotationsForChain(Chain, Data.BoneLengths, Data.ReferencePlaneNormal,
                             PrimaryAxis, SecondAxis, PoleTarget,
                             RootParentTransform, AlgorithmType);

    // 阶段5: 写入Hierarchy
    WriteChainToHierarchy(ExecuteContext, WorkData.CachedItems, Chain);
}
