#pragma once

#include "CoreMinimal.h"
#include "BoneControlPair.generated.h"

/**
 * 一个Bone与Control的配对结构，用于建立骨骼和控制杆之间的关联
 */
USTRUCT(BlueprintType, Blueprintable)
struct COMMON_API FBoneControlPair
{
	GENERATED_BODY()

	/** 骨骼名称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Control Pair")
	FName BoneName;

	/** Control名称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Control Pair")
	FName ControlName;

	FBoneControlPair()
		: BoneName(NAME_None)
		, ControlName(NAME_None)
	{
	}

	FBoneControlPair(const FName InBoneName, const FName InControlName)
		: BoneName(InBoneName)
		, ControlName(InControlName)
	{
	}

	bool operator==(const FBoneControlPair& Other) const
	{
		return BoneName == Other.BoneName && ControlName == Other.ControlName;
	}
};
