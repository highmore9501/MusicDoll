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

    // 清理ScanResults中的所有数据
    ScanResults.Empty();

    // 不再需要手动清理BakeProcessor，作为TWeakObjectPtr会自动管理
}

void SBakeOperationsPanelBase::SetActor(AActor* InActor) {
    CurrentActor = InActor;

    // 切换Actor时清理之前的扫描结果
    ScanResults.Empty();
    bHasValidScanResults = false;

    // 不再需要初始化BakeProcessor，直接使用静态方法
    if (InActor) {
        UpdateStatusText(
            FString::Printf(TEXT("Actor set: %s"), *InActor->GetName()));
    } else {
        UpdateStatusText(TEXT("No actor set"));
    }
}

void SBakeOperationsPanelBase::Construct(const FArguments& InArgs) {
    // 初始化成员变量
    bHasValidScanResults = false;
    bIsBaking = false;
    CurrentStatusText = TEXT("Ready");

    // 初始化烘焙设置
    CurrentBakeSettings.StartFrame = 0;
    CurrentBakeSettings.EndFrame = 100;
    CurrentBakeSettings.FrameStep = 1;
    CurrentBakeSettings.bOverwriteExistingKeys = true;

    // 如果有活动的 Level Sequence，则使用它的播放范围作为默认烘焙范围
    ULevelSequence* ActiveSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (ActiveSequence) {
        FFrameNumber StartFrame =
            UMovieSceneSequenceExtensions::GetPlaybackStart(ActiveSequence);
        FFrameNumber EndFrame =
            UMovieSceneSequenceExtensions::GetPlaybackEnd(ActiveSequence);
        CurrentBakeSettings.StartFrame = StartFrame.Value;
        CurrentBakeSettings.EndFrame = EndFrame.Value;
    }

    // 创建烘焙界面内容
    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f)[CreateScanSection()] +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f)[CreateBakeSettingsSection()] +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f)[CreateControlSelectionSection()] +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f)[CreateActionButtonsSection()] +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f)[CreateStatusSection()]];
}

TSharedRef<SWidget> SBakeOperationsPanelBase::CreateScanSection() {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SAssignNew(ScanButton, SButton)
                         .Text(FText::FromString(TEXT("Scan Control Rigs")))
                         .OnClicked(this, &SBakeOperationsPanelBase::
                                              HandleScanButtonClicked)] +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               5.0f)[SAssignNew(ScanResultsContainer, SVerticalBox)];
}

TSharedRef<SWidget> SBakeOperationsPanelBase::CreateBakeSettingsSection() {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               2.0f)[SNew(STextBlock)
                         .Text(FText::FromString(TEXT("Bake Settings")))
                         .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))] +
           SVerticalBox::Slot().AutoHeight().Padding(2.0f)
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().AutoWidth().Padding(
                    5.0f)[SNew(STextBlock)
                              .Text(FText::FromString(TEXT("Start Frame:")))] +
                SHorizontalBox::Slot().FillWidth(0.3f).Padding(
                    5.0f)[SAssignNew(StartFrameTextBox, SEditableTextBox)
                              .Text_Lambda([this]() -> FText {
                                  return FText::FromString(FString::FromInt(
                                      CurrentBakeSettings.StartFrame));
                              })] +
                SHorizontalBox::Slot().AutoWidth().Padding(
                    5.0f)[SNew(STextBlock)
                              .Text(FText::FromString(TEXT("End Frame:")))] +
                SHorizontalBox::Slot().FillWidth(0.3f).Padding(
                    5.0f)[SAssignNew(EndFrameTextBox, SEditableTextBox)
                              .Text_Lambda([this]() -> FText {
                                  return FText::FromString(FString::FromInt(
                                      CurrentBakeSettings.EndFrame));
                              })] +
                SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("Step:")))] +
                SHorizontalBox::Slot().FillWidth(0.3f).Padding(
                    5.0f)[SAssignNew(FrameStepTextBox, SEditableTextBox)
                              .Text_Lambda([this]() -> FText {
                                  return FText::FromString(FString::FromInt(
                                      CurrentBakeSettings.FrameStep));
                              })]] +
           SVerticalBox::Slot().AutoHeight().Padding(
               2.0f)[SAssignNew(OverwriteCheckBox, SCheckBox)
                         .IsChecked(ECheckBoxState::Checked)
                             [SNew(STextBlock)
                                  .Text(FText::FromString(
                                      TEXT("Overwrite existing keyframes")))]];
}

TSharedRef<SWidget> SBakeOperationsPanelBase::CreateControlSelectionSection() {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               2.0f)[SNew(STextBlock)
                         .Text(
                             FText::FromString(TEXT("Select Controls to Bake")))
                         .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))] +
           SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[CreateControlSelectionWidget()] +
           SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[SNew(SHorizontalBox) +
                     SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                         [SNew(SButton)
                              .Text(FText::FromString(TEXT("Add Selected")))
                              .OnClicked(this, &SBakeOperationsPanelBase::
                                                   HandleAddSelectedClicked)]] +
           SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[SNew(STextBlock)
                         .Text(FText::FromString(TEXT("Selected Controls:")))
                         .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))] +
           SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[SAssignNew(SelectedControlsListView,
                                SListView<TSharedPtr<FSelectedControlEntry>>)
                         .ListItemsSource(&SelectedControlsItems)
                         .OnGenerateRow(this, &SBakeOperationsPanelBase::
                                                  GenerateSelectedControlRow)
                         .SelectionMode(ESelectionMode::None)];
}

TSharedRef<SWidget> SBakeOperationsPanelBase::CreateActionButtonsSection() {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SAssignNew(BakeButton, SButton)
                         .Text(FText::FromString(TEXT("Bake Animation")))
                         .OnClicked(
                             this,
                             &SBakeOperationsPanelBase::HandleBakeButtonClicked)
                         .IsEnabled_Lambda([this]() {
                             return SelectedControls.Num() > 0 && !bIsBaking;
                         })] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SAssignNew(CancelButton, SButton)
                         .Text(FText::FromString(TEXT("Cancel")))
                         .OnClicked(this, &SBakeOperationsPanelBase::
                                              HandleCancelButtonClicked)
                         .IsEnabled_Lambda([this]() { return bIsBaking; })] +
           SHorizontalBox::Slot().AutoWidth().Padding(
               5.0f)[SAssignNew(ClearTracksButton, SButton)
                         .Text(FText::FromString(
                             TEXT("Clear Selected Control Tracks")))
                         .OnClicked(this,
                                    &SBakeOperationsPanelBase::
                                        HandleClearSelectedTracksButtonClicked)
                         .IsEnabled_Lambda([this]() {
                             return SelectedControls.Num() > 0 && !bIsBaking;
                         })];
}

TSharedRef<SWidget> SBakeOperationsPanelBase::CreateStatusSection() {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               2.0f)[SNew(STextBlock)
                         .Text(FText::FromString(TEXT("Status")))
                         .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))] +
           SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[SAssignNew(StatusTextBlock, STextBlock)
                         .Text_Lambda([this]() {
                             return FText::FromString(CurrentStatusText);
                         })] +
           SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[SAssignNew(Progressbar, SProgressBar)
                         .Percent_Lambda([this]() {
                             return bIsBaking ? 0.5f : 0.0f;
                         })  // TODO: 实际进度
    ];
}

void SBakeOperationsPanelBase::OnBakeButtonClicked() {
    // 防止重复点击的核心防护
    if (bIsBaking) {
        return;
    }

    if (!ValidateBakeSettings()) {
        UpdateStatusText(TEXT("Invalid bake settings"));
        return;
    }

    TArray<FString> SelectedControlNames = GetSelectedControlNames();
    if (SelectedControlNames.Num() == 0) {
        UpdateStatusText(TEXT("No controls selected for baking"));
        return;
    }

    // 不再需要检查BakeProcessor，直接使用静态方法
    SetBakingState(true);

    // 准备多实例烘焙数据
    TMap<UControlRig*, TArray<FString>> ControlMap;
    int32 TotalControlCount = 0;

    // 按ControlRig Instance分组Control
    for (const FSelectedControlEntry& Entry : SelectedControls) {
        if (Entry.ControlRigInstance) {
            ControlMap.FindOrAdd(Entry.ControlRigInstance)
                .Add(Entry.ControlName);
            TotalControlCount++;
        }
    }

    // 转换为所需的格式
    TArray<TPair<UControlRig*, TArray<FString>>> ControlGroups;
    for (const auto& Pair : ControlMap) {
        ControlGroups.Emplace(Pair.Key, Pair.Value);
    }

    UpdateStatusText(
        FString::Printf(TEXT("Baking %d control(s) across %d instances..."),
                        TotalControlCount, ControlGroups.Num()));

    // 使用新的多实例烘焙处理器
    auto ProgressCallback = [this](int32 Current, int32 Total,
                                   const FString& ControlName) {
        HandleBakeProgress(Current, Total);
    };

    // 直接调用AnimationBaker的静态方法进行烘焙
    int32 SuccessCount = UAnimationBaker::BakeMultipleControlGroups(
        UInstrumentAnimationUtility::GetCurrentLevelSequence(), ControlGroups,
        CurrentBakeSettings, ProgressCallback);

    // 烘焙完成后重置状态
    SetBakingState(false);
    UpdateStatusText(
        FString::Printf(TEXT("Bake completed: %d/%d controls successful"),
                        SuccessCount, SelectedControlNames.Num()));
}

void SBakeOperationsPanelBase::OnCancelButtonClicked() {
    SetBakingState(false);
    UpdateStatusText(TEXT("Bake cancelled"));
}

void SBakeOperationsPanelBase::OnClearSelectedTracksButtonClicked() {
    if (SelectedControls.Num() == 0) {
        UpdateStatusText(TEXT("No controls selected for track clearing"));
        return;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UpdateStatusText(TEXT("No Level Sequence is currently open"));
        return;
    }

    int32 ClearedChannelCount = 0;
    int32 FailedTrackCount = 0;

    // 按ControlRig Instance分组处理
    TMap<UControlRig*, TSet<FString>> ControlGroups;
    for (const FSelectedControlEntry& Entry : SelectedControls) {
        if (Entry.ControlRigInstance) {
            ControlGroups.FindOrAdd(Entry.ControlRigInstance)
                .Add(Entry.ControlName);
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
            UE_LOG(LogTemp, Warning,
                   TEXT("[BakeOperationsPanel] ControlRig %s is not bound to "
                        "any track"),
                   *ControlRigInstance->GetName());
            FailedTrackCount++;
            continue;
        }

        // 获取轨道上的所有Section
        TArray<UMovieSceneSection*> AllSections = TargetTrack->GetAllSections();
        if (AllSections.Num() == 0) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[BakeOperationsPanel] ControlRig track for %s has no "
                        "sections"),
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
        TEXT("Cleared %d channels for selected controls, failed %d tracks"),
        ClearedChannelCount, FailedTrackCount));
}

FReply SBakeOperationsPanelBase::HandleScanButtonClicked() {
    RefreshScanResults();
    return FReply::Handled();
}

FReply SBakeOperationsPanelBase::HandleBakeButtonClicked() {
    OnBakeButtonClicked();
    return FReply::Handled();
}

FReply SBakeOperationsPanelBase::HandleCancelButtonClicked() {
    OnCancelButtonClicked();
    return FReply::Handled();
}

FReply SBakeOperationsPanelBase::HandleClearSelectedTracksButtonClicked() {
    OnClearSelectedTracksButtonClicked();
    return FReply::Handled();
}

void SBakeOperationsPanelBase::HandleBakeProgress(int32 Current, int32 Total) {
    float Progress = Total > 0 ? (float)Current / Total : 0.0f;
    UpdateStatusText(FString::Printf(TEXT("Baking progress: %d/%d (%.1f%%)"),
                                     Current, Total, Progress * 100.0f));
}

void SBakeOperationsPanelBase::UpdateStatusText(const FString& Status) {
    CurrentStatusText = Status;
    if (StatusTextBlock.IsValid()) {
        StatusTextBlock->SetText(FText::FromString(Status));
    }
}

void SBakeOperationsPanelBase::SetBakingState(bool bBaking) {
    bIsBaking = bBaking;

    // 更新按钮状态
    if (ScanButton.IsValid()) {
        ScanButton->SetEnabled(!bBaking);
    }

    if (BakeButton.IsValid()) {
        bool shouldBeEnabled = !bBaking && SelectedControls.Num() > 0;
        BakeButton->SetEnabled(shouldBeEnabled);
    }

    if (CancelButton.IsValid()) {
        CancelButton->SetEnabled(bBaking);
    }
}

void SBakeOperationsPanelBase::UpdateButtonStates() {
    // 只更新烘焙按钮状态，基于选中的controls数量
    if (BakeButton.IsValid()) {
        bool bCanBake = !bIsBaking && SelectedControls.Num() > 0;
        BakeButton->SetEnabled(bCanBake);
    }
}

bool SBakeOperationsPanelBase::ValidateBakeSettings() {
    CurrentBakeSettings.StartFrame = ParseIntFromTextBox(StartFrameTextBox, 0);
    CurrentBakeSettings.EndFrame = ParseIntFromTextBox(EndFrameTextBox, 100);
    CurrentBakeSettings.FrameStep = ParseIntFromTextBox(FrameStepTextBox, 1);
    CurrentBakeSettings.bOverwriteExistingKeys = OverwriteCheckBox->IsChecked();

    return CurrentBakeSettings.IsValid();
}

int32 SBakeOperationsPanelBase::ParseIntFromTextBox(
    TSharedPtr<SEditableTextBox> TextBox, int32 DefaultValue) const {
    if (!TextBox.IsValid()) {
        return DefaultValue;
    }

    FString Text = TextBox->GetText().ToString();
    int32 Value = DefaultValue;

    if (!Text.IsEmpty()) {
        Value = FCString::Atoi(*Text);
    }

    return Value;
}

void SBakeOperationsPanelBase::AddSelectedControl() {
    // 基类实现 - 子类需要重写具体的添加逻辑
    UpdateStatusText(
        TEXT("AddSelectedControl needs to be implemented in derived class"));
}

void SBakeOperationsPanelBase::RemoveSelectedControl(int32 Index) {
    if (Index >= 0 && Index < SelectedControls.Num()) {
        FString RemovedControl = SelectedControls[Index].DisplayName;
        SelectedControls.RemoveAt(Index);
        RefreshSelectedControlsList();
        UpdateButtonStates();  // 更新按钮状态
        UpdateStatusText(
            FString::Printf(TEXT("Removed control: %s. Total selected: %d"),
                            *RemovedControl, SelectedControls.Num()));
    }
}

bool SBakeOperationsPanelBase::AddControlToSelection(
    UControlRig* ControlRig, const FString& ControlName,
    const FString& DisplayName) {
    if (!ControlRig || ControlName.IsEmpty()) {
        return false;
    }

    // 检查是否已存在
    for (const FSelectedControlEntry& Entry : SelectedControls) {
        if (Entry.ControlRigInstance == ControlRig &&
            Entry.ControlName == ControlName) {
            return false;  // 已存在
        }
    }

    // 添加到选择列表
    SelectedControls.Emplace(ControlRig, ControlName, DisplayName);
    return true;
}

void SBakeOperationsPanelBase::FinalizeAddSelectedControl(bool bAddedAny) {
    if (bAddedAny) {
        RefreshSelectedControlsList();
        UpdateButtonStates();  // 更新按钮状态
        UpdateStatusText(
            FString::Printf(TEXT("Added controls. Total selected: %d"),
                            SelectedControls.Num()));
    } else {
        UpdateStatusText(
            TEXT("No new controls added (already selected or invalid)"));
    }
}

FReply SBakeOperationsPanelBase::HandleAddSelectedClicked() {
    AddSelectedControl();
    return FReply::Handled();
}

TSharedRef<ITableRow> SBakeOperationsPanelBase::GenerateSelectedControlRow(
    TSharedPtr<FSelectedControlEntry> Item,
    const TSharedRef<STableViewBase>& OwnerTable) {
    return SNew(STableRow<TSharedPtr<FSelectedControlEntry>>, OwnerTable)
        .Padding(
            2.0f)[SNew(SHorizontalBox) +
                  SHorizontalBox::Slot().FillWidth(
                      1.0f)[SNew(STextBlock)
                                .Text(FText::FromString(Item->DisplayName))] +
                  SHorizontalBox::Slot()
                      .AutoWidth()[SNew(SButton)
                                       .Text(FText::FromString(TEXT("-")))
                                       .OnClicked_Lambda([this, Item]() {
                                           // 找到并移除这个item
                                           for (int32 i = 0;
                                                i < SelectedControlsItems.Num();
                                                ++i) {
                                               if (SelectedControlsItems[i] ==
                                                   Item) {
                                                   RemoveSelectedControl(i);
                                                   break;
                                               }
                                           }
                                           return FReply::Handled();
                                       })]];
}

void SBakeOperationsPanelBase::RefreshSelectedControlsList() {
    // 清空现有items
    SelectedControlsItems.Empty();

    // 为每个选中的control创建shared ptr
    for (const FSelectedControlEntry& Entry : SelectedControls) {
        SelectedControlsItems.Add(
            MakeShareable(new FSelectedControlEntry(Entry)));
    }

    // 刷新list view
    if (SelectedControlsListView.IsValid()) {
        SelectedControlsListView->RequestListRefresh();
    }
}

#undef LOCTEXT_NAMESPACE