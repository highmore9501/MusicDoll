#include "ArcDistributedIK.h"

#include "AnimationCore.h"
#include "ControlRig.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "RigVMDeveloper/Public/RigVMModel/RigVMFunctionLibrary.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcDistributedIK)

struct FArcDistributedIKData {
    TArray<float> BoneLengths;
    float TotalChainLength;
    FVector ReferencePlaneNormal;
    FVector RootPosition;
    FVector InitialEffectorDistance;
};

FRigUnit_ArcDistributedIK_Execute() {
    struct Local {
        // ========================================
        // 数据结构和基础计算
        // ========================================

        static float CalculateBoneLength(const FVector& Start,
                                         const FVector& End) {
            return FVector::Dist(Start, End);
        }

        static FVector CalculateReferencePlaneNormal(
            const FVector& RootPosition, const FVector& EffectorPosition,
            const FVector& PoleTarget) {
            FVector RootToEffector =
                (EffectorPosition - RootPosition).GetSafeNormal();
            FVector RootToPole = (PoleTarget - RootPosition).GetSafeNormal();

            FVector PlaneNormal =
                FVector::CrossProduct(RootToEffector, RootToPole);

            if (PlaneNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
                PlaneNormal =
                    FVector::CrossProduct(RootToEffector, FVector::UpVector);

                if (PlaneNormal.IsNearlyZero(KINDA_SMALL_NUMBER)) {
                    PlaneNormal = FVector::CrossProduct(RootToEffector,
                                                        FVector::ForwardVector);
                }
            }

            return PlaneNormal.GetSafeNormal();
        }

        static FArcDistributedIKData GatherChainData(
            const TArray<FCCDIKChainLink>& Chain,
            const FVector& EffectorPosition, const FVector& PoleTarget) {
            FArcDistributedIKData Data;

            if (Chain.Num() < 2) {
                return Data;
            }

            Data.BoneLengths.SetNum(Chain.Num());
            Data.TotalChainLength = 0.0f;

            for (int32 i = 0; i < Chain.Num() - 1; ++i) {
                float Length = CalculateBoneLength(
                    Chain[i].Transform.GetLocation(),
                    Chain[i + 1].Transform.GetLocation());
                Data.BoneLengths[i] = Length;
                Data.TotalChainLength += Length;
            }

            Data.BoneLengths[Chain.Num() - 1] = 0.0f;
            Data.RootPosition = Chain[0].Transform.GetLocation();
            Data.ReferencePlaneNormal = CalculateReferencePlaneNormal(
                Data.RootPosition, EffectorPosition, PoleTarget);

            return Data;
        }

        // ========================================
        // 算法判断
        // ========================================

        static bool IsEffectorTooFar(float TotalChainLength,
                                     float EffectorDistance) {
            return EffectorDistance > TotalChainLength;
        }

        static bool DetermineAlgorithmBranch(float TotalChainLength,
                                             float EffectorDistance,
                                             int32& OutAlgorithmType) {
            if (IsEffectorTooFar(TotalChainLength, EffectorDistance)) {
                OutAlgorithmType = 1;
                return true;
            }

            OutAlgorithmType = 2;
            return true;
        }

        // ========================================
        // 链条变换
        // ========================================

        static float CalculateEffectorDistance(
            const TArray<FCCDIKChainLink>& Chain,
            const FVector& EffectorPosition) {
            if (Chain.Num() < 1) {
                return 0.0f;
            }

            FVector EndEffectorPosition =
                Chain[Chain.Num() - 1].Transform.GetLocation();
            return FVector::Dist(EndEffectorPosition, EffectorPosition);
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
                CurrentPosition =
                    CurrentPosition + Direction * BoneLengths[i - 1];
                Chain[i].Transform.SetLocation(CurrentPosition);
            }
        }

        static FVector RotatePointAroundAxis(const FVector& Position,
                                             const FVector& PivotPoint,
                                             const FVector& RotationAxis,
                                             float Angle) {
            FVector RelativePos = Position - PivotPoint;
            FQuat RotationQuat =
                FQuat(RotationAxis.GetSafeNormal(), Angle);
            FVector RotatedRelativePos =
                RotationQuat.RotateVector(RelativePos);
            return PivotPoint + RotatedRelativePos;
        }

        static void RotateChainAroundAxis(TArray<FCCDIKChainLink>& Chain,
                                          const FVector& RootPosition,
                                          const FVector& RotationAxis,
                                          float Angle) {
            for (int32 i = 1; i < Chain.Num(); ++i) {
                FVector OldPos = Chain[i].Transform.GetLocation();
                FVector NewPos = RotatePointAroundAxis(OldPos, RootPosition,
                                                       RotationAxis, Angle);
                Chain[i].Transform.SetLocation(NewPos);
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

            StretchChainAlongDirection(Chain, BoneLengths,
                                       DirectionToEffector);
        }

        // ========================================
        // 准备阶段
        // ========================================

        static float PreparePhaseStretchChain(TArray<FCCDIKChainLink>& Chain,
                                              const TArray<float>& BoneLengths,
                                              const FVector& EffectorPosition) {
            if (Chain.Num() < 1) {
                return 0.0f;
            }

            FVector RootPosition = Chain[0].Transform.GetLocation();
            FVector DirectionToEffector =
                (EffectorPosition - RootPosition).GetSafeNormal();

            StretchChainAlongDirection(Chain, BoneLengths,
                                       DirectionToEffector);

            return CalculateEffectorDistance(Chain, EffectorPosition);
        }

        // ========================================
        // 旋转重建相关方法
        // ========================================

        static FVector FindSecondaryAxisPointOnPlane(
            const FVector& CurrentPosition, const FVector& PlaneNormal,
            const FVector& PrimaryDirection, const FVector& MiddlePosition,
            const FVector& PoleTarget, bool bUseMiddlePosition,
            float Distance) {
            FVector PerpendicularInPlane =
                FVector::CrossProduct(PlaneNormal,
                                      PrimaryDirection.GetSafeNormal());

            if (PerpendicularInPlane.IsNearlyZero()) {
                if (FMath::Abs(PrimaryDirection.X) < 0.9f) {
                    PerpendicularInPlane = FVector::CrossProduct(
                        PrimaryDirection, FVector::RightVector);
                } else {
                    PerpendicularInPlane = FVector::CrossProduct(
                        PrimaryDirection, FVector::ForwardVector);
                }
            }

            PerpendicularInPlane.Normalize();

            FVector ReferencePosition =
                bUseMiddlePosition ? MiddlePosition : PoleTarget;

            if (!ReferencePosition.IsNearlyZero()) {
                FVector ToReference = ReferencePosition - CurrentPosition;

                if (ToReference.Length() > KINDA_SMALL_NUMBER) {
                    ToReference = ToReference.GetSafeNormal();
                    float DotWithReference = FVector::DotProduct(
                        PerpendicularInPlane, ToReference);

                    if (DotWithReference < 0) {
                        PerpendicularInPlane = -PerpendicularInPlane;
                    }
                }
            }

            return CurrentPosition + PerpendicularInPlane * Distance;
        }

        static FQuat BuildRotationFromTwoAxes(
            const FVector& PrimaryDir, const FVector& SecondaryDir,
            const FVector& LocalPrimaryAxis,
            const FVector& LocalSecondaryAxis, const FVector& PlaneNormal) {
            FVector WorldX = PrimaryDir.GetSafeNormal();
            FVector WorldY = SecondaryDir.GetSafeNormal();
            FVector WorldZ = PlaneNormal.GetSafeNormal();

            FVector LocalX = LocalPrimaryAxis.GetSafeNormal();
            FVector LocalY = LocalSecondaryAxis.GetSafeNormal();
            FVector LocalZ =
                FVector::CrossProduct(LocalX, LocalY).GetSafeNormal();

            FMatrix LocalToWorldMatrix(
                FPlane(WorldX.X, WorldX.Y, WorldX.Z, 0.0f),
                FPlane(WorldY.X, WorldY.Y, WorldY.Z, 0.0f),
                FPlane(WorldZ.X, WorldZ.Y, WorldZ.Z, 0.0f),
                FPlane(0.0f, 0.0f, 0.0f, 1.0f));

            FMatrix LocalAxisMatrix(FPlane(LocalX.X, LocalX.Y, LocalX.Z, 0.0f),
                                    FPlane(LocalY.X, LocalY.Y, LocalY.Z, 0.0f),
                                    FPlane(LocalZ.X, LocalZ.Y, LocalZ.Z, 0.0f),
                                    FPlane(0.0f, 0.0f, 0.0f, 1.0f));

            FMatrix RotationMatrix =
                LocalAxisMatrix.Inverse() * LocalToWorldMatrix;

            FQuat ResultQuat(RotationMatrix);
            return ResultQuat.GetNormalized();
        }

        static FQuat CalculateBoneRotation(
            const FVector& CurrentPosition, const FVector& NextPosition,
            const FVector& PlaneNormal, const FVector& PrimaryAxis,
            const FVector& SecondaryAxis, const FVector& MiddlePosition,
            const FVector& PoleTarget, int32 AlgorithmType,
            bool bIsLastBone) {
            FVector WorldPrimaryDir =
                (NextPosition - CurrentPosition).GetSafeNormal();

            FVector SecondaryAxisPoint = FindSecondaryAxisPointOnPlane(
                CurrentPosition, PlaneNormal, WorldPrimaryDir, MiddlePosition,
                PoleTarget, true, 50.0f);

            FVector WorldSecondaryDir =
                (CurrentPosition - SecondaryAxisPoint).GetSafeNormal();

            FQuat Rotation = BuildRotationFromTwoAxes(
                WorldPrimaryDir, WorldSecondaryDir, PrimaryAxis, SecondaryAxis,
                PlaneNormal);

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

            FVector RootPosition = Chain[0].Transform.GetLocation();
            FVector EffectorPosition =
                Chain[Chain.Num() - 1].Transform.GetLocation();
            FVector MiddlePosition = (RootPosition + EffectorPosition) * 0.5f;
            float TotalChainLength =
                CalculateBoneLength(RootPosition, EffectorPosition);

            FVector DirectionToPole =
                (PoleTarget - MiddlePosition).GetSafeNormal();
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
                    NextPosition = Chain[i + 1].Transform.GetLocation();
                } else {
                    FVector PrevToCurrent =
                        (CurrentPosition -
                         Chain[i - 1].Transform.GetLocation())
                            .GetSafeNormal();
                    NextPosition = CurrentPosition + PrevToCurrent * 50.0f;
                }

                FQuat NewRotation = CalculateBoneRotation(
                    CurrentPosition, NextPosition, ReferencePlaneNormal,
                    PrimaryAxis, SecondaryAxis, AdjustedMiddlePosition,
                    PoleTarget, AlgorithmType, bIsLastBone);

                Chain[i].Transform.SetRotation(NewRotation);
            }
        }

        // ========================================
        // FABRIK 求解器
        // ========================================

        static void ApplyFABRIKSolver(TArray<FCCDIKChainLink>& Chain,
                                      const TArray<float>& BoneLengths,
                                      const FVector& EffectorPosition,
                                      float Precision, int32 MaxIterations) {
            if (Chain.Num() < 2) {
                return;
            }

            FVector RootPosition = Chain[0].Transform.GetLocation();
            const int32 LastLinkIndex = Chain.Num() - 1;

            for (int32 Iter = 0; Iter < MaxIterations; ++Iter) {
                float CurrentDistance =
                    CalculateEffectorDistance(Chain, EffectorPosition);
                if (CurrentDistance < Precision) {
                    return;
                }

                Chain[LastLinkIndex].Transform.SetLocation(EffectorPosition);

                for (int32 i = LastLinkIndex - 1; i >= 0; --i) {
                    FVector CurrentPos = Chain[i].Transform.GetLocation();
                    FVector NextPos =
                        Chain[i + 1].Transform.GetLocation();
                    FVector Direction = (CurrentPos - NextPos).GetSafeNormal();
                    FVector NewPos =
                        NextPos + Direction * BoneLengths[i];
                    Chain[i].Transform.SetLocation(NewPos);
                }

                Chain[0].Transform.SetLocation(RootPosition);

                for (int32 i = 0; i < LastLinkIndex; ++i) {
                    FVector CurrentPos = Chain[i].Transform.GetLocation();
                    FVector NextPos =
                        Chain[i + 1].Transform.GetLocation();
                    FVector Direction = (NextPos - CurrentPos).GetSafeNormal();
                    FVector NewPos =
                        CurrentPos + Direction * BoneLengths[i];
                    Chain[i + 1].Transform.SetLocation(NewPos);
                }
            }
        }

        static void IterativePhaseFABRIK(
            TArray<FCCDIKChainLink>& Chain,
            const TArray<float>& BoneLengths,
            const FVector& EffectorPosition, const FVector& PoleTarget,
            const FVector& ReferencePlaneNormal, float Precision,
            int32 MaxIterations) {
            if (Chain.Num() < 1) {
                return;
            }

            FVector RootPosition = Chain[0].Transform.GetLocation();

            FVector RotationAxis = ReferencePlaneNormal.GetSafeNormal();
            const float RotationAngle = PI / 2.0f;
            RotateChainAroundAxis(Chain, RootPosition, RotationAxis,
                                  RotationAngle);

            ApplyFABRIKSolver(Chain, BoneLengths, EffectorPosition, Precision,
                              MaxIterations);
        }

        // ========================================
        // 写入层级
        // ========================================

        static void WriteChainToHierarchy(
            FControlRigExecuteContext& ExecuteContext,
            const TArray<FCachedRigElement>& CachedItems,
            const TArray<FCCDIKChainLink>& Chain,
            bool bPropagateToChildren) {
            URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
            if (!Hierarchy || CachedItems.Num() != Chain.Num()) {
                return;
            }

            for (int32 i = 0; i < CachedItems.Num(); ++i) {
                const FCachedRigElement& CachedBone = CachedItems[i];
                if (CachedBone.IsValid()) {
                    Hierarchy->SetGlobalTransform(
                        CachedBone.GetKey(), Chain[i].Transform, false,
                        bPropagateToChildren, false);
                }
            }
        }
    };

    // ============================================================================
    // 执行主逻辑
    // ============================================================================

    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy) {
        return;
    }

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

    FTransform RootParentTransform = FTransform::Identity;
    FRigElementKey RootBoneKey = WorkData.CachedItems[0].GetKey();
    FRigBoneElement* RootBone = Hierarchy->Find<FRigBoneElement>(RootBoneKey);

    if (RootBone && RootBone->ParentElement) {
        RootParentTransform = RootBone->ParentElement->GetTransform().Get(
            ERigTransformType::CurrentGlobal);
    }

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
            Chain[i].LocalTransform =
                BoneTransform.GetRelativeTransform(RootParentTransform);
        }
        Chain[i].CurrentAngleDelta = 0.0;
    }

    // Phase 1: Data gathering
    FArcDistributedIKData Data = Local::GatherChainData(
        Chain, EffectorTransform.GetLocation(), PoleTarget);

    if (Data.ReferencePlaneNormal.IsNearlyZero()) {
        return;
    }

    // Phase 2: Algorithm branching
    int32 AlgorithmType = 0;
    float EffectorDistance =
        FVector::Dist(Data.RootPosition, EffectorTransform.GetLocation());

    bool bEffectorTooClose = false;
    if (Chain.Num() >= 2 && Data.BoneLengths.Num() >= 2) {
        float MaxBoneLength = 0.0f;
        for (int32 i = 0; i < Data.BoneLengths.Num() - 1; ++i) {
            MaxBoneLength = FMath::Max(MaxBoneLength, Data.BoneLengths[i]);
        }

        float OtherBonesLength = Data.TotalChainLength - MaxBoneLength;
        float MinReachableDistance = MaxBoneLength - OtherBonesLength;
        bEffectorTooClose = (EffectorDistance < MinReachableDistance);
    }

    if (bEffectorTooClose) {
        return;
    }

    if (!Local::DetermineAlgorithmBranch(Data.TotalChainLength,
                                         EffectorDistance, AlgorithmType)) {
        return;
    }

    // Phase 3: Position calculation
    if (AlgorithmType == 1) {
        Local::HandleTooFarCase(Chain, Data.BoneLengths,
                                EffectorTransform.GetLocation());
    } else if (AlgorithmType == 2) {
        float InitialDistance = Local::PreparePhaseStretchChain(
            Chain, Data.BoneLengths, EffectorTransform.GetLocation());

        Local::IterativePhaseFABRIK(
            Chain, Data.BoneLengths, EffectorTransform.GetLocation(),
            PoleTarget, Data.ReferencePlaneNormal,
            Precision > 0.f ? Precision : 0.001f,
            MaxIterations > 0 ? MaxIterations : 10);
    }

    // Phase 4: Rotation rebuild
    Local::RebuildRotationsForChain(Chain, Data.BoneLengths,
                                    Data.ReferencePlaneNormal, PrimaryAxis,
                                    SecondAxis, PoleTarget,
                                    RootParentTransform, AlgorithmType);

    // Phase 5: Write to hierarchy
    Local::WriteChainToHierarchy(ExecuteContext, WorkData.CachedItems, Chain,
                                 bPropagateToChildren);
}
