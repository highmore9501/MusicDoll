#include "UI/BakeOperationsPanelBase.h"

#include "Baking/AnimationBaker.h"
#include "ExtensionLibraries/MovieSceneSequenceExtensions.h"
#include "InstrumentAnimationUtility.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SBakeOperationsPanelBase"

SBakeOperationsPanelBase::~SBakeOperationsPanelBase() {
    UE_LOG(LogTemp, Warning,
           TEXT("SBakeOperationsPanelBase::~SBakeOperationsPanelBase() - "
                "Destroying panel %p"),
           this);

    // 解绑事件监听
    if (BakeTaskManager.IsValid()) {
        if (TaskListChangedHandle.IsValid()) {
            BakeTaskManager->OnTaskListChanged.Remove(TaskListChangedHandle);
        }
        if (BakeStateChangedHandle.IsValid()) {
            BakeTaskManager->OnBakeStateChanged.Remove(BakeStateChangedHandle);
        }
    }

    // 清理ScanResults中的所有数据
    ScanResults.Empty();
}

void SBakeOperationsPanelBase::SetActor(AActor* InActor) {
    CurrentActor = InActor;

    // 切换Actor时清理之前的扫描结果
    ScanResults.Empty();
    bHasValidScanResults = false;

    // 获取 BakeTaskManager 单例
    if (!BakeTaskManager.IsValid()) {
        BakeTaskManager = GEngine->GetEngineSubsystem<UBakeTaskManager>();
        if (BakeTaskManager.IsValid()) {
            // 绑定事件监听
            TaskListChangedHandle =
                BakeTaskManager->OnTaskListChanged.AddLambda(
                    [this]() { OnTaskListChanged(); });
            BakeStateChangedHandle =
                BakeTaskManager->OnBakeStateChanged.AddLambda(
                    [this](bool bIsBaking) { OnBakeStateChanged(bIsBaking); });
        }
    }

    // 刷新当前Actor的任务显示
    RefreshMyTasks();
}

void SBakeOperationsPanelBase::Construct(const FArguments& InArgs) {
    // 初始化成员变量
    bHasValidScanResults = false;
    bIsBaking = false;
    CurrentStatusText = TEXT("Ready");

    // 获取 BakeTaskManager 单例
    BakeTaskManager = GEngine->GetEngineSubsystem<UBakeTaskManager>();
    if (BakeTaskManager.IsValid()) {
        // 绑定事件监听
        TaskListChangedHandle = BakeTaskManager->OnTaskListChanged.AddLambda(
            [this]() { OnTaskListChanged(); });
        BakeStateChangedHandle = BakeTaskManager->OnBakeStateChanged.AddLambda(
            [this](bool bIsBaking) { OnBakeStateChanged(bIsBaking); });
    }

    // 创建烘焙界面内容
    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f)[CreateControlSelectionSection()] +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f)[CreateActionButtonsSection()]];
}

// 移除了 CreateBakeSettingsSection 方法，因为设置现在在全局队列面板中统一管理

TSharedRef<SWidget> SBakeOperationsPanelBase::CreateControlSelectionSection() {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               2.0f)[SNew(STextBlock)
                         .Text(
                             FText::FromString(TEXT("Select Controls to Bake")))
                         .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))] +
           SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[CreateControlSelectionWidget()] +
           SVerticalBox::Slot().AutoHeight().Padding(5.0f)
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                    [SAssignNew(ScanButton, SButton)
                         .Text(FText::FromString(TEXT("Scan Control Rigs")))
                         .OnClicked(this, &SBakeOperationsPanelBase::
                                              HandleScanButtonClicked)] +
                SHorizontalBox::Slot().AutoWidth().Padding(
                    5.0f)[SNew(SButton)
                              .Text(FText::FromString(TEXT("Add Selected")))
                              .OnClicked(this, &SBakeOperationsPanelBase::
                                                   HandleAddSelectedClicked)]] +
           SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(TEXT("My Submitted Tasks:")))
                         .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))] +
           SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[SAssignNew(MyTasksListView,
                                SListView<TSharedPtr<FBakeTask>>)
                         .ListItemsSource(&MyTasksItems)
                         .OnGenerateRow(
                             this, &SBakeOperationsPanelBase::GenerateMyTaskRow)
                         .SelectionMode(ESelectionMode::None)];
}

TSharedRef<SWidget> SBakeOperationsPanelBase::CreateActionButtonsSection() {
    return SNew(SHorizontalBox) +

           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SAssignNew(ClearTracksButton, SButton)
                         .Text(FText::FromString(TEXT("Remove All My Tasks")))
                         .OnClicked(this,
                                    &SBakeOperationsPanelBase::
                                        HandleClearSelectedTracksButtonClicked)
                         .IsEnabled_Lambda([this]() {
                             return MyTasksItems.Num() > 0 && !bIsBaking;
                         })];
}

// 移除了 CreateStatusSection 方法，因为状态显示已移至全局队列面板

void SBakeOperationsPanelBase::OnClearSelectedTracksButtonClicked() {
    if (!CurrentActor.IsValid() || !BakeTaskManager.IsValid()) {
        return;
    }

    // 移除当前Actor的所有任务
    BakeTaskManager->RemoveTasksByOwner(CurrentActor.Get());
}

FReply SBakeOperationsPanelBase::HandleScanButtonClicked() {
    RefreshScanResults();
    return FReply::Handled();
}

FReply SBakeOperationsPanelBase::HandleClearSelectedTracksButtonClicked() {
    OnClearSelectedTracksButtonClicked();
    return FReply::Handled();
}

void SBakeOperationsPanelBase::HandleBakeProgress(int32 Current, int32 Total) {
    float Progress = Total > 0 ? (float)Current / Total : 0.0f;
}

void SBakeOperationsPanelBase::SetBakingState(bool bBaking) {
    bIsBaking = bBaking;

    // 更新按钮状态
    if (ScanButton.IsValid()) {
        ScanButton->SetEnabled(!bBaking);
    }

    if (ClearTracksButton.IsValid()) {
        ClearTracksButton->SetEnabled(!bBaking);
    }
}

void SBakeOperationsPanelBase::UpdateButtonStates() {
    // 更新按钮状态
    if (ClearTracksButton.IsValid()) {
        bool bCanClear = !bIsBaking && MyTasksItems.Num() > 0;
        ClearTracksButton->SetEnabled(bCanClear);
    }
}

bool SBakeOperationsPanelBase::AddControlToSelection(
    UControlRig* ControlRig, const FString& ControlName,
    const FString& DisplayName) {
    if (!ControlRig || ControlName.IsEmpty() || !CurrentActor.IsValid() ||
        !BakeTaskManager.IsValid()) {
        return false;
    }

    // 通过 BakeTaskManager 添加任务
    FGuid TaskId =
        BakeTaskManager->AddTask(CurrentActor.Get(), GetModuleName().ToString(),
                                 ControlRig, ControlName, DisplayName);

    return TaskId.IsValid();
}

void SBakeOperationsPanelBase::FinalizeAddSelectedControl(bool bAddedAny) {
    if (bAddedAny) {
        RefreshMyTasks();
        UpdateButtonStates();  // 更新按钮状态
    }
}

void SBakeOperationsPanelBase::AddSelectedControl() {};

FReply SBakeOperationsPanelBase::HandleAddSelectedClicked() {
    AddSelectedControl();
    return FReply::Handled();
}

TSharedRef<ITableRow> SBakeOperationsPanelBase::GenerateMyTaskRow(
    TSharedPtr<FBakeTask> Item, const TSharedRef<STableViewBase>& OwnerTable) {
    return SNew(STableRow<TSharedPtr<FBakeTask>>, OwnerTable)
        .Padding(
            2.0f)[SNew(SHorizontalBox) +
                  SHorizontalBox::Slot().FillWidth(
                      1.0f)[SNew(STextBlock)
                                .Text(FText::FromString(Item->DisplayName))] +
                  SHorizontalBox::Slot()
                      .AutoWidth()[SNew(SButton)
                                       .Text(FText::FromString(TEXT("×")))
                                       .OnClicked_Lambda([this, Item]() {
                                           if (BakeTaskManager.IsValid() &&
                                               Item.IsValid()) {
                                               BakeTaskManager->RemoveTask(
                                                   Item->TaskId);
                                           }
                                           return FReply::Handled();
                                       })]];
}

void SBakeOperationsPanelBase::RefreshMyTasks() {
    MyTasksItems.Empty();

    if (CurrentActor.IsValid() && BakeTaskManager.IsValid()) {
        TArray<FBakeTask> MyTasks =
            BakeTaskManager->GetTasksByOwner(CurrentActor.Get());
        for (const FBakeTask& Task : MyTasks) {
            MyTasksItems.Add(MakeShareable(new FBakeTask(Task)));
        }
    }

    // 刷新list view
    if (MyTasksListView.IsValid()) {
        MyTasksListView->RequestListRefresh();
    }

    UpdateButtonStates();
}

void SBakeOperationsPanelBase::OnTaskListChanged() { RefreshMyTasks(); }

void SBakeOperationsPanelBase::OnBakeStateChanged(bool InIsBaking) {
    SetBakingState(InIsBaking);
}

#undef LOCTEXT_NAMESPACE