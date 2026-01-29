#pragma once

#include "ControlRig/Public/Units/Highlevel/Hierarchy/RigUnit_CCDIK.h"
#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "PoleTargetFABRIK.generated.h"

USTRUCT(meta = (DisplayName = "FABRIK With Pole Target", Category = "Hierarchy",
                Keywords = "N-Bone,IK,Pole", Version = "5.7"))
struct COMMON_API FRigUnit_FABRIKWithPole : public FRigUnit_CCDIKItemArray {
    GENERATED_BODY()

    UPROPERTY(meta = (Input))
    FVector PoleTarget;

    UPROPERTY(meta = (Input))
    FVector SecondAxis;

    FRigUnit_FABRIKWithPole()
        : PoleTarget(FVector::ZeroVector), SecondAxis(FVector::UpVector) {}

    RIGVM_METHOD()
    virtual void Execute() override;
};