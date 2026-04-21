#include "Baking/BakeTaskManager.h"
#include "InstrumentAnimationUtility.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#define LOCTEXT_NAMESPACE "BakeTaskManager"

void UBakeTaskManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Initialized"));
	
	// 初始化默认烘焙设置
	SharedBakeSettings.StartFrame = 0;
	SharedBakeSettings.EndFrame = 100;
	SharedBakeSettings.FrameStep = 1;
	SharedBakeSettings.bOverwriteExistingKeys = true;
	SharedBakeSettings.BakePrecision = 0.001f;
	
	bIsBaking = false;
}

void UBakeTaskManager::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Deinitializing - Clearing all tasks"));
	ClearAllTasks();
	Super::Deinitialize();
}

FGuid UBakeTaskManager::AddTask(AActor* OwnerActor, const FString& ModuleName,
							   UControlRig* ControlRig, const FString& ControlName,
							   const FString& DisplayName, const FString& InstrumentInstanceId)
{
	if (!OwnerActor || !ControlRig || ControlName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BakeTaskManager] Invalid parameters for AddTask"));
		return FGuid();
	}

	// 检查是否重复(考虑乐器实例ID)
	if (IsTaskDuplicate(ControlRig, ControlName, InstrumentInstanceId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BakeTaskManager] Task already exists: %s.%s [Instance: %s]"), 
			   *ModuleName, *ControlName, *InstrumentInstanceId);
		return FGuid();
	}

	FBakeTask NewTask(OwnerActor, ModuleName, ControlRig, ControlName, DisplayName, InstrumentInstanceId);
	PendingTasks.Add(NewTask);
	
	UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Added task: %s [Instance: %s] (TaskId: %s)"), 
		   *DisplayName, *InstrumentInstanceId, *NewTask.TaskId.ToString());

	BroadcastTaskListChanged();
	return NewTask.TaskId;
}

void UBakeTaskManager::RemoveTask(const FGuid& TaskId)
{
	for (int32 i = PendingTasks.Num() - 1; i >= 0; --i)
	{
		if (PendingTasks[i].TaskId == TaskId)
		{
			FString DisplayName = PendingTasks[i].DisplayName;
			PendingTasks.RemoveAt(i);
			UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Removed task: %s"), *DisplayName);
			BroadcastTaskListChanged();
			return;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[BakeTaskManager] Task not found for removal: %s"), *TaskId.ToString());
}

void UBakeTaskManager::RemoveTasksByOwner(const AActor* OwnerActor)
{
	if (!OwnerActor)
		return;

	int32 RemovedCount = 0;
	for (int32 i = PendingTasks.Num() - 1; i >= 0; --i)
	{
		if (PendingTasks[i].OwnerActor.Get() == OwnerActor)
		{
			PendingTasks.RemoveAt(i);
			RemovedCount++;
		}
	}

	if (RemovedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Removed %d tasks for actor: %s"), 
			   RemovedCount, *OwnerActor->GetName());
		BroadcastTaskListChanged();
	}
}

const TArray<FBakeTask>& UBakeTaskManager::GetAllTasks() const
{
	return PendingTasks;
}

TArray<FBakeTask> UBakeTaskManager::GetTasksByOwner(const AActor* OwnerActor) const
{
	TArray<FBakeTask> Result;
	
	if (!OwnerActor)
		return Result;

	for (const FBakeTask& Task : PendingTasks)
	{
		if (Task.OwnerActor.Get() == OwnerActor)
		{
			Result.Add(Task);
		}
	}
	
	return Result;
}

bool UBakeTaskManager::HasTasks() const
{
	return PendingTasks.Num() > 0;
}

void UBakeTaskManager::ClearAllTasks()
{
	int32 TaskCount = PendingTasks.Num();
	PendingTasks.Empty();
	
	if (TaskCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Cleared all %d tasks"), TaskCount);
		BroadcastTaskListChanged();
	}
}

void UBakeTaskManager::PurgeInvalidTasks()
{
	int32 InitialCount = PendingTasks.Num();
	
	for (int32 i = PendingTasks.Num() - 1; i >= 0; --i)
	{
		if (!PendingTasks[i].IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Purged invalid task: %s"), 
				   *PendingTasks[i].DisplayName);
			PendingTasks.RemoveAt(i);
		}
	}
	
	int32 PurgedCount = InitialCount - PendingTasks.Num();
	if (PurgedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Purged %d invalid tasks"), PurgedCount);
		BroadcastTaskListChanged();
	}
}

FAnimationBakeSettings& UBakeTaskManager::GetSharedSettings()
{
	return SharedBakeSettings;
}

const FAnimationBakeSettings& UBakeTaskManager::GetSharedSettings() const
{
	return SharedBakeSettings;
}

int32 UBakeTaskManager::ExecuteAllTasks(TFunction<void(int32, int32, const FString&)> ProgressCallback)
{
	if (bIsBaking)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BakeTaskManager] Already baking, ignoring request"));
		return 0;
	}

	if (PendingTasks.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BakeTaskManager] No tasks to execute"));
		return 0;
	}

	// 清理无效任务
	PurgeInvalidTasks();
	
	if (PendingTasks.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BakeTaskManager] No valid tasks after purge"));
		return 0;
	}

	bIsBaking = true;
	OnBakeStateChanged.Broadcast(true);
	
	UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Starting execution of %d tasks"), PendingTasks.Num());

	// 按 ControlRig Instance 分组任务
	TMap<UControlRig*, TArray<FString>> ControlMap;
	int32 TotalControlCount = 0;

	for (const FBakeTask& Task : PendingTasks)
	{
		if (Task.ControlRigInstance.IsValid())
		{
			ControlMap.FindOrAdd(Task.ControlRigInstance.Get()).Add(Task.ControlName);
			TotalControlCount++;
		}
	}

	// 转换为所需的格式
	TArray<TPair<UControlRig*, TArray<FString>>> ControlGroups;
	for (const auto& Pair : ControlMap)
	{
		ControlGroups.Emplace(Pair.Key, Pair.Value);
	}

	// 执行烘焙
	int32 SuccessCount = UAnimationBaker::BakeMultipleControlGroups(
		UInstrumentAnimationUtility::GetCurrentLevelSequence(),
		ControlGroups,
		SharedBakeSettings,
		ProgressCallback);

	// 完成后重置状态并清空队列
	bIsBaking = false;
	ClearAllTasks();
	OnBakeStateChanged.Broadcast(false);

	UE_LOG(LogTemp, Log, TEXT("[BakeTaskManager] Execution completed: %d/%d controls successful"), 
		   SuccessCount, TotalControlCount);

	return SuccessCount;
}

bool UBakeTaskManager::IsBaking() const
{
	return bIsBaking;
}

void UBakeTaskManager::BroadcastTaskListChanged()
{
	OnTaskListChanged.Broadcast();
}

bool UBakeTaskManager::IsTaskDuplicate(UControlRig* ControlRig, const FString& ControlName, const FString& InstrumentInstanceId) const
{
	for (const FBakeTask& Task : PendingTasks)
	{
		// 如果提供了InstrumentInstanceId,则必须完全匹配(包括实例ID)
		if (!InstrumentInstanceId.IsEmpty())
		{
			if (Task.ControlRigInstance.Get() == ControlRig && 
				Task.ControlName == ControlName &&
				Task.InstrumentInstanceId == InstrumentInstanceId)
			{
				return true;
			}
		}
		else
		{
			// 如果没有提供InstrumentInstanceId,则只检查ControlRig和ControlName(向后兼容)
			if (Task.ControlRigInstance.Get() == ControlRig && Task.ControlName == ControlName)
			{
				return true;
			}
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE