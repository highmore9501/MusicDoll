#include "Nodes/AccordionBellowsIK.h"

#include "Algo/Reverse.h"
#include "ControlRig.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AccordionBellowsIK)

// ============================================================================
// 手风琴风箱 IK
//
// 与 ArcDistributedIK / BlenderStyleIK 不同，这个节点不解 IK：
// 「链根、effector、effector 旋转」一给，链条形状就唯一确定了，一次算完。
//
// 推导要点（全部在链根/effector/pivot 三点确定的平面内进行）：
//
//   u  = 弦方向（链根 -> effector，单位向量）
//   n  = 平面法线
//   p  = n x u              （平面内、垂直于弦，和 u 构成右手平面基）
//   a  = effector 旋转偏移投影到该平面的带符号角度（切线-弦夹角）
//
//   圆心角 = 2a
//   半径   = c / (2·sin|a|)                        c = 弦长
//   弧长   = 2·|a|·半径 = c·a / sin a
//   圆心   = 弦中点 - (c / (2·tan a)) · p          （tan 带符号，a
//   的正负自动决定圆心在哪一侧） 弧上点 = 圆心 + Rotate(链根 - 圆心, 绕 n 转
//   -2a·比例)
//
//   最后一条式子对 a 的正负都成立（a > 0 顺时针绕 n，a < 0 逆时针绕 n），
//   所以不需要分正负号写两套代码；比例 = 1 时结果精确等于 effector 位置。
//
// 为什么这样和 Blender 那套等价：
//   Blender 里每节骨骼的 local 旋转相同、IK stretch 均摊，所以每节缩放也相同、
//   每节的 roll 跟着前一层一起转。本节点里"弧长按 rest
//   长度比例分配"给出的正好是 每节缩放相同；而"roll 指向 pivot"和"roll
//   跟着转"在平面曲线上是同一件事
//   （都是平面内、垂直于该节方向的向量，只差一个全局符号）。
// ============================================================================

namespace AccordionBellowsIKImpl {

// 低于这个弧度就按直线处理：此时圆弧半径趋于无穷，公式在数值上不可靠，
// 而直线与圆弧的结果差异量级只有 a²/6，肉眼不可见。
constexpr double MinBendAngleRadians = 1.0e-4;

// 长度/距离低于它视为退化
constexpr double MinSegmentLength = 1.0e-4;

// ============================================================
// 基础工具
// ============================================================

/** 挑一根和给定方向最不平行的坐标轴，用于各种"方向参数写歪了"的兜底 */
FVector PickPerpendicularAxis(const FVector& Direction) {
    const FVector UnitDirection = Direction.GetSafeNormal();
    const FVector CandidateAxes[3] = {FVector::ForwardVector,
                                      FVector::RightVector, FVector::UpVector};

    FVector BestAxis = FVector::UpVector;
    double BestAlignment = 2.0;
    for (const FVector& CandidateAxis : CandidateAxes) {
        const double Alignment =
            FMath::Abs(FVector::DotProduct(UnitDirection, CandidateAxis));
        if (Alignment < BestAlignment) {
            BestAlignment = Alignment;
            BestAxis = CandidateAxis;
        }
    }
    return BestAxis;
}

/**
 * 把两根轴参数正交化成一组可用的本地基。
 * 用户在引脚里填的两根轴理论上应该互相垂直，但填歪了也得能算（否则会构造出非法旋转）。
 */
bool BuildOrthonormalAxes(const FVector& RadialAxis, const FVector& PivotAxis,
                          FVector& OutRadialAxis, FVector& OutPivotAxis) {
    OutRadialAxis = RadialAxis.GetSafeNormal();
    if (OutRadialAxis.IsNearlyZero()) {
        return false;
    }

    OutPivotAxis = PivotAxis.GetSafeNormal();
    OutPivotAxis -=
        OutRadialAxis * FVector::DotProduct(OutPivotAxis, OutRadialAxis);
    if (!OutPivotAxis.Normalize(MinSegmentLength)) {
        // 两根轴平行（或 pivot 轴是零向量）：随便挑一根垂直于径向轴的方向顶上，
        // 保证后面不会构造出退化矩阵。
        OutPivotAxis = FVector::CrossProduct(
                           OutRadialAxis, PickPerpendicularAxis(OutRadialAxis))
                           .GetSafeNormal();
    }
    return !OutPivotAxis.IsNearlyZero();
}

/**
 * 由「世界径向方向 + 世界 pivot 方向 + 本地两根轴」构造骨骼的世界旋转。
 *
 * 分两步走，比直接拼矩阵更直观，也不会踩 FMatrix 行列约定的坑：
 *   1. 把本地径向轴转到世界径向轴（决定骨骼朝向）；
 *   2. 绕世界径向轴拧一下，让本地 pivot 轴对上世界 pivot 轴（只动
 * roll，不改朝向）。
 */
FQuat BuildBoneRotation(const FVector& WorldRadialDirection,
                        const FVector& WorldPivotDirection,
                        const FVector& LocalRadialAxis,
                        const FVector& LocalPivotAxis) {
    FVector LocalRadial;
    FVector LocalPivot;
    if (!BuildOrthonormalAxes(LocalRadialAxis, LocalPivotAxis, LocalRadial,
                              LocalPivot)) {
        return FQuat::Identity;
    }

    const FVector WorldRadial = WorldRadialDirection.GetSafeNormal();
    if (WorldRadial.IsNearlyZero()) {
        return FQuat::Identity;
    }

    FVector WorldPivot = WorldPivotDirection;
    WorldPivot -= WorldRadial * FVector::DotProduct(WorldPivot, WorldRadial);
    if (!WorldPivot.Normalize(MinSegmentLength)) {
        // 世界 pivot 方向和骨骼朝向平行，roll 无从约束，只做朝向对齐。
        return FQuat::FindBetweenNormals(LocalRadial, WorldRadial);
    }

    const FQuat AlignRotation =
        FQuat::FindBetweenNormals(LocalRadial, WorldRadial);

    const FVector AlignedPivot = AlignRotation.RotateVector(LocalPivot);
    FVector FromPivot = AlignedPivot;
    FromPivot -= WorldRadial * FVector::DotProduct(FromPivot, WorldRadial);
    FromPivot = FromPivot.GetSafeNormal();

    // 绕世界径向轴的带符号夹角：叉积在新轴上的分量给符号，点积给大小。
    const double TwistAngle = FMath::Atan2(
        FVector::DotProduct(FVector::CrossProduct(FromPivot, WorldPivot),
                            WorldRadial),
        FVector::DotProduct(FromPivot, WorldPivot));

    return (FQuat(WorldRadial, TwistAngle) * AlignRotation).GetNormalized();
}

/**
 * 取 control
 * 相对自己初始姿态的旋转偏移，并投影到弯曲平面上，返回带符号的角度（弧度）。
 *
 * 为什么用"相对父级的局部偏移"而不是"当前全局 vs 初始全局"：
 * control
 * 的父级会跟着身体/乐器整体运动，用全局比较会把这部分整体旋转也算成弯曲，
 * 表现为"身体一动风箱就自己弯了"。取局部偏移再转回世界轴，整体运动就被抵消掉了。
 */
double ExtractBendAngle(const URigHierarchy* Hierarchy,
                        const FRigElementKey& EffectorKey,
                        const FVector& PlaneNormal) {
    const FQuat CurrentGlobalRotation =
        Hierarchy->GetGlobalTransform(EffectorKey).GetRotation();
    const FQuat InitialLocalRotation =
        Hierarchy->GetInitialLocalTransform(EffectorKey).GetRotation();

    FQuat ParentCurrentRotation = FQuat::Identity;
    const FRigElementKey ParentKey = Hierarchy->GetFirstParent(EffectorKey);
    if (Hierarchy->Contains(ParentKey)) {
        ParentCurrentRotation =
            Hierarchy->GetGlobalTransform(ParentKey).GetRotation();
    }

    // 当前局部 = 父级当前全局的逆 × 当前全局
    const FQuat CurrentLocalRotation =
        (ParentCurrentRotation.Inverse() * CurrentGlobalRotation)
            .GetNormalized();

    // 动画真正施加在这个 control 上的局部旋转增量
    const FQuat LocalDelta =
        (CurrentLocalRotation * InitialLocalRotation.Inverse()).GetNormalized();

    // 换回世界轴，才能和平面法线比较
    FQuat WorldDelta =
        (ParentCurrentRotation * LocalDelta * ParentCurrentRotation.Inverse())
            .GetNormalized();

    // 四元数是"双覆盖"的：q 和 -q 表示同一个旋转，但分量整体取反后，
    // 下面 atan2 会把"正着转了 15°"读成"反着转了 345°"。于是切线角从 +15° 直接跳到
    // -345°，弧长和伸缩比瞬间爆炸（实测：只微调两度就崩，正是这个原因）。
    // 谁把这个符号搞反的都有可能（control 的当前旋转、初始旋转、父级旋转，任意一个
    // 在某一帧以 -q 的形式返回都会传染下来），所以统一在最后一步处理：
    // 先规约到最短表示（W >= 0），保证"输入小角度 -> 输出小角度"。
    if (WorldDelta.W < 0.0) {
        WorldDelta.X = -WorldDelta.X;
        WorldDelta.Y = -WorldDelta.Y;
        WorldDelta.Z = -WorldDelta.Z;
        WorldDelta.W = -WorldDelta.W;
    }

    // swing-twist 分解：只取绕平面法线的那一部分（即"投影到骨骼链平面上"）。
    // 规约到最短表示后 atan2 落在 (-90°, 90°]，所以这里的结果落在 (-180°, 180°]，
    // 即绕平面法线的旋转不超过半圈——超过半圈才会在 ±180° 处跳变，而 MaxBendAngle
    // 的默认 150° 已经挡在跳变点之内了。
    const FVector VectorPart(WorldDelta.X, WorldDelta.Y, WorldDelta.Z);
    const double Projection = FVector::DotProduct(VectorPart, PlaneNormal);
    // 负号是实测校准出来的方向约定（control 往这边拧，风箱就往这边弯）。
    return -2.0 * FMath::Atan2(Projection, WorldDelta.W);
}

// ============================================================
// 弧线 / 直线轨迹
// ============================================================

/**
 * 从链根到 effector 的一条平面曲线。
 * 当 |切线-弦夹角| 足够小时退化成直线，其余情况是一段圆弧。
 */
struct FChainTrack {
    FVector RootLocation = FVector::ZeroVector;
    FVector EffectorLocation = FVector::ZeroVector;
    FVector PlaneNormal = FVector::UpVector;
    FVector ChordDirection = FVector::ForwardVector;
    /** 平面内、垂直于弦，和 ChordDirection 构成右手平面基 */
    FVector ChordPerpendicular = FVector::RightVector;
    FVector CircleCenter = FVector::ZeroVector;
    double CircleRadius = 0.0;
    double ArcLength = 0.0;
    double BendAngle = 0.0;
    bool bIsStraight = true;

    void Build(const FVector& InRootLocation, const FVector& InEffectorLocation,
               const FVector& InPlaneNormal, double InBendAngle) {
        RootLocation = InRootLocation;
        EffectorLocation = InEffectorLocation;
        PlaneNormal = InPlaneNormal.GetSafeNormal();
        BendAngle = InBendAngle;

        const FVector Chord = EffectorLocation - RootLocation;
        const double ChordLength = Chord.Size();
        ChordDirection = (ChordLength > MinSegmentLength)
                             ? (Chord / ChordLength)
                             : FVector::ForwardVector;

        ChordPerpendicular =
            FVector::CrossProduct(PlaneNormal, ChordDirection).GetSafeNormal();
        if (ChordPerpendicular.IsNearlyZero()) {
            ChordPerpendicular =
                FVector::CrossProduct(ChordDirection,
                                      PickPerpendicularAxis(ChordDirection))
                    .GetSafeNormal();
        }

        bIsStraight = (FMath::Abs(BendAngle) < MinBendAngleRadians) ||
                      (ChordLength <= MinSegmentLength);
        if (bIsStraight) {
            CircleCenter = RootLocation;
            CircleRadius = 0.0;
            ArcLength = ChordLength;
            return;
        }

        const double AbsBendAngle = FMath::Abs(BendAngle);
        CircleRadius = ChordLength / (2.0 * FMath::Sin(AbsBendAngle));
        ArcLength = CircleRadius * 2.0 * AbsBendAngle;

        // c / (2·tan a) 自带符号：a 为正时圆心在 -p 一侧，为负时在 +p 一侧。
        const double CenterOffset = ChordLength / (2.0 * FMath::Tan(BendAngle));
        CircleCenter = (RootLocation + EffectorLocation) * 0.5 -
                       ChordPerpendicular * CenterOffset;
    }

    /** ArcLengthRatio：沿弧长（直线时沿长度）的比例，0 = 链根，1 = effector */
    FVector Evaluate(double ArcLengthRatio) const {
        const double ClampedRatio = FMath::Clamp(ArcLengthRatio, 0.0, 1.0);
        if (bIsStraight) {
            return RootLocation +
                   (EffectorLocation - RootLocation) * ClampedRatio;
        }

        // 绕平面法线总共转 -2a，a 的正负号已经把绕行方向包含进去了。
        const FQuat SweepRotation(PlaneNormal, -2.0 * BendAngle * ClampedRatio);
        return CircleCenter +
               SweepRotation.RotateVector(RootLocation - CircleCenter);
    }
};

// ============================================================
// 静止（rest）姿态数据
// ============================================================

struct FRestChainData {
    /** 每根链骨骼静止姿态的关节位置 */
    TArray<FVector> HeadLocations;
    /**
     * 每节骨骼的静止长度。前 NumChainBones-1 节是相邻关节的距离，
     * 最后一节 = 末端关节 -> effector 的静止位置（Unreal 只有关节没有骨尖，
     * 所以末端这半截长度只能靠 effector 的静止位置补出来）。
     */
    TArray<double> SegmentLengths;
    double TotalLength = 0.0;
    FVector EffectorLocation = FVector::ZeroVector;
    FVector PivotLocation = FVector::ZeroVector;
    /** 静止姿态下的弯曲平面法线，只在当前平面退化时用来兜底 */
    FVector PlaneNormal = FVector::UpVector;
};

/** 两点 + 一个参考点算平面法线，退化时返回零向量 */
FVector ComputePlaneNormal(const FVector& OriginLocation,
                           const FVector& FirstLocation,
                           const FVector& ReferenceLocation) {
    const FVector FirstDirection =
        (FirstLocation - OriginLocation).GetSafeNormal();
    const FVector ReferenceDirection =
        (ReferenceLocation - OriginLocation).GetSafeNormal();
    return FVector::CrossProduct(FirstDirection, ReferenceDirection);
}

}  // namespace AccordionBellowsIKImpl

// ============================================================================
// 执行
// ============================================================================

FRigUnit_AccordionBellowsIK_Execute() {
    using namespace AccordionBellowsIKImpl;

    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy) {
        return;
    }

    if (ChainLength < 2) {
        if (bUseDebug) {
            UE_LOG(LogControlRig, Error,
                   TEXT("[AccordionBellowsIK] ChainLength 至少要 2，当前是 %d"),
                   ChainLength);
        }
        return;
    }

    if (!Hierarchy->Contains(EndBone) ||
        !Hierarchy->Contains(EffectorControl) ||
        !Hierarchy->Contains(PivotControl)) {
        if (bUseDebug) {
            UE_LOG(LogControlRig, Error,
                   TEXT("[AccordionBellowsIK] EndBone / EffectorControl / "
                        "PivotControl 里有无效的条目：EndBone=%s "
                        "Effector=%s Pivot=%s"),
                   *EndBone.Name.ToString(), *EffectorControl.Name.ToString(),
                   *PivotControl.Name.ToString());
        }
        return;
    }

    // ---- 1. 从末端关节沿父级向上取链，翻转成「根 -> 末端」----
    TArray<FRigElementKey> ChainBones;
    ChainBones.Reserve(ChainLength);
    {
        int32 CurrentIndex = Hierarchy->GetIndex(EndBone);
        for (int32 BoneIndex = 0;
             BoneIndex < ChainLength && CurrentIndex != INDEX_NONE;
             ++BoneIndex) {
            const FRigElementKey CurrentKey = Hierarchy->GetKey(CurrentIndex);
            if (!Hierarchy->Find<FRigBoneElement>(CurrentKey)) {
                if (bUseDebug) {
                    UE_LOG(LogControlRig, Error,
                           TEXT("[AccordionBellowsIK] %s 不是骨骼"),
                           *CurrentKey.Name.ToString());
                }
                return;
            }
            ChainBones.Add(CurrentKey);
            CurrentIndex = Hierarchy->GetFirstParent(CurrentIndex);
        }
        Algo::Reverse(ChainBones);
    }

    const int32 NumChainBones = ChainBones.Num();
    if (NumChainBones < 2) {
        if (bUseDebug) {
            UE_LOG(LogControlRig, Error,
                   TEXT("[AccordionBellowsIK] 取到的链只有 %d 根骨骼"),
                   NumChainBones);
        }
        return;
    }

    // ---- 2. 静止姿态：每节长度、末端半截长度、静止平面 ----
    FRestChainData RestData;
    RestData.HeadLocations.SetNum(NumChainBones);
    RestData.SegmentLengths.SetNum(NumChainBones);
    RestData.TotalLength = 0.0;

    for (int32 BoneIndex = 0; BoneIndex < NumChainBones; ++BoneIndex) {
        RestData.HeadLocations[BoneIndex] =
            Hierarchy->GetInitialGlobalTransform(ChainBones[BoneIndex])
                .GetLocation();
    }

    for (int32 BoneIndex = 0; BoneIndex < NumChainBones - 1; ++BoneIndex) {
        const double SegmentLength =
            FVector::Dist(RestData.HeadLocations[BoneIndex],
                          RestData.HeadLocations[BoneIndex + 1]);
        RestData.SegmentLengths[BoneIndex] = SegmentLength;
        RestData.TotalLength += SegmentLength;
    }

    RestData.EffectorLocation =
        Hierarchy->GetInitialGlobalTransform(EffectorControl).GetLocation();
    RestData.PivotLocation =
        Hierarchy->GetInitialGlobalTransform(PivotControl).GetLocation();

    const double LastSegmentLength =
        FVector::Dist(RestData.HeadLocations.Last(), RestData.EffectorLocation);
    RestData.SegmentLengths.Last() = LastSegmentLength;
    RestData.TotalLength += LastSegmentLength;

    if (RestData.TotalLength <= MinSegmentLength) {
        if (bUseDebug) {
            UE_LOG(LogControlRig, Error,
                   TEXT("[AccordionBellowsIK] 链的总 rest 长度接近 0"));
        }
        return;
    }

    RestData.PlaneNormal =
        ComputePlaneNormal(RestData.HeadLocations[0], RestData.EffectorLocation,
                           RestData.PivotLocation);

    if (bUseDebug) {
        UE_LOG(LogControlRig, Warning,
               TEXT("[AccordionBellowsIK] 链骨骼数=%d 总 rest 长度=%.3f "
                    "最后一节=%.3f（静止时末端关节到 effector 的距离）"),
               NumChainBones, RestData.TotalLength, LastSegmentLength);
    }

    // ---- 3. 当前姿态输入 ----
    const FVector RootLocation =
        Hierarchy->GetGlobalTransform(ChainBones[0]).GetLocation();
    const FVector EffectorLocation =
        Hierarchy->GetGlobalTransform(EffectorControl).GetLocation();
    const FVector PivotLocation =
        Hierarchy->GetGlobalTransform(PivotControl).GetLocation();

    const FVector RootToEffector = EffectorLocation - RootLocation;
    const double ChordLength = RootToEffector.Size();
    if (ChordLength <= MinSegmentLength) {
        if (bUseDebug) {
            UE_LOG(LogControlRig, Error,
                   TEXT("[AccordionBellowsIK] effector "
                        "和链根重合，弯曲平面无法确定"));
        }
        return;
    }
    const FVector ChordDirection = RootToEffector / ChordLength;

    // ---- 4. 弯曲平面 ----
    FVector PlaneNormal = FVector::CrossProduct(
        ChordDirection, (PivotLocation - RootLocation).GetSafeNormal());
    if (!PlaneNormal.Normalize(MinSegmentLength)) {
        // 退化：链根、effector、pivot
        // 共线。用静止姿态的平面，跟着链根父级的运动一起转，
        // 至少保证这一帧不会出 NaN 或者乱翻。
        FQuat ParentDeltaRotation = FQuat::Identity;
        const FRigElementKey RootParentKey =
            Hierarchy->GetFirstParent(ChainBones[0]);
        if (Hierarchy->Contains(RootParentKey)) {
            ParentDeltaRotation =
                (Hierarchy->GetGlobalTransform(RootParentKey).GetRotation() *
                 Hierarchy->GetInitialGlobalTransform(RootParentKey)
                     .GetRotation()
                     .Inverse())
                    .GetNormalized();
        }

        PlaneNormal = ParentDeltaRotation.RotateVector(RestData.PlaneNormal);
        if (!PlaneNormal.Normalize(MinSegmentLength)) {
            PlaneNormal =
                FVector::CrossProduct(ChordDirection,
                                      PickPerpendicularAxis(ChordDirection))
                    .GetSafeNormal();
        }

        if (bUseDebug) {
            UE_LOG(LogControlRig, Warning,
                   TEXT("[AccordionBellowsIK] pivot 和链轴共线，弯曲平面退化，"
                        "本帧退回静止平面"));
        }
    }

    // ---- 5. 弯曲角 ----
    const double MaxBendRadians =
        FMath::DegreesToRadians(MaxBendAngle > 0.0f ? MaxBendAngle : 150.0f);
    const double RawBendAngle =
        ExtractBendAngle(Hierarchy, EffectorControl, PlaneNormal);
    const double BendAngle =
        FMath::Clamp(RawBendAngle, -MaxBendRadians, MaxBendRadians);

    // ---- 6. 轨迹 + 每根骨骼的新位置 ----
    FChainTrack Track;
    Track.Build(RootLocation, EffectorLocation, PlaneNormal, BendAngle);

    const double StretchRatio = Track.ArcLength / RestData.TotalLength;

    TArray<FVector> JointLocations;
    JointLocations.SetNum(NumChainBones + 1);
    JointLocations[0] = Track.Evaluate(0.0);
    {
        double AccumulatedLength = 0.0;
        for (int32 BoneIndex = 0; BoneIndex < NumChainBones; ++BoneIndex) {
            AccumulatedLength += RestData.SegmentLengths[BoneIndex];
            JointLocations[BoneIndex + 1] =
                Track.Evaluate(AccumulatedLength / RestData.TotalLength);
        }
    }

    // ---- 7. 每根骨骼的朝向 ----
    // roll 朝 pivot 哪一侧：整个链只用同一个符号，避免弧线绕过 pivot 附近时
    // roll 突然翻面。
    const double PivotSideSign =
        FVector::DotProduct(
            Track.ChordPerpendicular,
            PivotLocation - (RootLocation + EffectorLocation) * 0.5) >= 0.0
            ? 1.0
            : -1.0;

    TArray<FQuat> SolverRotations;
    SolverRotations.SetNum(NumChainBones);
    for (int32 BoneIndex = 0; BoneIndex < NumChainBones; ++BoneIndex) {
        FVector SegmentDirection =
            (JointLocations[BoneIndex + 1] - JointLocations[BoneIndex])
                .GetSafeNormal();
        if (SegmentDirection.IsNearlyZero()) {
            SegmentDirection = ChordDirection;
        }

        // 平面内、垂直于这一节方向的向量；乘同一个符号保证 roll 连续。
        const FVector WorldPivotDirection =
            FVector::CrossProduct(PlaneNormal, SegmentDirection)
                .GetSafeNormal() *
            PivotSideSign;

        SolverRotations[BoneIndex] = BuildBoneRotation(
            SegmentDirection, WorldPivotDirection, RadialAxis, PivotAxis);
    }

    // ---- 8. 混合基准：「如果本节点不存在，这条链现在会是什么姿态」----
    // 只用于 Weight < 1 的插值。用 InitialLocal
    // 逐级级联，不会被本节点自己上一帧的写回污染。
    TArray<FTransform> BaseTransforms;
    BaseTransforms.SetNum(NumChainBones);
    {
        FTransform ParentLiveTransform = FTransform::Identity;
        if (const FRigBoneElement* RootBoneElement =
                Hierarchy->Find<FRigBoneElement>(ChainBones[0])) {
            if (RootBoneElement->ParentElement) {
                ParentLiveTransform =
                    RootBoneElement->ParentElement->GetTransform().Get(
                        ERigTransformType::CurrentGlobal);
            }
        }

        FTransform AccumulatedTransform = ParentLiveTransform;
        for (int32 BoneIndex = 0; BoneIndex < NumChainBones; ++BoneIndex) {
            AccumulatedTransform =
                Hierarchy->GetInitialLocalTransform(ChainBones[BoneIndex]) *
                AccumulatedTransform;
            BaseTransforms[BoneIndex] = AccumulatedTransform;
        }
    }

    // ---- 9. 写回链骨骼 ----
    const double ClampedWeight =
        FMath::Clamp(static_cast<double>(Weight), 0.0, 1.0);
    for (int32 BoneIndex = 0; BoneIndex < NumChainBones; ++BoneIndex) {
        // 伸缩比不写在链骨骼上（写上去会让整套坐标系被压缩，形变和位置两套数据打架），
        // 链骨骼只负责位置和朝向，缩放沿用 FK 姿态。
        FTransform SolvedTransform(SolverRotations[BoneIndex],
                                   JointLocations[BoneIndex],
                                   BaseTransforms[BoneIndex].GetScale3D());

        if (ClampedWeight < 1.0) {
            SolvedTransform.SetLocation(
                FMath::Lerp(BaseTransforms[BoneIndex].GetLocation(),
                            JointLocations[BoneIndex], ClampedWeight));
            SolvedTransform.SetRotation(
                FQuat::Slerp(BaseTransforms[BoneIndex].GetRotation(),
                             SolverRotations[BoneIndex], ClampedWeight));
        }

        Hierarchy->SetGlobalTransform(ChainBones[BoneIndex], SolvedTransform,
                                      false, true, false);
    }

    // ---- 10. 把伸缩比写到每根链骨骼的第一个子骨骼上 ----
    // 只放大 ChildScaleAxis 指定的那一根轴，其余两轴保持它自己的初始 local
    // scale， 这样风箱只在径向伸缩，另外两个方向不变形。
    const FVector ScaleAxis = ChildScaleAxis.GetSafeNormal();
    for (int32 BoneIndex = 0; BoneIndex < NumChainBones; ++BoneIndex) {
        const TArray<FRigElementKey> ChildKeys =
            Hierarchy->GetChildren(ChainBones[BoneIndex], false);

        FRigElementKey DeformChildKey;
        for (const FRigElementKey& ChildKey : ChildKeys) {
            if (Hierarchy->Find<FRigBoneElement>(ChildKey)) {
                DeformChildKey = ChildKey;
                break;
            }
        }
        if (!Hierarchy->Contains(DeformChildKey)) {
            continue;
        }

        const FVector InitialScale =
            Hierarchy->GetInitialLocalTransform(DeformChildKey).GetScale3D();

        // 沿指定轴放大 StretchRatio 倍：把该轴上的分量乘以 (StretchRatio - 1)
        // 再加回去。 轴参数应该是 X/Y/Z 之一，这样不会引入剪切。
        FVector TargetScale = InitialScale;
        if (!ScaleAxis.IsNearlyZero()) {
            const double AxisScaleComponent =
                FVector::DotProduct(InitialScale, ScaleAxis);
            TargetScale +=
                ScaleAxis * (AxisScaleComponent * (StretchRatio - 1.0));
        }

        FTransform ChildLocalTransform =
            Hierarchy->GetLocalTransform(DeformChildKey);
        ChildLocalTransform.SetScale3D(
            (ClampedWeight < 1.0)
                ? FMath::Lerp(InitialScale, TargetScale, ClampedWeight)
                : TargetScale);

        Hierarchy->SetLocalTransform(DeformChildKey, ChildLocalTransform, false,
                                     true, false);
    }

    if (bUseDebug) {
        UE_LOG(LogControlRig, Warning,
               TEXT("[AccordionBellowsIK] 弦长=%.3f 切线角=%.3f°（原始 "
                    "%.3f°）弧长=%.3f 伸缩比=%.4f 权重=%.2f"),
               ChordLength, FMath::RadiansToDegrees(BendAngle),
               FMath::RadiansToDegrees(RawBendAngle), Track.ArcLength,
               StretchRatio, ClampedWeight);
    }
}
