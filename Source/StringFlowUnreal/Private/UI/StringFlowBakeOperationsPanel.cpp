
#include "UI/StringFlowBakeOperationsPanel.h"

#include "InstrumentAnimationUtility.h"
#include "Widgets/Input/STextComboBox.h"

void SStringFlowBakeOperationsPanel::Construct(const FArguments& InArgs) {
    // 初始化选项列表
    InitializeControlOptions();

    // 调用基类构造函数（不传递参数）
    SBakeOperationsPanelBase::Construct(SBakeOperationsPanelBase::FArguments());
}

void SStringFlowBakeOperationsPanel::SetActor(AActor* InActor) {
    StringFlowActor = Cast<AStringFlowUnreal>(InActor);

    // 调用基类方法设置Actor
    SBakeOperationsPanelBase::SetActor(InActor);

    if (StringFlowActor.IsValid()) {
        // 更新状态文本
        UpdateStatusText(FString::Printf(TEXT("StringFlow actor set: %s"),
                                         *StringFlowActor->GetName()));
    }
}

TSharedRef<SWidget>
SStringFlowBakeOperationsPanel::CreateControlSelectionWidget() {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(5.0f)
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().AutoWidth().Padding(
                    5.0f)[SNew(STextBlock)
                              .Text(FText::FromString(
                                  TEXT("Performer Controls:")))] +
                SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
                    [SAssignNew(PerformerControlCombo, STextComboBox)
                         .OptionsSource(&PerformerControlOptions)
                         .OnSelectionChanged(
                             this,
                             &SStringFlowBakeOperationsPanel::
                                 HandlePerformerControlSelectionChanged)]] +
           SVerticalBox::Slot().AutoHeight().Padding(5.0f)
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().AutoWidth().Padding(
                    5.0f)[SNew(STextBlock)
                              .Text(FText::FromString(
                                  TEXT("Instrument Controls:")))] +
                SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
                    [SAssignNew(InstrumentControlCombo, STextComboBox)
                         .OptionsSource(&InstrumentControlOptions)
                         .OnSelectionChanged(
                             this,
                             &SStringFlowBakeOperationsPanel::
                                 HandleInstrumentControlSelectionChanged)]] +
           SVerticalBox::Slot().AutoHeight().Padding(5.0f)
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().AutoWidth().Padding(
                    5.0f)[SNew(STextBlock)
                              .Text(FText::FromString(TEXT("Bow Controls:")))] +
                SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                    5.0f)[SAssignNew(BowControlCombo, STextComboBox)
                              .OptionsSource(&BowControlOptions)
                              .OnSelectionChanged(
                                  this, &SStringFlowBakeOperationsPanel::
                                            HandleBowControlSelectionChanged)]];
}

TArray<FString> SStringFlowBakeOperationsPanel::GetSelectedControlNames()
    const {
    TArray<FString> SelectedControlNames;

    if (SelectedPerformerControl.IsValid() &&
        !SelectedPerformerControl->IsEmpty()) {
        SelectedControlNames.Add(*SelectedPerformerControl);
    }

    if (SelectedInstrumentControl.IsValid() &&
        !SelectedInstrumentControl->IsEmpty()) {
        SelectedControlNames.Add(*SelectedInstrumentControl);
    }

    if (SelectedBowControl.IsValid() && !SelectedBowControl->IsEmpty()) {
        SelectedControlNames.Add(*SelectedBowControl);
    }

    return SelectedControlNames;
}

void SStringFlowBakeOperationsPanel::InitializeControlOptions() {
    // 初始化默认选项
    PerformerControlOptions.Empty();
    InstrumentControlOptions.Empty();
    BowControlOptions.Empty();

    // 添加空选项
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    InstrumentControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    BowControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // 添加一些常见的StringFlow控制器作为示例
    TArray<FString> CommonPerformerControls = {
        TEXT("LeftHand_Control"), TEXT("RightHand_Control"),
        TEXT("Head_Control"), TEXT("Spine_Control")};

    TArray<FString> CommonInstrumentControls = {TEXT("StringVibration_Control"),
                                                TEXT("Bridge_Control"),
                                                TEXT("Fingerboard_Control")};

    TArray<FString> CommonBowControls = {TEXT("BowTip_Control"),
                                         TEXT("BowFrog_Control"),
                                         TEXT("BowHair_Control")};

    for (const FString& Control : CommonPerformerControls) {
        PerformerControlOptions.Add(MakeShareable(new FString(Control)));
    }

    for (const FString& Control : CommonInstrumentControls) {
        InstrumentControlOptions.Add(MakeShareable(new FString(Control)));
    }

    for (const FString& Control : CommonBowControls) {
        BowControlOptions.Add(MakeShareable(new FString(Control)));
    }

    // 设置默认选择
    SelectedPerformerControl = PerformerControlOptions[0];
    SelectedInstrumentControl = InstrumentControlOptions[0];
    SelectedBowControl = BowControlOptions[0];
}

void SStringFlowBakeOperationsPanel::UpdateControlOptionsFromScan() {
    if (!StringFlowActor.IsValid()) {
        return;
    }

    // 清空现有选项
    PerformerControlOptions.Empty();
    InstrumentControlOptions.Empty();
    BowControlOptions.Empty();

    // 添加空选项
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    InstrumentControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    BowControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // 从扫描结果中提取Control列表
    for (const auto& Pair : ScanResults) {
        const FControlRigScanResult& Result = Pair.Value;

        if (!Result.IsValid()) {
            continue;
        }

        // 根据Actor类型分类Control
        if (Result.BoundActor == StringFlowActor->SkeletalMeshActor) {
            // 演奏者Control
            for (const FString& ControlName : Result.AvailableControls) {
                PerformerControlOptions.Add(
                    MakeShareable(new FString(ControlName)));
            }
        } else if (Result.BoundActor == StringFlowActor->StringInstrument) {
            // 乐器Control
            for (const FString& ControlName : Result.AvailableControls) {
                InstrumentControlOptions.Add(
                    MakeShareable(new FString(ControlName)));
            }
        } else if (Result.BoundActor == StringFlowActor->Bow) {
            // 琴弓Control
            for (const FString& ControlName : Result.AvailableControls) {
                BowControlOptions.Add(MakeShareable(new FString(ControlName)));
            }
        }
    }

    // 刷新ComboBox - 添加调试日志
    UE_LOG(LogTemp, Log,
           TEXT("UpdateControlOptionsFromScan - Checking ComboBox validity: "
                "Performer=%s, Instrument=%s, Bow=%s"),
           PerformerControlCombo.IsValid() ? TEXT("Valid") : TEXT("Invalid"),
           InstrumentControlCombo.IsValid() ? TEXT("Valid") : TEXT("Invalid"),
           BowControlCombo.IsValid() ? TEXT("Valid") : TEXT("Invalid"));

    // 添加选项数组大小的日志
    UE_LOG(
        LogTemp, Log,
        TEXT("Control Options Count - Performer: %d, Instrument: %d, Bow: %d"),
        PerformerControlOptions.Num(), InstrumentControlOptions.Num(),
        BowControlOptions.Num());

    // 显示前几个选项作为验证
    if (PerformerControlOptions.Num() > 1) {
        UE_LOG(
            LogTemp, Log, TEXT("First few Performer options: %s, %s, %s"),
            *(*PerformerControlOptions[0]),
            PerformerControlOptions.Num() > 1 ? *(*PerformerControlOptions[1])
                                              : TEXT("N/A"),
            PerformerControlOptions.Num() > 2 ? *(*PerformerControlOptions[2])
                                              : TEXT("N/A"));
    }

    if (PerformerControlCombo.IsValid()) {
        PerformerControlCombo->RefreshOptions();
        UE_LOG(LogTemp, Log,
               TEXT("Successfully refreshed PerformerControlCombo"));
    } else {
        UE_LOG(
            LogTemp, Warning,
            TEXT("PerformerControlCombo is invalid - options may not display"));
    }

    if (InstrumentControlCombo.IsValid()) {
        InstrumentControlCombo->RefreshOptions();
        UE_LOG(LogTemp, Log,
               TEXT("Successfully refreshed InstrumentControlCombo"));
    } else {
        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "InstrumentControlCombo is invalid - options may not display"));
    }

    if (BowControlCombo.IsValid()) {
        BowControlCombo->RefreshOptions();
        UE_LOG(LogTemp, Log, TEXT("Successfully refreshed BowControlCombo"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("BowControlCombo is invalid - options may not display"));
    }

    UpdateStatusText(FString::Printf(
        TEXT("Updated control options: Performer(%d), Instrument(%d), Bow(%d)"),
        PerformerControlOptions.Num() - 1, InstrumentControlOptions.Num() - 1,
        BowControlOptions.Num() - 1));
}

void SStringFlowBakeOperationsPanel::HandlePerformerControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
    if (NewSelection.IsValid()) {
        SelectedPerformerControl = NewSelection;
        UpdateStatusText(FString::Printf(TEXT("Selected performer control: %s"),
                                         **NewSelection));
    }
}

void SStringFlowBakeOperationsPanel::HandleInstrumentControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
    if (NewSelection.IsValid()) {
        SelectedInstrumentControl = NewSelection;
        UpdateStatusText(FString::Printf(
            TEXT("Selected instrument control: %s"), **NewSelection));
    }
}

void SStringFlowBakeOperationsPanel::HandleBowControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
    if (NewSelection.IsValid()) {
        SelectedBowControl = NewSelection;
        UpdateStatusText(
            FString::Printf(TEXT("Selected bow control: %s"), **NewSelection));
    }
}

void SStringFlowBakeOperationsPanel::RefreshScanResults() {
    UpdateStatusText(TEXT("Scanning StringFlow Control Rigs..."));

    if (!StringFlowActor.IsValid()) {
        UpdateStatusText(TEXT("No StringFlow actor set"));
        return;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UpdateStatusText(TEXT("No Level Sequence is currently open"));
        return;
    }

    // 不再需要初始化BakeProcessor，直接使用静态方法

    // 清空现有选项
    PerformerControlOptions.Empty();
    InstrumentControlOptions.Empty();
    BowControlOptions.Empty();

    // 添加空选项
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    InstrumentControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    BowControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // 扫描演奏者Control Rig
    if (StringFlowActor->SkeletalMeshActor) {
        UControlRig* PerformerControlRig = nullptr;
        UControlRigBlueprint* PerformerBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                StringFlowActor->SkeletalMeshActor, LevelSequence,
                PerformerControlRig, PerformerBlueprint) &&
            PerformerControlRig && PerformerBlueprint) {
            // 获取演奏者Control列表 - 只获取Transform类型的Control
            URigHierarchy* Hierarchy = PerformerBlueprint->GetHierarchy();
            if (Hierarchy) {
                TArray<FRigControlElement*> ControlElements =
                    Hierarchy->GetControls(true);
                for (const FRigControlElement* Element : ControlElements) {
                    // 只添加Transform和EulerTransform类型的Control，排除Float等自定义Channel
                    if (Element->Settings.ControlType ==
                            ERigControlType::Transform ||
                        Element->Settings.ControlType ==
                            ERigControlType::EulerTransform) {
                        PerformerControlOptions.Add(MakeShareable(
                            new FString(Element->GetDisplayName().ToString())));
                    }
                }
            }

            UE_LOG(LogTemp, Log, TEXT("Found %d performer controls"),
                   PerformerControlOptions.Num() - 1);
        }
    }

    // 扫描乐器Control Rig
    if (StringFlowActor->StringInstrument) {
        UControlRig* InstrumentControlRig = nullptr;
        UControlRigBlueprint* InstrumentBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                StringFlowActor->StringInstrument, LevelSequence,
                InstrumentControlRig, InstrumentBlueprint) &&
            InstrumentControlRig && InstrumentBlueprint) {
            // 获取乐器Control列表 - 只获取Transform类型的Control
            URigHierarchy* Hierarchy = InstrumentBlueprint->GetHierarchy();
            if (Hierarchy) {
                TArray<FRigControlElement*> ControlElements =
                    Hierarchy->GetControls(true);
                for (const FRigControlElement* Element : ControlElements) {
                    // 只添加Transform和EulerTransform类型的Control，排除Float等自定义Channel
                    if (Element->Settings.ControlType ==
                            ERigControlType::Transform ||
                        Element->Settings.ControlType ==
                            ERigControlType::EulerTransform) {
                        InstrumentControlOptions.Add(MakeShareable(
                            new FString(Element->GetDisplayName().ToString())));
                    }
                }
            }

            UE_LOG(LogTemp, Log, TEXT("Found %d instrument controls"),
                   InstrumentControlOptions.Num() - 1);
        }
    }

    // 扫描琴弓Control Rig
    if (StringFlowActor->Bow) {
        UControlRig* BowControlRig = nullptr;
        UControlRigBlueprint* BowBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                StringFlowActor->Bow, LevelSequence, BowControlRig,
                BowBlueprint) &&
            BowControlRig && BowBlueprint) {
            // 获取琴弓Control列表 - 只获取Transform类型的Control
            URigHierarchy* Hierarchy = BowBlueprint->GetHierarchy();
            if (Hierarchy) {
                TArray<FRigControlElement*> ControlElements =
                    Hierarchy->GetControls(true);
                for (const FRigControlElement* Element : ControlElements) {
                    // 只添加Transform和EulerTransform类型的Control，排除Float等自定义Channel
                    if (Element->Settings.ControlType ==
                            ERigControlType::Transform ||
                        Element->Settings.ControlType ==
                            ERigControlType::EulerTransform) {
                        BowControlOptions.Add(MakeShareable(
                            new FString(Element->GetDisplayName().ToString())));
                    }
                }
            }

            UE_LOG(LogTemp, Log, TEXT("Found %d bow controls"),
                   BowControlOptions.Num() - 1);
        }
    }

    // 刷新ComboBox
    if (PerformerControlCombo.IsValid()) {
        PerformerControlCombo->RefreshOptions();
    }

    if (InstrumentControlCombo.IsValid()) {
        InstrumentControlCombo->RefreshOptions();
    }

    if (BowControlCombo.IsValid()) {
        BowControlCombo->RefreshOptions();
    }

    UpdateStatusText(FString::Printf(
        TEXT("Scan completed: Performer(%d), Instrument(%d), Bow(%d) controls "
             "found"),
        PerformerControlOptions.Num() - 1, InstrumentControlOptions.Num() - 1,
        BowControlOptions.Num() - 1));

    // 扫描完成后设置有效的扫描结果标志
    bHasValidScanResults = true;

    UpdateStatusText(
        TEXT("Scan completed. You can now select controls and bake."));
}

void SStringFlowBakeOperationsPanel::AddSelectedControl() {
    if (!StringFlowActor.IsValid()) {
        UpdateStatusText(TEXT("No StringFlow actor set"));
        return;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UpdateStatusText(TEXT("No Level Sequence is currently open"));
        return;
    }

    bool bAddedAny = false;

    // 添加演奏者Control - 使用基类帮助方法
    if (SelectedPerformerControl.IsValid() &&
        !SelectedPerformerControl->IsEmpty() &&
        StringFlowActor->SkeletalMeshActor) {
        UControlRig* PerformerControlRig = nullptr;
        UControlRigBlueprint* PerformerBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                StringFlowActor->SkeletalMeshActor, LevelSequence,
                PerformerControlRig, PerformerBlueprint) &&
            PerformerControlRig) {
            FString ControlName = *SelectedPerformerControl;
            FString DisplayName =
                FString::Printf(TEXT("Performer.%s"), *ControlName);

            if (AddControlToSelection(PerformerControlRig, ControlName,
                                      DisplayName)) {
                bAddedAny = true;
            }
        }
    }

    // 添加乐器Control - 使用基类帮助方法
    if (SelectedInstrumentControl.IsValid() &&
        !SelectedInstrumentControl->IsEmpty() &&
        StringFlowActor->StringInstrument) {
        UControlRig* InstrumentControlRig = nullptr;
        UControlRigBlueprint* InstrumentBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                StringFlowActor->StringInstrument, LevelSequence,
                InstrumentControlRig, InstrumentBlueprint) &&
            InstrumentControlRig) {
            FString ControlName = *SelectedInstrumentControl;
            FString DisplayName =
                FString::Printf(TEXT("Instrument.%s"), *ControlName);

            if (AddControlToSelection(InstrumentControlRig, ControlName,
                                      DisplayName)) {
                bAddedAny = true;
            }
        }
    }

    // 添加琴弓Control - 使用基类帮助方法
    if (SelectedBowControl.IsValid() && !SelectedBowControl->IsEmpty() &&
        StringFlowActor->Bow) {
        UControlRig* BowControlRig = nullptr;
        UControlRigBlueprint* BowBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                StringFlowActor->Bow, LevelSequence, BowControlRig,
                BowBlueprint) &&
            BowControlRig) {
            FString ControlName = *SelectedBowControl;
            FString DisplayName = FString::Printf(TEXT("Bow.%s"), *ControlName);

            if (AddControlToSelection(BowControlRig, ControlName,
                                      DisplayName)) {
                bAddedAny = true;
            }
        }
    }

    // 使用基类方法完成添加操作
    FinalizeAddSelectedControl(bAddedAny);
}