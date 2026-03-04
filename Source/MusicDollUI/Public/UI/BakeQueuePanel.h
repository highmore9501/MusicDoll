#pragma once

#include "CoreMinimal.h"
#include "Baking/BakeTaskManager.h"
#include "Widgets/SCompoundWidget.h"
#include "LevelSequence.h"
#include "ExtensionLibraries/MovieSceneSequenceExtensions.h"

/**
 * 全局烘焙队列面板
 * 显示所有待执行的烘焙任务，提供统一的烘焙控制界面
 */
class MUSICDOLLUI_API SBakeQueuePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBakeQueuePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SBakeQueuePanel();

private:
	// UI 创建
	TSharedRef<SWidget> CreateSettingsSection();
	TSharedRef<SWidget> CreateTaskListSection();
	TSharedRef<SWidget> CreateActionSection();
	TSharedRef<SWidget> CreateStatusSection();

	// 事件处理
	FReply OnBakeAllClicked();
	FReply OnClearAllClicked();
	FReply OnClearTracksClicked();
	void OnRemoveTask(FGuid TaskId);

	// 刷新
	void RefreshTaskList();
	void UpdateBakeState(bool bIsBaking);
	void UpdateStatusText(const FString& Text);

	// UI 组件
	TSharedPtr<SListView<TSharedPtr<FBakeTask>>> TaskListView;
	TArray<TSharedPtr<FBakeTask>> TaskListItems;
	
	TSharedPtr<SEditableTextBox> StartFrameBox;
	TSharedPtr<SEditableTextBox> EndFrameBox;
	TSharedPtr<SEditableTextBox> FrameStepBox;
	TSharedPtr<SCheckBox> OverwriteCheckBox;
	
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<class SProgressBar> ProgressBar;
	TSharedPtr<SButton> BakeAllButton;
	TSharedPtr<SButton> ClearAllButton;
	TSharedPtr<SButton> ClearTracksButton;

	// 管理器引用
	TWeakObjectPtr<UBakeTaskManager> BakeTaskManager;
	
	// 代理句柄
	FDelegateHandle TaskListChangedHandle;
	FDelegateHandle BakeStateChangedHandle;
	
	// 状态数据
	FString CurrentStatusText;
	TOptional<float> CurrentProgress;
	
	// 行生成方法
	TSharedRef<ITableRow> GenerateTaskRow(TSharedPtr<FBakeTask> Item, const TSharedRef<STableViewBase>& OwnerTable);
	
	// 可见性控制
	EVisibility GetPanelVisibility() const;
};