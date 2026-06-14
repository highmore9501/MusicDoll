#include "Nodes/FixFoot.h"

#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "Units/RigUnit.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FixFoot)

FRigUnit_FixFoot_Execute() {
    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy) {
        UE_LOG(LogControlRig, Error, TEXT("[FixFoot] Hierarchy is null."));
        return;
    }

    // Resolve and cache the bone element
    if (!CachedBone.UpdateCache(Bone, Hierarchy)) {
        UE_LOG(
            LogControlRig, Error,
            TEXT("[FixFoot] Failed to resolve Bone element. Name='%s' Type=%d"),
            *Bone.Name.ToString(), (int32)Bone.Type);
        return;
    }

    // Resolve and cache the control element
    if (!CachedControl.UpdateCache(Control, Hierarchy)) {
        UE_LOG(LogControlRig, Error,
               TEXT("[FixFoot] Failed to resolve Control element. Name='%s' "
                    "Type=%d"),
               *Control.Name.ToString(), (int32)Control.Type);
        return;
    }

    // Use GetIndex() to read the bone transform — consistent with the rest of
    // this codebase and avoids a secondary key-lookup that can return Identity
    // when the element type in the key doesn't match the stored element.
    const FTransform BoneTransform =
        Hierarchy->GetGlobalTransform(CachedBone.GetIndex());
    const float BoneHeight = BoneTransform.GetLocation().Z;

    const bool bIsBelowGround = BoneHeight < GroundHeight;

    UE_LOG(LogControlRig, Verbose,
           TEXT("[FixFoot] BonePos=(%.2f,%.2f,%.2f) BoneHeight=%.4f "
                "GroundHeight=%.4f bIsBelowGround=%s"),
           BoneTransform.GetLocation().X, BoneTransform.GetLocation().Y,
           BoneTransform.GetLocation().Z, BoneHeight, GroundHeight,
           bIsBelowGround ? TEXT("true") : TEXT("false"));

    if (!bIsBelowGround) {
        // Bone is at or above the ground — update the locked transform and
        // drive the control directly with the bone transform.
        WorkData.LockedTransform = BoneTransform;
        Hierarchy->SetGlobalTransform(CachedControl.GetKey(), BoneTransform,
                                      false, bPropagateToChildren, false);

        UE_LOG(LogControlRig, Verbose,
               TEXT("[FixFoot] Above ground — control set to bone pos "
                    "(%.2f,%.2f,%.2f)"),
               BoneTransform.GetLocation().X, BoneTransform.GetLocation().Y,
               BoneTransform.GetLocation().Z);
    } else {
        // Bone is below the ground — lock the control at the captured
        // transform from the moment the bone first descended to ground level.
        Hierarchy->SetGlobalTransform(CachedControl.GetKey(),
                                      WorkData.LockedTransform, false,
                                      bPropagateToChildren, false);

        UE_LOG(
            LogControlRig, Verbose,
            TEXT("[FixFoot] Below ground — control locked at (%.2f,%.2f,%.2f)"),
            WorkData.LockedTransform.GetLocation().X,
            WorkData.LockedTransform.GetLocation().Y,
            WorkData.LockedTransform.GetLocation().Z);
    }

    WorkData.bWasBelowGround = bIsBelowGround;
}
