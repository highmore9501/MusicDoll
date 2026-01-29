#pragma once

#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "ControlRig/Public/Units/Highlevel/Hierarchy/RigUnit_FABRIK.h"
#include "AnimationCore/Public/FABRIK.h"
#include "PoleTargetFABRIK.h"
#include "RigUnit_FABRIKWithPole.generated.h"

USTRUCT(meta=(DisplayName="FABRIK With Pole Target", Category="IK"))
struct KEYRIPPLEUNREAL_API FRigUnit_FABRIKWithPole : public FRigUnit_FABRIK
{
	GENERATED_BODY()

	// 新增PoleTarget输入参数，蓝图可编辑
	UPROPERTY(EditAnywhere, Category="IK")
	FVector PoleTarget;

	UPROPERTY(EditAnywhere, Category="IK", meta=(PinShownByDefault))
	float Precision;

	UPROPERTY(EditAnywhere, Category="IK", meta=(PinShownByDefault))
	int32 MaxIterations;

	FRigUnit_FABRIKWithPole()
		: PoleTarget(FVector::ZeroVector)
		, Precision(0.01f)
		, MaxIterations(10)
	{}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};