#pragma once

#include "ControlRig/Public/Units/Highlevel/Hierarchy/RigUnit_CCDIK.h"
#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "ArcDistributedIK.generated.h"

USTRUCT(meta = (DisplayName = "Arc Distributed IK With Pole Target",
                Category = "Hierarchy", Keywords = "N-Bone,IK,Pole,Arc",
                Version = "5.7"))
struct COMMON_API FRigUnit_ArcDistributedIK : public FRigUnit_CCDIKItemArray {
    GENERATED_BODY()

    UPROPERTY(meta = (Input))
    FVector PoleTarget;

    UPROPERTY(meta = (Input))
    FVector PrimaryAxis = FVector(1.0f, 0.0f, 0.0f);

    UPROPERTY(meta = (Input))
    FVector SecondAxis = FVector(0.0f, 1.0f, 0.0f);

    UPROPERTY(meta = (Input))
    bool bUseDebug = false;

    FRigUnit_ArcDistributedIK()
        : PoleTarget(FVector::ZeroVector),
          SecondAxis(FVector(0.0f, 1.0f, 0.0f)) {}

    RIGVM_METHOD()
    virtual void Execute() override;
};
