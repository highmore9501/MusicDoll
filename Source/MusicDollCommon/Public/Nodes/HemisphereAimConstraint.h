#pragma once

#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "Units/RigUnit.h"
#include "HemisphereAimConstraint.generated.h"

/**
 * Aim-constrains a control/bone so it looks toward a target, but clamps the
 * look direction to the front hemisphere defined by HemisphereAxis (world space).
 *
 * When the target falls behind the hemisphere (dot(dir, HemisphereAxis) < 0),
 * the direction is projected onto the equatorial plane so the head never spins
 * more than 90° from the front direction, avoiding sudden 180° flips.
 *
 * Output ClampedLookAtPosition is the point where the head-to-target ray
 * intersects the hemisphere surface — useful for feeding into other nodes.
 */
USTRUCT(meta = (DisplayName = "Hemisphere Aim Constraint",
                Category = "Hierarchy",
                Keywords = "Aim,LookAt,Hemisphere,Clamp,Head,Constraint",
                NodeColor = "0.2, 0.8, 0.4",
                Version = "1.0"))
struct MUSICDOLLCOMMON_API FRigUnit_HemisphereAimConstraint : public FRigUnitMutable {
    GENERATED_BODY()

    FRigUnit_HemisphereAimConstraint()
        : Item(NAME_None, ERigElementType::Control),
          LookAtTarget(FVector::ZeroVector),
          AimAxis(FVector(0.0f, 1.0f, 0.0f)),
          UpAxis(FVector(0.0f, 0.0f, 1.0f)),
          HemisphereAxis(FVector(0.0f, 1.0f, 0.0f)),
          HemisphereRadius(100.0f),
          Weight(1.0f),
          bPropagateToChildren(true),
          ClampedLookAtPosition(FVector::ZeroVector) {}

    RIGVM_METHOD()
    virtual void Execute() override;

    /** The control or bone to aim (e.g. the head control). */
    UPROPERTY(meta = (Input, ExpandByDefault))
    FRigElementKey Item;

    /** World-space position of the look-at target (e.g. the lookAt control). */
    UPROPERTY(meta = (Input))
    FVector LookAtTarget;

    /**
     * Local-space axis of the item that should point toward the target.
     * Default: Y axis (0, 1, 0) — Unreal's typical "forward" for controls.
     */
    UPROPERTY(meta = (Input))
    FVector AimAxis;

    /**
     * World-space up direction used to stabilise roll.
     * Default: Z axis (0, 0, 1).
     */
    UPROPERTY(meta = (Input))
    FVector UpAxis;

    /**
     * World-space axis that defines the "front" of the hemisphere.
     * The item is never rotated more than 90° away from this direction.
     * Default: Y axis (0, 1, 0).
     */
    UPROPERTY(meta = (Input))
    FVector HemisphereAxis;

    /**
     * Radius of the virtual hemisphere used to compute ClampedLookAtPosition.
     * Has no effect on rotation, only on the output position value.
     */
    UPROPERTY(meta = (Input, UIMin = "1.0"))
    float HemisphereRadius;

    /** Blend weight between the original rotation and the constrained rotation. */
    UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "1.0"))
    float Weight;

    /** Whether to propagate the transform change to child elements. */
    UPROPERTY(meta = (Input))
    bool bPropagateToChildren;

    /**
     * World-space position where the head-to-target ray intersects the
     * hemisphere surface after clamping.
     */
    UPROPERTY(meta = (Output))
    FVector ClampedLookAtPosition;

    // Internal cache
    UPROPERTY()
    FCachedRigElement CachedItem;
};
