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

    // 眼睛睁开 Control：在 base_root 下按「なごみ」曲线创建同名 Float channel
    FControlRigCreationUtility::AddEyeControl(Blueprint);

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
    FVector PalmLocation = FVector::ZeroVector;
    bool bHasPalm = false;
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

        if (CtrlName == TEXT("H_L")) {
            PalmLocation = T.GetLocation();
            bHasPalm = true;
        }

        FZhengDriftRecorderTransform RecT;
        RecT.FromTransform(T);
        ZhengDriftActor->RecorderTransforms.FindOrAdd(RecName) = RecT;
        Saved++;
    }

    // 顺手采一条头部样本：这个手部状态下"这只手在哪、头该在哪"
    if (bHasPalm) {
        CaptureHeadSample(ZhengDriftActor, ControlRig, TEXT("left"),
                          AZhengDriftUnreal::GetLeftHandActionString(Action) +
                              TEXT("_") +
                              AZhengDriftUnreal::GetHandPositionString(Position),
                          PalmLocation);
    }

    // 每次保存手部状态时，同步记录所有弦位置控制器的当前值
    SaveStringPositionStates(ZhengDriftActor, ControlRig);

    // 同步记录所有脚部控制器的当前值
    SaveFootControllerStates(ZhengDriftActor, ControlRig);

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
    FVector PalmLocation = FVector::ZeroVector;
    bool bHasPalm = false;
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

        if (CtrlName == TEXT("H_R")) {
            PalmLocation = T.GetLocation();
            bHasPalm = true;
        }

        FZhengDriftRecorderTransform RecT;
        RecT.FromTransform(T);
        ZhengDriftActor->RecorderTransforms.FindOrAdd(RecName) = RecT;
        Saved++;
    }

    // 顺手采一条头部样本：这个手部状态下"这只手在哪、头该在哪"
    if (bHasPalm) {
        CaptureHeadSample(ZhengDriftActor, ControlRig, TEXT("right"),
                          AZhengDriftUnreal::GetRightHandActionString(Action) +
                              TEXT("_") +
                              AZhengDriftUnreal::GetHandPositionString(Position),
                          PalmLocation);
    }

    // 每次保存手部状态时，同步记录所有弦位置控制器的当前值
    SaveStringPositionStates(ZhengDriftActor, ControlRig);

    // 同步记录所有脚部控制器的当前值
    SaveFootControllerStates(ZhengDriftActor, ControlRig);

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift SaveRightHandState: Saved %d controllers"), Saved);

    if (ZhengDriftActor) {
        ZhengDriftActor->MarkPackageDirty();
    }

    return Saved > 0;
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

    // 头部位置：跟着主导手（离头部原点更远的那只手）走。左右手状态若是一次
    // 同时采集的，两条样本的头位置相同，取哪条都一样；若分别采集，只有主导手
    // 那条样本是为眼前这对手位摆的。
    {
        const FString LeftKey =
            TEXT("left_") +
            AZhengDriftUnreal::GetLeftHandActionString(
                ZhengDriftActor->CurrentLeftHandAction) +
            TEXT("_") +
            AZhengDriftUnreal::GetHandPositionString(
                ZhengDriftActor->CurrentLeftHandPosition);
        const FString RightKey =
            TEXT("right_") +
            AZhengDriftUnreal::GetRightHandActionString(
                ZhengDriftActor->CurrentRightHandAction) +
            TEXT("_") +
            AZhengDriftUnreal::GetHandPositionString(
                ZhengDriftActor->CurrentRightHandPosition);

        bool bDominantIsLeft = true;
        FTransform LeftPalm;
        FTransform RightPalm;
        const bool bHasLeftPalm =
            FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), TEXT("H_L"), LeftPalm);
        const bool bHasRightPalm =
            FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), TEXT("H_R"), RightPalm);
        if (bHasLeftPalm && bHasRightPalm) {
            bDominantIsLeft =
                FVector::Dist(LeftPalm.GetLocation(),
                              ZhengDriftActor->HeadOrigin) >=
                FVector::Dist(RightPalm.GetLocation(),
                              ZhengDriftActor->HeadOrigin);
        }

        const FZhengDriftHeadSample* HeadSample =
            ZhengDriftActor->HeadSamples.Find(bDominantIsLeft ? LeftKey
                                                              : RightKey);
        if (HeadSample) {
            FTransform HeadTransform;
            if (FInstrumentControlRigUtility::GetControlLocalTransform(
                    ControlRig->GetHierarchy(), TEXT("Head_Control"),
                    HeadTransform)) {
                HeadTransform.SetLocation(HeadSample->HeadLocation);
                FInstrumentControlRigUtility::SetControlLocalTransform(
                    ControlRig->GetHierarchy(), TEXT("Head_Control"),
                    HeadTransform);
            }
        }
    }

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
// 头部采样（SaveHeadOrigin / LoadHeadOrigin / CaptureHeadSample）
// ============================================================
//
// 头部位置靠"查表"决定：拿主导手（离头部原点更远的那只手）的位置，去同一只手
// 的样本里按距离加权。因此需要两样东西：
//   - 头部原点：默认坐姿下头在哪，只用于判断哪只手离头更远；
//   - 各手部状态的样本：保存左右手状态时顺手采一条（这只手在哪、头在哪），
//     每只手 6 条（远/中/近 × 动作），没有单独的采样按钮。
//
// Head_Control 的**朝向**不在这里：它由 Track To 约束跟随 Look_At 自动算。

bool UZhengDriftControlRigProcessor::SaveHeadOrigin(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ValidateZhengDriftActor(ZhengDriftActor, TEXT("SaveHeadOrigin")))
        return false;

    UControlRig* ControlRig = GetControlRig(ZhengDriftActor);
    if (!ControlRig) {
        ZhengDriftActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveHeadOrigin"));
        ControlRig = GetControlRig(ZhengDriftActor);
        if (!ControlRig) return false;
    }

    const bool bSaved =
        SaveHeadOriginInternal(ZhengDriftActor, ControlRig);
    if (bSaved) {
        ZhengDriftActor->MarkPackageDirty();
    }
    return bSaved;
}

bool UZhengDriftControlRigProcessor::LoadHeadOrigin(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ValidateZhengDriftActor(ZhengDriftActor, TEXT("LoadHeadOrigin")))
        return false;

    UControlRig* ControlRig = GetControlRig(ZhengDriftActor);
    if (!ControlRig) {
        ZhengDriftActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during LoadHeadOrigin"));
        ControlRig = GetControlRig(ZhengDriftActor);
        if (!ControlRig) return false;
    }

    return LoadHeadOriginInternal(ZhengDriftActor, ControlRig);
}

bool UZhengDriftControlRigProcessor::SaveHeadOriginInternal(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig) {
    if (!ZhengDriftActor || !ControlRig) return false;

    FTransform HeadTransform;
    if (!FInstrumentControlRigUtility::GetControlLocalTransform(
            ControlRig->GetHierarchy(), TEXT("Head_Control"), HeadTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("ZhengDrift SaveHeadOrigin: controller 'Head_Control' not "
                    "found in rig"));
        return false;
    }

    ZhengDriftActor->HeadOrigin = HeadTransform.GetLocation();

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift SaveHeadOrigin: origin (%.2f, %.2f, %.2f)"),
           ZhengDriftActor->HeadOrigin.X, ZhengDriftActor->HeadOrigin.Y,
           ZhengDriftActor->HeadOrigin.Z);
    return true;
}

bool UZhengDriftControlRigProcessor::LoadHeadOriginInternal(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig) {
    if (!ZhengDriftActor || !ControlRig) return false;

    FTransform HeadTransform;
    if (!FInstrumentControlRigUtility::GetControlLocalTransform(
            ControlRig->GetHierarchy(), TEXT("Head_Control"), HeadTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("ZhengDrift LoadHeadOrigin: controller 'Head_Control' not "
                    "found in rig"));
        return false;
    }

    HeadTransform.SetLocation(ZhengDriftActor->HeadOrigin);
    if (!FInstrumentControlRigUtility::SetControlLocalTransform(
            ControlRig->GetHierarchy(), TEXT("Head_Control"), HeadTransform)) {
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("ZhengDrift LoadHeadOrigin: restored"));
    return true;
}

void UZhengDriftControlRigProcessor::CaptureHeadSample(
    AZhengDriftUnreal* ZhengDriftActor, UControlRig* ControlRig,
    const FString& SideKey, const FString& StateKey,
    const FVector& HandLocation) {
    if (!ZhengDriftActor || !ControlRig) return;

    FTransform HeadTransform;
    if (!FInstrumentControlRigUtility::GetControlLocalTransform(
            ControlRig->GetHierarchy(), TEXT("Head_Control"), HeadTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("ZhengDrift CaptureHeadSample: controller 'Head_Control' "
                    "not found in rig, head position of '%s_%s' not recorded"),
               *SideKey, *StateKey);
        return;
    }

    FZhengDriftHeadSample Sample;
    Sample.HandLocation = HandLocation;
    Sample.HeadLocation = HeadTransform.GetLocation();
    ZhengDriftActor->HeadSamples.FindOrAdd(SideKey + TEXT("_") + StateKey) =
        Sample;

    UE_LOG(LogTemp, Verbose,
           TEXT("ZhengDrift CaptureHeadSample: %s_%s hand (%.2f, %.2f, %.2f) "
                "head (%.2f, %.2f, %.2f)"),
           *SideKey, *StateKey, Sample.HandLocation.X, Sample.HandLocation.Y,
           Sample.HandLocation.Z, Sample.HeadLocation.X, Sample.HeadLocation.Y,
           Sample.HeadLocation.Z);
}

#undef LOCTEXT_NAMESPACE
