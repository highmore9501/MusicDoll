#include "Nodes/SetExtControl.h"

#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SetExtControl)

FRigUnit_SetExtControl_Execute()
{
    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy)
    {
        return;
    }

    // 三个元素全部解析成功才继续，否则本帧不动任何东西
    if (!CachedOriginControl.UpdateCache(OriginControl, Hierarchy) ||
        !CachedMidpointControl.UpdateCache(MidpointControl, Hierarchy) ||
        !CachedExtControl.UpdateCache(ExtControl, Hierarchy))
    {
        return;
    }

    // 实时读取原点的完整变换（旋转用来取次轴）和中点的位置
    const FTransform OriginTransform =
        Hierarchy->GetGlobalTransform(CachedOriginControl.GetIndex());
    const FVector MidpointPosition =
        Hierarchy->GetGlobalTransform(CachedMidpointControl.GetIndex()).GetLocation();

    // ExtControl 落在 原点→中点 直线的延长线上（中点关于原点的外推点）
    const FVector ExtPosition =
        MidpointPosition * 2.0f - OriginTransform.GetLocation();

    // 保留原有的缩放和朝向，先只改位置
    FTransform ExtTransform =
        Hierarchy->GetGlobalTransform(CachedExtControl.GetIndex());
    ExtTransform.SetLocation(ExtPosition);

    // 主轴指向中点。注意 ExtControl 在延长线上，所以这个方向等价于 中点→原点 的方向
    const FVector AimDirection = (MidpointPosition - ExtPosition).GetSafeNormal();
    const FVector PrimaryAxisLocal = PrimaryAxis.GetSafeNormal();
    const FVector SecondAxisLocal = SecondAxis.GetSafeNormal();

    if (AimDirection.IsNearlyZero() || PrimaryAxisLocal.IsNearlyZero())
    {
        // 原点与中点重合，或者主轴给成了零向量：位置照写，朝向保持原样
        Hierarchy->SetGlobalTransform(CachedExtControl.GetKey(), ExtTransform,
                                      /*bInitial=*/false, bPropagateToChildren,
                                      /*bSetupUndo=*/false);
        return;
    }

    // 原点次轴在世界空间中的方向：它和直线张成那个平面
    const FVector OriginSecondAxisWorld =
        OriginTransform.GetRotation().RotateVector(SecondAxisLocal);

    // 该平面内垂直于直线的方向 = 把原点次轴投影到垂直于直线的平面上。
    // 投影结果与被投影向量同侧（两者点积非负），所以天然满足"和原点次轴指向同一侧"。
    const FVector DesiredSecondAxisWorld =
        (OriginSecondAxisWorld -
         AimDirection * FVector::DotProduct(OriginSecondAxisWorld, AimDirection))
            .GetSafeNormal();

    // 本地次轴中垂直于本地主轴的那部分（次轴通常不与主轴严格垂直，这里做正交化）
    const FVector LocalSecondAxisPerpendicular =
        (SecondAxisLocal -
         PrimaryAxisLocal * FVector::DotProduct(SecondAxisLocal, PrimaryAxisLocal))
            .GetSafeNormal();

    FQuat FinalRotation = ExtTransform.GetRotation();

    if (DesiredSecondAxisWorld.IsNearlyZero() ||
        LocalSecondAxisPerpendicular.IsNearlyZero())
    {
        // 原点次轴与直线完全重合（平面无定义），或本地次轴与主轴共线：
        // 退化成只约束主轴，用最小旋转把当前主轴转过去，已有的滚转原样保留
        const FVector CurrentAimDirection =
            FinalRotation.RotateVector(PrimaryAxisLocal);
        FinalRotation =
            (FQuat::FindBetweenNormals(CurrentAimDirection, AimDirection) *
             FinalRotation)
                .GetNormalized();
    }
    else
    {
        // 1) 最小旋转：把本地主轴转到直线方向
        const FQuat PrimaryRotation =
            FQuat::FindBetweenNormals(PrimaryAxisLocal, AimDirection);

        // 2) 绕主轴扭转：把本地次轴转到平面上"同一侧"的那个方向
        const FVector RotatedSecondAxisWorld =
            PrimaryRotation.RotateVector(LocalSecondAxisPerpendicular);
        const float CosTwist = FMath::Clamp(
            FVector::DotProduct(RotatedSecondAxisWorld, DesiredSecondAxisWorld),
            -1.0f, 1.0f);
        const float SinTwist = FVector::DotProduct(
            FVector::CrossProduct(RotatedSecondAxisWorld,
                                  DesiredSecondAxisWorld),
            AimDirection);
        const FQuat TwistRotation(AimDirection,
                                  FMath::Atan2(SinTwist, CosTwist));

        // 先做主轴对齐，再绕主轴扭转（UE 里 Q1*Q2 表示先应用 Q2）
        FinalRotation = (TwistRotation * PrimaryRotation).GetNormalized();
    }

    ExtTransform.SetRotation(FinalRotation);

    Hierarchy->SetGlobalTransform(CachedExtControl.GetKey(), ExtTransform,
                                  /*bInitial=*/false, bPropagateToChildren,
                                  /*bSetupUndo=*/false);
}
