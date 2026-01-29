// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoleTargetFABRIK.h"

#include "AnimationCore.h"
#include "ControlRig.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "RigVMDeveloper/Public/RigVMModel/RigVMFunctionLibrary.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoleTargetFABRIK)

// --- 只对根骨骼做pole平面修正 ---
static void ApplyPolePlaneCorrection_RootOnly(
    TArray<FVector>& BonePositions, const FVector& PoleTarget,
    URigHierarchy* Hierarchy, const TArray<FCachedRigElement>& CachedItems,
    float Weight, bool bPropagateToChildren) {
    int32 NumChainLinks = BonePositions.Num();
    if (NumChainLinks < 3) return;  // 至少3个骨骼
    int32 MiddleIndex = FMath::RoundToInt((NumChainLinks - 1) / 2.0f);
    FVector StartPos = BonePositions[0];
    FVector EndPos = BonePositions.Last();
    FVector MiddlePos = BonePositions[MiddleIndex];
    FVector ChainAxis = (EndPos - StartPos).GetSafeNormal();
    // 计算目标平面法线
    FVector PlaneNormal =
        FVector::CrossProduct(EndPos - StartPos, PoleTarget - StartPos)
            .GetSafeNormal();
    // middle bone到平面的投影
    float PlaneD = -FVector::DotProduct(PlaneNormal, PoleTarget);
    float DistToPlane = FVector::DotProduct(MiddlePos, PlaneNormal) + PlaneD;
    FVector MiddleToPlane = -DistToPlane * PlaneNormal;
    FVector MiddleTarget = MiddlePos + MiddleToPlane;
    FVector MiddleVec = (MiddlePos - StartPos);
    FVector TargetVec = (MiddleTarget - StartPos);
    float Angle =
        FMath::Acos(FMath::Clamp(FVector::DotProduct(MiddleVec.GetSafeNormal(),
                                                     TargetVec.GetSafeNormal()),
                                 -1.f, 1.f));
    float Sign = FVector::DotProduct(
                     ChainAxis, FVector::CrossProduct(MiddleVec, TargetVec)) < 0
                     ? -1.f
                     : 1.f;
    float FinalAngle = Angle * Sign;
    const float SideThreshold = 0.01f;
    FVector ToPoleTarget = (PoleTarget - MiddleTarget).GetSafeNormal();
    FVector ToMiddle = (MiddlePos - MiddleTarget).GetSafeNormal();
    if (FVector::DotProduct(ToPoleTarget, ToMiddle) < -SideThreshold) {
        FinalAngle += PI;
    }
    if (FMath::Abs(FinalAngle) > KINDA_SMALL_NUMBER) {
        FQuat RotQuat(ChainAxis, FinalAngle);
        // 只对根骨骼做旋转
        const FCachedRigElement& RootBone = CachedItems[0];
        if (RootBone.IsValid()) {
            FTransform OrigTransform =
                Hierarchy->GetGlobalTransform(RootBone.GetIndex());
            FTransform NewTransform = OrigTransform;
            NewTransform.SetRotation(
                (RotQuat * OrigTransform.GetRotation()).GetNormalized());
            if (Weight >= 1.f) {
                Hierarchy->SetGlobalTransform(RootBone.GetKey(), NewTransform,
                                              false, true,
                                              bPropagateToChildren);
            } else {
                FTransform InterpTransform;
                InterpTransform.Blend(OrigTransform, NewTransform, Weight);
                Hierarchy->SetGlobalTransform(RootBone.GetKey(),
                                              InterpTransform, false, true,
                                              bPropagateToChildren);
            }
        }
    }
}

FRigUnit_FABRIKWithPole_Execute() {
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
    CCDIKChain.SetNum(NumChainLinks);  // 保证成员初始化
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
    // 先将CCDIK结果写回Hierarchy（位置+旋转）
    for (int32 i = 0; i < NumChainLinks; ++i) {
        const FCachedRigElement& CachedBone = WorkData.CachedItems[i];
        if (CachedBone.IsValid()) {
            Hierarchy->SetGlobalTransform(CachedBone.GetKey(),
                                          CCDIKChain[i].Transform, false, true,
                                          bPropagateToChildren);
        }
    }
    // CCDIK结果写回BonePositions用于pole修正
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

    ApplyPolePlaneCorrection_RootOnly(BonePositions, PoleTarget, Hierarchy,
                                      WorkData.CachedItems, Weight,
                                      bPropagateToChildren);
}