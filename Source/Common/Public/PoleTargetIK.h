#pragma once

#include "CoreMinimal.h"
#include "ControlRig/Public/Units/Highlevel/Hierarchy/RigUnit_CCDIK.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "PoleTargetIK.generated.h"

USTRUCT(meta = (DisplayName = "IK Solver With Pole Target",
                Category = "Hierarchy", Keywords = "N-Bone,IK,Pole",
                Version = "5.7"))
struct COMMON_API FRigUnit_IKWithPole : public FRigUnit_CCDIKItemArray {
    GENERATED_BODY()

    UPROPERTY(meta = (Input))
    FVector PoleTarget;

    UPROPERTY(meta = (Input))
    FVector PrimaryAxis = FVector(1.0f, 0.0f, 0.0f);  // 默认X轴

    UPROPERTY(meta = (Input))
    FVector SecondAxis = FVector(0.0f, 1.0f, 0.0f);  // 默认Y轴

    UPROPERTY(meta = (Input))
    bool bUseSecondaryAxisCorrection = true; // 新增布尔值，默认开启次轴修正

    FRigUnit_IKWithPole()
        : PoleTarget(FVector::ZeroVector), SecondAxis(FVector::UpVector), bUseSecondaryAxisCorrection(true) {}

    RIGVM_METHOD()
    virtual void Execute() override;
};