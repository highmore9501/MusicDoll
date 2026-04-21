#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Subsystems/EngineSubsystem.h"
#include "Baking/AnimationBaker.h"
#include "BakeTaskManager.generated.h"

/**
 * 单个烘焙任务条目
 */
USTRUCT(BlueprintType)
struct FBakeTask
{
	GENERATED_BODY()

public:
	/** 唯一标识 */
	UPROPERTY(BlueprintReadOnly, Category = "Bake Task", Meta = (IgnoreForMemberInitializationTest))
	FGuid TaskId;

	/** 提交任务的 Actor（用于归属判断） */
	TWeakObjectPtr<AActor> OwnerActor;

	/** 模块名称，用于 UI 显示前缀（如 "StringFlow" / "KeyRipple"） */
	UPROPERTY(BlueprintReadOnly, Category = "Bake Task")
	FString OwnerModuleName;

	/** ControlRig 实例（弱引用，避免阻碍 GC） */
	TWeakObjectPtr<UControlRig> ControlRigInstance;

	/** Control 名称 */
	UPROPERTY(BlueprintReadOnly, Category = "Bake Task")
	FString ControlName;

	/** UI 显示名称(如 "StringFlow.Performer.controller_root") */
	UPROPERTY(BlueprintReadOnly, Category = "Bake Task")
	FString DisplayName;
	
	/** 乐器实例标识符(用于区分同一类型的不同乐器实例,如 Guitar_1, Guitar_2) */
	UPROPERTY(BlueprintReadOnly, Category = "Bake Task")
	FString InstrumentInstanceId;

	FBakeTask()
		: TaskId(FGuid::NewGuid())
	{}

	FBakeTask(AActor* InOwnerActor, const FString& InModuleName, 
			  UControlRig* InControlRig, const FString& InControlName, 
			  const FString& InDisplayName, const FString& InInstrumentInstanceId = TEXT(""))
		: TaskId(FGuid::NewGuid())
		, OwnerActor(InOwnerActor)
		, OwnerModuleName(InModuleName)
		, ControlRigInstance(InControlRig)
		, ControlName(InControlName)
		, DisplayName(InDisplayName)
		, InstrumentInstanceId(InInstrumentInstanceId)
	{}

	bool IsValid() const
	{
		return OwnerActor.IsValid() && ControlRigInstance.IsValid() && !ControlName.IsEmpty();
	}
};

/**
 * 全局烘焙任务管理器
 * 作为 Engine Subsystem 单例运行，管理跨 Actor 的烘焙任务队列
 */
UCLASS()
class MUSICDOLLCOMMON_API UBakeTaskManager : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	// ========== 生命周期 ==========
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ========== 任务管理 ==========

	/** 添加任务,返回任务 ID */
	FGuid AddTask(AActor* OwnerActor, const FString& ModuleName,
				  UControlRig* ControlRig, const FString& ControlName,
				  const FString& DisplayName, const FString& InstrumentInstanceId = TEXT(""));

	/** 移除指定任务 */
	void RemoveTask(const FGuid& TaskId);

	/** 移除指定 Actor 提交的所有任务 */
	void RemoveTasksByOwner(const AActor* OwnerActor);

	/** 获取所有待执行任务 */
	const TArray<FBakeTask>& GetAllTasks() const;

	/** 获取指定 Actor 提交的任务 */
	TArray<FBakeTask> GetTasksByOwner(const AActor* OwnerActor) const;

	/** 是否有待执行任务 */
	bool HasTasks() const;

	/** 清空所有任务 */
	void ClearAllTasks();

	/** 清理失效任务（OwnerActor 或 ControlRig 已被 GC 回收的） */
	void PurgeInvalidTasks();

	// ========== 烘焙设置 ==========

	/** 获取共享烘焙设置（可修改） */
	FAnimationBakeSettings& GetSharedSettings();

	/** 获取共享烘焙设置（只读） */
	const FAnimationBakeSettings& GetSharedSettings() const;

	// ========== 烘焙执行 ==========

	/** 执行所有待执行任务，完成后自动清空队列 */
	int32 ExecuteAllTasks(
		TFunction<void(int32, int32, const FString&)> ProgressCallback = nullptr);

	/** 是否正在烘焙 */
	bool IsBaking() const;

	// ========== 变更通知 ==========

	/** 任务列表变更时广播（UI 面板监听此事件刷新显示） */
	DECLARE_MULTICAST_DELEGATE(FOnTaskListChanged);
	FOnTaskListChanged OnTaskListChanged;

	/** 烘焙状态变更时广播 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBakeStateChanged, bool /* bIsBaking */);
	FOnBakeStateChanged OnBakeStateChanged;

private:
	/** 待执行任务队列 */
	TArray<FBakeTask> PendingTasks;

	/** 共享烘焙设置 */
	FAnimationBakeSettings SharedBakeSettings;

	/** 烘焙状态 */
	bool bIsBaking = false;

	/** 广播任务列表变更 */
	void BroadcastTaskListChanged();

	/** 检查任务是否已存在(防止重复添加) */
	bool IsTaskDuplicate(UControlRig* ControlRig, const FString& ControlName, const FString& InstrumentInstanceId = TEXT("")) const;
};