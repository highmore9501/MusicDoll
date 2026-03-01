
#include "UI/KeyRippleBakeOperationsPanel.h"

#include "ControlRigBlueprintLegacy.h"
#include "Widgets/Input/STextComboBox.h"

void SKeyRippleBakeOperationsPanel::Construct(const FArguments& InArgs) {
    // 初始化选项列表
    InitializeControlOptions();

    // 调用基类构造函数（不传递参数）
    SBakeOperationsPanelBase::Construct(SBakeOperationsPanelBase::FArguments());
}

void SKeyRippleBakeOperationsPanel::SetActor(AActor* InActor) {
    KeyRippleActor = Cast<AKeyRippleUnreal>(InActor);

    // 调用基类方法设置Actor
    SBakeOperationsPanelBase::SetActor(InActor);

    if (KeyRippleActor.IsValid()) {
        // 更新状态文本
        UpdateStatusText(FString::Printf(TEXT("KeyRipple actor set: %s"),
                                         *KeyRippleActor->GetName()));
    }
}

TSharedRef<SWidget>
SKeyRippleBakeOperationsPanel::CreateControlSelectionWidget() {
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
                             &SKeyRippleBakeOperationsPanel::
                                 HandlePerformerControlSelectionChanged)]] +
           SVerticalBox::Slot().AutoHeight().Padding(5.0f)
               [SNew(SHorizontalBox) +
                SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                    [SNew(STextBlock)
                         .Text(FText::FromString(TEXT("Piano Controls:")))] +
                SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                    5.0f)[SAssignNew(PianoControlCombo, STextComboBox)
                              .OptionsSource(&PianoControlOptions)
                              .OnSelectionChanged(
                                  this,
                                  &SKeyRippleBakeOperationsPanel::
                                      HandlePianoControlSelectionChanged)]];
}

TArray<FString> SKeyRippleBakeOperationsPanel::GetSelectedControlNames() const {
    TArray<FString> SelectedControlNames;

    if (SelectedPerformerControl.IsValid() &&
        !SelectedPerformerControl->IsEmpty()) {
        SelectedControlNames.Add(*SelectedPerformerControl);
    }

    if (SelectedPianoControl.IsValid() && !SelectedPianoControl->IsEmpty()) {
        SelectedControlNames.Add(*SelectedPianoControl);
    }

    return SelectedControlNames;
}

void SKeyRippleBakeOperationsPanel::InitializeControlOptions() {
    // 初始化默认选项
    PerformerControlOptions.Empty();
    PianoControlOptions.Empty();

    // 添加空选项
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    PianoControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // 添加一些常见的KeyRipple控制器作为示例
    TArray<FString> CommonPerformerControls = {
        TEXT("LeftHand_Control"), TEXT("RightHand_Control"),
        TEXT("Head_Control"), TEXT("Spine_Control")};

    TArray<FString> CommonPianoControls = {TEXT("PianoPedal_Control"),
                                           TEXT("Keyboard_Control"),
                                           TEXT("Lid_Control")};

    for (const FString& Control : CommonPerformerControls) {
        PerformerControlOptions.Add(MakeShareable(new FString(Control)));
    }

    for (const FString& Control : CommonPianoControls) {
        PianoControlOptions.Add(MakeShareable(new FString(Control)));
    }

    // 设置默认选择
    SelectedPerformerControl = PerformerControlOptions[0];
    SelectedPianoControl = PianoControlOptions[0];
}

void SKeyRippleBakeOperationsPanel::UpdateControlOptionsFromScan() {
    if (!KeyRippleActor.IsValid()) {
        return;
    }

    // 清空现有选项
    PerformerControlOptions.Empty();
    PianoControlOptions.Empty();

    // 添加空选项
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    PianoControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // 从扫描结果中提取Control列表
    for (const auto& Pair : ScanResults) {
        const FControlRigScanResult& Result = Pair.Value;

        if (!Result.IsValid()) {
            continue;
        }

        // 根据Actor类型分类Control
        if (Result.BoundActor == KeyRippleActor->SkeletalMeshActor) {
            // 演奏者Control
            for (const FString& ControlName : Result.AvailableControls) {
                PerformerControlOptions.Add(
                    MakeShareable(new FString(ControlName)));
            }
        } else if (Result.BoundActor == KeyRippleActor->Piano) {
            // 钢琴Control
            for (const FString& ControlName : Result.AvailableControls) {
                PianoControlOptions.Add(
                    MakeShareable(new FString(ControlName)));
            }
        }
    }

    // 刷新ComboBox - 添加调试日志
    UE_LOG(LogTemp, Log,
           TEXT("UpdateControlOptionsFromScan - Checking ComboBox validity: "
                "Performer=%s, Piano=%s"),
           PerformerControlCombo.IsValid() ? TEXT("Valid") : TEXT("Invalid"),
           PianoControlCombo.IsValid() ? TEXT("Valid") : TEXT("Invalid"));

    // 添加选项数组大小的日志
    UE_LOG(LogTemp, Log,
           TEXT("Control Options Count - Performer: %d, Piano: %d"),
           PerformerControlOptions.Num(), PianoControlOptions.Num());

    // 显示前几个选项作为验证
    if (PerformerControlOptions.Num() > 1) {
        UE_LOG(LogTemp, Log, TEXT("First few Performer options: %s, %s"),
               *(*PerformerControlOptions[0]),
               PerformerControlOptions.Num() > 1
                   ? *(*PerformerControlOptions[1])
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

    if (PianoControlCombo.IsValid()) {
        PianoControlCombo->RefreshOptions();
        UE_LOG(LogTemp, Log, TEXT("Successfully refreshed PianoControlCombo"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("PianoControlCombo is invalid - options may not display"));
    }

    UpdateStatusText(FString::Printf(
        TEXT("Updated control options from scan: Performer(%d), Piano(%d)"),
        PerformerControlOptions.Num() - 1, PianoControlOptions.Num() - 1));
}

void SKeyRippleBakeOperationsPanel::HandlePerformerControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
    if (NewSelection.IsValid()) {
        SelectedPerformerControl = NewSelection;
        UpdateStatusText(FString::Printf(TEXT("Selected performer control: %s"),
                                         **NewSelection));
    }
}

void SKeyRippleBakeOperationsPanel::HandlePianoControlSelectionChanged(
    TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo) {
    if (NewSelection.IsValid()) {
        SelectedPianoControl = NewSelection;
        UpdateStatusText(FString::Printf(TEXT("Selected piano control: %s"),
                                         **NewSelection));
    }
}

void SKeyRippleBakeOperationsPanel::RefreshScanResults() {
    UpdateStatusText(TEXT("Scanning KeyRipple Control Rigs..."));

    if (!KeyRippleActor.IsValid()) {
        UpdateStatusText(TEXT("No KeyRipple actor set"));
        return;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UpdateStatusText(TEXT("No Level Sequence is currently open"));
        return;
    }

    // 清空现有选项
    PerformerControlOptions.Empty();
    PianoControlOptions.Empty();

    // 添加空选项
    PerformerControlOptions.Add(MakeShareable(new FString(TEXT(""))));
    PianoControlOptions.Add(MakeShareable(new FString(TEXT(""))));

    // 扫描演奏者Control Rig
    if (KeyRippleActor->SkeletalMeshActor) {
        UControlRig* PerformerControlRig = nullptr;
        UControlRigBlueprint* PerformerBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                KeyRippleActor->SkeletalMeshActor, LevelSequence,
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

    // 扫描钢琴Control Rig
    if (KeyRippleActor->Piano) {
        UControlRig* PianoControlRig = nullptr;
        UControlRigBlueprint* PianoBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                KeyRippleActor->Piano, LevelSequence, PianoControlRig,
                PianoBlueprint) &&
            PianoControlRig && PianoBlueprint) {
            // 获取钢琴Control列表 - 只获取Transform类型的Control
            URigHierarchy* Hierarchy = PianoBlueprint->GetHierarchy();
            if (Hierarchy) {
                TArray<FRigControlElement*> ControlElements =
                    Hierarchy->GetControls(true);
                for (const FRigControlElement* Element : ControlElements) {
                    // 只添加Transform和EulerTransform类型的Control，排除Float等自定义Channel
                    if (Element->Settings.ControlType ==
                            ERigControlType::Transform ||
                        Element->Settings.ControlType ==
                            ERigControlType::EulerTransform) {
                        PianoControlOptions.Add(MakeShareable(
                            new FString(Element->GetDisplayName().ToString())));
                    }
                }
            }

            UE_LOG(LogTemp, Log, TEXT("Found %d piano controls"),
                   PianoControlOptions.Num() - 1);
        }
    }

    // 刷新ComboBox
    if (PerformerControlCombo.IsValid()) {
        PerformerControlCombo->RefreshOptions();
    }

    if (PianoControlCombo.IsValid()) {
        PianoControlCombo->RefreshOptions();
    }

    UpdateStatusText(FString::Printf(
        TEXT("Scan completed: Performer(%d), Piano(%d) controls found"),
        PerformerControlOptions.Num() - 1, PianoControlOptions.Num() - 1));

    // 扫描完成后设置有效的扫描结果标志
    bHasValidScanResults = true;

    // 更新按钮状态 - 基于选中的controls而不是扫描结果
    UpdateButtonStates();

    UpdateStatusText(
        TEXT("Scan completed. You can now select controls and bake."));
}

void SKeyRippleBakeOperationsPanel::AddSelectedControl() {
    if (!KeyRippleActor.IsValid()) {
        UpdateStatusText(TEXT("No KeyRipple actor set"));
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
        KeyRippleActor->SkeletalMeshActor) {
        UControlRig* PerformerControlRig = nullptr;
        UControlRigBlueprint* PerformerBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                KeyRippleActor->SkeletalMeshActor, LevelSequence,
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

    // 添加钢琴Control - 使用基类帮助方法
    if (SelectedPianoControl.IsValid() && !SelectedPianoControl->IsEmpty() &&
        KeyRippleActor->Piano) {
        UControlRig* PianoControlRig = nullptr;
        UControlRigBlueprint* PianoBlueprint = nullptr;

        if (UAnimationBaker::GetControlRigsForActor(
                KeyRippleActor->Piano, LevelSequence, PianoControlRig,
                PianoBlueprint) &&
            PianoControlRig) {
            FString ControlName = *SelectedPianoControl;
            FString DisplayName =
                FString::Printf(TEXT("Piano.%s"), *ControlName);

            if (AddControlToSelection(PianoControlRig, ControlName,
                                      DisplayName)) {
                bAddedAny = true;
            }
        }
    }

    // 使用基类方法完成添加操作
    FinalizeAddSelectedControl(bAddedAny);
}