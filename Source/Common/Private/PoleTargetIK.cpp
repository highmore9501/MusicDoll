// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoleTargetIK.h"

#include "AnimationCore.h"
#include "ControlRig.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "RigVMDeveloper/Public/RigVMModel/RigVMFunctionLibrary.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoleTargetIK)

static void ApplyPoleRotationToChain(TArray<FCCDIKChainLink>& Chain,
                                     const FQuat& TargetRotation,
                                     float PoleWeight, int32 PivotIndex);

// 辅助方法：在平面上找到与primary axis垂直的点
static FVector FindPointOnPlanePerpendicularToAxis(
    const FVector& PointOnPlane,    // 当前关节位置
    const FVector& PlaneNormal,     // 平面法线
    const FVector& PrimaryAxisDir,  // primary axis方向（指向下一个关节）
    const FVector& PoleTarget,      // pole target位置
    float Distance = 100.0f)        // 偏移距离
{
    // 确保primary axis方向与平面法线不平行
    FVector PrimaryDirNormalized = PrimaryAxisDir.GetSafeNormal();

    // 计算一个在平面内且垂直于primary axis的方向
    // 方法：平面法线 × primary axis方向
    FVector PerpendicularInPlane =
        FVector::CrossProduct(PlaneNormal, PrimaryDirNormalized);
    PerpendicularInPlane.Normalize();

    // 检查这个方向是否朝向pole target所在的一侧
    FVector ToPoleTarget = (PoleTarget - PointOnPlane).GetSafeNormal();
    float DotWithTarget =
        FVector::DotProduct(PerpendicularInPlane, ToPoleTarget);

    // 如果点积为负，说明方向相反，需要翻转
    if (DotWithTarget < 0) {
        PerpendicularInPlane = -PerpendicularInPlane;
    }

    // 在平面上沿垂直方向移动指定距离，得到second axis点
    return PointOnPlane + PerpendicularInPlane * Distance;
}

// 辅助方法：从三个点构建Transform
static FTransform BuildTransformFromThreePoints(
    const FVector& Origin,              // 原点（当前关节位置）
    const FVector& PrimaryAxisPoint,    // primary axis上的点（下一个关节位置）
    const FVector& SecondaryAxisPoint)  // secondary axis上的点（平面上的点）
{
    // 计算primary axis方向
    FVector PrimaryDir = (PrimaryAxisPoint - Origin).GetSafeNormal();

    // 计算secondary axis方向（指向平面上的点）
    FVector SecondaryDir = (SecondaryAxisPoint - Origin).GetSafeNormal();

    // 计算第三个轴（叉积得到）
    FVector TertiaryDir =
        FVector::CrossProduct(PrimaryDir, SecondaryDir).GetSafeNormal();

    // 重新正交化secondary axis
    SecondaryDir =
        FVector::CrossProduct(TertiaryDir, PrimaryDir).GetSafeNormal();

    // 构建旋转矩阵
    FMatrix RotationMatrix(
        FPlane(PrimaryDir.X, PrimaryDir.Y, PrimaryDir.Z, 0),
        FPlane(SecondaryDir.X, SecondaryDir.Y, SecondaryDir.Z, 0),
        FPlane(TertiaryDir.X, TertiaryDir.Y, TertiaryDir.Z, 0),
        FPlane(0, 0, 0, 1));

    FTransform Result;
    Result.SetLocation(Origin);
    Result.SetRotation(RotationMatrix.ToQuat());

    return Result;
}

// 辅助方法：计算骨骼需要绕主轴旋转的角度
static float CalculateRotationAngleToPlane(const FTransform& BoneTransform,
                                           const FVector& BonePosition,
                                           const FVector& PlaneNormal,
                                           const FVector& PoleTarget,
                                           const FVector& PrimaryAxisVector,
                                           const FVector& SecondaryAxisVector,
                                           float Tolerance = 0.001f) {
    // 1. 获取骨骼的实际次轴向量
    FVector BoneSecondaryAxis = SecondaryAxisVector;

    // 2. 计算次轴在参考平面上的投影
    // 投影 = 向量 - (向量·法线) * 法线
    float DotWithNormal = FVector::DotProduct(BoneSecondaryAxis, PlaneNormal);
    FVector ProjectedVector = BoneSecondaryAxis - (DotWithNormal * PlaneNormal);

    // 3. 如果投影太小（次轴几乎垂直于平面），无法确定方向
    if (ProjectedVector.SizeSquared() < Tolerance * Tolerance) {
        // 次轴垂直于平面，返回0（不需要旋转）
        return 0.0f;
    }

    // 4. 标准化投影向量
    ProjectedVector.Normalize();

    // 5. 计算从骨骼指向pole target的方向在平面上的投影
    FVector ToPoleTarget = (PoleTarget - BonePosition).GetSafeNormal();
    float DotToTargetWithNormal =
        FVector::DotProduct(ToPoleTarget, PlaneNormal);
    FVector TargetProjected =
        ToPoleTarget - (DotToTargetWithNormal * PlaneNormal);

    // 如果投影太小（目标方向几乎垂直于平面）
    if (TargetProjected.SizeSquared() < Tolerance * Tolerance) {
        // 使用与平面法线垂直的任意方向
        // 创建一个在平面内的参考方向
        FVector ArbitraryVector = FVector(1, 0, 0);
        if (FMath::Abs(FVector::DotProduct(ArbitraryVector, PlaneNormal)) >
            0.9f) {
            ArbitraryVector = FVector(0, 1, 0);
        }

        // 通过叉积得到平面内的向量
        TargetProjected = FVector::CrossProduct(PlaneNormal, ArbitraryVector);
        TargetProjected.Normalize();
    } else {
        TargetProjected.Normalize();
    }

    // 6. 计算投影向量到目标投影向量的角度
    float DotAngle = FVector::DotProduct(ProjectedVector, TargetProjected);
    float Angle = FMath::Acos(FMath::Clamp(DotAngle, -1.0f, 1.0f));

    // 7. 确定旋转方向（确保朝向pole target所在的一半平面）
    // 计算叉积来确定方向
    FVector CrossResult =
        FVector::CrossProduct(ProjectedVector, TargetProjected);
    float DirectionDot = FVector::DotProduct(CrossResult, PlaneNormal);

    // 如果叉积方向与平面法线相反，需要反转角度
    if (DirectionDot < 0) {
        Angle = -Angle;
    }

    return Angle;
}
// 辅助方法：计算参考平面法线（基于根、末端和pole target）
static FVector CalculateReferencePlaneNormal(const FVector& RootPosition,
                                             const FVector& EffectorPosition,
                                             const FVector& PoleTarget) {
    // 计算平面法线： (末端-根) × (pole target-根)
    FVector PlaneNormal =
        FVector::CrossProduct((EffectorPosition - RootPosition).GetSafeNormal(),
                              (PoleTarget - RootPosition).GetSafeNormal());

    // 如果叉积结果太小（共线），使用备用方案
    if (PlaneNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
        FVector RootToEffector =
            (EffectorPosition - RootPosition).GetSafeNormal();

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

static FTransform BuildTransformFromAxisDirections(
    const FVector& Origin, const FVector& PrimaryAxisPoint,
    const FVector& SecondaryAxisPoint, const FVector& PrimaryAxis,
    const FVector& SecondaryAxis) {
    // 计算世界空间方向
    FVector WorldPrimaryDir = (PrimaryAxisPoint - Origin).GetSafeNormal();
    FVector WorldSecondaryDir = (SecondaryAxisPoint - Origin).GetSafeNormal();

    // 正交化世界方向
    float Dot = FVector::DotProduct(WorldPrimaryDir, WorldSecondaryDir);
    WorldSecondaryDir =
        (WorldSecondaryDir - WorldPrimaryDir * Dot).GetSafeNormal();

    // 使用UE的RotationFromXZ或类似函数
    // 但更通用的是：

    // 1. 先对齐主轴
    FQuat Rotation =
        FQuat::FindBetweenNormals(PrimaryAxis.GetSafeNormal(), WorldPrimaryDir);

    // 2. 检查次轴对齐情况
    FVector CurrentSecondary =
        Rotation.RotateVector(SecondaryAxis.GetSafeNormal());
    float Alignment = FVector::DotProduct(CurrentSecondary, WorldSecondaryDir);

    // 如果对齐度不够好，可以尝试优化
    if (Alignment < 0.99f) {
        // 可以应用一个额外的绕主轴的小旋转来改进
        float Angle = FMath::Acos(FMath::Clamp(Alignment, -1.0f, 1.0f));
        FQuat AdditionalRotation = FQuat(WorldPrimaryDir, Angle);
        Rotation = AdditionalRotation * Rotation;
    }

    FTransform Result;
    Result.SetLocation(Origin);
    Result.SetRotation(Rotation.GetNormalized());

    return Result;
}

// 主修正方法
static void ApplySecondaryAxisCorrection(
    TArray<FCCDIKChainLink>& Chain, const FVector& RootPosition,
    const FVector& EffectorPosition, const FVector& PoleTarget,
    const FVector& PrimaryAxis,    // 主轴方向，如(1,0,0)
    const FVector& SecondaryAxis,  // 次轴方向，如(0,1,0)
    float CorrectionWeight = 1.0f, float SecondaryAxisDistance = 50.0f) {
    if (Chain.Num() < 2 || CorrectionWeight < KINDA_SMALL_NUMBER) return;

    // 1. 计算参考平面法线
    FVector PlaneNormal = CalculateReferencePlaneNormal(
        RootPosition, EffectorPosition, PoleTarget);

    if (PlaneNormal.IsNearlyZero()) return;

    // 2. 保存原始位置和缩放
    TArray<FVector> OriginalPositions;
    TArray<FVector> OriginalScales;
    OriginalPositions.SetNum(Chain.Num());
    OriginalScales.SetNum(Chain.Num());

    for (int32 i = 0; i < Chain.Num(); ++i) {
        OriginalPositions[i] = Chain[i].Transform.GetLocation();
        OriginalScales[i] = Chain[i].Transform.GetScale3D();
    }

    // 3. 为每个关节计算目标Transform
    TArray<FTransform> TargetTransforms;
    TargetTransforms.SetNum(Chain.Num());

    for (int32 i = 0; i < Chain.Num(); ++i) {
        FVector CurrentPosition = OriginalPositions[i];

        // 确定primary axis方向点
        FVector PrimaryAxisPoint;

        if (i < Chain.Num() - 1) {
            // 有下一个关节：primary axis指向下一个关节
            PrimaryAxisPoint = OriginalPositions[i + 1];
        } else {
            // 最后一个关节：使用前一个关节的方向
            if (i > 0) {
                FVector PrevToCurrent =
                    (CurrentPosition - OriginalPositions[i - 1])
                        .GetSafeNormal();
                PrimaryAxisPoint =
                    CurrentPosition + PrevToCurrent * 50.0f;  // 虚拟点
            } else {
                // 只有一个关节的情况（不应该发生）
                TargetTransforms[i] = Chain[i].Transform;
                continue;
            }
        }

        // 计算secondary axis方向点
        // 首先计算主轴在世界空间中的方向
        FVector WorldPrimaryDir =
            (PrimaryAxisPoint - CurrentPosition).GetSafeNormal();

        // 在平面上找到与主轴垂直且朝向pole target的方向
        FVector SecondaryAxisPoint = FindPointOnPlanePerpendicularToAxis(
            CurrentPosition, PlaneNormal, WorldPrimaryDir, PoleTarget,
            SecondaryAxisDistance);

        // 使用新的方法构建Transform
        TargetTransforms[i] = BuildTransformFromAxisDirections(
            CurrentPosition, PrimaryAxisPoint, SecondaryAxisPoint,
            PrimaryAxis,   // 传入的主轴参数
            SecondaryAxis  // 传入的次轴参数
        );

        // 保持原始缩放
        TargetTransforms[i].SetScale3D(OriginalScales[i]);
    }

    // 4. 应用修正的Transform（从根骨骼开始）
    for (int32 i = 0; i < Chain.Num(); ++i) {
        // 插值当前Transform和目标Transform
        FTransform CurrentTransform = Chain[i].Transform;
        FTransform TargetTransform = TargetTransforms[i];

        // 位置和缩放保持不变（使用原始值）
        FVector FinalPosition = OriginalPositions[i];
        FVector FinalScale = OriginalScales[i];

        // 旋转插值
        FQuat CurrentRotation = CurrentTransform.GetRotation();
        FQuat TargetRotation = TargetTransform.GetRotation();
        FQuat FinalRotation =
            FQuat::Slerp(CurrentRotation, TargetRotation, CorrectionWeight);

        // 创建最终的Transform
        FTransform FinalTransform;
        FinalTransform.SetLocation(FinalPosition);
        FinalTransform.SetRotation(FinalRotation);
        FinalTransform.SetScale3D(FinalScale);

        // 应用Transform到骨骼链
        Chain[i].Transform = FinalTransform;

        // 更新局部变换
        if (i > 0) {
            const FTransform& ParentTransform = Chain[i - 1].Transform;
            Chain[i].LocalTransform =
                FinalTransform.GetRelativeTransform(ParentTransform);
        }
    }

    // 5. 确保关节链长度不变（重新计算位置）
    for (int32 i = 1; i < Chain.Num(); ++i) {
        // 计算原始骨骼长度
        float OriginalLength =
            FVector::Dist(OriginalPositions[i], OriginalPositions[i - 1]);

        // 当前父骨骼位置
        FVector ParentPosition = Chain[i - 1].Transform.GetLocation();

        // 获取修正后的主轴方向（从旋转中提取）
        FVector WorldPrimaryDir =
            Chain[i - 1].Transform.GetRotation().RotateVector(
                PrimaryAxis.GetSafeNormal());

        // 保持原始长度，沿主轴方向放置子骨骼
        FVector CorrectedChildPosition =
            ParentPosition + WorldPrimaryDir * OriginalLength;

        // 更新子骨骼位置
        Chain[i].Transform.SetLocation(CorrectedChildPosition);

        // 更新局部变换
        Chain[i].LocalTransform =
            Chain[i].Transform.GetRelativeTransform(Chain[i - 1].Transform);
    }
}

// 改进的方法：直接计算目标旋转
static void ApplySecondaryAxisCorrectionV2(TArray<FCCDIKChainLink>& Chain,
                                           const FVector& RootPosition,
                                           const FVector& EffectorPosition,
                                           const FVector& PoleTarget,
                                           const FVector& PrimaryAxisVector,
                                           const FVector& SecondaryAxisVector,
                                           float CorrectionWeight = 1.0f) {
    if (Chain.Num() < 2 || CorrectionWeight < KINDA_SMALL_NUMBER) return;

    // 1. 保存所有关节的原始位置和旋转
    TArray<FVector> OriginalPositions;
    TArray<FQuat> OriginalRotations;
    OriginalPositions.SetNum(Chain.Num());
    OriginalRotations.SetNum(Chain.Num());

    for (int32 i = 0; i < Chain.Num(); ++i) {
        OriginalPositions[i] = Chain[i].Transform.GetLocation();
        OriginalRotations[i] = Chain[i].Transform.GetRotation();
    }

    // 2. 计算参考平面法线
    FVector PlaneNormal = CalculateReferencePlaneNormal(
        RootPosition, EffectorPosition, PoleTarget);

    if (PlaneNormal.IsNearlyZero()) return;

    // 3. 处理根骨骼
    if (Chain.Num() > 0) {
        // 计算根骨骼的旋转角度
        float RootRotationAngle = CalculateRotationAngleToPlane(
            Chain[0].Transform, OriginalPositions[0], PlaneNormal, PoleTarget,
            PrimaryAxisVector, SecondaryAxisVector);

        if (FMath::Abs(RootRotationAngle) > KINDA_SMALL_NUMBER) {
            RootRotationAngle *= CorrectionWeight;
            FVector RootRotationAxis = PrimaryAxisVector;
            FQuat RootRotationQuat = FQuat(RootRotationAxis, RootRotationAngle);
            Chain[0].Transform.SetRotation(
                (RootRotationQuat * OriginalRotations[0]).GetNormalized());
            Chain[0].Transform.SetLocation(OriginalPositions[0]);
        }
    }

    // 4. 处理其他骨骼
    for (int32 i = 1; i < Chain.Num(); ++i) {
        // 方法：在局部空间中计算需要的旋转

        // 先获取父骨骼的当前旋转
        FQuat ParentRotation = Chain[i - 1].Transform.GetRotation();
        FQuat InverseParentRotation = ParentRotation.Inverse();

        // 计算当前骨骼在局部空间中的方向
        FVector LocalDirection =
            (OriginalPositions[i] - OriginalPositions[i - 1]).GetSafeNormal();

        // 计算当前骨骼的世界空间方向（应用父骨骼旋转）
        FVector WorldDirection = ParentRotation.RotateVector(LocalDirection);

        // 创建一个临时的变换来计算旋转
        FTransform TempTransform;
        TempTransform.SetLocation(OriginalPositions[i - 1]);
        TempTransform.SetRotation(ParentRotation);

        // 沿着世界方向移动来创建当前骨骼的变换
        FVector TargetPosition =
            OriginalPositions[i - 1] + WorldDirection * LocalDirection.Size();
        FTransform CurrentBoneTransform;
        CurrentBoneTransform.SetLocation(TargetPosition);

        // 保持当前骨骼与父骨骼的相对旋转
        CurrentBoneTransform.SetRotation(ParentRotation *
                                         Chain[i].LocalTransform.GetRotation());

        // 计算需要的旋转角度
        float RotationAngle = CalculateRotationAngleToPlane(
            CurrentBoneTransform, TargetPosition, PlaneNormal, PoleTarget,
            PrimaryAxisVector, SecondaryAxisVector);

        if (FMath::Abs(RotationAngle) > KINDA_SMALL_NUMBER) {
            RotationAngle *= CorrectionWeight;

            // 在局部空间中应用旋转
            FVector LocalRotationAxis = PrimaryAxisVector;
            FQuat LocalRotationQuat = FQuat(LocalRotationAxis, RotationAngle);

            // 计算新的世界旋转
            FQuat NewWorldRotation =
                LocalRotationQuat * CurrentBoneTransform.GetRotation();

            // 应用旋转，保持位置不变
            Chain[i].Transform.SetRotation(NewWorldRotation);
            Chain[i].Transform.SetLocation(OriginalPositions[i]);

            // 更新局部变换
            Chain[i].LocalTransform =
                Chain[i].Transform.GetRelativeTransform(Chain[i - 1].Transform);
        } else {
            // 保持原始旋转
            Chain[i].Transform.SetRotation(OriginalRotations[i]);
            Chain[i].Transform.SetLocation(OriginalPositions[i]);
        }
    }
}

static void ApplyPoleRotationToChain(
    TArray<FCCDIKChainLink>& Chain, const FQuat& TargetRotation,
    float PoleWeight,
    int32 PivotIndex = 0) {  // 默认以第一个关节为轴
    if (Chain.Num() == 0 || PoleWeight < KINDA_SMALL_NUMBER) return;

    // 1. 确定旋转中心（通常是根关节或指定的轴关节）
    int32 RotationCenterIndex = FMath::Clamp(PivotIndex, 0, Chain.Num() - 1);
    FVector CenterPos = Chain[RotationCenterIndex].Transform.GetLocation();

    // 2. 应用旋转
    FQuat FinalRotation =
        FQuat::Slerp(FQuat::Identity, TargetRotation, PoleWeight);

    // 3. 旋转链上旋转中心之后的所有关节
    for (int32 i = RotationCenterIndex; i < Chain.Num(); ++i) {
        // 相对于旋转中心的位置
        FVector LocalPos = Chain[i].Transform.GetLocation() - CenterPos;
        FVector RotatedPos = FinalRotation.RotateVector(LocalPos);
        Chain[i].Transform.SetLocation(CenterPos + RotatedPos);

        // 更新旋转
        if (i == RotationCenterIndex) {
            // 旋转中心关节直接应用
            Chain[i].Transform.SetRotation(
                (FinalRotation * Chain[i].Transform.GetRotation())
                    .GetNormalized());
        } else {
            // 子关节：保持相对于父关节的局部旋转
            // 这确保关节链保持相对角度不变
            const FTransform& ParentTransform = Chain[i - 1].Transform;
            const FTransform& LocalTransform = Chain[i].LocalTransform;

            Chain[i].Transform.SetRotation(
                (ParentTransform.GetRotation() * LocalTransform.GetRotation())
                    .GetNormalized());
        }
    }
}

FRigUnit_IKWithPole_Execute() {
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
    // 构造CCDIK链
    TArray<FCCDIKChainLink> CCDIKChain;
    CCDIKChain.SetNum(NumChainLinks);
    for (int32 i = 0; i < NumChainLinks; ++i) {
        const FCachedRigElement& CachedBone = WorkData.CachedItems[i];
        FTransform BoneTransform =
            Hierarchy->GetGlobalTransform(CachedBone.GetIndex());
        CCDIKChain[i].Transform = BoneTransform;
        if (i > 0) {
            const FCachedRigElement& ParentBone = WorkData.CachedItems[i - 1];
            FTransform ParentTransform =
                Hierarchy->GetGlobalTransform(ParentBone.GetIndex());
            CCDIKChain[i].LocalTransform =
                BoneTransform.GetRelativeTransform(ParentTransform);
        } else {
            CCDIKChain[i].LocalTransform = BoneTransform;
        }
        CCDIKChain[i].CurrentAngleDelta = 0.0;
    }
    // Rotation limits
    TArray<float> RotationLimitsPerJoint;
    RotationLimitsPerJoint.SetNum(NumChainLinks);
    for (int32 i = 0; i < NumChainLinks; ++i) {
        RotationLimitsPerJoint[i] = BaseRotationLimit;
    }
    // CCDIK算法主流程（官方实现）
    AnimationCore::SolveCCDIK(CCDIKChain, EffectorTransform.GetLocation(),
                              Precision > 0.f ? Precision : 0.001f,
                              MaxIterations > 0 ? MaxIterations : 10,
                              bStartFromTail,
                              false,  // bEnableRotationLimit
                              RotationLimitsPerJoint);
    // CCDIK结果写回BonePositions
    TArray<FVector> BonePositions;
    BonePositions.SetNum(NumChainLinks);
    for (int32 i = 0; i < NumChainLinks; ++i) {
        BonePositions[i] = CCDIKChain[i].Transform.GetLocation();
    }
    // CCD IK后，pole target平面修正
    FVector RootPosition = BonePositions[0];
    FVector PlaneNormal =
        FVector::CrossProduct(EffectorTransform.GetLocation() - RootPosition,
                              PoleTarget - RootPosition)
            .GetSafeNormal();
    if (PlaneNormal.IsZero()) {
        FVector BaseVector =
            (EffectorTransform.GetLocation() - RootPosition).GetSafeNormal();
        if (FMath::Abs(BaseVector.Z) < 0.9f) {
            PlaneNormal = FVector::CrossProduct(BaseVector, FVector(0, 0, 1))
                              .GetSafeNormal();
        } else {
            PlaneNormal = FVector::CrossProduct(BaseVector, FVector(1, 0, 0))
                              .GetSafeNormal();
        }
    }

    int MiddleIndex = NumChainLinks / 2;
    FVector MiddlePosition = BonePositions[MiddleIndex];
    FVector CunrrentPlaneNormal =
        FVector::CrossProduct(EffectorTransform.GetLocation() - RootPosition,
                              MiddlePosition - RootPosition)
            .GetSafeNormal();

    FQuat TargetRotator =
        FQuat::FindBetweenNormals(CunrrentPlaneNormal, PlaneNormal);

    ApplyPoleRotationToChain(CCDIKChain, TargetRotator, 1.0, 0);

    ApplySecondaryAxisCorrection(CCDIKChain, RootPosition,
                                 EffectorTransform.GetLocation(), PoleTarget,
                                 PrimaryAxis, SecondAxis, Weight);

    // 写回Hierarchy
    for (int32 i = 0; i < NumChainLinks; ++i) {
        const FCachedRigElement& CachedBone = WorkData.CachedItems[i];
        if (CachedBone.IsValid()) {
            // 检查这个骨骼是否在我们的链中（防止传播到外部）
            bool bIsInOurChain = false;
            for (int32 j = 0; j < WorkData.CachedItems.Num(); ++j) {
                if (WorkData.CachedItems[j].GetKey() == CachedBone.GetKey()) {
                    bIsInOurChain = true;
                    break;
                }
            }

            if (bIsInOurChain) {
                // 对于链内的骨骼，不需要传播，因为我们会按顺序设置
                Hierarchy->SetGlobalTransform(
                    CachedBone.GetKey(), CCDIKChain[i].Transform,
                    false,   // bInitial
                    true,    // bAffectChildren
                    false);  // bPropagateToChildren - 设为false!
            }
        }
    }
}