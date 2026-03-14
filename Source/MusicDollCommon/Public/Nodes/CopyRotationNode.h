#pragma once

#include "CoreMinimal.h"
#include "EulerTransform.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "Units/RigUnit.h"
#include "CopyRotationNode.generated.h"

UENUM(BlueprintType)
enum class ERotationCopyAxis : uint8 {
    X UMETA(DisplayName = "X (Roll)"),
    Y UMETA(DisplayName = "Y (Pitch)"),
    Z UMETA(DisplayName = "Z (Yaw)")
};

/**
 * Copies the rotation offset of a single axis from a source bone to a target bone.
 * Computes offset = SourceCurrent[SourceAxis] - SourceInitial[SourceAxis],
 * then applies Target[TargetAxis] = TargetInitial[TargetAxis] + offset * Weight.
 */
USTRUCT(meta = (DisplayName = "Copy Rotation Offset (Single Axis)",
                Category = "Transforms",
                Keywords = "Copy,Rotation,Offset,Axis,Euler,Mirror",
                NodeColor = "0, 0.364706, 1.0",
                Varying))
struct MUSICDOLLCOMMON_API FRigUnit_CopyRotation : public FRigUnitMutable {
    GENERATED_BODY()

    FRigUnit_CopyRotation()
        : SourceBone(NAME_None, ERigElementType::Bone),
          TargetBone(NAME_None, ERigElementType::Bone),
          bUseSourceLocal(true),
          bUseTargetLocal(true),
          SourceAxis(ERotationCopyAxis::X),
          TargetAxis(ERotationCopyAxis::X),
          Weight(1.0f),
          RotationOrder(EEulerRotationOrder::XZY),
          bPropagateToChildren(true),
          bUseDebug(false) {}

    RIGVM_METHOD()
    virtual void Execute() override;

    /** The bone to read rotation from */
    UPROPERTY(meta = (Input, ExpandByDefault))
    FRigElementKey SourceBone;

    /** The bone to write rotation to */
    UPROPERTY(meta = (Input, ExpandByDefault))
    FRigElementKey TargetBone;

    /** If true, read source rotation in local space; otherwise global */
    UPROPERTY(meta = (Input))
    bool bUseSourceLocal;

    /** If true, read/write target rotation in local space; otherwise global */
    UPROPERTY(meta = (Input))
    bool bUseTargetLocal;

    /** Which axis to read from the source bone's rotation */
    UPROPERTY(meta = (Input))
    ERotationCopyAxis SourceAxis;

    /** Which axis to write to on the target bone's rotation */
    UPROPERTY(meta = (Input))
    ERotationCopyAxis TargetAxis;

    /**
     * Blend weight between target's original rotation and the source rotation.
     * 0.0 = keep target's original, 1.0 = fully use source's value.
     */
    UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "1.0"))
    float Weight;

    /** Euler rotation decomposition order */
    UPROPERTY(meta = (Input))
    EEulerRotationOrder RotationOrder;

    /** If true, propagate transform changes to children */
    UPROPERTY(meta = (Input))
    bool bPropagateToChildren;

    /** Enable debug logging */
    UPROPERTY(meta = (Input))
    bool bUseDebug;

    // Used to cache the internally used indices
    UPROPERTY()
    FCachedRigElement CachedSourceBone;

    UPROPERTY()
    FCachedRigElement CachedTargetBone;
};
