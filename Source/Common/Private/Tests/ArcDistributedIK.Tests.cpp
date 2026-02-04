// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcDistributedIK.h"

#include "ControlRig/Public/Units/Highlevel/Hierarchy/RigUnit_CCDIK.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

// ============================================================================
// 自动化测试
// ============================================================================

/**
 * 测试：基础结构初始化
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcDistributedIK_BasicStructure,
                                 "MusicDoll.IK.ArcDistributed.BasicStructure",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_BasicStructure::RunTest(const FString& Parameters) {
    // 创建测试用的RigUnit结构
    FRigUnit_ArcDistributedIK IKUnit;

    // 验证初始化值
    TestTrue(TEXT("PrimaryAxis应该初始化为(1,0,0)"),
             IKUnit.PrimaryAxis.Equals(FVector(1.0f, 0.0f, 0.0f)));
    TestTrue(TEXT("SecondAxis应该初始化为(0,1,0)"),
             IKUnit.SecondAxis.Equals(FVector(0.0f, 1.0f, 0.0f)));
    TestTrue(TEXT("PoleTarget应该初始化为零向量"), IKUnit.PoleTarget.IsZero());

    return true;
}

/**
 * 测试：骨骼长度计算
 * 验证 CalculateBoneLength 函数
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArcDistributedIK_BoneLengthCalculation,
    "MusicDoll.IK.ArcDistributed.BoneLengthCalculation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_BoneLengthCalculation::RunTest(
    const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    // 测试数据
    const float BoneLength = 100.0f;
    FVector Start = FVector::ZeroVector;
    FVector End = FVector(BoneLength, 0.0f, 0.0f);

    // 调用实际的骨骼长度计算函数
    float CalculatedLength = CalculateBoneLength(Start, End);

    // 验证结果
    TestTrue(FString::Printf(TEXT("骨骼长度应该是 %.2f，实际: %.2f"),
                             BoneLength, CalculatedLength),
             FMath::Abs(CalculatedLength - BoneLength) < 0.01f);

    return true;
}

/**
 * 测试：Effector太远的情况
 * 当effector距离超过总链长时，返回true
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcDistributedIK_EffectorTooFar,
                                 "MusicDoll.IK.ArcDistributed.EffectorTooFar",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_EffectorTooFar::RunTest(const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    // 测试数据
    const float TotalChainLength = 300.0f;
    const float TooFarDistance = TotalChainLength * 1.5f;

    // 调用实际的函数来检查effector是否太远
    bool bIsTooFar = IsEffectorTooFar(TotalChainLength, TooFarDistance);

    // 验证结果
    TestTrue(TEXT("当effector距离大于总链长时，应该返回true"), bIsTooFar);

    // 测试正常距离情况
    const float NormalDistance = TotalChainLength * 0.5f;
    bool bIsNormalTooFar = IsEffectorTooFar(TotalChainLength, NormalDistance);
    TestTrue(TEXT("当effector距离小于总链长时，应该返回false"),
             !bIsNormalTooFar);

    return true;
}

/**
 * 测试：Effector太近的情况
 * 当effector距离小于总链长/(2π)时，返回true
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcDistributedIK_EffectorTooClose,
                                 "MusicDoll.IK.ArcDistributed.EffectorTooClose",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_EffectorTooClose::RunTest(const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    const float TotalChainLength = 300.0f;
    const float MinDistance = TotalChainLength / (2.0f * PI);
    const float TooCloseDistance = MinDistance * 0.5f;

    // 调用实际的函数来检查effector是否太近
    bool bIsTooClose = IsEffectorTooClose(TotalChainLength, TooCloseDistance);

    // 验证结果
    TestTrue(TEXT("当effector距离小于最小距离时，应该返回true"), bIsTooClose);

    // 测试正常距离情况
    const float NormalDistance = TotalChainLength * 0.5f;
    bool bIsNormalTooClose =
        IsEffectorTooClose(TotalChainLength, NormalDistance);
    TestTrue(TEXT("当effector距离大于最小距离时，应该返回false"),
             !bIsNormalTooClose);

    return true;
}

/**
 * 测试：参考平面法线计算
 * 验证CalculateReferencePlaneNormal函数的正确性
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArcDistributedIK_ReferencePlaneNormal,
    "MusicDoll.IK.ArcDistributed.ReferencePlaneNormal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_ReferencePlaneNormal::RunTest(
    const FString& Parameters) {
    // 测试参考平面法线的计算
    FVector RootPosition = FVector::ZeroVector;
    FVector EffectorPosition = FVector(100.0f, 0.0f, 0.0f);
    FVector PoleTarget = FVector(50.0f, 50.0f, 0.0f);

    // 调用实际实现的平面法线计算函数 - 使用完全限定名
    FVector PlaneNormal =
        FArcDistributedIKHelper::CalculateReferencePlaneNormal(
            RootPosition, EffectorPosition, PoleTarget);

    // 验证法线不为零
    TestTrue(TEXT("参考平面法线不应该为零"), !PlaneNormal.IsNearlyZero());

    // 验证法线长度约为1（因为已normalize）
    TestTrue(FString::Printf(TEXT("法线长度应该接近1，实际: %.4f"),
                             PlaneNormal.Length()),
             FMath::Abs(PlaneNormal.Length() - 1.0f) < 0.01f);

    return true;
}

/**
 * 测试：向量旋转
 * 验证 RotatePointAroundAxis 函数的正确性
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcDistributedIK_VectorRotation,
                                 "MusicDoll.IK.ArcDistributed.VectorRotation",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_VectorRotation::RunTest(const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    // 测试向量旋转
    FVector Position = FVector(100.0f, 0.0f, 0.0f);
    FVector PivotPoint = FVector::ZeroVector;
    FVector RotationAxis = FVector(0.0f, 0.0f, 1.0f);  // Z轴

    // 旋转90度
    float Angle = PI / 2.0f;  // 90度
    FVector RotatedPos =
        RotatePointAroundAxis(Position, PivotPoint, RotationAxis, Angle);

    // 验证旋转后的位置
    // 预期：(0, 100, 0)，允许小的浮点误差
    TestTrue(
        FString::Printf(TEXT("旋转后X应该接近0，实际: %.4f"), RotatedPos.X),
        FMath::Abs(RotatedPos.X) < 1.0f);
    TestTrue(
        FString::Printf(TEXT("旋转后Y应该接近100，实际: %.4f"), RotatedPos.Y),
        FMath::Abs(RotatedPos.Y - 100.0f) < 1.0f);

    return true;
}

/**
 * 测试：Effector距离计算
 * 验证 CalculateEffectorDistance 函数
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArcDistributedIK_EffectorDistanceCalculation,
    "MusicDoll.IK.ArcDistributed.EffectorDistanceCalculation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_EffectorDistanceCalculation::RunTest(
    const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    // 创建一个简单的链
    TArray<FCCDIKChainLink> Chain;
    FCCDIKChainLink Link1, Link2, Link3;

    Link1.Transform.SetLocation(FVector(0.0f, 0.0f, 0.0f));
    Link2.Transform.SetLocation(FVector(100.0f, 0.0f, 0.0f));
    Link3.Transform.SetLocation(FVector(200.0f, 0.0f, 0.0f));

    Chain.Add(Link1);
    Chain.Add(Link2);
    Chain.Add(Link3);

    // Effector目标位置
    FVector EffectorPosition = FVector(200.0f, 50.0f, 0.0f);

    // 调用实际实现的距离计算函数
    float Distance = CalculateEffectorDistance(Chain, EffectorPosition);

    // 预期距离应该是50（从(200,0,0)到(200,50,0)）
    TestTrue(
        FString::Printf(TEXT("Effector距离应该接近50，实际: %.4f"), Distance),
        FMath::Abs(Distance - 50.0f) < 1.0f);

    return true;
}

/**
 * 测试：收敛判断
 * 验证 IsConverged 函数
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcDistributedIK_ConvergenceCheck,
                                 "MusicDoll.IK.ArcDistributed.ConvergenceCheck",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_ConvergenceCheck::RunTest(const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    const float Precision = 0.01f;

    // 测试已收敛的情况
    float SmallDistance = Precision * 0.5f;
    bool bConverged = IsConverged(SmallDistance, Precision);
    TestTrue(TEXT("距离小于精度时应该收敛"), bConverged);

    // 测试未收敛的情况
    float LargeDistance = Precision * 2.0f;
    bool bNotConverged = IsConverged(LargeDistance, Precision);
    TestTrue(TEXT("距离大于精度时不应该收敛"), !bNotConverged);

    return true;
}

/**
 * 测试：从两个轴构建旋转四元数
 * 验证 BuildRotationFromTwoAxes 方法的正确性
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArcDistributedIK_BuildRotationFromTwoAxes,
    "MusicDoll.IK.ArcDistributed.BuildRotationFromTwoAxes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_BuildRotationFromTwoAxes::RunTest(
    const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    // 测试用例 1: 恒等旋转 - 本地与世界坐标相同
    {
        FVector PrimaryDir = FVector(1.0f, 0.0f, 0.0f);
        FVector SecondaryDir = FVector(0.0f, 1.0f, 0.0f);
        FVector LocalPrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector LocalSecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        FQuat ResultQuat = BuildRotationFromTwoAxes(
            PrimaryDir, SecondaryDir, LocalPrimaryAxis, LocalSecondaryAxis);

        // 验证主轴对齐
        FVector RotatedPrimary = ResultQuat.RotateVector(LocalPrimaryAxis);
        float DotPrimary = FVector::DotProduct(RotatedPrimary, PrimaryDir);

        TestTrue(
            FString::Printf(TEXT("用例1-主轴对齐：%.6f > 0.99"), DotPrimary),
            DotPrimary > 0.99f);
    }

    // 测试用例 2: 90度旋转 - X轴旋转到Y轴
    {
        FVector PrimaryDir = FVector(0.0f, 1.0f, 0.0f);
        FVector SecondaryDir = FVector(-1.0f, 0.0f, 0.0f);
        FVector LocalPrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector LocalSecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        FQuat ResultQuat = BuildRotationFromTwoAxes(
            PrimaryDir, SecondaryDir, LocalPrimaryAxis, LocalSecondaryAxis);

        // 验证主轴对齐
        FVector RotatedPrimary = ResultQuat.RotateVector(LocalPrimaryAxis);
        float DotPrimary = FVector::DotProduct(RotatedPrimary, PrimaryDir);

        TestTrue(
            FString::Printf(TEXT("用例2-主轴对齐：%.6f > 0.99"), DotPrimary),
            DotPrimary > 0.99f);
    }

    // 测试用例 3: 四元数规范化验证
    {
        FVector PrimaryDir = FVector(0.577f, 0.577f, 0.577f).GetSafeNormal();
        FVector SecondaryDir = FVector(-0.707f, 0.707f, 0.0f).GetSafeNormal();
        FVector LocalPrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector LocalSecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        FQuat ResultQuat = BuildRotationFromTwoAxes(
            PrimaryDir, SecondaryDir, LocalPrimaryAxis, LocalSecondaryAxis);

        // 验证四元数长度为1
        float QuatLength = FMath::Sqrt(
            ResultQuat.X * ResultQuat.X + ResultQuat.Y * ResultQuat.Y +
            ResultQuat.Z * ResultQuat.Z + ResultQuat.W * ResultQuat.W);

        TestTrue(FString::Printf(TEXT("用例3-规范化：长度=%.6f"), QuatLength),
                 FMath::Abs(QuatLength - 1.0f) < 0.001f);
    }

    // 测试用例 4: 次轴对齐验证
    {
        FVector PrimaryDir = FVector(1.0f, 0.0f, 0.0f);
        FVector SecondaryDir = FVector(0.0f, 1.0f, 0.0f);
        FVector LocalPrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector LocalSecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        FQuat ResultQuat = BuildRotationFromTwoAxes(
            PrimaryDir, SecondaryDir, LocalPrimaryAxis, LocalSecondaryAxis);

        // 验证次轴对齐
        FVector RotatedSecondary = ResultQuat.RotateVector(LocalSecondaryAxis);
        float DotSecondary =
            FVector::DotProduct(RotatedSecondary, SecondaryDir);

        TestTrue(
            FString::Printf(TEXT("用例4-次轴对齐：%.6f > 0.99"), DotSecondary),
            DotSecondary > 0.99f);
    }

    // 测试用例 5: 复杂旋转场景
    {
        FVector PrimaryDir = FVector(0.707f, 0.0f, 0.707f).GetSafeNormal();
        FVector SecondaryDir = FVector(0.0f, 1.0f, 0.0f);
        FVector LocalPrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector LocalSecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        FQuat ResultQuat = BuildRotationFromTwoAxes(
            PrimaryDir, SecondaryDir, LocalPrimaryAxis, LocalSecondaryAxis);

        // 验证主轴对齐
        FVector RotatedPrimary = ResultQuat.RotateVector(LocalPrimaryAxis);
        float DotPrimary = FVector::DotProduct(RotatedPrimary, PrimaryDir);

        TestTrue(FString::Printf(TEXT("用例5-复杂旋转主轴：%.6f > 0.99"),
                                 DotPrimary),
                 DotPrimary > 0.99f);
    }

    // 测试用例 6: 旋转可逆性验证
    {
        FVector PrimaryDir = FVector(0.707f, 0.707f, 0.0f).GetSafeNormal();
        FVector SecondaryDir = FVector(-0.707f, 0.707f, 0.0f).GetSafeNormal();
        FVector LocalPrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector LocalSecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        FQuat ResultQuat = BuildRotationFromTwoAxes(
            PrimaryDir, SecondaryDir, LocalPrimaryAxis, LocalSecondaryAxis);

        // 通过逆旋转恢复原向量
        FQuat InverseQuat = ResultQuat.Inverse();
        FVector RecoveredPrimary = InverseQuat.RotateVector(PrimaryDir);

        float RecoveryError = FVector::Dist(RecoveredPrimary, LocalPrimaryAxis);
        TestTrue(FString::Printf(TEXT("用例6-可逆性误差：%.6f < 0.001"),
                                 RecoveryError),
                 RecoveryError < 0.001f);
    }

    return true;
}

/**
 * 测试：旋转重建
 * 验证 RebuildRotationsForChain 能否正确重建旋转信息
 * 测试逻辑：
 * 1. 创建多条关节链（直线和曲线）
 * 2. 调用 RebuildRotationsForChain 重建旋转
 * 3. 通过骨骼长度和旋转值推导关节位置
 * 4. 验证推导出的位置与原始位置一致
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArcDistributedIK_RebuildRotationsForChain,
    "MusicDoll.IK.ArcDistributed.RebuildRotationsForChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_RebuildRotationsForChain::RunTest(
    const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    const float PositionTolerance = 1.0f;  // 允许1cm的误差

    // ========== 测试用例1：直线链（沿X轴）==========
    {
        TArray<FCCDIKChainLink> Chain;
        TArray<float> BoneLengths;
        TArray<FVector> OriginalPositions;

        const int32 NumBones = 4;
        const float BoneLength = 100.0f;

        // 创建链
        for (int32 i = 0; i < NumBones; ++i) {
            FCCDIKChainLink Bone;
            FVector Position = FVector(BoneLength * i, 0.0f, 0.0f);
            Bone.Transform.SetLocation(Position);
            Bone.Transform.SetRotation(FQuat::Identity);

            if (i > 0) {
                FTransform ParentTransform = Chain[i - 1].Transform;
                Bone.LocalTransform =
                    Bone.Transform.GetRelativeTransform(ParentTransform);
            } else {
                Bone.LocalTransform = Bone.Transform;
            }

            Chain.Add(Bone);
            OriginalPositions.Add(Position);
        }

        // 记录骨骼长度
        for (int32 i = 0; i < NumBones - 1; ++i) {
            float Length = CalculateBoneLength(OriginalPositions[i],
                                               OriginalPositions[i + 1]);
            BoneLengths.Add(Length);
        }

        // 调用重建函数
        FVector ReferencePlaneNormal = FVector(0.0f, 0.0f, 1.0f);
        FVector PrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector SecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        RebuildRotationsForChain(Chain, BoneLengths, ReferencePlaneNormal,
                                 PrimaryAxis, SecondaryAxis);

        // 验证：通过旋转和骨骼长度推导位置
        TArray<FVector> DerivedPositions;
        DerivedPositions.Add(Chain[0].Transform.GetLocation());

        for (int32 i = 1; i < NumBones; ++i) {
            FVector ParentPosition = DerivedPositions[i - 1];
            FQuat ParentRotation =
                Chain[i - 1].Transform.Rotator().Quaternion();
            FVector WorldPrimaryAxis = ParentRotation.RotateVector(PrimaryAxis);
            FVector DerivedPosition =
                ParentPosition + WorldPrimaryAxis * BoneLengths[i - 1];
            DerivedPositions.Add(DerivedPosition);
        }

        // 验证位置一致性
        for (int32 i = 0; i < NumBones; ++i) {
            float Distance =
                FVector::Dist(DerivedPositions[i], OriginalPositions[i]);
            TestTrue(
                FString::Printf(
                    TEXT("用例1-关节%d：距离=%.4f (原始%.2f,%.2f,%.2f vs "
                         "推导%.2f,%.2f,%.2f)"),
                    i, Distance, OriginalPositions[i].X, OriginalPositions[i].Y,
                    OriginalPositions[i].Z, DerivedPositions[i].X,
                    DerivedPositions[i].Y, DerivedPositions[i].Z),
                Distance < PositionTolerance);
        }
    }

    // ========== 测试用例2：直线链（沿Y轴）==========
    {
        TArray<FCCDIKChainLink> Chain;
        TArray<float> BoneLengths;
        TArray<FVector> OriginalPositions;

        const int32 NumBones = 3;
        const float BoneLength = 80.0f;

        // 创建链（沿Y轴）
        for (int32 i = 0; i < NumBones; ++i) {
            FCCDIKChainLink Bone;
            FVector Position = FVector(0.0f, BoneLength * i, 0.0f);
            Bone.Transform.SetLocation(Position);
            Bone.Transform.SetRotation(FQuat::Identity);

            if (i > 0) {
                FTransform ParentTransform = Chain[i - 1].Transform;
                Bone.LocalTransform =
                    Bone.Transform.GetRelativeTransform(ParentTransform);
            } else {
                Bone.LocalTransform = Bone.Transform;
            }

            Chain.Add(Bone);
            OriginalPositions.Add(Position);
        }

        // 记录骨骼长度
        for (int32 i = 0; i < NumBones - 1; ++i) {
            float Length = CalculateBoneLength(OriginalPositions[i],
                                               OriginalPositions[i + 1]);
            BoneLengths.Add(Length);
        }

        // 调用重建函数
        FVector ReferencePlaneNormal = FVector(0.0f, 0.0f, 1.0f);
        FVector PrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector SecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        RebuildRotationsForChain(Chain, BoneLengths, ReferencePlaneNormal,
                                 PrimaryAxis, SecondaryAxis);

        // 验证
        TArray<FVector> DerivedPositions;
        DerivedPositions.Add(Chain[0].Transform.GetLocation());

        for (int32 i = 1; i < NumBones; ++i) {
            FVector ParentPosition = DerivedPositions[i - 1];
            FQuat ParentRotation =
                Chain[i - 1].Transform.Rotator().Quaternion();
            FVector WorldPrimaryAxis = ParentRotation.RotateVector(PrimaryAxis);
            FVector DerivedPosition =
                ParentPosition + WorldPrimaryAxis * BoneLengths[i - 1];
            DerivedPositions.Add(DerivedPosition);
        }

        for (int32 i = 0; i < NumBones; ++i) {
            float Distance =
                FVector::Dist(DerivedPositions[i], OriginalPositions[i]);
            TestTrue(
                FString::Printf(TEXT("用例2-关节%d：距离=%.4f"), i, Distance),
                Distance < PositionTolerance);
        }
    }

    // ========== 测试用例3：斜线链（XZ平面对角线）==========
    {
        TArray<FCCDIKChainLink> Chain;
        TArray<float> BoneLengths;
        TArray<FVector> OriginalPositions;

        const int32 NumBones = 5;
        const float BoneLength = 70.0f;

        // 创建链（沿45度角）
        for (int32 i = 0; i < NumBones; ++i) {
            FCCDIKChainLink Bone;
            float Distance = BoneLength * i;
            FVector Position =
                FVector(Distance * 0.707f, 0.0f, Distance * 0.707f);  // 45度
            Bone.Transform.SetLocation(Position);
            Bone.Transform.SetRotation(FQuat::Identity);

            if (i > 0) {
                FTransform ParentTransform = Chain[i - 1].Transform;
                Bone.LocalTransform =
                    Bone.Transform.GetRelativeTransform(ParentTransform);
            } else {
                Bone.LocalTransform = Bone.Transform;
            }

            Chain.Add(Bone);
            OriginalPositions.Add(Position);
        }

        // 记录骨骼长度
        for (int32 i = 0; i < NumBones - 1; ++i) {
            float Length = CalculateBoneLength(OriginalPositions[i],
                                               OriginalPositions[i + 1]);
            BoneLengths.Add(Length);
        }

        // 调用重建函数
        FVector ReferencePlaneNormal = FVector(0.0f, 1.0f, 0.0f);  // YZ平面
        FVector PrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector SecondaryAxis = FVector(0.0f, 0.0f, 1.0f);

        RebuildRotationsForChain(Chain, BoneLengths, ReferencePlaneNormal,
                                 PrimaryAxis, SecondaryAxis);

        // 验证
        TArray<FVector> DerivedPositions;
        DerivedPositions.Add(Chain[0].Transform.GetLocation());

        for (int32 i = 1; i < NumBones; ++i) {
            FVector ParentPosition = DerivedPositions[i - 1];
            FQuat ParentRotation =
                Chain[i - 1].Transform.Rotator().Quaternion();
            FVector WorldPrimaryAxis = ParentRotation.RotateVector(PrimaryAxis);
            FVector DerivedPosition =
                ParentPosition + WorldPrimaryAxis * BoneLengths[i - 1];
            DerivedPositions.Add(DerivedPosition);
        }

        for (int32 i = 0; i < NumBones; ++i) {
            float Distance =
                FVector::Dist(DerivedPositions[i], OriginalPositions[i]);
            TestTrue(
                FString::Printf(TEXT("用例3-关节%d：距离=%.4f"), i, Distance),
                Distance < PositionTolerance);
        }
    }

    // ========== 测试用例4：短链（2个关节）==========
    {
        TArray<FCCDIKChainLink> Chain;
        TArray<float> BoneLengths;
        TArray<FVector> OriginalPositions;

        FCCDIKChainLink Bone0;
        Bone0.Transform.SetLocation(FVector(0.0f, 0.0f, 0.0f));
        Bone0.Transform.SetRotation(FQuat::Identity);
        Bone0.LocalTransform = Bone0.Transform;
        Chain.Add(Bone0);
        OriginalPositions.Add(FVector(0.0f, 0.0f, 0.0f));

        FCCDIKChainLink Bone1;
        Bone1.Transform.SetLocation(FVector(50.0f, 0.0f, 0.0f));
        Bone1.Transform.SetRotation(FQuat::Identity);
        Bone1.LocalTransform =
            Bone1.Transform.GetRelativeTransform(Bone0.Transform);
        Chain.Add(Bone1);
        OriginalPositions.Add(FVector(50.0f, 0.0f, 0.0f));

        BoneLengths.Add(
            CalculateBoneLength(OriginalPositions[0], OriginalPositions[1]));

        // 调用重建函数
        FVector ReferencePlaneNormal = FVector(0.0f, 0.0f, 1.0f);
        FVector PrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector SecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        RebuildRotationsForChain(Chain, BoneLengths, ReferencePlaneNormal,
                                 PrimaryAxis, SecondaryAxis);

        // 验证
        TestTrue(
            TEXT("用例4-关节0位置应该是原点"),
            Chain[0].Transform.GetLocation().Equals(FVector(0.0f, 0.0f, 0.0f)));

        FVector DerivedPos1 =
            Chain[0].Transform.GetLocation() +
            Chain[0].Transform.Rotator().Quaternion().RotateVector(
                PrimaryAxis) *
                BoneLengths[0];
        float Distance = FVector::Dist(DerivedPos1, OriginalPositions[1]);
        TestTrue(FString::Printf(TEXT("用例4-关节1：距离=%.4f"), Distance),
                 Distance < PositionTolerance);
    }

    // ========== 测试用例5：长链（6个关节）==========
    {
        TArray<FCCDIKChainLink> Chain;
        TArray<float> BoneLengths;
        TArray<FVector> OriginalPositions;

        const int32 NumBones = 6;
        const float BoneLength = 50.0f;

        for (int32 i = 0; i < NumBones; ++i) {
            FCCDIKChainLink Bone;
            FVector Position = FVector(BoneLength * i, 0.0f, 0.0f);
            Bone.Transform.SetLocation(Position);
            Bone.Transform.SetRotation(FQuat::Identity);

            if (i > 0) {
                FTransform ParentTransform = Chain[i - 1].Transform;
                Bone.LocalTransform =
                    Bone.Transform.GetRelativeTransform(ParentTransform);
            } else {
                Bone.LocalTransform = Bone.Transform;
            }

            Chain.Add(Bone);
            OriginalPositions.Add(Position);
        }

        for (int32 i = 0; i < NumBones - 1; ++i) {
            float Length = CalculateBoneLength(OriginalPositions[i],
                                               OriginalPositions[i + 1]);
            BoneLengths.Add(Length);
        }

        FVector ReferencePlaneNormal = FVector(0.0f, 0.0f, 1.0f);
        FVector PrimaryAxis = FVector(1.0f, 0.0f, 0.0f);
        FVector SecondaryAxis = FVector(0.0f, 1.0f, 0.0f);

        RebuildRotationsForChain(Chain, BoneLengths, ReferencePlaneNormal,
                                 PrimaryAxis, SecondaryAxis);

        TArray<FVector> DerivedPositions;
        DerivedPositions.Add(Chain[0].Transform.GetLocation());

        for (int32 i = 1; i < NumBones; ++i) {
            FVector ParentPosition = DerivedPositions[i - 1];
            FQuat ParentRotation =
                Chain[i - 1].Transform.Rotator().Quaternion();
            FVector WorldPrimaryAxis = ParentRotation.RotateVector(PrimaryAxis);
            FVector DerivedPosition =
                ParentPosition + WorldPrimaryAxis * BoneLengths[i - 1];
            DerivedPositions.Add(DerivedPosition);
        }

        for (int32 i = 0; i < NumBones; ++i) {
            float Distance =
                FVector::Dist(DerivedPositions[i], OriginalPositions[i]);
            TestTrue(
                FString::Printf(TEXT("用例5-关节%d：距离=%.4f"), i, Distance),
                Distance < PositionTolerance);
        }
    }

    return true;
}

/**
 * 测试：骨骼链旋转
 * 验证 ApplyRotationToBoneChain 能否正确旋转骨骼链
 * 场景：4根骨骼的直线链，每根长100cm，沿X轴排列
 * 旋转：绕Z轴旋转90度
 * 期望结果：
 * Bone0: (0, 0, 0) - 保持不变
 * Bone1: (0, 100, 0)
 * Bone2: (100, 100, 0)
 * Bone3: (100, 0, 0)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArcDistributedIK_ApplyRotationToBoneChain,
    "MusicDoll.IK.ArcDistributed.ApplyRotationToBoneChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_ApplyRotationToBoneChain::RunTest(
    const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    // ========== 初始设置：4根骨骼的直线链 ==========
    TArray<FCCDIKChainLink> Chain;
    TArray<float> BoneLengths;

    const int32 NumBones = 4;
    const float BoneLength = 100.0f;
    const float RotationAngleDegrees = 90.0f;
    const float RotationAngleRadians =
        FMath::DegreesToRadians(RotationAngleDegrees);

    // 创建初始链：沿X轴排列
    // Bone0: (0, 0, 0)
    // Bone1: (100, 0, 0)
    // Bone2: (200, 0, 0)
    // Bone3: (300, 0, 0)
    for (int32 i = 0; i < NumBones; ++i) {
        FCCDIKChainLink Bone;
        FVector Position = FVector(BoneLength * i, 0.0f, 0.0f);
        Bone.Transform.SetLocation(Position);
        Bone.Transform.SetRotation(FQuat::Identity);

        if (i > 0) {
            FTransform ParentTransform = Chain[i - 1].Transform;
            Bone.LocalTransform =
                Bone.Transform.GetRelativeTransform(ParentTransform);
        } else {
            Bone.LocalTransform = Bone.Transform;
        }

        Chain.Add(Bone);
    }

    // 记录骨骼长度
    for (int32 i = 0; i < NumBones - 1; ++i) {
        BoneLengths.Add(BoneLength);
    }

    // 保存原始位置用于验证
    TArray<FVector> OriginalPositions;
    for (int32 i = 0; i < NumBones; ++i) {
        OriginalPositions.Add(Chain[i].Transform.GetLocation());
    }

    UE_LOG(LogTemp, Warning, TEXT("=== 旋转前的位置 ==="));
    for (int32 i = 0; i < NumBones; ++i) {
        const FVector& Pos = Chain[i].Transform.GetLocation();
        UE_LOG(LogTemp, Warning, TEXT("Bone%d: (%.2f, %.2f, %.2f)"), i, Pos.X,
               Pos.Y, Pos.Z);
    }

    // ========== 应用旋转 ==========
    FVector RotationAxis = FVector(0.0f, 0.0f, 1.0f);  // Z轴
    ApplyRotationToBoneChain(Chain, BoneLengths, RotationAxis,
                             RotationAngleRadians);

    UE_LOG(LogTemp, Warning, TEXT("=== 旋转后的位置（旋转%.0f度） ==="),
           RotationAngleDegrees);
    for (int32 i = 0; i < NumBones; ++i) {
        const FVector& Pos = Chain[i].Transform.GetLocation();
        UE_LOG(LogTemp, Warning, TEXT("Bone%d: (%.2f, %.2f, %.2f)"), i, Pos.X,
               Pos.Y, Pos.Z);
    }

    const float PositionTolerance = 1.0f;  // 允许1cm的误差

    // ========== 验证骨骼长度保持不变 ==========
    UE_LOG(LogTemp, Warning, TEXT("=== 骨骼长度验证 ==="));
    for (int32 i = 0; i < NumBones - 1; ++i) {
        FVector Pos1 = Chain[i].Transform.GetLocation();
        FVector Pos2 = Chain[i + 1].Transform.GetLocation();
        float ActualLength = FVector::Dist(Pos1, Pos2);
        float LengthError = FMath::Abs(ActualLength - BoneLength);

        UE_LOG(LogTemp, Warning,
               TEXT("Bone%d -> Bone%d: 长度=%.2f cm（期望=%.2f cm，误差=%.4f "
                    "cm）"),
               i, i + 1, ActualLength, BoneLength, LengthError);

        TestTrue(
            FString::Printf(TEXT("骨骼%d长度应保持%.2f cm"), i, BoneLength),
            LengthError < PositionTolerance);
    }

    // ========== 验证根骨骼位置不变 ==========
    FVector RootPos = Chain[0].Transform.GetLocation();
    FVector ExpectedRootPos = FVector(0.0f, 0.0f, 0.0f);
    float RootPosDist = FVector::Dist(RootPos, ExpectedRootPos);

    UE_LOG(LogTemp, Warning,
           TEXT("根骨骼位置：实际(%.2f, %.2f, %.2f) 期望(%.2f, %.2f, %.2f) "
                "误差=%.4f cm"),
           RootPos.X, RootPos.Y, RootPos.Z, ExpectedRootPos.X,
           ExpectedRootPos.Y, ExpectedRootPos.Z, RootPosDist);

    TestTrue(TEXT("根骨骼应保持在原位"), RootPosDist < PositionTolerance);

    // ========== 验证旋转后的位置 ==========
    // 期望的位置：
    // Bone0: (0, 0, 0)
    // Bone1: (0, 100, 0)
    // Bone2: (100, 100, 0)
    // Bone3: (100, 0, 0)
    FVector ExpectedPositions[NumBones] = {
        FVector(0.0f, 0.0f, 0.0f),      // Bone0
        FVector(0.0f, 100.0f, 0.0f),    // Bone1
        FVector(100.0f, 100.0f, 0.0f),  // Bone2
        FVector(100.0f, 0.0f, 0.0f)     // Bone3
    };

    UE_LOG(LogTemp, Warning, TEXT("=== 位置验证（期望值） ==="));
    for (int32 i = 0; i < NumBones; ++i) {
        FVector ActualPos = Chain[i].Transform.GetLocation();
        FVector ExpectedPos = ExpectedPositions[i];
        float Distance = FVector::Dist(ActualPos, ExpectedPos);

        UE_LOG(LogTemp, Warning,
               TEXT("Bone%d: 实际(%.2f, %.2f, %.2f) 期望(%.2f, %.2f, %.2f) "
                    "误差=%.4f cm"),
               i, ActualPos.X, ActualPos.Y, ActualPos.Z, ExpectedPos.X,
               ExpectedPos.Y, ExpectedPos.Z, Distance);

        TestTrue(FString::Printf(TEXT("Bone%d 位置正确（误差=%.4f cm）"), i,
                                 Distance),
                 Distance < PositionTolerance);
    }

    // ========== 测试说明 ==========
    UE_LOG(LogTemp, Warning, TEXT("=== 测试完成 ==="));
    UE_LOG(LogTemp, Warning,
           TEXT("ApplyRotationToBoneChain 已成功执行旋转操作"));
    UE_LOG(LogTemp, Warning, TEXT("所有骨骼长度保持不变"));
    UE_LOG(LogTemp, Warning, TEXT("根骨骼保持在原位"));
    UE_LOG(LogTemp, Warning, TEXT("其他骨骼已按预期旋转到方形结构"));

    return true;
}

/**
 * 测试：牛顿法求解最优旋转角
 * 测试 SolveOptimalAngleWithNewton 函数的收敛性能
 * 测试场景：使用真实的牛顿法求解，观察迭代次数和收敛过程
 * 测试重点：验证牛顿法用多少次迭代可以收敛到最优解
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArcDistributedIK_SolveOptimalAngleWithNewton,
    "MusicDoll.IK.ArcDistributed.SolveOptimalAngleWithNewton",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArcDistributedIK_SolveOptimalAngleWithNewton::RunTest(
    const FString& Parameters) {
    using namespace FArcDistributedIKHelper;

    UE_LOG(LogTemp, Warning,
           TEXT("========== 牛顿法旋转角度求解测试 =========="));

    // ========== 测试用例1: 3骨骼链，小角度求解 ========= =
    {
        UE_LOG(LogTemp, Warning,
               TEXT("\n【测试用例1】3骨骼链，使用牛顿法求解最优角度"));

        TArray<FCCDIKChainLink> Chain;
        TArray<float> BoneLengths;

        const int32 NumBones = 3;
        const float BoneLength = 100.0f;
        const float TrueTargetAngle = FMath::DegreesToRadians(45.0f);

        // 创建初始链：沿X轴排列
        for (int32 i = 0; i < NumBones; ++i) {
            FCCDIKChainLink Bone;
            FVector Position = FVector(BoneLength * i, 0.0f, 0.0f);
            Bone.Transform.SetLocation(Position);
            Bone.Transform.SetRotation(FQuat::Identity);

            if (i > 0) {
                FTransform ParentTransform = Chain[i - 1].Transform;
                Bone.LocalTransform =
                    Bone.Transform.GetRelativeTransform(ParentTransform);
            } else {
                Bone.LocalTransform = Bone.Transform;
            }

            Chain.Add(Bone);
            BoneLengths.Add(BoneLength);
        }

        FVector RootPosition = Chain[0].Transform.GetLocation();
        FVector PlaneNormal = FVector(0.0f, 0.0f, 1.0f);  // XY平面

        // 创建真实目标链：应用目标旋转角度获取末端位置
        TArray<FCCDIKChainLink> TargetChain = Chain;
        ApplyRotationToBoneChain(TargetChain, BoneLengths, PlaneNormal,
                                 TrueTargetAngle);
        FVector TrueTargetEffectorPos =
            TargetChain[NumBones - 1].Transform.GetLocation();

        // 初始猜测：30度（错误的初值）
        float InitialGuess = FMath::DegreesToRadians(30.0f);
        float Precision = 0.001f;

        UE_LOG(LogTemp, Warning, TEXT("  真实目标角度: %.2f 度"),
               FMath::RadiansToDegrees(TrueTargetAngle));
        UE_LOG(LogTemp, Warning, TEXT("  真实目标位置: (%.2f, %.2f, %.2f)"),
               TrueTargetEffectorPos.X, TrueTargetEffectorPos.Y,
               TrueTargetEffectorPos.Z);
        UE_LOG(LogTemp, Warning, TEXT("  初始角度猜测: %.2f 度"),
               FMath::RadiansToDegrees(InitialGuess));
        UE_LOG(LogTemp, Warning, TEXT("  精度要求: %.6f cm"), Precision);

        // 调用牛顿法求解
        float SolvedAngle =
            FArcDistributedIKHelper::SolveOptimalAngleWithNewton(
                Chain, BoneLengths, RootPosition, TrueTargetEffectorPos,
                PlaneNormal, InitialGuess, Precision, 100);

        UE_LOG(LogTemp, Warning, TEXT("  牛顿法求解结果: %.2f 度"),
               FMath::RadiansToDegrees(SolvedAngle));

        // 应用求解出的角度验证结果
        TArray<FCCDIKChainLink> VerifyChain = Chain;
        ApplyRotationToBoneChain(VerifyChain, BoneLengths, PlaneNormal,
                                 SolvedAngle);

        FVector FinalEffectorPos =
            VerifyChain[NumBones - 1].Transform.GetLocation();
        float FinalDistance =
            FVector::Dist(FinalEffectorPos, TrueTargetEffectorPos);

        UE_LOG(LogTemp, Warning, TEXT("  求解后末端位置: (%.2f, %.2f, %.2f)"),
               FinalEffectorPos.X, FinalEffectorPos.Y, FinalEffectorPos.Z);
        UE_LOG(LogTemp, Warning, TEXT("  最终距离误差: %.6f cm"),
               FinalDistance);

        TestTrue(TEXT("用例1-应该收敛到目标（误差 < 0.01cm）"),
                 FinalDistance < 0.01f);

        UE_LOG(LogTemp, Warning, TEXT("  ✓ 测试用例1完成\n"));
    }

    // ========== 测试用例2: 4骨骼链，大角度求解 ========= =
    {
        UE_LOG(LogTemp, Warning,
               TEXT("\n【测试用例2】4骨骼链，90度大角度求解"));

        TArray<FCCDIKChainLink> Chain;
        TArray<float> BoneLengths;

        const int32 NumBones = 4;
        const float BoneLength = 50.0f;
        const float TrueTargetAngle = FMath::DegreesToRadians(90.0f);

        for (int32 i = 0; i < NumBones; ++i) {
            FCCDIKChainLink Bone;
            FVector Position = FVector(BoneLength * i, 0.0f, 0.0f);
            Bone.Transform.SetLocation(Position);
            Bone.Transform.SetRotation(FQuat::Identity);

            if (i > 0) {
                FTransform ParentTransform = Chain[i - 1].Transform;
                Bone.LocalTransform =
                    Bone.Transform.GetRelativeTransform(ParentTransform);
            } else {
                Bone.LocalTransform = Bone.Transform;
            }

            Chain.Add(Bone);
            BoneLengths.Add(BoneLength);
        }

        FVector RootPosition = Chain[0].Transform.GetLocation();
        FVector PlaneNormal = FVector(0.0f, 0.0f, 1.0f);

        // 创建真实目标链：应用目标旋转角度获取末端位置
        TArray<FCCDIKChainLink> TargetChain = Chain;
        ApplyRotationToBoneChain(TargetChain, BoneLengths, PlaneNormal,
                                 TrueTargetAngle);
        FVector TrueTargetEffectorPos =
            TargetChain[NumBones - 1].Transform.GetLocation();

        // 初始猜测：60度（30度误差）
        float InitialGuess = FMath::DegreesToRadians(60.0f);
        float Precision = 0.001f;

        UE_LOG(LogTemp, Warning, TEXT("  真实目标角度: %.2f 度"),
               FMath::RadiansToDegrees(TrueTargetAngle));
        UE_LOG(LogTemp, Warning, TEXT("  初始猜测: %.2f 度（误差 30度）"),
               FMath::RadiansToDegrees(InitialGuess));

        float SolvedAngle =
            FArcDistributedIKHelper::SolveOptimalAngleWithNewton(
                Chain, BoneLengths, RootPosition, TrueTargetEffectorPos,
                PlaneNormal, InitialGuess, Precision, 10);

        UE_LOG(LogTemp, Warning, TEXT("  牛顿法求解: %.2f 度"),
               FMath::RadiansToDegrees(SolvedAngle));

        // 验证
        TArray<FCCDIKChainLink> VerifyChain = Chain;
        ApplyRotationToBoneChain(VerifyChain, BoneLengths, PlaneNormal,
                                 SolvedAngle);

        FVector FinalEffectorPos =
            VerifyChain[NumBones - 1].Transform.GetLocation();
        float FinalDistance =
            FVector::Dist(FinalEffectorPos, TrueTargetEffectorPos);

        UE_LOG(LogTemp, Warning, TEXT("  最终距离误差: %.6f cm"),
               FinalDistance);

        TestTrue(TEXT("用例2-应该收敛（误差 < 0.01cm）"),
                 FinalDistance < 0.01f);

        UE_LOG(LogTemp, Warning, TEXT("  ✓ 测试用例2完成\n"));
    }

    // ========== 测试用例3: 5骨骼链，多个初值测试 ========= =
    {
        UE_LOG(LogTemp, Warning,
               TEXT("\n【测试用例3】5骨骼链，不同初值的收敛比较"));

        TArray<FCCDIKChainLink> Chain;
        TArray<float> BoneLengths;

        const int32 NumBones = 5;
        const float BoneLength = 60.0f;
        const float TrueTargetAngle = FMath::DegreesToRadians(60.0f);

        for (int32 i = 0; i < NumBones; ++i) {
            FCCDIKChainLink Bone;
            FVector Position = FVector(BoneLength * i, 0.0f, 0.0f);
            Bone.Transform.SetLocation(Position);
            Bone.Transform.SetRotation(FQuat::Identity);

            if (i > 0) {
                FTransform ParentTransform = Chain[i - 1].Transform;
                Bone.LocalTransform =
                    Bone.Transform.GetRelativeTransform(ParentTransform);
            } else {
                Bone.LocalTransform = Bone.Transform;
            }

            Chain.Add(Bone);
            BoneLengths.Add(BoneLength);
        }

        FVector RootPosition = Chain[0].Transform.GetLocation();
        FVector PlaneNormal = FVector(0.0f, 0.0f, 1.0f);

        // 创建真实目标链：应用目标旋转角度获取末端位置
        TArray<FCCDIKChainLink> TargetChain = Chain;
        ApplyRotationToBoneChain(TargetChain, BoneLengths, PlaneNormal,
                                 TrueTargetAngle);
        FVector TrueTargetEffectorPos =
            TargetChain[NumBones - 1].Transform.GetLocation();

        float Precision = 0.001f;

        UE_LOG(LogTemp, Warning, TEXT("  真实目标角度: %.2f 度"),
               FMath::RadiansToDegrees(TrueTargetAngle));
        UE_LOG(LogTemp, Warning, TEXT("  真实目标位置: (%.2f, %.2f, %.2f)"),
               TrueTargetEffectorPos.X, TrueTargetEffectorPos.Y,
               TrueTargetEffectorPos.Z);

        // 测试4个不同的初值
        float InitialGuesses[] = {
            FMath::DegreesToRadians(10.0f),  // 50度误差
            FMath::DegreesToRadians(30.0f),  // 30度误差
            FMath::DegreesToRadians(75.0f),  // 15度误差
            FMath::DegreesToRadians(55.0f),  // 5度误差
        };

        for (int32 GuessIdx = 0; GuessIdx < 4; ++GuessIdx) {
            float InitialGuess = InitialGuesses[GuessIdx];
            float InitialErrorDegrees =
                FMath::Abs(FMath::RadiansToDegrees(TrueTargetAngle) -
                           FMath::RadiansToDegrees(InitialGuess));

            float SolvedAngle =
                FArcDistributedIKHelper::SolveOptimalAngleWithNewton(
                    Chain, BoneLengths, RootPosition, TrueTargetEffectorPos,
                    PlaneNormal, InitialGuess, Precision, 10);

            // 验证
            TArray<FCCDIKChainLink> VerifyChain = Chain;
            ApplyRotationToBoneChain(VerifyChain, BoneLengths, PlaneNormal,
                                     SolvedAngle);

            FVector FinalEffectorPos =
                VerifyChain[NumBones - 1].Transform.GetLocation();
            float FinalDistance =
                FVector::Dist(FinalEffectorPos, TrueTargetEffectorPos);

            UE_LOG(LogTemp, Warning,
                   TEXT("    初值%.2f度(误差%.1f度) -> 求解: %.2f度, 误差: "
                        "%.6f cm"),
                   FMath::RadiansToDegrees(InitialGuess), InitialErrorDegrees,
                   FMath::RadiansToDegrees(SolvedAngle), FinalDistance);

            TestTrue(FString::Printf(TEXT("用例3-初值%.0f度"),
                                     FMath::RadiansToDegrees(InitialGuess)),
                     FinalDistance < 0.01f);
        }

        UE_LOG(LogTemp, Warning, TEXT("  ✓ 测试用例3完成\n"));
    }

    // ========== 测试用例4: 3骨骼链，极端初值 ========= =
    {
        UE_LOG(LogTemp, Warning,
               TEXT("\n【测试用例4】3骨骼链，极端初值（接近边界条件）"));

        TArray<FCCDIKChainLink> Chain;
        TArray<float> BoneLengths;

        const int32 NumBones = 3;
        const float BoneLength = 100.0f;
        const float TrueTargetAngle = FMath::DegreesToRadians(70.0f);

        for (int32 i = 0; i < NumBones; ++i) {
            FCCDIKChainLink Bone;
            FVector Position = FVector(BoneLength * i, 0.0f, 0.0f);
            Bone.Transform.SetLocation(Position);
            Bone.Transform.SetRotation(FQuat::Identity);

            if (i > 0) {
                FTransform ParentTransform = Chain[i - 1].Transform;
                Bone.LocalTransform =
                    Bone.Transform.GetRelativeTransform(ParentTransform);
            } else {
                Bone.LocalTransform = Bone.Transform;
            }

            Chain.Add(Bone);
            BoneLengths.Add(BoneLength);
        }

        FVector RootPosition = Chain[0].Transform.GetLocation();
        FVector PlaneNormal = FVector(0.0f, 0.0f, 1.0f);

        // 创建真实目标链：应用目标旋转角度获取末端位置
        TArray<FCCDIKChainLink> TargetChain = Chain;
        ApplyRotationToBoneChain(TargetChain, BoneLengths, PlaneNormal,
                                 TrueTargetAngle);
        FVector TrueTargetEffectorPos =
            TargetChain[NumBones - 1].Transform.GetLocation();

        // 极端初值：接近 π/4（45度）的上限
        float InitialGuess = FMath::DegreesToRadians(45.0f);
        float Precision = 0.001f;

        UE_LOG(LogTemp, Warning, TEXT("  真实目标角度: %.2f 度"),
               FMath::RadiansToDegrees(TrueTargetAngle));
        UE_LOG(LogTemp, Warning, TEXT("  真实目标位置: (%.2f, %.2f, %.2f)"),
               TrueTargetEffectorPos.X, TrueTargetEffectorPos.Y,
               TrueTargetEffectorPos.Z);
        UE_LOG(LogTemp, Warning, TEXT("  初始猜测: %.2f 度（在步长限制附近）"),
               FMath::RadiansToDegrees(InitialGuess));

        float SolvedAngle =
            FArcDistributedIKHelper::SolveOptimalAngleWithNewton(
                Chain, BoneLengths, RootPosition, TrueTargetEffectorPos,
                PlaneNormal, InitialGuess, Precision, 10);

        UE_LOG(LogTemp, Warning, TEXT("  牛顿法求解: %.2f 度"),
               FMath::RadiansToDegrees(SolvedAngle));

        TArray<FCCDIKChainLink> VerifyChain = Chain;
        ApplyRotationToBoneChain(VerifyChain, BoneLengths, PlaneNormal,
                                 SolvedAngle);

        FVector FinalEffectorPos =
            VerifyChain[NumBones - 1].Transform.GetLocation();
        float FinalDistance =
            FVector::Dist(FinalEffectorPos, TrueTargetEffectorPos);

        UE_LOG(LogTemp, Warning, TEXT("  最终距离误差: %.6f cm"),
               FinalDistance);

        TestTrue(TEXT("用例4-应该在步长限制下收敛"), FinalDistance < 0.01f);

        UE_LOG(LogTemp, Warning, TEXT("  ✓ 测试用例4完成\n"));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== 牛顿法旋转角度求解测试完成 ==========\n"));
    UE_LOG(LogTemp, Warning, TEXT("所有测试用例都验证了牛顿法的收敛性能\n"));

    return true;
}

#endif  // WITH_AUTOMATION_TESTS
