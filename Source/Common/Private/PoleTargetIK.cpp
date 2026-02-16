
#include "PoleTargetIK.h"

#include "AnimationCore.h"
#include "ControlRig.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "RigVMDeveloper/Public/RigVMModel/RigVMFunctionLibrary.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoleTargetIK)

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

// 辅助方法：计算参考平面法线（基于根、末端和pole target）
static FVector CalculateReferencePlaneNormalOld(const FVector& RootPosition,
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
    TArray<FCCDIKChainLink>& Chain, const FVector& EffectorPosition,
    const FVector& PoleTarget,
    const FVector& PrimaryAxis,    // 主轴方向，如(1,0,0)
    const FVector& SecondaryAxis,  // 次轴方向，如(0,1,0)
    float CorrectionWeight = 1.0f, float SecondaryAxisDistance = 50.0f) {
    if (Chain.Num() < 2 || CorrectionWeight < KINDA_SMALL_NUMBER) return;

    FVector RootPosition = Chain[0].Transform.GetLocation();

    // 1. 计算参考平面法线
    FVector PlaneNormal = CalculateReferencePlaneNormalOld(
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

    if (bUseSecondaryAxisCorrection) {
        ApplySecondaryAxisCorrection(
            CCDIKChain, EffectorTransform.GetLocation(), PoleTarget,
            PrimaryAxis, SecondAxis, Weight);
    }

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