#pragma once

#include "CoreMinimal.h"
#include "BoneControlPair.h"
#include "Rigs/RigHierarchy.h"

class AInstrumentBase;

/**
 * Bone-Control映射工具类
 * 提供为ControlRigBlueprint添加和管理BoneControlMapping变量的功能
 */
class COMMON_API FBoneControlMappingUtility
{
public:
	/**
	 * 为ControlRigBlueprint添加BoneControlMapping变量
	 * @param ControlRigBlueprint 目标ControlRigBlueprint
	 * @param InstrumentActor 乐器Actor，用于获取ControlRig信息
	 * @return 是否成功添加
	 */
	static bool AddBoneControlMappingVariable(
		class UControlRigBlueprint* ControlRigBlueprint,
		AInstrumentBase* InstrumentActor);

	/**
	 * 获取指定ControlRigBlueprint的BoneControlMapping变量值
	 * @param ControlRigBlueprint 目标ControlRigBlueprint
	 * @param OutMapping 输出的映射数组
	 * @return 是否成功获取
	 */
	static bool GetBoneControlMapping(
		class UControlRigBlueprint* ControlRigBlueprint,
		TArray<FBoneControlPair>& OutMapping);

	/**
	 * 设置指定ControlRigBlueprint的BoneControlMapping变量值
	 * @param ControlRigBlueprint 目标ControlRigBlueprint
	 * @param InMapping 要设置的映射数组
	 * @return 是否成功设置
	 */
	static bool SetBoneControlMapping(
		class UControlRigBlueprint* ControlRigBlueprint,
		const TArray<FBoneControlPair>& InMapping);

	/**
	 * 从ControlRigBlueprint的Hierarchy中获取所有Bone名称
	 * @param ControlRigBlueprint 目标ControlRigBlueprint
	 * @param OutBoneNames 输出的Bone名称数组
	 * @return 是否成功获取
	 */
	static bool GetAllBoneNamesFromHierarchy(
		class UControlRigBlueprint* ControlRigBlueprint,
		TArray<FString>& OutBoneNames);

	/**
	 * 从ControlRigBlueprint的Hierarchy中获取所有Control名称
	 * @param ControlRigBlueprint 目标ControlRigBlueprint
	 * @param OutControlNames 输出的Control名称数组
	 * @return 是否成功获取
	 */
	static bool GetAllControlNamesFromHierarchy(
		class UControlRigBlueprint* ControlRigBlueprint,
		TArray<FString>& OutControlNames);

	/**
	 * 同步Bone与Control的位置变换
	 * 将Control的initTransform设置为与其对应Bone相同的世界变换,类似"zero offset with nearest bone"的效果
	 * 但offset设置为0,所有数据写入initTransform
	 * @param ControlRigBlueprint 目标ControlRigBlueprint
	 * @param InstrumentActor 乐器Actor,用于获取骨骼信息
	 * @param OutSyncedCount 输出成功同步的数量
	 * @param OutFailedCount 输出失败的数量
	 * @return 是否成功执行同步
	 */
	static bool SyncBoneControlPairs(
		class UControlRigBlueprint* ControlRigBlueprint,
		AInstrumentBase* InstrumentActor,
		int32& OutSyncedCount,
		int32& OutFailedCount);
};
