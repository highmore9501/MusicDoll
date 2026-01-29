#include "RigUnit_FABRIKWithPole.h"
#include "ControlRig/ControlRigLog.h"
#include "AnimationCore/Public/FABRIK.h"

void FRigUnit_FABRIKWithPole::Execute(const FRigUnitContext& Context)
{
	if (!ExecuteContext || !FKCCache.IsValid() || Chain.Num() == 0)
	{
		return;
	}

	// 将FRigUnit_FABRIK的数据转换为TArray<FFABRIKChainLink>格式
	TArray<FFABRIKChainLink> FABRIKChain;
	for (const FRigUnit_FABRIK_Constraint& Constraint : Chain)
	{
		FFABRIKChainLink Link;
		Link.BoneIndex = Constraint.Bone.Index;
		Link.Position = Constraint.Transform.GetLocation();
		
		// 设置长度为到下一个约束的距离
		if (&Constraint != &Chain.Last()) // 如果不是最后一个元素
		{
			const int32 NextIndex = &Constraint - &Chain[0] + 1;
			if (NextIndex < Chain.Num())
			{
				Link.Length = FVector::Distance(Constraint.Transform.GetLocation(), Chain[NextIndex].Transform.GetLocation());
			}
			else
			{
				Link.Length = 0.0f;
			}
		}
		else
		{
			Link.Length = 0.0f;
		}
		
		FABRIKChain.Add(Link);
	}

	// 计算最大可达距离
	double MaximumReach = 0.0;
	for (const FFABRIKChainLink& Link : FABRIKChain)
	{
		MaximumReach += Link.Length;
	}

	// 使用我们已有的PoleTarget FABRIK求解器
	bool bResult = AnimationCore::SolveFabrikWithPoleTarget(
		FABRIKChain, 
		Target.Transform.GetLocation(), 
		PoleTarget, 
		MaximumReach, 
		static_cast<double>(Precision), 
		MaxIterations
	);

	// 将结果写回原Chain数组
	if (bResult)
	{
		for (int32 i = 0; i < FABRIKChain.Num() && i < Chain.Num(); ++i)
		{
			FRigUnit_FABRIK_Constraint& Constraint = Chain[i];
			
			// 仅更新位置，保持旋转不变
			FTransform NewTransform = Constraint.Transform;
			NewTransform.SetLocation(FABRIKChain[i].Position);
			Constraint.Transform = NewTransform;
		}
	}
}