#pragma once

#include "Baking/AnimationBaker.h"
#include "Baking/BakeTaskManager.h"
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Control Rig烘焙操作面板基类
 * 提供通用的烘焙操作界面和功能
 */
class MUSICDOLLCOMMON_API SBakeOperationsPanelBase : public SCompoundWidget {
   public:
    SLATE_BEGIN_ARGS(SBakeOperationsPanelBase) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SBakeOperationsPanelBase();

    // 基础功能接口
    virtual void SetActor(AActor* InActor);
    virtual void RefreshScanResults() = 0;  // 纯虚函数，子类必须实现
    virtual void OnClearSelectedTracksButtonClicked();
    virtual bool IsBakingInProgress() const { return bIsBaking; }

    // Control选择管理
    virtual void AddSelectedControl();

    // 通用的帮助方法 - 子类可以复用
    bool AddControlToSelection(UControlRig* ControlRig,
                               const FString& ControlName,
                               const FString& DisplayName);
    void FinalizeAddSelectedControl(bool bAddedAny);

    // UI事件处理
    FReply HandleAddSelectedClicked();

   protected:
    // Actor引用
    TWeakObjectPtr<AActor> CurrentActor;

    // 扫描相关
    TMap<FString, FControlRigScanResult> ScanResults;
    bool bHasValidScanResults;

    // UI组件 - 当前Actor提交的任务显示
    TSharedPtr<SListView<TSharedPtr<FBakeTask>>> MyTasksListView;
    TArray<TSharedPtr<FBakeTask>> MyTasksItems;

    // 烘焙相关
    bool bIsBaking;
    TWeakObjectPtr<UBakeTaskManager> BakeTaskManager;
    FDelegateHandle TaskListChangedHandle;
    FDelegateHandle BakeStateChangedHandle;

    // UI控件
    TSharedPtr<SVerticalBox> ScanResultsContainer;

    TSharedPtr<SButton> ScanButton;
    TSharedPtr<SButton> ClearTracksButton;

    // 子类需要实现的纯虚函数
    virtual TSharedRef<SWidget> CreateControlSelectionWidget() = 0;
    virtual TArray<FString> GetSelectedControlNames() const = 0;

    // 通用UI创建方法
    TSharedRef<SWidget> CreateControlSelectionSection();
    TSharedRef<SWidget> CreateActionButtonsSection();

    // 事件处理
    FReply HandleScanButtonClicked();
    FReply HandleClearSelectedTracksButtonClicked();
    void HandleBakeProgress(int32 Current, int32 Total);

    // 任务管理相关
    void RefreshMyTasks();
    void OnTaskListChanged();
    void OnBakeStateChanged(bool bIsBaking);
    TSharedRef<ITableRow> GenerateMyTaskRow(
        TSharedPtr<FBakeTask> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    // 状态管理
    void SetBakingState(bool bBaking);
    void UpdateButtonStates();

    // 辅助方法

    // 模块标识方法
    virtual FName GetModuleName() const { return NAME_None; }

   private:
    FString CurrentStatusText;
};