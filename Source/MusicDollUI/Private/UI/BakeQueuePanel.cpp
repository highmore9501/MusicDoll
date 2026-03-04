#include "UI/BakeQueuePanel.h"

#include "Baking/AnimationBaker.h"
#include "ControlRig.h"
#include "ExtensionLibraries/MovieSceneSequenceExtensions.h"
#include "InstrumentAnimationUtility.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SBakeQueuePanel"

void SBakeQueuePanel::Construct(const FArguments& InArgs) {
    // 获取 BakeTaskManager 单例
    BakeTaskManager = GEngine->GetEngineSubsystem<UBakeTaskManager>();
    if (BakeTaskManager.IsValid()) {
        // 绑定事件监听
        TaskListChangedHandle = BakeTaskManager->OnTaskListChanged.AddLambda(
            [this]() { RefreshTaskList(); });
        BakeStateChangedHandle = BakeTaskManager->OnBakeStateChanged.AddLambda(
            [this](bool bIsBaking) { UpdateBakeState(bIsBaking); });
    }

    // 创建界面
    ChildSlot
        [SNew(SBorder)
             .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
             .Padding(8.0f)
             .Visibility_Lambda([this]() -> EVisibility {
                 return GetPanelVisibility();
             })[SNew(SVerticalBox) +
                SVerticalBox::Slot().AutoHeight().Padding(
                    0, 0, 0,
                    8)[SNew(STextBlock)
                           .Text(LOCTEXT("BakeQueueTitle", "Bake Queue"))
                           .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))] +
                SVerticalBox::Slot().AutoHeight().Padding(
                    0, 0, 0, 8)[CreateSettingsSection()] +
                SVerticalBox::Slot().FillHeight(1.0f).Padding(
                    0, 0, 0, 8)[CreateTaskListSection()] +
                SVerticalBox::Slot().AutoHeight().Padding(
                    0, 0, 0, 8)[CreateActionSection()] +
                SVerticalBox::Slot().AutoHeight()[CreateStatusSection()]]];

    // 初始化显示
    RefreshTaskList();
    UpdateBakeState(false);
    UpdateStatusText(TEXT("Ready"));
}

SBakeQueuePanel::~SBakeQueuePanel() {
    // 解绑事件监听
    if (BakeTaskManager.IsValid()) {
        if (TaskListChangedHandle.IsValid()) {
            BakeTaskManager->OnTaskListChanged.Remove(TaskListChangedHandle);
        }
        if (BakeStateChangedHandle.IsValid()) {
            BakeTaskManager->OnBakeStateChanged.Remove(BakeStateChangedHandle);
        }
    }
}

TSharedRef<SWidget> SBakeQueuePanel::CreateSettingsSection() {
    // 初始化默认值
    int32 DefaultStartFrame = 0;
    int32 DefaultEndFrame = 100;

    // 如果有活动的 Level Sequence，则使用它的播放范围作为默认烘焙范围
    ULevelSequence* ActiveSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (ActiveSequence) {
        FFrameNumber StartFrame =
            UMovieSceneSequenceExtensions::GetPlaybackStart(ActiveSequence);
        FFrameNumber EndFrame =
            UMovieSceneSequenceExtensions::GetPlaybackEnd(ActiveSequence);
        DefaultStartFrame = StartFrame.Value;
        DefaultEndFrame = EndFrame.Value;

        // 更新 BakeTaskManager 的默认设置
        if (BakeTaskManager.IsValid()) {
            BakeTaskManager->GetSharedSettings().StartFrame = DefaultStartFrame;
            BakeTaskManager->GetSharedSettings().EndFrame = DefaultEndFrame;
        }
    }

    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               0, 0, 0,
               4)[SNew(STextBlock)
                      .Text(LOCTEXT("BakeSettings", "Bake Settings"))
                      .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))] +
           SVerticalBox::Slot().AutoHeight()
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().FillWidth(0.3f).Padding(0, 0, 4, 0)
                    [SNew(SHorizontalBox) +
                     SHorizontalBox::Slot()
                         .AutoWidth()
                         .VAlign(VAlign_Center)
                         .Padding(0, 0, 4, 0)[SNew(STextBlock)
                                                  .Text(LOCTEXT("StartFrame",
                                                                "Start:"))] +
                     SHorizontalBox::Slot().FillWidth(
                         1.0f)[SAssignNew(StartFrameBox, SEditableTextBox)
                                   .Text_Lambda([this,
                                                 DefaultStartFrame]() -> FText {
                                       if (BakeTaskManager.IsValid()) {
                                           return FText::FromString(
                                               FString::FromInt(
                                                   BakeTaskManager
                                                       ->GetSharedSettings()
                                                       .StartFrame));
                                       }
                                       return FText::FromString(
                                           FString::FromInt(DefaultStartFrame));
                                   })
                                   .OnTextCommitted_Lambda(
                                       [this](const FText& NewText,
                                              ETextCommit::Type CommitMethod) {
                                           if (BakeTaskManager.IsValid()) {
                                               int32 Value = FCString::Atoi(
                                                   *NewText.ToString());
                                               BakeTaskManager
                                                   ->GetSharedSettings()
                                                   .StartFrame = Value;
                                           }
                                       })]] +
                SHorizontalBox::Slot().FillWidth(0.3f).Padding(4, 0, 4, 0)
                    [SNew(SHorizontalBox) +
                     SHorizontalBox::Slot()
                         .AutoWidth()
                         .VAlign(VAlign_Center)
                         .Padding(0, 0, 4,
                                  0)[SNew(STextBlock)
                                         .Text(LOCTEXT("EndFrame", "End:"))] +
                     SHorizontalBox::Slot().FillWidth(1.0f)
                         [SAssignNew(EndFrameBox, SEditableTextBox)
                              .Text_Lambda([this, DefaultEndFrame]() -> FText {
                                  if (BakeTaskManager.IsValid()) {
                                      return FText::FromString(FString::FromInt(
                                          BakeTaskManager->GetSharedSettings()
                                              .EndFrame));
                                  }
                                  return FText::FromString(
                                      FString::FromInt(DefaultEndFrame));
                              })
                              .OnTextCommitted_Lambda(
                                  [this](const FText& NewText,
                                         ETextCommit::Type CommitMethod) {
                                      if (BakeTaskManager.IsValid()) {
                                          int32 Value = FCString::Atoi(
                                              *NewText.ToString());
                                          BakeTaskManager->GetSharedSettings()
                                              .EndFrame = Value;
                                      }
                                  })]] +
                SHorizontalBox::Slot().FillWidth(0.3f).Padding(4, 0, 0, 0)
                    [SNew(SHorizontalBox) +
                     SHorizontalBox::Slot()
                         .AutoWidth()
                         .VAlign(VAlign_Center)
                         .Padding(0, 0, 4,
                                  0)[SNew(STextBlock)
                                         .Text(LOCTEXT("FrameStep", "Step:"))] +
                     SHorizontalBox::Slot().FillWidth(
                         1.0f)[SAssignNew(FrameStepBox, SEditableTextBox)
                                   .Text_Lambda([this]() -> FText {
                                       if (BakeTaskManager.IsValid()) {
                                           return FText::FromString(
                                               FString::FromInt(
                                                   BakeTaskManager
                                                       ->GetSharedSettings()
                                                       .FrameStep));
                                       }
                                       return FText::FromString(TEXT("1"));
                                   })
                                   .OnTextCommitted_Lambda(
                                       [this](const FText& NewText,
                                              ETextCommit::Type CommitMethod) {
                                           if (BakeTaskManager.IsValid()) {
                                               int32 Value = FCString::Atoi(
                                                   *NewText.ToString());
                                               if (Value > 0) {
                                                   BakeTaskManager
                                                       ->GetSharedSettings()
                                                       .FrameStep = Value;
                                               }
                                           }
                                       })]]] +
           SVerticalBox::Slot().AutoHeight().Padding(
               0, 4, 0,
               0)[SAssignNew(OverwriteCheckBox, SCheckBox)
                      .IsChecked_Lambda([this]() -> ECheckBoxState {
                          if (BakeTaskManager.IsValid()) {
                              return BakeTaskManager->GetSharedSettings()
                                             .bOverwriteExistingKeys
                                         ? ECheckBoxState::Checked
                                         : ECheckBoxState::Unchecked;
                          }
                          return ECheckBoxState::Checked;
                      })
                      .OnCheckStateChanged_Lambda([this](
                                                      ECheckBoxState NewState) {
                          if (BakeTaskManager.IsValid()) {
                              BakeTaskManager->GetSharedSettings()
                                  .bOverwriteExistingKeys =
                                  (NewState == ECheckBoxState::Checked);
                          }
                      })[SNew(STextBlock)
                             .Text(LOCTEXT("OverwriteKeys",
                                           "Overwrite existing keyframes"))]];
}

TSharedRef<SWidget> SBakeQueuePanel::CreateTaskListSection() {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               0, 0, 0,
               4)[SNew(STextBlock)
                      .Text_Lambda([this]() -> FText {
                          if (BakeTaskManager.IsValid()) {
                              int32 TaskCount =
                                  BakeTaskManager->GetAllTasks().Num();
                              return FText::Format(LOCTEXT("TaskListHeader",
                                                           "{0} tasks pending"),
                                                   FText::AsNumber(TaskCount));
                          }
                          return LOCTEXT("TaskListHeaderZero",
                                         "0 tasks pending");
                      })
                      .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))] +
           SVerticalBox::Slot().FillHeight(1.0f)
               [SNew(SBorder)
                    .BorderImage(
                        FCoreStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
                    .Padding(
                        4.0f)[SAssignNew(TaskListView,
                                         SListView<TSharedPtr<FBakeTask>>)
                                  .ListItemsSource(&TaskListItems)
                                  .OnGenerateRow(
                                      this, &SBakeQueuePanel::GenerateTaskRow)
                                  .SelectionMode(ESelectionMode::None)]];
}

TSharedRef<SWidget> SBakeQueuePanel::CreateActionSection() {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               0, 0, 4,
               0)[SAssignNew(BakeAllButton, SButton)
                      .Text(LOCTEXT("BakeAll", "Bake All"))
                      .OnClicked(this, &SBakeQueuePanel::OnBakeAllClicked)
                      .IsEnabled_Lambda([this]() {
                          return BakeTaskManager.IsValid() &&
                                 BakeTaskManager->HasTasks() &&
                                 !BakeTaskManager->IsBaking();
                      })] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               4, 0, 4,
               0)[SAssignNew(ClearAllButton, SButton)
                      .Text(LOCTEXT("ClearAll", "Clear All"))
                      .OnClicked(this, &SBakeQueuePanel::OnClearAllClicked)
                      .IsEnabled_Lambda([this]() {
                          return BakeTaskManager.IsValid() &&
                                 BakeTaskManager->HasTasks() &&
                                 !BakeTaskManager->IsBaking();
                      })] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               4, 0, 0,
               0)[SAssignNew(ClearTracksButton, SButton)
                      .Text(LOCTEXT("ClearTracks", "Clear Tracks"))
                      .OnClicked(this, &SBakeQueuePanel::OnClearTracksClicked)
                      .IsEnabled_Lambda([this]() {
                          return BakeTaskManager.IsValid() &&
                                 BakeTaskManager->HasTasks() &&
                                 !BakeTaskManager->IsBaking();
                      })];
}

TSharedRef<SWidget> SBakeQueuePanel::CreateStatusSection() {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               0, 0, 0,
               4)[SNew(STextBlock)
                      .Text(LOCTEXT("Status", "Status"))
                      .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))] +
           SVerticalBox::Slot().AutoHeight().Padding(
               0, 0, 0, 4)[SAssignNew(StatusText, STextBlock)
                               .Text_Lambda([this]() -> FText {
                                   return FText::FromString(CurrentStatusText);
                               })] +
           SVerticalBox::Slot()
               .AutoHeight()[SAssignNew(ProgressBar, SProgressBar)
                                 .Percent_Lambda([this]() -> TOptional<float> {
                                     return CurrentProgress;
                                 })];
}

FReply SBakeQueuePanel::OnBakeAllClicked() {
    if (!BakeTaskManager.IsValid() || !BakeTaskManager->HasTasks()) {
        return FReply::Handled();
    }

    // 更新设置
    if (StartFrameBox.IsValid()) {
        BakeTaskManager->GetSharedSettings().StartFrame =
            FCString::Atoi(*StartFrameBox->GetText().ToString());
    }
    if (EndFrameBox.IsValid()) {
        BakeTaskManager->GetSharedSettings().EndFrame =
            FCString::Atoi(*EndFrameBox->GetText().ToString());
    }
    if (FrameStepBox.IsValid()) {
        int32 Step = FCString::Atoi(*FrameStepBox->GetText().ToString());
        if (Step > 0) {
            BakeTaskManager->GetSharedSettings().FrameStep = Step;
        }
    }
    if (OverwriteCheckBox.IsValid()) {
        BakeTaskManager->GetSharedSettings().bOverwriteExistingKeys =
            OverwriteCheckBox->IsChecked();
    }

    // 执行烘焙
    auto ProgressCallback = [this](int32 Current, int32 Total,
                                   const FString& ControlName) {
        float Progress = Total > 0 ? (float)Current / Total : 0.0f;
        CurrentProgress = Progress;
        UpdateStatusText(FString::Printf(TEXT("Baking: %s (%d/%d)"),
                                         *ControlName, Current, Total));
    };

    int32 SuccessCount = BakeTaskManager->ExecuteAllTasks(ProgressCallback);
    UpdateStatusText(FString::Printf(
        TEXT("Bake completed: %d controls successful"), SuccessCount));

    return FReply::Handled();
}

FReply SBakeQueuePanel::OnClearAllClicked() {
    if (BakeTaskManager.IsValid()) {
        BakeTaskManager->ClearAllTasks();
        UpdateStatusText(TEXT("All tasks cleared"));
    }
    return FReply::Handled();
}

FReply SBakeQueuePanel::OnClearTracksClicked() {
    if (!BakeTaskManager.IsValid()) {
        return FReply::Handled();
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UpdateStatusText(TEXT("No Level Sequence is currently open"));
        return FReply::Handled();
    }

    const TArray<FBakeTask>& AllTasks = BakeTaskManager->GetAllTasks();
    if (AllTasks.Num() == 0) {
        UpdateStatusText(TEXT("No tasks to clear tracks for"));
        return FReply::Handled();
    }

    int32 ClearedChannelCount = 0;
    int32 FailedTrackCount = 0;

    // 按ControlRig Instance分组处理
    TMap<UControlRig*, TSet<FString>> ControlGroups;
    for (const FBakeTask& Task : AllTasks) {
        if (Task.ControlRigInstance.IsValid()) {
            ControlGroups.FindOrAdd(Task.ControlRigInstance.Get())
                .Add(Task.ControlName);
        }
    }

    // 为每个ControlRig实例清理指定Control的通道
    for (const auto& GroupPair : ControlGroups) {
        UControlRig* ControlRigInstance = GroupPair.Key;
        const TSet<FString>& ControlNames = GroupPair.Value;

        // 查找对应的Control Rig轨道
        UMovieSceneControlRigParameterTrack* TargetTrack =
            FControlRigSequencerHelpers::FindControlRigTrack(
                LevelSequence, ControlRigInstance);

        if (!TargetTrack) {
            UE_LOG(
                LogTemp, Warning,
                TEXT(
                    "[BakeQueuePanel] ControlRig %s is not bound to any track"),
                *ControlRigInstance->GetName());
            FailedTrackCount++;
            continue;
        }

        // 获取轨道上的所有Section
        TArray<UMovieSceneSection*> AllSections = TargetTrack->GetAllSections();
        if (AllSections.Num() == 0) {
            UE_LOG(
                LogTemp, Warning,
                TEXT(
                    "[BakeQueuePanel] ControlRig track for %s has no sections"),
                *ControlRigInstance->GetName());
            continue;
        }

        // 为每个Section清除指定Control的通道
        for (UMovieSceneSection* Section : AllSections) {
            if (!Section) continue;

            for (const FString& ControlName : ControlNames) {
                FString Prefix = ControlName + TEXT(".");

                // 查找并重置位置通道
                FMovieSceneFloatChannel* LocationX =
                    UInstrumentAnimationUtility::FindFloatChannel(
                        Section,
                        *FString::Printf(TEXT("%sLocation.X"), *Prefix));
                FMovieSceneFloatChannel* LocationY =
                    UInstrumentAnimationUtility::FindFloatChannel(
                        Section,
                        *FString::Printf(TEXT("%sLocation.Y"), *Prefix));
                FMovieSceneFloatChannel* LocationZ =
                    UInstrumentAnimationUtility::FindFloatChannel(
                        Section,
                        *FString::Printf(TEXT("%sLocation.Z"), *Prefix));

                // 查找并重置旋转通道
                FMovieSceneFloatChannel* RotationX =
                    UInstrumentAnimationUtility::FindFloatChannel(
                        Section,
                        *FString::Printf(TEXT("%sRotation.X"), *Prefix));
                FMovieSceneFloatChannel* RotationY =
                    UInstrumentAnimationUtility::FindFloatChannel(
                        Section,
                        *FString::Printf(TEXT("%sRotation.Y"), *Prefix));
                FMovieSceneFloatChannel* RotationZ =
                    UInstrumentAnimationUtility::FindFloatChannel(
                        Section,
                        *FString::Printf(TEXT("%sRotation.Z"), *Prefix));

                // 重置找到的通道
                if (LocationX) {
                    LocationX->Reset();
                    ClearedChannelCount++;
                }
                if (LocationY) {
                    LocationY->Reset();
                    ClearedChannelCount++;
                }
                if (LocationZ) {
                    LocationZ->Reset();
                    ClearedChannelCount++;
                }
                if (RotationX) {
                    RotationX->Reset();
                    ClearedChannelCount++;
                }
                if (RotationY) {
                    RotationY->Reset();
                    ClearedChannelCount++;
                }
                if (RotationZ) {
                    RotationZ->Reset();
                    ClearedChannelCount++;
                }
            }
        }
    }

    // 标记LevelSequence为已修改
    LevelSequence->MarkPackageDirty();

    UpdateStatusText(FString::Printf(
        TEXT("Cleared %d channels for all tasks, failed %d tracks"),
        ClearedChannelCount, FailedTrackCount));

    return FReply::Handled();
}

void SBakeQueuePanel::OnRemoveTask(FGuid TaskId) {
    if (BakeTaskManager.IsValid()) {
        BakeTaskManager->RemoveTask(TaskId);
    }
}

void SBakeQueuePanel::RefreshTaskList() {
    TaskListItems.Empty();

    if (BakeTaskManager.IsValid()) {
        const TArray<FBakeTask>& AllTasks = BakeTaskManager->GetAllTasks();
        for (const FBakeTask& Task : AllTasks) {
            TaskListItems.Add(MakeShareable(new FBakeTask(Task)));
        }
    }

    if (TaskListView.IsValid()) {
        TaskListView->RequestListRefresh();
    }
}

void SBakeQueuePanel::UpdateBakeState(bool bIsBaking) {
    if (BakeAllButton.IsValid()) {
        BakeAllButton->SetEnabled(!bIsBaking);
    }
    if (ClearAllButton.IsValid()) {
        ClearAllButton->SetEnabled(!bIsBaking);
    }
    if (ClearTracksButton.IsValid()) {
        ClearTracksButton->SetEnabled(!bIsBaking);
    }

    CurrentProgress =
        bIsBaking ? TOptional<float>(0.5f) : TOptional<float>(0.0f);
}

void SBakeQueuePanel::UpdateStatusText(const FString& Text) {
    CurrentStatusText = Text;
    if (StatusText.IsValid()) {
        StatusText->SetText(FText::FromString(Text));
    }
}

TSharedRef<ITableRow> SBakeQueuePanel::GenerateTaskRow(
    TSharedPtr<FBakeTask> Item, const TSharedRef<STableViewBase>& OwnerTable) {
    return SNew(
        STableRow<TSharedPtr<FBakeTask>>,
        OwnerTable)[SNew(SHorizontalBox) +
                    SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                        4, 2)[SNew(STextBlock)
                                  .Text(FText::FromString(Item->DisplayName))] +
                    SHorizontalBox::Slot().AutoWidth().Padding(
                        4, 2)[SNew(SButton)
                                  .Text(FText::FromString(TEXT("×")))
                                  .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                                  .OnClicked_Lambda([this, Item]() {
                                      if (Item.IsValid()) {
                                          OnRemoveTask(Item->TaskId);
                                      }
                                      return FReply::Handled();
                                  })]];
}

EVisibility SBakeQueuePanel::GetPanelVisibility() const {
    if (BakeTaskManager.IsValid()) {
        return BakeTaskManager->HasTasks() ? EVisibility::Visible
                                           : EVisibility::Collapsed;
    }
    return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE