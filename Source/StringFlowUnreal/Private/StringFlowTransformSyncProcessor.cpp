#include "StringFlowTransformSyncProcessor.h"

#include "Animation/SkeletalMeshActor.h"
#include "Common/Public/InstrumentControlRigUtility.h"
#include "ControlRig/Public/ControlRig.h"
#include "Engine/Engine.h"
#include "StringFlowControlRigProcessor.h"

bool UStringFlowTransformSyncProcessor::SyncAllInstrumentTransforms(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncAllInstrumentTransforms: StringFlowActor is null"));
        return false;
    }

    if (!StringFlowActor->bEnableRealtimeSync) {
        return true;
    }

    bool bStringSuccess = SyncStringInstrumentTransform(StringFlowActor);
    bool bBowSuccess = SyncBowTransform(StringFlowActor);

    return bStringSuccess && bBowSuccess;
}

bool UStringFlowTransformSyncProcessor::SyncStringInstrumentTransform(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncStringInstrumentTransform: StringFlowActor is null"));
        return false;
    }

    // 如果禁用实时同步，直接返回，避免任何日志
    if (!StringFlowActor->bEnableRealtimeSync) {
        return true;
    }

    if (!StringFlowActor->StringInstrument) {
        return false;
    }

    // 使用通用的 ParentBetweenControlRig 方法建立父子关系
    // 人物的 controller_root 作为父 Control
    // 乐器的 violin_root 作为子 Control
    return FInstrumentControlRigUtility::ParentBetweenControlRig(
        StringFlowActor->SkeletalMeshActor, TEXT("controller_root"),
        StringFlowActor->StringInstrument, TEXT("violin_root"));
}

bool UStringFlowTransformSyncProcessor::SyncBowTransform(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncBowTransform: StringFlowActor is null"));
        return false;
    }

    // 如果禁用实时同步，直接返回，避免任何日志
    if (!StringFlowActor->bEnableRealtimeSync) {
        return true;
    }

    if (!StringFlowActor->Bow) {
        return false;
    }

    // 从人物身上获取琴弓位置源：bow_controller
    FTransform BowControllerTransform;
    if (!FInstrumentControlRigUtility::GetControlRigControlTransform(
            StringFlowActor->SkeletalMeshActor, TEXT("bow_controller"),
            BowControllerTransform)) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("SyncBowTransform: Failed to get bow_controller transform"));
        return false;
    }

    FVector BowPosition = BowControllerTransform.GetLocation();

    // 从人物身上获取琴弓朝向源：string_touch_point
    FTransform StringTouchPointTransform;
    if (!FInstrumentControlRigUtility::GetControlRigControlTransform(
            StringFlowActor->SkeletalMeshActor, TEXT("string_touch_point"),
            StringTouchPointTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncBowTransform: Failed to get string_touch_point "
                    "transform"));
        return false;
    }

    FVector StringTouchPointPosition = StringTouchPointTransform.GetLocation();

    // ========== 计算琴弓的目标旋转 =========
    // 计算琴弓应该指向的方向
    FVector DirectionToString =
        (StringTouchPointPosition - BowPosition).GetSafeNormal();

    // 获取琴弓当前的旋转（从 bow_controller）
    FQuat CurrentBowRotation = BowControllerTransform.GetRotation();

    // 获取琴弓在其自身坐标系中的"指向"轴方向
    // BowAxisTowardString
    // 应该是一个单位向量，表示琴弓在局部坐标系中应该指向的轴 例如：(1,0,0) 表示
    // X 轴，(0,1,0) 表示 Y 轴
    FVector LocalForwardAxis =
        StringFlowActor->BowAxisTowardString.GetSafeNormal();

    if (LocalForwardAxis.IsNearlyZero()) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncBowTransform: BowAxisTowardString is zero"));
        return false;
    }

    // 琴弓当前局部坐标系中该轴在世界坐标中的方向
    FVector CurrentForwardInWorld =
        CurrentBowRotation.RotateVector(LocalForwardAxis);

    // ========== 计算需要的旋转增量 =========
    // 找到需要将当前朝向转变为目标朝向的旋转
    FQuat DeltaRotation =
        FQuat::FindBetweenNormals(CurrentForwardInWorld, DirectionToString);

    // 目标旋转 = 增量旋转 × 当前旋转
    FQuat TargetRotation = DeltaRotation * CurrentBowRotation;

    // 构建目标变换（包含位置和旋转）
    FTransform TargetBowTransform(TargetRotation, BowPosition,
                                  FVector(1.0f, 1.0f, 1.0f));

    // 变换已变化，更新琴弓：应用位置和旋转
    if (!FInstrumentControlRigUtility::SetControllerTransform(
            StringFlowActor->Bow, TEXT("bow_ctrl"), BowPosition,
            TargetRotation)) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncBowTransform: Failed to set bow_ctrl transform"));
        return false;
    }

    return true;
}

bool UStringFlowTransformSyncProcessor::GetBoneTransform(
    ASkeletalMeshActor* SkeletalActor, const FString& BoneName,
    FVector& OutLocation, FQuat& OutRotation) {
    if (!SkeletalActor) {
        UE_LOG(LogTemp, Error, TEXT("GetBoneTransform: SkeletalActor is null"));
        return false;
    }

    USkeletalMeshComponent* SkeletalMeshComponent =
        SkeletalActor->GetSkeletalMeshComponent();
    if (!SkeletalMeshComponent) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetBoneTransform: SkeletalMeshComponent is null"));
        return false;
    }

    int32 BoneIndex = SkeletalMeshComponent->GetBoneIndex(*BoneName);
    if (BoneIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetBoneTransform: Bone '%s' not found in skeletal mesh"),
               *BoneName);
        return false;
    }

    // 获取骨骼的世界变换
    // 使用 GetBoneTransform 的正确版本
    const FTransform& ComponentSpaceTransform =
        SkeletalMeshComponent->GetComponentSpaceTransforms()[BoneIndex];

    FTransform BoneWorldTransform =
        ComponentSpaceTransform * SkeletalActor->GetActorTransform();

    OutLocation = BoneWorldTransform.GetLocation();
    OutRotation = BoneWorldTransform.Rotator().Quaternion();

    return true;
}

FQuat UStringFlowTransformSyncProcessor::RotateTowardDirection(
    const FQuat& CurrentRotation, const FVector& AxisToRotate,
    const FVector& TargetDirection) {
    if (TargetDirection.IsNearlyZero()) {
        return CurrentRotation;
    }

    // 获取当前旋转的前向向量（假设默认沿 X 轴）
    FVector CurrentAxis =
        CurrentRotation.RotateVector(AxisToRotate.GetSafeNormal());

    // 计算从当前轴到目标方向的旋转
    // 使用 FindBetween 来找到两个向量之间的旋转
    FQuat DeltaRotation =
        FQuat::FindBetweenNormals(CurrentAxis, TargetDirection);

    // 应用增量旋转
    FQuat ResultRotation = DeltaRotation * CurrentRotation;

    return ResultRotation;
}

FQuat UStringFlowTransformSyncProcessor::RotateWithTwoConstraints(
    const FVector& ForwardAxis, const FVector& UpAxis,
    const FVector& TargetForwardDirection, const FVector& TargetUpDirection) {
    // 规范化输入向量
    FVector NormalizedForwardAxis = ForwardAxis.GetSafeNormal();
    FVector NormalizedUpAxis = UpAxis.GetSafeNormal();
    FVector NormalizedTargetForward = TargetForwardDirection.GetSafeNormal();
    FVector NormalizedTargetUp = TargetUpDirection.GetSafeNormal();

    // 如果向量几乎平行，返回单位四元数
    if (NormalizedForwardAxis.IsNearlyZero() ||
        NormalizedUpAxis.IsNearlyZero() ||
        NormalizedTargetForward.IsNearlyZero() ||
        NormalizedTargetUp.IsNearlyZero()) {
        return FQuat::Identity;
    }

    // 检查前向轴和上轴是否几乎平行（不能作为坐标系的两个轴）
    float ForwardUpDot = FMath::Abs(NormalizedForwardAxis | NormalizedUpAxis);
    if (ForwardUpDot > 0.95f) {
        UE_LOG(LogTemp, Warning,
               TEXT("RotateWithTwoConstraints: ForwardAxis and UpAxis are "
                    "nearly parallel"));
        return FQuat::Identity;
    }

    // 使用 LookAt 方式构建坐标系
    // 首先处理前向约束（指向目标）
    FVector RightAxis =
        NormalizedTargetUp ^ NormalizedTargetForward;  // 叉积获得右轴
    RightAxis = RightAxis.GetSafeNormal();

    // 根据右轴和前向轴重新计算上轴，确保三个轴正交
    FVector RecomputedUpAxis = NormalizedTargetForward ^ RightAxis;
    RecomputedUpAxis = RecomputedUpAxis.GetSafeNormal();

    // 构建目标旋转矩阵
    // 注意：FMatrix 的构造函数参数为行向量（Forward, Right, Up）
    FMatrix TargetMatrix(
        FPlane(NormalizedTargetForward.X, NormalizedTargetForward.Y,
               NormalizedTargetForward.Z, 0.0f),
        FPlane(RightAxis.X, RightAxis.Y, RightAxis.Z, 0.0f),
        FPlane(RecomputedUpAxis.X, RecomputedUpAxis.Y, RecomputedUpAxis.Z,
               0.0f),
        FPlane(0.0f, 0.0f, 0.0f, 1.0f));

    // 构建源旋转矩阵（基于输入的轴定义）
    FVector SourceRightAxis = NormalizedUpAxis ^ NormalizedForwardAxis;
    SourceRightAxis = SourceRightAxis.GetSafeNormal();
    FVector RecomputedSourceUpAxis = NormalizedForwardAxis ^ SourceRightAxis;
    RecomputedSourceUpAxis = RecomputedSourceUpAxis.GetSafeNormal();

    FMatrix SourceMatrix(
        FPlane(NormalizedForwardAxis.X, NormalizedForwardAxis.Y,
               NormalizedForwardAxis.Z, 0.0f),
        FPlane(SourceRightAxis.X, SourceRightAxis.Y, SourceRightAxis.Z, 0.0f),
        FPlane(RecomputedSourceUpAxis.X, RecomputedSourceUpAxis.Y,
               RecomputedSourceUpAxis.Z, 0.0f),
        FPlane(0.0f, 0.0f, 0.0f, 1.0f));

    // 计算相对旋转矩阵并转换为四元数
    FMatrix RelativeMatrix = TargetMatrix * SourceMatrix.Inverse();
    FQuat ResultQuat(RelativeMatrix);

    return ResultQuat;
}
