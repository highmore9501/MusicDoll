#include "ZhengDriftControlRigProcessor.h"

#include "ControlRig.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Rigs/RigHierarchyController.h"

#define LOCTEXT_NAMESPACE "ZhengDriftControlRigProcessor"

// ============================================================
// 1. SetupControllers
// ============================================================

int32 UZhengDriftControlRigProcessor::SetupControllers(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ValidateZhengDriftActor(ZhengDriftActor, TEXT("SetupControllers"))) {
        return 0;
    }

    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupControllers [ZhengDrift]: GEngine is null"));
        return 0;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupControllers [ZhengDrift]: CacheSubsystem not found"));
        return 0;
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("SetupControllers [ZhengDrift]: No LevelSequence found"));
        return 0;
    }

    UControlRigBlueprint* Blueprint = CacheSubsystem->GetControlRigBlueprint(
        ZhengDriftActor->SkeletalMeshActor, LevelSequence);
    if (!Blueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupControllers [ZhengDrift]: Failed to get "
                    "ControlRigBlueprint"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== ZhengDrift SetupControllers Started =========="));

    int32 CreatedCount = 0;

    // 1. 根节点
    if (CreateController(Blueprint, TEXT("base_root"))) CreatedCount++;
    if (CreateController(Blueprint, TEXT("controller_root"), TEXT("base_root")))
        CreatedCount++;

    // 2. 左手控制器（主控制器直接挂在 controller_root 下）
    const TArray<FString> LeftMain = {
        TEXT("H_L"),
        TEXT("HP_L"),
    };
    for (const FString& Name : LeftMain) {
        if (CreateController(Blueprint, Name, TEXT("controller_root")))
            CreatedCount++;
    }

    // 手指挂在对应手掌（H_L）下
    const TArray<FString> LeftFingers = {TEXT("T_L"), TEXT("I_L"), TEXT("M_L"),
                                         TEXT("R_L"), TEXT("P_L")};
    for (const FString& Name : LeftFingers) {
        if (CreateController(Blueprint, Name, TEXT("H_L"))) CreatedCount++;
    }

    // 辅助控件（ext_）— 每个手指一个，与手指同级（都挂 H_L）
    for (const FString& Name : LeftFingers) {
        if (FControlRigCreationUtility::EnsureControl(
                Blueprint, FString::Printf(TEXT("ext_%s"), *Name), TEXT("H_L")))
            CreatedCount++;
    }

    // 极向量（pole）— 重挂到对应 ext_ 下（拇指 pole 为 TP_L，其余 <手指>_pole）
    const TArray<FString> LeftPoles = {
        TEXT("TP_L"),     TEXT("I_L_pole"), TEXT("M_L_pole"),
        TEXT("R_L_pole"), TEXT("P_L_pole"),
    };
    for (const FString& Name : LeftPoles) {
        FString FingerName = Name;
        if (FingerName.StartsWith(TEXT("TP"))) {
            // TP_L → T_L
            FingerName = TEXT("T") + FingerName.Mid(2);
        } else if (FingerName.EndsWith(TEXT("_pole"))) {
            FingerName = FingerName.LeftChop(5);
        }
        if (FControlRigCreationUtility::EnsureControl(
                Blueprint, Name, FString::Printf(TEXT("ext_%s"), *FingerName)))
            CreatedCount++;
    }

    // 3. 右手控制器（8 个主 + 4 个 pole）
    const TArray<FString> RightMain = {
        TEXT("H_R"),
        TEXT("HP_R"),
    };
    for (const FString& Name : RightMain) {
        if (CreateController(Blueprint, Name, TEXT("controller_root")))
            CreatedCount++;
    }

    // 手指挂在对应手掌（H_R）下
    const TArray<FString> RightFingers = {TEXT("T_R"), TEXT("I_R"), TEXT("M_R"),
                                          TEXT("R_R"), TEXT("P_R")};
    for (const FString& Name : RightFingers) {
        if (CreateController(Blueprint, Name, TEXT("H_R"))) CreatedCount++;
    }

    // 辅助控件（ext_）— 每个手指一个，与手指同级（都挂 H_R）
    for (const FString& Name : RightFingers) {
        if (FControlRigCreationUtility::EnsureControl(
                Blueprint, FString::Printf(TEXT("ext_%s"), *Name), TEXT("H_R")))
            CreatedCount++;
    }

    // 极向量（pole）— 重挂到对应 ext_ 下（拇指 pole 为 TP_R，其余 <手指>_pole）
    const TArray<FString> RightPoles = {
        TEXT("TP_R"),     TEXT("I_R_pole"), TEXT("M_R_pole"),
        TEXT("R_R_pole"), TEXT("P_R_pole"),
    };
    for (const FString& Name : RightPoles) {
        FString FingerName = Name;
        if (FingerName.StartsWith(TEXT("TP"))) {
            // TP_R → T_R
            FingerName = TEXT("T") + FingerName.Mid(2);
        } else if (FingerName.EndsWith(TEXT("_pole"))) {
            FingerName = FingerName.LeftChop(5);
        }
        if (FControlRigCreationUtility::EnsureControl(
                Blueprint, Name, FString::Printf(TEXT("ext_%s"), *FingerName)))
            CreatedCount++;
    }

    // 4. 脚部控制器（主控挂 controller_root，pole 挂对应主控）
    if (CreateController(Blueprint, TEXT("F_L"), TEXT("controller_root")))
        CreatedCount++;
    if (CreateController(Blueprint, TEXT("F_L_pole"), TEXT("F_L")))
        CreatedCount++;
    if (CreateController(Blueprint, TEXT("F_R"), TEXT("controller_root")))
        CreatedCount++;
    if (CreateController(Blueprint, TEXT("F_R_pole"), TEXT("F_R")))
        CreatedCount++;

    // 5. Target 控制器（特殊朝向控制器）
    // Middle_Hand 和 Head_Control 挂在 controller_root 下
    if (CreateController(Blueprint, TEXT("Middle_Hand"),
                         TEXT("controller_root")))
        CreatedCount++;
    if (CreateController(Blueprint, TEXT("Head_Control"),
                         TEXT("controller_root")))
        CreatedCount++;
    // Look_At 挂在 Middle_Hand 下
    if (CreateController(Blueprint, TEXT("Look_At"), TEXT("Middle_Hand")))
        CreatedCount++;

    // 7. 弦位置控制器（63 个：21弦 × 3点 head/mid/end，全部挂在 controller_root
    // 下）
    for (int32 i = 0; i <= 20; ++i) {
        if (CreateController(Blueprint, FString::Printf(TEXT("s%dhead"), i),
                             TEXT("controller_root")))
            CreatedCount++;
        if (CreateController(Blueprint, FString::Printf(TEXT("s%dmid"), i),
                             TEXT("controller_root")))
            CreatedCount++;
        if (CreateController(Blueprint, FString::Printf(TEXT("s%dend"), i),
                             TEXT("controller_root")))
            CreatedCount++;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift SetupControllers: Created %d controllers"),
           CreatedCount);
    return CreatedCount;
}

// ============================================================
// 2. CheckObjectsStatus
// ============================================================

bool UZhengDriftControlRigProcessor::CheckObjectsStatus(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ValidateZhengDriftActor(ZhengDriftActor, TEXT("CheckObjectsStatus")))
        return false;

    UControlRig* ControlRig = GetControlRig(ZhengDriftActor);
    if (!ControlRig) {
        UE_LOG(
            LogTemp, Error,
            TEXT("CheckObjectsStatus [ZhengDrift]: Failed to get ControlRig"));
        return false;
    }

    TArray<FString> Expected = GetExpectedControllerNames(ZhengDriftActor);
    int32 Found = 0;
    int32 Missing = 0;

    for (const FString& Name : Expected) {
        FRigElementKey Key(*Name, ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(Key)) {
            Found++;
        } else {
            Missing++;
            UE_LOG(LogTemp, Warning,
                   TEXT("CheckObjectsStatus [ZhengDrift]: Missing '%s'"),
                   *Name);
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("ZhengDrift CheckObjectsStatus: Expected=%d Found=%d Missing=%d"),
        Expected.Num(), Found, Missing);

    return Missing == 0;
}

// ============================================================
// 3. SetupAllObjects
// ============================================================

bool UZhengDriftControlRigProcessor::SetupAllObjects(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ValidateZhengDriftActor(ZhengDriftActor, TEXT("SetupAllObjects")))
        return false;

    ZhengDriftActor->RegisterAllControlRigs();

    int32 Created = SetupControllers(ZhengDriftActor);
    if (Created == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupAllObjects [ZhengDrift]: No controllers created"));
        return false;
    }

    return CheckObjectsStatus(ZhengDriftActor);
}

// ============================================================
// 4. SaveState / SaveLeftHandState / SaveRightHandState
// ============================================================

bool UZhengDriftControlRigProcessor::SaveLeftHandState(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ValidateZhengDriftActor(ZhengDriftActor, TEXT("SaveLeftHandState")))
        return false;

    UControlRig* ControlRig = GetControlRig(ZhengDriftActor);
    if (!ControlRig) {
        ZhengDriftActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveLeftHandState"));
        ControlRig = GetControlRig(ZhengDriftActor);
        if (!ControlRig) return false;
    }

    EZhengDriftHandPosition Position = ZhengDriftActor->CurrentLeftHandPosition;
    EZhengDriftLeftHandAction Action = ZhengDriftActor->CurrentLeftHandAction;

    TMap<FString, FString> Mapping =
        ZhengDriftActor->GetLeftHandControllerToRecorderMapping(Position,
                                                                Action);

    int32 Saved = 0;
    for (const auto& Pair : Mapping) {
        const FString& CtrlName = Pair.Key;
        const FString& RecName = Pair.Value;

        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlName, T)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("SaveLeftHandState [ZhengDrift]: Controller '%s' not "
                        "found"),
                   *CtrlName);
            continue;
        }

        FZhengDriftRecorderTransform RecT;
        RecT.FromTransform(T);
        ZhengDriftActor->RecorderTransforms.FindOrAdd(RecName) = RecT;
        Saved++;
    }

    // 每次保存手部状态时，同步记录所有弦位置控制器的当前值
    SaveStringPositionStates(ZhengDriftActor, ControlRig);

    // 同步记录所有脚部控制器的当前值
    SaveFootControllerStates(ZhengDriftActor, ControlRig);

    // 检测四态并自动保存双线性辅助记录器
    CheckAndSaveBilinearHelpers(ZhengDriftActor, ControlRig);

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift SaveLeftHandState: Saved %d controllers"), Saved);

    if (ZhengDriftActor) {
        ZhengDriftActor->MarkPackageDirty();
    }

    return Saved > 0;
}

bool UZhengDriftControlRigProcessor::SaveRightHandState(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ValidateZhengDriftActor(ZhengDriftActor, TEXT("SaveRightHandState")))
        return false;

    UControlRig* ControlRig = GetControlRig(ZhengDriftActor);
    if (!ControlRig) {
        ZhengDriftActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveRightHandState"));
        ControlRig = GetControlRig(ZhengDriftActor);
        if (!ControlRig) return false;
    }

    EZhengDriftHandPosition Position =
        ZhengDriftActor->CurrentRightHandPosition;
    EZhengDriftRightHandAction Action = ZhengDriftActor->CurrentRightHandAction;

    TMap<FString, FString> Mapping =
        ZhengDriftActor->GetRightHandControllerToRecorderMapping(Position,
                                                                 Action);

    int32 Saved = 0;
    for (const auto& Pair : Mapping) {
        const FString& CtrlName = Pair.Key;
        const FString& RecName = Pair.Value;

        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlName, T)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("SaveRightHandState [ZhengDrift]: Controller '%s' not "
                        "found"),
                   *CtrlName);
            continue;
        }

        FZhengDriftRecorderTransform RecT;
        RecT.FromTransform(T);
        ZhengDriftActor->RecorderTransforms.FindOrAdd(RecName) = RecT;
        Saved++;
    }

    // 每次保存手部状态时，同步记录所有弦位置控制器的当前值
    SaveStringPositionStates(ZhengDriftActor, ControlRig);

    // 同步记录所有脚部控制器的当前值
    SaveFootControllerStates(ZhengDriftActor, ControlRig);

    // 检测四态并自动保存双线性辅助记录器
    CheckAndSaveBilinearHelpers(ZhengDriftActor, ControlRig);

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift SaveRightHandState: Saved %d controllers"), Saved);

    if (ZhengDriftActor) {
        ZhengDriftActor->MarkPackageDirty();
    }

    return Saved > 0;
}

bool UZhengDriftControlRigProcessor::SaveState(
    AZhengDriftUnreal* ZhengDriftActor,
    TMap<FString, FTransform>& OutStateData) {
    bool bLeft = SaveLeftHandState(ZhengDriftActor);
    bool bRight = SaveRightHandState(ZhengDriftActor);

    if (ZhengDriftActor) {
        for (const auto& Pair : ZhengDriftActor->RecorderTransforms) {
            OutStateData.Add(Pair.Key, Pair.Value.ToTransform());
        }
    }

    // 在 Sequencer 中为已保存控制器写入关键帧，防止后续操作导致控件复位
    {  // 影响 LeftHand: H_L,HP_L,T_L,I_L,M_L,R_L,P_L (7)
        //       RightHand: H_R,HP_R,T_R,I_R,M_R,R_R,P_R (7)
        //       Foot: F_L,F_L_pole,F_R,F_R_pole (4)
        //       Target: Head_Control (仅，Middle_Hand/Look_At 不需要)
        //       — 共 19 个
        UControlRig* CR = GetControlRig(ZhengDriftActor);
        if (CR) {
            TArray<FString> CtrlNames = {
                TEXT("H_L"),          TEXT("HP_L"), TEXT("T_L"),
                TEXT("I_L"),          TEXT("M_L"),  TEXT("R_L"),
                TEXT("P_L"),          TEXT("H_R"),  TEXT("HP_R"),
                TEXT("T_R"),          TEXT("I_R"),  TEXT("M_R"),
                TEXT("R_R"),          TEXT("P_R"),  TEXT("F_L"),
                TEXT("F_L_pole"),     TEXT("F_R"),  TEXT("F_R_pole"),
                TEXT("Head_Control"),
            };
            UInstrumentAnimationUtility::InsertCurrentPoseKeyframes(CR,
                                                                    CtrlNames);
        }
    }

    return bLeft && bRight;
}

// ============================================================
// 5. LoadState
// ============================================================

bool UZhengDriftControlRigProcessor::LoadState(
    AZhengDriftUnreal* ZhengDriftActor,
    const TMap<FString, FTransform>& StateData) {
    if (!ValidateZhengDriftActor(ZhengDriftActor, TEXT("LoadState")))
        return false;

    UControlRig* ControlRig = GetControlRig(ZhengDriftActor);
    if (!ControlRig) {
        ZhengDriftActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during LoadState"));
        ControlRig = GetControlRig(ZhengDriftActor);
        if (!ControlRig) return false;
    }

    int32 Loaded = 0;
    int32 Failed = 0;

    auto ApplyMapping = [&](const TMap<FString, FString>& RecToCtrl) {
        for (const auto& Pair : RecToCtrl) {
            const FString& RecName = Pair.Key;
            const FString& CtrlName = Pair.Value;

            const FZhengDriftRecorderTransform* FoundT =
                ZhengDriftActor->RecorderTransforms.Find(RecName);
            if (!FoundT) {
                Failed++;
                continue;
            }

            FTransform NewT;
            NewT.SetLocation(FoundT->Location);
            NewT.SetRotation(FoundT->Rotation);

            if (!FInstrumentControlRigUtility::SetControlLocalTransform(
                    ControlRig->GetHierarchy(), CtrlName, NewT)) {
                Failed++;
                continue;
            }

            Loaded++;
        }
    };

    // 左手
    ApplyMapping(ZhengDriftActor->GetLeftHandRecorderToControllerMapping(
        ZhengDriftActor->CurrentLeftHandPosition,
        ZhengDriftActor->CurrentLeftHandAction));

    // 右手
    ApplyMapping(ZhengDriftActor->GetRightHandRecorderToControllerMapping(
        ZhengDriftActor->CurrentRightHandPosition,
        ZhengDriftActor->CurrentRightHandAction));

    // 加载弦位置控制器
    LoadStringPositionStates(ZhengDriftActor, ControlRig);

    // 加载脚部控制器
    LoadFootControllerStates(ZhengDriftActor, ControlRig);

    // 检测四态并将双线性辅助记录器数据应用到控制器
    CheckAndLoadBilinearHelpers(ZhengDriftActor, ControlRig);

    // 注意：这里不能调用 Evaluate_AnyThread() / ForceEvaluate，
    // 否则 Sequencer 会用当前帧的旧关键帧覆盖刚写入的目标值。
    // 值的传播与最终求值由 InsertCurrentPoseKeyframes 末尾的 ForceEvaluate
    // 完成。

    // 在 Sequencer 中为已恢复控制器写入关键帧，防止后续操作导致控件复位
    {  // 影响 LeftHand: H_L,HP_L,T_L,I_L,M_L,R_L,P_L (7)
        //       RightHand: H_R,HP_R,T_R,I_R,M_R,R_R,P_R (7)
        //       Foot: F_L,F_L_pole,F_R,F_R_pole (4)
        //       Target: Head_Control (仅，Middle_Hand/Look_At 不需要)
        //       — 共 19 个
        UControlRig* CR = GetControlRig(ZhengDriftActor);
        if (CR) {
            TArray<FString> CtrlNames = {
                TEXT("H_L"),          TEXT("HP_L"), TEXT("T_L"),
                TEXT("I_L"),          TEXT("M_L"),  TEXT("R_L"),
                TEXT("P_L"),          TEXT("H_R"),  TEXT("HP_R"),
                TEXT("T_R"),          TEXT("I_R"),  TEXT("M_R"),
                TEXT("R_R"),          TEXT("P_R"),  TEXT("F_L"),
                TEXT("F_L_pole"),     TEXT("F_R"),  TEXT("F_R_pole"),
                TEXT("Head_Control"),
            };
            UInstrumentAnimationUtility::InsertCurrentPoseKeyframes(CR,
                                                                    CtrlNames);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("ZhengDrift LoadState: Loaded=%d Failed=%d"),
           Loaded, Failed);
    return Loaded > Failed;
}

// ============================================================
// 私有辅助方法
// ============================================================

bool UZhengDriftControlRigProcessor::ValidateZhengDriftActor(
    AZhengDriftUnreal* Actor, const FString& FunctionName) {
    if (!Actor) {
        UE_LOG(LogTemp, Error, TEXT("%s [ZhengDrift]: Actor is null"),
               *FunctionName);
        return false;
    }
    if (!Actor->SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("%s [ZhengDrift]: SkeletalMeshActor is null"),
               *FunctionName);
        return false;
    }
    return true;
}

UControlRig* UZhengDriftControlRigProcessor::GetControlRig(
    AZhengDriftUnreal* Actor) {
    return Actor->GetCachedControlRig(TEXT("Performer"));
}

bool UZhengDriftControlRigProcessor::CreateController(
    UControlRigBlueprint* Blueprint, const FString& ControllerName,
    const FString& ParentName, const FTransform& Transform) {
    if (!Blueprint || ControllerName.IsEmpty()) return false;

    // 使用 EnsureControl：控件不存在则创建；已存在则校验父级是否匹配，
    // 不匹配时 reparent 修正（bMaintainGlobalTransform 保持世界位姿）
    return FControlRigCreationUtility::EnsureControl(Blueprint, ControllerName,
                                                     ParentName);
}

int32 UZhengDriftControlRigProcessor::LinearDistributeControls(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ValidateZhengDriftActor(ZhengDriftActor,
                                 TEXT("LinearDistributeControls")))
        return -1;

    UControlRig* ControlRig = GetControlRig(ZhengDriftActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls [ZhengDrift]: Failed to get "
                    "ControlRig"));
        return -1;
    }

    return FControlRigCreationUtility::LinearDistributeControls(ControlRig);
}

TArray<FString> UZhengDriftControlRigProcessor::GetExpectedControllerNames(
    AZhengDriftUnreal* Actor) {
    TArray<FString> Names;

    Names.Add(TEXT("base_root"));
    Names.Add(TEXT("controller_root"));

    for (const auto& Pair : Actor->LeftHandControllers) Names.Add(Pair.Value);
    for (const auto& Pair : Actor->RightHandControllers) Names.Add(Pair.Value);
    for (const auto& Pair : Actor->FootControllers) Names.Add(Pair.Value);
    for (const auto& Pair : Actor->TargetControllers) Names.Add(Pair.Value);

    // 辅助控件（ext_）— 每个手指一个，与手指同级
    const TArray<FString> LeftFingerNames = {
        TEXT("T_L"), TEXT("I_L"), TEXT("M_L"), TEXT("R_L"), TEXT("P_L")};
    const TArray<FString> RightFingerNames = {
        TEXT("T_R"), TEXT("I_R"), TEXT("M_R"), TEXT("R_R"), TEXT("P_R")};
    for (const FString& Name : LeftFingerNames) {
        Names.Add(FString::Printf(TEXT("ext_%s"), *Name));
    }
    for (const FString& Name : RightFingerNames) {
        Names.Add(FString::Printf(TEXT("ext_%s"), *Name));
    }

    // 弦位置控制器（63 个）
    for (const auto& Pair : Actor->StringPositionRecorders)
        Names.Add(Pair.Value);

    return Names;
    // 总计：2 + 12 + 12 + 4 + 3 + 10(ext) + 63 = 106
}

// ============================================================
// SaveStringPositionStates
// ============================================================

void UZhengDriftControlRigProcessor::SaveStringPositionStates(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig) {
    if (!ZhengDriftActor || !ControlRig) return;

    int32 Saved = 0;
    for (const auto& Pair : ZhengDriftActor->StringPositionRecorders) {
        // Pair.Value 是控制器名（也是 RecorderTransforms 的键），例如 "s0head"
        const FString& CtrlName = Pair.Value;

        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlName, T))
            continue;

        FZhengDriftRecorderTransform RecT;
        RecT.FromTransform(T);
        ZhengDriftActor->RecorderTransforms.FindOrAdd(CtrlName) = RecT;
        Saved++;
    }

    UE_LOG(
        LogTemp, Verbose,
        TEXT("ZhengDrift SaveStringPositionStates: Saved %d string controls"),
        Saved);
}

// ============================================================
// ApplyStringPositionToControlRig
// ============================================================

void UZhengDriftControlRigProcessor::ApplyStringPositionToControlRig(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig) {
    if (!ZhengDriftActor || !ControlRig) return;

    int32 Applied = 0;
    int32 Failed = 0;
    for (const auto& Pair : ZhengDriftActor->StringPositionRecorders) {
        const FString& CtrlName = Pair.Value;

        const FZhengDriftRecorderTransform* FoundT =
            ZhengDriftActor->RecorderTransforms.Find(CtrlName);
        if (!FoundT) {
            Failed++;
            continue;
        }

        FTransform NewT;
        NewT.SetLocation(FoundT->Location);
        NewT.SetRotation(FoundT->Rotation);

        if (!FInstrumentControlRigUtility::SetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlName, NewT)) {
            Failed++;
            continue;
        }
        Applied++;
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT(
            "ZhengDrift ApplyStringPositionToControlRig: Applied=%d Failed=%d"),
        Applied, Failed);
}

// ============================================================
// LoadStringPositionStates
// ============================================================

void UZhengDriftControlRigProcessor::LoadStringPositionStates(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig) {
    if (!ZhengDriftActor || !ControlRig) return;

    int32 Loaded = 0;
    int32 Failed = 0;
    for (const auto& Pair : ZhengDriftActor->StringPositionRecorders) {
        const FString& CtrlName = Pair.Value;

        const FZhengDriftRecorderTransform* FoundT =
            ZhengDriftActor->RecorderTransforms.Find(CtrlName);
        if (!FoundT) {
            Failed++;
            continue;
        }

        FRigElementKey Key(*CtrlName, ERigElementType::Control);
        if (!ControlRig->GetHierarchy()->Contains(Key)) {
            Failed++;
            continue;
        }

        FRigControlElement* Elem =
            ControlRig->GetHierarchy()->Find<FRigControlElement>(Key);
        if (!Elem) {
            Failed++;
            continue;
        }

        FTransform NewT;
        NewT.SetLocation(FoundT->Location);
        NewT.SetRotation(FoundT->Rotation);

        if (!FInstrumentControlRigUtility::SetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlName, NewT)) {
            Failed++;
            continue;
        }
        Loaded++;
    }

    UE_LOG(LogTemp, Verbose,
           TEXT("ZhengDrift LoadStringPositionStates: Loaded=%d Failed=%d"),
           Loaded, Failed);
}

// ============================================================
// SaveFootControllerStates
// ============================================================

void UZhengDriftControlRigProcessor::SaveFootControllerStates(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig) {
    if (!ZhengDriftActor || !ControlRig) return;

    int32 Saved = 0;
    for (const auto& Pair : ZhengDriftActor->FootControllers) {
        const FString& CtrlName = Pair.Value;

        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlName, T))
            continue;

        FZhengDriftRecorderTransform RecT;
        RecT.FromTransform(T);
        ZhengDriftActor->RecorderTransforms.FindOrAdd(CtrlName) = RecT;
        Saved++;
    }

    UE_LOG(LogTemp, Verbose,
           TEXT("ZhengDrift SaveFootControllerStates: Saved %d foot controls"),
           Saved);
}

// ============================================================
// LoadFootControllerStates
// ============================================================

void UZhengDriftControlRigProcessor::LoadFootControllerStates(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig) {
    if (!ZhengDriftActor || !ControlRig) return;

    int32 Loaded = 0;
    int32 Failed = 0;
    for (const auto& Pair : ZhengDriftActor->FootControllers) {
        const FString& CtrlName = Pair.Value;

        const FZhengDriftRecorderTransform* FoundT =
            ZhengDriftActor->RecorderTransforms.Find(CtrlName);
        if (!FoundT) {
            Failed++;
            continue;
        }

        FTransform NewT;
        NewT.SetLocation(FoundT->Location);
        NewT.SetRotation(FoundT->Rotation);

        if (!FInstrumentControlRigUtility::SetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlName, NewT)) {
            Failed++;
            continue;
        }
        Loaded++;
    }

    UE_LOG(LogTemp, Verbose,
           TEXT("ZhengDrift LoadFootControllerStates: Loaded=%d Failed=%d"),
           Loaded, Failed);
}

// ============================================================
// CheckAndSaveBilinearHelpers
// ============================================================

void UZhengDriftControlRigProcessor::CheckAndSaveBilinearHelpers(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig) {
    if (!ZhengDriftActor || !ControlRig) return;

    EZhengDriftHandPosition LeftPos = ZhengDriftActor->CurrentLeftHandPosition;
    EZhengDriftHandPosition RightPos =
        ZhengDriftActor->CurrentRightHandPosition;
    EZhengDriftLeftHandAction LeftAction =
        ZhengDriftActor->CurrentLeftHandAction;
    EZhengDriftRightHandAction RightAction =
        ZhengDriftActor->CurrentRightHandAction;

    // 判断当前是否为四态之一
    FString StateKey;

    // A 态：左手 Normal（拨奏）+ 右手 Tremolo（摇指）+ 双手 Far
    if (LeftAction == EZhengDriftLeftHandAction::NORMAL &&
        RightAction == EZhengDriftRightHandAction::TREMOLO &&
        LeftPos == EZhengDriftHandPosition::FAR &&
        RightPos == EZhengDriftHandPosition::FAR) {
        StateKey = TEXT("A");
    }
    // B 态：左手 Press（按弦）+ 右手 Normal（拨奏）+ 双手 Far
    else if (LeftAction == EZhengDriftLeftHandAction::PRESS &&
             RightAction == EZhengDriftRightHandAction::NORMAL &&
             LeftPos == EZhengDriftHandPosition::FAR &&
             RightPos == EZhengDriftHandPosition::FAR) {
        StateKey = TEXT("B");
    }
    // C 态：左手 Normal（拨奏）+ 右手 Tremolo（摇指）+ 双手 Near
    else if (LeftAction == EZhengDriftLeftHandAction::NORMAL &&
             RightAction == EZhengDriftRightHandAction::TREMOLO &&
             LeftPos == EZhengDriftHandPosition::NEAR &&
             RightPos == EZhengDriftHandPosition::NEAR) {
        StateKey = TEXT("C");
    }
    // D 态：左手 Press（按弦）+ 右手 Normal（拨奏）+ 双手 Near
    else if (LeftAction == EZhengDriftLeftHandAction::PRESS &&
             RightAction == EZhengDriftRightHandAction::NORMAL &&
             LeftPos == EZhengDriftHandPosition::NEAR &&
             RightPos == EZhengDriftHandPosition::NEAR) {
        StateKey = TEXT("D");
    }

    if (StateKey.IsEmpty()) return;

    // 读取 Middle_Hand 和 Head_Control 的当前位置，写入对应辅助控制器
    TArray<TPair<FString, FString>> HelperPairs = {
        {TEXT("Middle_Hand"),
         FString::Printf(TEXT("Middle_Hand_%s"), *StateKey)},
        {TEXT("Head_Control"),
         FString::Printf(TEXT("Head_Control_%s"), *StateKey)},
    };

    for (const auto& HP : HelperPairs) {
        const FString& SrcCtrl = HP.Key;
        const FString& DestName = HP.Value;

        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), SrcCtrl, T))
            continue;

        // 只保存 location
        FZhengDriftRecorderTransform RecT;
        RecT.Location = T.GetLocation();
        ZhengDriftActor->RecorderTransforms.FindOrAdd(DestName) = RecT;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift CheckAndSaveBilinearHelpers: Saved state '%s'"),
           *StateKey);
}

// ============================================================
// CheckAndLoadBilinearHelpers
// ============================================================

void UZhengDriftControlRigProcessor::CheckAndLoadBilinearHelpers(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig) {
    if (!ZhengDriftActor || !ControlRig) return;

    EZhengDriftHandPosition LeftPos = ZhengDriftActor->CurrentLeftHandPosition;
    EZhengDriftHandPosition RightPos =
        ZhengDriftActor->CurrentRightHandPosition;
    EZhengDriftLeftHandAction LeftAction =
        ZhengDriftActor->CurrentLeftHandAction;
    EZhengDriftRightHandAction RightAction =
        ZhengDriftActor->CurrentRightHandAction;

    FString StateKey;

    if (LeftAction == EZhengDriftLeftHandAction::NORMAL &&
        RightAction == EZhengDriftRightHandAction::TREMOLO &&
        LeftPos == EZhengDriftHandPosition::FAR &&
        RightPos == EZhengDriftHandPosition::FAR) {
        StateKey = TEXT("A");
    } else if (LeftAction == EZhengDriftLeftHandAction::PRESS &&
               RightAction == EZhengDriftRightHandAction::NORMAL &&
               LeftPos == EZhengDriftHandPosition::FAR &&
               RightPos == EZhengDriftHandPosition::FAR) {
        StateKey = TEXT("B");
    } else if (LeftAction == EZhengDriftLeftHandAction::NORMAL &&
               RightAction == EZhengDriftRightHandAction::TREMOLO &&
               LeftPos == EZhengDriftHandPosition::NEAR &&
               RightPos == EZhengDriftHandPosition::NEAR) {
        StateKey = TEXT("C");
    } else if (LeftAction == EZhengDriftLeftHandAction::PRESS &&
               RightAction == EZhengDriftRightHandAction::NORMAL &&
               LeftPos == EZhengDriftHandPosition::NEAR &&
               RightPos == EZhengDriftHandPosition::NEAR) {
        StateKey = TEXT("D");
    }

    if (StateKey.IsEmpty()) return;

    // 将对应辅助记录器中保存的位置应用到 Head_Control 控制器
    TArray<TPair<FString, FString>> HelperPairs = {
        {FString::Printf(TEXT("Head_Control_%s"), *StateKey),
         TEXT("Head_Control")},
    };

    for (const auto& HP : HelperPairs) {
        const FString& SrcName = HP.Key;
        const FString& DestCtrl = HP.Value;

        const FZhengDriftRecorderTransform* FoundT =
            ZhengDriftActor->RecorderTransforms.Find(SrcName);
        if (!FoundT) continue;

        // 只应用 location
        FTransform NewT;
        NewT.SetLocation(FoundT->Location);

        FInstrumentControlRigUtility::SetControlLocalTransform(
            ControlRig->GetHierarchy(), DestCtrl, NewT);
    }

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift CheckAndLoadBilinearHelpers: Loaded state '%s'"),
           *StateKey);
}

#undef LOCTEXT_NAMESPACE
