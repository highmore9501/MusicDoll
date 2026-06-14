#pragma once

#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "Units/RigUnit.h"
#include "FixFoot.generated.h"

/** Persistent work data for FRigUnit_FixFoot. */
USTRUCT()
struct MUSICDOLLCOMMON_API FRigUnit_FixFoot_WorkData {
    GENERATED_BODY()

    /** The transform locked at the moment the bone descended to ground level. */
    UPROPERTY()
    FTransform LockedTransform = FTransform::Identity;

    /** Whether the bone was below ground in the previous frame. */
    UPROPERTY()
    bool bWasBelowGround = false;
};

/**
 * Fix Foot — keeps a control pinned at the last above-ground bone transform
 * while the bone is below the specified ground height.
 *
 * - When bone Z >= GroundHeight : control global transform tracks the bone,
 *   and LockedTransform is updated to the current bone transform.
 * - When bone Z < GroundHeight  : control global transform is held at
 *   LockedTransform (captured at the moment the bone crossed ground level).
 */
USTRUCT(meta = (DisplayName = "Fix Foot",
                Category = "Hierarchy",
                Keywords = "Foot,IK,Ground,Lock,Plant",
                NodeColor = "0.8, 0.5, 0.2",
                Version = "1.0"))
struct MUSICDOLLCOMMON_API FRigUnit_FixFoot : public FRigUnitMutable {
    GENERATED_BODY()

    FRigUnit_FixFoot()
        : Bone(NAME_None, ERigElementType::Bone),
          Control(NAME_None, ERigElementType::Control),
          GroundHeight(0.0f),
          bPropagateToChildren(true) {}

    RIGVM_METHOD()
    virtual void Execute() override;

    /** The bone whose world-space height is monitored. */
    UPROPERTY(meta = (Input, ExpandByDefault))
    FRigElementKey Bone;

    /** The control whose global transform will be driven by this node. */
    UPROPERTY(meta = (Input, ExpandByDefault))
    FRigElementKey Control;

    /** World-space Z height that defines the ground plane. */
    UPROPERTY(meta = (Input))
    float GroundHeight;

    /** Whether to propagate the transform change to child elements. */
    UPROPERTY(meta = (Input))
    bool bPropagateToChildren;

    // Internal persistent state
    UPROPERTY(transient)
    FRigUnit_FixFoot_WorkData WorkData;

    UPROPERTY(transient)
    FCachedRigElement CachedBone;

    UPROPERTY(transient)
    FCachedRigElement CachedControl;
};
