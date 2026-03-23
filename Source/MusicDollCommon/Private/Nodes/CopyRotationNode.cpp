#include "Nodes/CopyRotationNode.h"

#include "AnimationCoreLibrary.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CopyRotationNode)

FRigUnit_CopyRotation_Execute() {
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    if (Weight < SMALL_NUMBER) {
        return;
    }

    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy) {
        if (bUseDebug) {
            UE_LOG(LogControlRig, Error,
                   TEXT("[CopyRotation] Hierarchy is null."));
        }
        return;
    }

    if (!CachedSourceBone.UpdateCache(SourceBone, Hierarchy)) {
        UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(
            TEXT("Source bone '%s' is not valid."), *SourceBone.ToString());
        return;
    }

    if (!CachedTargetBone.UpdateCache(TargetBone, Hierarchy)) {
        UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(
            TEXT("Target bone '%s' is not valid."), *TargetBone.ToString());
        return;
    }

    // Step 1: Get source INITIAL rotation as Euler angles
    FTransform SourceInitialTransform =
        bUseSourceLocal
            ? Hierarchy->GetInitialLocalTransform(CachedSourceBone.GetIndex())
            : Hierarchy->GetInitialGlobalTransform(CachedSourceBone.GetIndex());

    FVector SourceInitialEuler =
        AnimationCore::EulerFromQuat(SourceInitialTransform.GetRotation(), RotationOrder);

    // Step 2: Get source CURRENT rotation as Euler angles
    FTransform SourceCurrentTransform =
        bUseSourceLocal
            ? Hierarchy->GetLocalTransformByIndex(CachedSourceBone)
            : Hierarchy->GetGlobalTransformByIndex(CachedSourceBone);

    FVector SourceCurrentEuler =
        AnimationCore::EulerFromQuat(SourceCurrentTransform.GetRotation(), RotationOrder);

    // Step 3: Get target INITIAL rotation as Euler angles
    FTransform TargetInitialTransform =
        bUseTargetLocal
            ? Hierarchy->GetInitialLocalTransform(CachedTargetBone.GetIndex())
            : Hierarchy->GetInitialGlobalTransform(CachedTargetBone.GetIndex());

    FVector TargetInitialEuler =
        AnimationCore::EulerFromQuat(TargetInitialTransform.GetRotation(), RotationOrder);

    // Step 4: Get target CURRENT transform (we need the full transform to write back)
    FTransform TargetCurrentTransform =
        bUseTargetLocal
            ? Hierarchy->GetLocalTransformByIndex(CachedTargetBone)
            : Hierarchy->GetGlobalTransformByIndex(CachedTargetBone);

    FVector TargetCurrentEuler =
        AnimationCore::EulerFromQuat(TargetCurrentTransform.GetRotation(), RotationOrder);

    if (bUseDebug) {
        UE_LOG(LogControlRig, Warning,
               TEXT("[CopyRotation] SourceInitialEuler: (%.2f, %.2f, %.2f), "
                    "SourceCurrentEuler: (%.2f, %.2f, %.2f)"),
               SourceInitialEuler.X, SourceInitialEuler.Y, SourceInitialEuler.Z,
               SourceCurrentEuler.X, SourceCurrentEuler.Y, SourceCurrentEuler.Z);
        UE_LOG(LogControlRig, Warning,
               TEXT("[CopyRotation] TargetInitialEuler: (%.2f, %.2f, %.2f), "
                    "TargetCurrentEuler: (%.2f, %.2f, %.2f)"),
               TargetInitialEuler.X, TargetInitialEuler.Y, TargetInitialEuler.Z,
               TargetCurrentEuler.X, TargetCurrentEuler.Y, TargetCurrentEuler.Z);
    }

    // Step 5: Compute source_offset = SourceCurrent[SourceAxis] - SourceInitial[SourceAxis]
    float SourceInitialValue = 0.0f;
    float SourceCurrentValue = 0.0f;
    switch (SourceAxis) {
        case ERotationCopyAxis::X:
            SourceInitialValue = SourceInitialEuler.X;
            SourceCurrentValue = SourceCurrentEuler.X;
            break;
        case ERotationCopyAxis::Y:
            SourceInitialValue = SourceInitialEuler.Y;
            SourceCurrentValue = SourceCurrentEuler.Y;
            break;
        case ERotationCopyAxis::Z:
            SourceInitialValue = SourceInitialEuler.Z;
            SourceCurrentValue = SourceCurrentEuler.Z;
            break;
    }

    float SourceOffset = SourceCurrentValue - SourceInitialValue;

    // Step 6: Compute current_target_offset = TargetCurrent[TargetAxis] - TargetInitial[TargetAxis]
    float TargetInitialValue = 0.0f;
    float TargetCurrentValue = 0.0f;
    switch (TargetAxis) {
        case ERotationCopyAxis::X:
            TargetInitialValue = TargetInitialEuler.X;
            TargetCurrentValue = TargetCurrentEuler.X;
            break;
        case ERotationCopyAxis::Y:
            TargetInitialValue = TargetInitialEuler.Y;
            TargetCurrentValue = TargetCurrentEuler.Y;
            break;
        case ERotationCopyAxis::Z:
            TargetInitialValue = TargetInitialEuler.Z;
            TargetCurrentValue = TargetCurrentEuler.Z;
            break;
    }

    float CurrentTargetOffset = TargetCurrentValue - TargetInitialValue;

    // Step 7: Blend: target_offset = current_target_offset * (1 - weight) + source_offset * weight
    float ClampedWeight = FMath::Clamp(Weight, 0.0f, 1.0f);
    float BlendedOffset = CurrentTargetOffset * (1.0f - ClampedWeight) + SourceOffset * ClampedWeight;

    // Step 8: FinalValue = TargetInitial[TargetAxis] + blended_offset
    float FinalValue = TargetInitialValue + BlendedOffset;

    if (bUseDebug) {
        UE_LOG(LogControlRig, Warning,
               TEXT("[CopyRotation] SourceAxis=%d SourceOffset=%.2f (%.2f - %.2f), "
                    "TargetAxis=%d CurrentTargetOffset=%.2f (%.2f - %.2f), "
                    "Weight=%.2f, BlendedOffset=%.2f, FinalValue=%.2f"),
               static_cast<int32>(SourceAxis), SourceOffset,
               SourceCurrentValue, SourceInitialValue,
               static_cast<int32>(TargetAxis), CurrentTargetOffset,
               TargetCurrentValue, TargetInitialValue,
               ClampedWeight, BlendedOffset, FinalValue);
    }

    // Step 9: Apply to target Euler (start from current Euler, only replace TargetAxis)
    FVector NewTargetEuler = TargetCurrentEuler;
    switch (TargetAxis) {
        case ERotationCopyAxis::X:
            NewTargetEuler.X = FinalValue;
            break;
        case ERotationCopyAxis::Y:
            NewTargetEuler.Y = FinalValue;
            break;
        case ERotationCopyAxis::Z:
            NewTargetEuler.Z = FinalValue;
            break;
    }

    // Step 10: Convert back to quaternion and apply
    FQuat NewTargetQuat =
        AnimationCore::QuatFromEuler(NewTargetEuler, RotationOrder);
    NewTargetQuat.Normalize();

    TargetCurrentTransform.SetRotation(NewTargetQuat);

    if (bUseTargetLocal) {
        Hierarchy->SetLocalTransformByIndex(CachedTargetBone, TargetCurrentTransform,
                                            false, bPropagateToChildren);
    } else {
        Hierarchy->SetGlobalTransformByIndex(CachedTargetBone, TargetCurrentTransform,
                                             false, bPropagateToChildren);
    }

    if (bUseDebug) {
        FVector VerifyEuler =
            AnimationCore::EulerFromQuat(NewTargetQuat, RotationOrder);
        UE_LOG(LogControlRig, Warning,
               TEXT("[CopyRotation] Applied. NewTargetEuler: (%.2f, %.2f, "
                    "%.2f)"),
               VerifyEuler.X, VerifyEuler.Y, VerifyEuler.Z);
    }
}
