#include "Nodes/HemisphereAimConstraint.h"

#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HemisphereAimConstraint)

// ---------------------------------------------------------------------------
// Clamp a direction vector to the front hemisphere defined by HemisphereNormal.
//
// If the direction already points into the front hemisphere
// (dot >= 0), it is returned unchanged.
// Otherwise it is projected onto the equatorial plane (the great circle at
// 90° from HemisphereNormal) and re-normalised.  This guarantees the
// returned direction is always within [-90°, +90°] of HemisphereNormal,
// completely eliminating 180° flips.
// ---------------------------------------------------------------------------
static FVector ClampDirectionToHemisphere(const FVector& Direction,
                                          const FVector& HemisphereNormal)
{
    const FVector HemNorm = HemisphereNormal.GetSafeNormal();
    const float Dot = FVector::DotProduct(Direction, HemNorm);

    if (Dot >= 0.0f)
    {
        // Already in the front hemisphere — nothing to do.
        return Direction;
    }

    // Project onto the equatorial plane: remove the component along HemNorm.
    FVector Projected = Direction - HemNorm * Dot;  // Dot is negative so this adds

    // Re-normalise.  If the direction was exactly opposite to HemNorm the
    // projection is zero; in that case fall back to any equatorial vector.
    if (!Projected.Normalize())
    {
        // Degenerate case: direction was anti-parallel to HemisphereNormal.
        // Pick an arbitrary perpendicular direction.
        Projected = FVector::CrossProduct(HemNorm, FVector::UpVector);
        if (!Projected.Normalize())
        {
            Projected = FVector::CrossProduct(HemNorm, FVector::ForwardVector);
            Projected.Normalize();
        }
    }

    return Projected;
}

// ---------------------------------------------------------------------------
// Build a rotation that:
//   1. Aligns AimAxisLocal (local space) with WorldAimDir (world space).
//   2. Minimises roll by keeping WorldUpDir (world space) as close as
//      possible to the local UpAxisLocal, via a secondary twist.
// ---------------------------------------------------------------------------
static FQuat BuildAimRotation(const FQuat&   CurrentRotation,
                              const FVector& AimAxisLocal,
                              const FVector& WorldAimDir,
                              const FVector& UpAxisLocal,
                              const FVector& WorldUpDir)
{
    const FVector AimLocal = AimAxisLocal.GetSafeNormal();
    const FVector AimWorld = WorldAimDir.GetSafeNormal();

    // --- Step 1: primary aim ---
    // Rotate so that AimAxisLocal (expressed in world space via CurrentRotation)
    // points toward WorldAimDir.
    const FVector CurrentAimWorld = CurrentRotation.RotateVector(AimLocal);
    FQuat PrimaryRot = FQuat::FindBetweenNormals(CurrentAimWorld, AimWorld);
    FQuat NewRotation = (PrimaryRot * CurrentRotation).GetNormalized();

    // --- Step 2: secondary twist (roll stabilisation) ---
    // After the primary rotation, compute where UpAxisLocal currently points
    // and twist around AimWorld to bring it as close as possible to WorldUpDir.
    const FVector UpLocal = UpAxisLocal.GetSafeNormal();
    FVector CurrentUpWorld = NewRotation.RotateVector(UpLocal);

    // Project both directions onto the plane perpendicular to AimWorld.
    auto ProjectOntoPlane = [&](const FVector& V) -> FVector
    {
        return (V - AimWorld * FVector::DotProduct(V, AimWorld)).GetSafeNormal();
    };

    FVector UpProjected       = ProjectOntoPlane(CurrentUpWorld);
    FVector TargetUpProjected = ProjectOntoPlane(WorldUpDir);

    if (!UpProjected.IsNearlyZero() && !TargetUpProjected.IsNearlyZero())
    {
        FQuat TwistRot = FQuat::FindBetweenNormals(UpProjected, TargetUpProjected);
        NewRotation = (TwistRot * NewRotation).GetNormalized();
    }

    return NewRotation;
}

// ---------------------------------------------------------------------------

FRigUnit_HemisphereAimConstraint_Execute()
{
    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy)
    {
        return;
    }

    // Resolve and cache the element.
    if (!CachedItem.UpdateCache(Item, Hierarchy))
    {
        return;
    }

    // Guard against degenerate inputs.
    const FVector HemNorm      = HemisphereAxis.GetSafeNormal();
    const FVector AimAxisLocal = AimAxis.GetSafeNormal();
    const FVector UpAxisLocal  = UpAxis.GetSafeNormal();

    if (HemNorm.IsNearlyZero() || AimAxisLocal.IsNearlyZero() || UpAxisLocal.IsNearlyZero())
    {
        return;
    }

    // Current world transform of the item.
    const FTransform CurrentTransform = Hierarchy->GetGlobalTransform(CachedItem.GetIndex());
    const FVector    ItemPosition     = CurrentTransform.GetLocation();

    // Direction from the item toward the raw look-at target.
    FVector RawDirection = (LookAtTarget - ItemPosition);
    if (!RawDirection.Normalize())
    {
        // Target coincides with the item — nothing sensible to do.
        return;
    }

    // Clamp to the front hemisphere.
    const FVector ClampedDirection = ClampDirectionToHemisphere(RawDirection, HemNorm);

    // Compute the output position (on the hemisphere surface).
    const float   Radius          = FMath::Max(HemisphereRadius, 1.0f);
    ClampedLookAtPosition         = ItemPosition + ClampedDirection * Radius;

    // Build the target rotation.
    const FQuat TargetRotation = BuildAimRotation(
        CurrentTransform.GetRotation(),
        AimAxisLocal,
        ClampedDirection,
        UpAxisLocal,
        UpAxis.GetSafeNormal()  // world-space up
    );

    // Blend with weight.
    const FQuat BlendedRotation = FQuat::Slerp(
        CurrentTransform.GetRotation(),
        TargetRotation,
        FMath::Clamp(Weight, 0.0f, 1.0f)
    ).GetNormalized();

    // Apply.
    FTransform NewTransform = CurrentTransform;
    NewTransform.SetRotation(BlendedRotation);

    Hierarchy->SetGlobalTransform(
        CachedItem.GetKey(),
        NewTransform,
        /*bInitial=*/false,
        bPropagateToChildren
    );
}
