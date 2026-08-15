#include "HarpGlideControlRigProcessor.h"

#include "ControlRig.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Rigs/RigHierarchyController.h"

#define LOCTEXT_NAMESPACE "HarpGlideControlRigProcessor"

// ============================================================
// 1. SetupControllers
// ============================================================

int32 UHarpGlideControlRigProcessor::SetupControllers(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!ValidateActor(HarpGlideActor, TEXT("SetupControllers"))) {
        return 0;
    }

    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupControllers [HarpGlide]: GEngine is null"));
        return 0;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupControllers [HarpGlide]: CacheSubsystem not found"));
        return 0;
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("SetupControllers [HarpGlide]: No LevelSequence found"));
        return 0;
    }

    UControlRigBlueprint* Blueprint = CacheSubsystem->GetControlRigBlueprint(
        HarpGlideActor->SkeletalMeshActor, LevelSequence);
    if (!Blueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupControllers [HarpGlide]: Failed to get "
                    "ControlRigBlueprint"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== HarpGlide SetupControllers Started =========="));

    int32 CreatedCount = 0;

    // 1. 两级根节点
    if (CreateController(Blueprint, TEXT("base_root"))) CreatedCount++;
    if (CreateController(Blueprint, TEXT("controller_root"), TEXT("base_root")))
        CreatedCount++;

    // 2. 左手：主控制器 → controller_root，手指 → H_L 下，极向量 → H_L 下
    // 主控
    if (CreateController(Blueprint, TEXT("H_L"), TEXT("controller_root")))
        CreatedCount++;
    if (CreateController(Blueprint, TEXT("HP_L"), TEXT("controller_root")))
        CreatedCount++;

    // 左手五指 → H_L 下
    const TArray<FString> LeftFingers = {TEXT("T_L"), TEXT("I_L"), TEXT("M_L"),
                                         TEXT("R_L"), TEXT("P_L")};
    for (const FString& Name : LeftFingers) {
        if (CreateController(Blueprint, Name, TEXT("H_L"))) CreatedCount++;
    }

    // 左手辅助控件（ext_）— 每个手指一个，与手指同级（都挂 H_L）
    for (const FString& Name : LeftFingers) {
        if (FControlRigCreationUtility::EnsureControl(
                Blueprint, FString::Printf(TEXT("ext_%s"), *Name), TEXT("H_L")))
            CreatedCount++;
    }

    // 左手手指极向量 → 对应 ext_ 下（不参与 Save/Load，仅手动调节手指弯曲方向）
    const TArray<FString> LeftFingerPoles = {TEXT("T_L_pole"), TEXT("I_L_pole"),
                                             TEXT("M_L_pole"), TEXT("R_L_pole"),
                                             TEXT("P_L_pole")};
    for (const FString& Name : LeftFingerPoles) {
        // 从 pole 名推导手指控件名: "T_L_pole" → "T_L" → ext_T_L
        FString FingerName = Name;
        if (FingerName.EndsWith(TEXT("_pole"))) {
            FingerName = FingerName.LeftChop(5);
        }
        if (FControlRigCreationUtility::EnsureControl(
                Blueprint, Name, FString::Printf(TEXT("ext_%s"), *FingerName)))
            CreatedCount++;
    }

    // 3. 右手：同上对称
    if (CreateController(Blueprint, TEXT("H_R"), TEXT("controller_root")))
        CreatedCount++;
    if (CreateController(Blueprint, TEXT("HP_R"), TEXT("controller_root")))
        CreatedCount++;

    const TArray<FString> RightFingers = {TEXT("T_R"), TEXT("I_R"), TEXT("M_R"),
                                          TEXT("R_R"), TEXT("P_R")};
    for (const FString& Name : RightFingers) {
        if (CreateController(Blueprint, Name, TEXT("H_R"))) CreatedCount++;
    }

    // 右手辅助控件（ext_）— 每个手指一个，与手指同级（都挂 H_R）
    for (const FString& Name : RightFingers) {
        if (FControlRigCreationUtility::EnsureControl(
                Blueprint, FString::Printf(TEXT("ext_%s"), *Name), TEXT("H_R")))
            CreatedCount++;
    }

    const TArray<FString> RightFingerPoles = {
        TEXT("T_R_pole"), TEXT("I_R_pole"), TEXT("M_R_pole"), TEXT("R_R_pole"),
        TEXT("P_R_pole")};
    for (const FString& Name : RightFingerPoles) {
        // 从 pole 名推导手指控件名: "T_R_pole" → "T_R" → ext_T_R
        FString FingerName = Name;
        if (FingerName.EndsWith(TEXT("_pole"))) {
            FingerName = FingerName.LeftChop(5);
        }
        if (FControlRigCreationUtility::EnsureControl(
                Blueprint, Name, FString::Printf(TEXT("ext_%s"), *FingerName)))
            CreatedCount++;
    }

    // 4. 脚部控制器（F_L 和 FP_L 同级，均挂在 controller_root 下）
    if (CreateController(Blueprint, TEXT("F_L"), TEXT("controller_root")))
        CreatedCount++;
    if (CreateController(Blueprint, TEXT("FP_L"), TEXT("controller_root")))
        CreatedCount++;
    if (CreateController(Blueprint, TEXT("F_R"), TEXT("controller_root")))
        CreatedCount++;
    if (CreateController(Blueprint, TEXT("FP_R"), TEXT("controller_root")))
        CreatedCount++;

    // 5. Target 控制器
    if (CreateController(Blueprint, TEXT("Mid_Hand"), TEXT("controller_root")))
        CreatedCount++;

    // Look_At → Mid_Hand 子级
    if (CreateController(Blueprint, TEXT("Look_At"), TEXT("Mid_Hand")))
        CreatedCount++;

    // 6. 竖琴支点控制器 → controller_root（与手掌同级）
    if (CreateController(Blueprint, TEXT("harp_pivot"),
                         TEXT("controller_root")))
        CreatedCount++;

    // 7. 身体控制器 → harp_pivot 下（与弦位置控制器同级，方便整体调整）
    if (CreateController(Blueprint, TEXT("Head"), TEXT("harp_pivot")))
        CreatedCount++;

    if (CreateController(Blueprint, TEXT("Shoulder_Harp"), TEXT("harp_pivot")))
        CreatedCount++;

    // 8. 弦位置控制器（94 个：47弦 × 2点 head/end，挂在 harp_pivot 下）
    for (int32 i = 0; i < HarpGlideActor->StringNumber; ++i) {
        if (CreateController(Blueprint, FString::Printf(TEXT("s%dhead"), i),
                             TEXT("harp_pivot")))
            CreatedCount++;
        if (CreateController(Blueprint, FString::Printf(TEXT("s%dend"), i),
                             TEXT("harp_pivot")))
            CreatedCount++;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("HarpGlide SetupControllers: Created %d controllers"),
           CreatedCount);
    return CreatedCount;
}

// ============================================================
// 2. CheckObjectsStatus
// ============================================================

bool UHarpGlideControlRigProcessor::CheckObjectsStatus(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!ValidateActor(HarpGlideActor, TEXT("CheckObjectsStatus")))
        return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        UE_LOG(
            LogTemp, Error,
            TEXT("CheckObjectsStatus [HarpGlide]: Failed to get ControlRig"));
        return false;
    }

    TArray<FString> Expected = GetExpectedControllerNames(HarpGlideActor);
    int32 Found = 0;
    int32 Missing = 0;

    for (const FString& Name : Expected) {
        FRigElementKey Key(*Name, ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(Key)) {
            Found++;
        } else {
            Missing++;
            UE_LOG(LogTemp, Warning,
                   TEXT("CheckObjectsStatus [HarpGlide]: Missing '%s'"), *Name);
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("HarpGlide CheckObjectsStatus: Expected=%d Found=%d Missing=%d"),
        Expected.Num(), Found, Missing);

    return Missing == 0;
}

// ============================================================
// 3. SetupAllObjects
// ============================================================

bool UHarpGlideControlRigProcessor::SetupAllObjects(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!ValidateActor(HarpGlideActor, TEXT("SetupAllObjects"))) return false;

    HarpGlideActor->RegisterAllControlRigs();

    int32 Created = SetupControllers(HarpGlideActor);
    if (Created == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupAllObjects [HarpGlide]: No controllers created"));
        return false;
    }

    return CheckObjectsStatus(HarpGlideActor);
}

// ============================================================
// 4. SaveState / SaveLeftHandState / SaveRightHandState
// ============================================================

bool UHarpGlideControlRigProcessor::SaveLeftHandState(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!ValidateActor(HarpGlideActor, TEXT("SaveLeftHandState"))) return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        HarpGlideActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveLeftHandState"));
        ControlRig = GetPerformerControlRig(HarpGlideActor);
        if (!ControlRig) return false;
    }

    EHarpGlideHandPose Pose = HarpGlideActor->CurrentLeftHandPose;
    TMap<FString, FString> Mapping =
        HarpGlideActor->GetLeftHandControllerToRecorderMapping(Pose);

    int32 Saved = 0;
    for (const auto& Pair : Mapping) {
        const FString& CtrlKey = Pair.Key;
        const FString& RecName = Pair.Value;

        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlKey, T)) {
            UE_LOG(
                LogTemp, Warning,
                TEXT(
                    "SaveLeftHandState [HarpGlide]: Controller '%s' not found"),
                *CtrlKey);
            continue;
        }

        FHarpGlideRecorderTransform RecT;
        RecT.FromTransform(T);
        HarpGlideActor->RecorderTransforms.FindOrAdd(RecName) = RecT;
        Saved++;
    }

    // 同步记录弦位置
    SaveStringPositionStates(HarpGlideActor, ControlRig);
    // 同步记录脚部
    SaveFootControllerStates(HarpGlideActor, ControlRig);

    UE_LOG(LogTemp, Warning,
           TEXT("HarpGlide SaveLeftHandState: Saved %d controllers"), Saved);
    HarpGlideActor->MarkPackageDirty();
    return Saved > 0;
}

bool UHarpGlideControlRigProcessor::SaveRightHandState(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!ValidateActor(HarpGlideActor, TEXT("SaveRightHandState")))
        return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        HarpGlideActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveRightHandState"));
        ControlRig = GetPerformerControlRig(HarpGlideActor);
        if (!ControlRig) return false;
    }

    EHarpGlideHandPose Pose = HarpGlideActor->CurrentRightHandPose;
    TMap<FString, FString> Mapping =
        HarpGlideActor->GetRightHandControllerToRecorderMapping(Pose);

    int32 Saved = 0;
    for (const auto& Pair : Mapping) {
        const FString& CtrlKey = Pair.Key;
        const FString& RecName = Pair.Value;

        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlKey, T)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("SaveRightHandState [HarpGlide]: Controller '%s' not "
                        "found"),
                   *CtrlKey);
            continue;
        }

        FHarpGlideRecorderTransform RecT;
        RecT.FromTransform(T);
        HarpGlideActor->RecorderTransforms.FindOrAdd(RecName) = RecT;
        Saved++;
    }

    SaveStringPositionStates(HarpGlideActor, ControlRig);
    SaveFootControllerStates(HarpGlideActor, ControlRig);

    UE_LOG(LogTemp, Warning,
           TEXT("HarpGlide SaveRightHandState: Saved %d controllers"), Saved);
    HarpGlideActor->MarkPackageDirty();
    return Saved > 0;
}

bool UHarpGlideControlRigProcessor::SaveState(
    AHarpGlideUnreal* HarpGlideActor) {
    bool bLeft = SaveLeftHandState(HarpGlideActor);
    bool bRight = SaveRightHandState(HarpGlideActor);

    // 在 Sequencer 中为已保存控制器写入关键帧，防止后续操作导致控件复位
    {  // 影响 LeftHand: H_L,HP_L,T_L,I_L,M_L,R_L,P_L (7)
        //       RightHand: H_R,HP_R,T_R,I_R,M_R,R_R,P_R (7)
        //       Foot: F_L,FP_L,F_R,FP_R (4)
        //       Body: Head,Shoulder_Harp (2)
        //       Target: Mid_Hand,Look_At 不需要写入关键帧
        //       Pivot: harp_pivot (1) — 共 21 个
        UControlRig* CR = GetPerformerControlRig(HarpGlideActor);
        if (CR) {
            TArray<FString> CtrlNames = {
                TEXT("H_L"),  TEXT("HP_L"),          TEXT("T_L"),
                TEXT("I_L"),  TEXT("M_L"),           TEXT("R_L"),
                TEXT("P_L"),  TEXT("H_R"),           TEXT("HP_R"),
                TEXT("T_R"),  TEXT("I_R"),           TEXT("M_R"),
                TEXT("R_R"),  TEXT("P_R"),           TEXT("F_L"),
                TEXT("FP_L"), TEXT("F_R"),           TEXT("FP_R"),
                TEXT("Head"), TEXT("Shoulder_Harp"), TEXT("harp_pivot"),
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

bool UHarpGlideControlRigProcessor::LoadState(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!ValidateActor(HarpGlideActor, TEXT("LoadState"))) return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        HarpGlideActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during LoadState"));
        ControlRig = GetPerformerControlRig(HarpGlideActor);
        if (!ControlRig) return false;
    }

    int32 Loaded = 0;
    int32 Failed = 0;

    auto ApplyMapping = [&](const TMap<FString, FString>& RecToCtrl) {
        for (const auto& Pair : RecToCtrl) {
            const FString& RecName = Pair.Key;
            const FString& CtrlKey = Pair.Value;

            const FHarpGlideRecorderTransform* FoundT =
                HarpGlideActor->RecorderTransforms.Find(RecName);
            if (!FoundT) {
                Failed++;
                continue;
            }

            FTransform NewT;
            NewT.SetLocation(FoundT->Location);
            NewT.SetRotation(FoundT->Rotation);

            if (!FInstrumentControlRigUtility::SetControlLocalTransform(
                    ControlRig->GetHierarchy(), CtrlKey, NewT)) {
                Failed++;
                continue;
            }

            Loaded++;
        }
    };

    // 加载左手
    ApplyMapping(HarpGlideActor->GetLeftHandRecorderToControllerMapping(
        HarpGlideActor->CurrentLeftHandPose));

    // 加载右手
    ApplyMapping(HarpGlideActor->GetRightHandRecorderToControllerMapping(
        HarpGlideActor->CurrentRightHandPose));

    // 加载弦位置
    LoadStringPositionStates(HarpGlideActor, ControlRig);
    // 加载脚部
    LoadFootControllerStates(HarpGlideActor, ControlRig);

    // 注意：这里不能调用 Evaluate_AnyThread() / ForceEvaluate，
    // 否则 Sequencer 会用当前帧的旧关键帧覆盖刚写入的目标值。
    // 值的传播与最终求值由 InsertCurrentPoseKeyframes 末尾的 ForceEvaluate
    // 完成。

    // 在 Sequencer 中为已恢复控制器写入关键帧，防止后续操作导致控件复位
    {  // 影响 LeftHand: H_L,HP_L,T_L,I_L,M_L,R_L,P_L (7)
        //       RightHand: H_R,HP_R,T_R,I_R,M_R,R_R,P_R (7)
        //       Foot: F_L,FP_L,F_R,FP_R (4)
        //       Body: Head,Shoulder_Harp (2)
        //       Target: Mid_Hand,Look_At 不需要写入关键帧
        //       Pivot: harp_pivot (1) — 共 21 个
        UControlRig* CR = GetPerformerControlRig(HarpGlideActor);
        if (CR) {
            TArray<FString> CtrlNames = {
                TEXT("H_L"),  TEXT("HP_L"),          TEXT("T_L"),
                TEXT("I_L"),  TEXT("M_L"),           TEXT("R_L"),
                TEXT("P_L"),  TEXT("H_R"),           TEXT("HP_R"),
                TEXT("T_R"),  TEXT("I_R"),           TEXT("M_R"),
                TEXT("R_R"),  TEXT("P_R"),           TEXT("F_L"),
                TEXT("FP_L"), TEXT("F_R"),           TEXT("FP_R"),
                TEXT("Head"), TEXT("Shoulder_Harp"), TEXT("harp_pivot"),
            };
            UInstrumentAnimationUtility::InsertCurrentPoseKeyframes(CR,
                                                                    CtrlNames);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("HarpGlide LoadState: Loaded=%d Failed=%d"),
           Loaded, Failed);
    return Loaded > Failed;
}

// ============================================================
// 私有辅助方法
// ============================================================

bool UHarpGlideControlRigProcessor::ValidateActor(AHarpGlideUnreal* Actor,
                                                  const FString& FunctionName) {
    if (!Actor) {
        UE_LOG(LogTemp, Error, TEXT("%s [HarpGlide]: Actor is null"),
               *FunctionName);
        return false;
    }
    if (!Actor->SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("%s [HarpGlide]: SkeletalMeshActor is null"),
               *FunctionName);
        return false;
    }
    return true;
}

UControlRig* UHarpGlideControlRigProcessor::GetPerformerControlRig(
    AHarpGlideUnreal* Actor) {
    return Actor->GetCachedControlRig(TEXT("Performer"));
}

bool UHarpGlideControlRigProcessor::CreateController(
    UControlRigBlueprint* Blueprint, const FString& ControllerName,
    const FString& ParentName, const FTransform& Transform) {
    if (!Blueprint || ControllerName.IsEmpty()) return false;

    // 使用 EnsureControl：控件不存在则创建；已存在则校验父级是否匹配，
    // 不匹配时 reparent 修正（bMaintainGlobalTransform 保持世界位姿）
    return FControlRigCreationUtility::EnsureControl(Blueprint, ControllerName,
                                                     ParentName);
}

int32 UHarpGlideControlRigProcessor::LinearDistributeControls(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!ValidateActor(HarpGlideActor, TEXT("LinearDistributeControls")))
        return -1;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls [HarpGlide]: No ControlRig"));
        return -1;
    }

    return FControlRigCreationUtility::LinearDistributeControls(ControlRig);
}

TArray<FString> UHarpGlideControlRigProcessor::GetExpectedControllerNames(
    AHarpGlideUnreal* Actor) {
    TArray<FString> Names;

    Names.Add(TEXT("base_root"));
    Names.Add(TEXT("controller_root"));

    for (const auto& Pair : Actor->BodyControllers) Names.Add(Pair.Value);
    for (const auto& Pair : Actor->LeftHandControllers) Names.Add(Pair.Value);
    for (const auto& Pair : Actor->RightHandControllers) Names.Add(Pair.Value);
    for (const auto& Pair : Actor->FootControllers) Names.Add(Pair.Value);
    for (const auto& Pair : Actor->TargetControllers) Names.Add(Pair.Value);
    for (const auto& Pair : Actor->HarpPivotControllers) Names.Add(Pair.Value);
    for (const auto& Pair : Actor->HandPoleControllers) Names.Add(Pair.Value);

    // 弦位置控制器（94 个）
    for (const auto& Pair : Actor->StringPositionRecorders)
        Names.Add(Pair.Value);

    return Names;
}

// ============================================================
// SaveStringPositionStates
// ============================================================

void UHarpGlideControlRigProcessor::SaveStringPositionStates(
    AHarpGlideUnreal* HarpGlideActor, UControlRig* ControlRig) {
    if (!HarpGlideActor || !ControlRig) return;

    int32 Saved = 0;
    for (const auto& Pair : HarpGlideActor->StringPositionRecorders) {
        const FString& CtrlName = Pair.Value;

        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlName, T))
            continue;

        FHarpGlideRecorderTransform RecT;
        RecT.FromTransform(T);
        HarpGlideActor->RecorderTransforms.FindOrAdd(CtrlName) = RecT;
        Saved++;
    }

    UE_LOG(LogTemp, Verbose,
           TEXT("HarpGlide SaveStringPositionStates: Saved %d string controls"),
           Saved);
}

// ============================================================
// ApplyStringPositionToControlRig
// ============================================================

void UHarpGlideControlRigProcessor::ApplyStringPositionToControlRig(
    AHarpGlideUnreal* HarpGlideActor, UControlRig* ControlRig) {
    if (!HarpGlideActor || !ControlRig) return;

    int32 Applied = 0;
    int32 Failed = 0;
    for (const auto& Pair : HarpGlideActor->StringPositionRecorders) {
        const FString& CtrlName = Pair.Value;

        const FHarpGlideRecorderTransform* FoundT =
            HarpGlideActor->RecorderTransforms.Find(CtrlName);
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

    UE_LOG(LogTemp, Verbose,
           TEXT("HarpGlide ApplyStringPosition: Applied=%d Failed=%d"), Applied,
           Failed);
}

// ============================================================
// SaveFootControllerStates
// ============================================================

void UHarpGlideControlRigProcessor::SaveFootControllerStates(
    AHarpGlideUnreal* HarpGlideActor, UControlRig* ControlRig) {
    if (!HarpGlideActor || !ControlRig) return;

    for (const auto& Pair : HarpGlideActor->FootControllers) {
        const FString& CtrlName = Pair.Value;

        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), CtrlName, T))
            continue;

        FHarpGlideRecorderTransform RecT;
        RecT.FromTransform(T);
        HarpGlideActor->RecorderTransforms.FindOrAdd(CtrlName) = RecT;
    }

    UE_LOG(LogTemp, Verbose, TEXT("HarpGlide SaveFootControllerStates done"));
}

// ============================================================
// LoadFootControllerStates
// ============================================================

void UHarpGlideControlRigProcessor::LoadFootControllerStates(
    AHarpGlideUnreal* HarpGlideActor, UControlRig* ControlRig) {
    if (!HarpGlideActor || !ControlRig) return;

    for (const auto& Pair : HarpGlideActor->FootControllers) {
        const FString& CtrlName = Pair.Value;

        const FHarpGlideRecorderTransform* FoundT =
            HarpGlideActor->RecorderTransforms.Find(CtrlName);
        if (!FoundT) continue;

        FTransform NewT;
        NewT.SetLocation(FoundT->Location);
        NewT.SetRotation(FoundT->Rotation);

        FInstrumentControlRigUtility::SetControlLocalTransform(
            ControlRig->GetHierarchy(), CtrlName, NewT);
    }
}

// ============================================================
// LoadStringPositionStates
// ============================================================

void UHarpGlideControlRigProcessor::LoadStringPositionStates(
    AHarpGlideUnreal* HarpGlideActor, UControlRig* ControlRig) {
    if (!HarpGlideActor || !ControlRig) return;

    for (const auto& Pair : HarpGlideActor->StringPositionRecorders) {
        const FString& CtrlName = Pair.Value;

        const FHarpGlideRecorderTransform* FoundT =
            HarpGlideActor->RecorderTransforms.Find(CtrlName);
        if (!FoundT) continue;

        FTransform NewT;
        NewT.SetLocation(FoundT->Location);
        NewT.SetRotation(FoundT->Rotation);

        FInstrumentControlRigUtility::SetControlLocalTransform(
            ControlRig->GetHierarchy(), CtrlName, NewT);
    }
}

// ============================================================
// SavePedalState / LoadPedalState
// ============================================================

bool UHarpGlideControlRigProcessor::SavePedalState(
    AHarpGlideUnreal* HarpGlideActor, EHarpGlidePedalNote Note,
    EHarpGlidePedalState State) {
    if (!ValidateActor(HarpGlideActor, TEXT("SavePedalState"))) return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        HarpGlideActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SavePedalState"));
        ControlRig = GetPerformerControlRig(HarpGlideActor);
        if (!ControlRig) return false;
    }

    // D/C/B → F_L; E/F/G/A → F_R（对应 Blender 插件中的脚部 IK 控制器）
    const FString CtrlName =
        (Note == EHarpGlidePedalNote::D || Note == EHarpGlidePedalNote::C ||
         Note == EHarpGlidePedalNote::B)
            ? TEXT("F_L")
            : TEXT("F_R");

    // 参考 Blender 插件：踏板记录器是 harp_pivot 的子级（局部坐标系），
    // 而 F_L/F_R 脚部控制器位于世界坐标系，Save 时需要坐标转换
    FRigElementKey PivotKey(TEXT("harp_pivot"), ERigElementType::Control);
    if (!ControlRig->GetHierarchy()->Contains(PivotKey)) {
        UE_LOG(LogTemp, Error,
               TEXT("SavePedalState [HarpGlide]: harp_pivot not found"));
        return false;
    }
    const FTransform PivotGlobal =
        ControlRig->GetHierarchy()->GetGlobalTransform(PivotKey);

    // 获取控制器的全局（世界）变换
    FRigElementKey CtrlKey(*CtrlName, ERigElementType::Control);
    if (!ControlRig->GetHierarchy()->Contains(CtrlKey)) {
        UE_LOG(LogTemp, Warning,
               TEXT("SavePedalState [HarpGlide]: Controller '%s' not found"),
               *CtrlName);
        return false;
    }
    const FTransform CtrlGlobal =
        ControlRig->GetHierarchy()->GetGlobalTransform(CtrlKey);

    // 转换为 harp_pivot 局部空间
    // Blender 参考：local_mat = pivot.inverted() @ world_mat
    // 注意：UE 的 GetRelativeTransform 实际返回 this * Other^(-1)，
    // 等价于 blender 中的 pivot.inverted() @ world_mat 效果
    const FTransform LocalInPivot =
        CtrlGlobal.GetRelativeTransform(PivotGlobal);

    const FString RecKey = FString::Printf(
        TEXT("pedal_%s_state%d"), *AHarpGlideUnreal::GetPedalNoteString(Note),
        (int32)State);

    UE_LOG(LogTemp, Warning, TEXT("SavePedalState [HarpGlide] >>> %s"),
           *RecKey);
    UE_LOG(
        LogTemp, Warning,
        TEXT("SavePedalState [HarpGlide]     CtrlGlobal: Loc=(%.2f,%.2f,%.2f) "
             "Rot=(%.2f,%.2f,%.2f,%.2f)"),
        CtrlGlobal.GetLocation().X, CtrlGlobal.GetLocation().Y,
        CtrlGlobal.GetLocation().Z, CtrlGlobal.GetRotation().X,
        CtrlGlobal.GetRotation().Y, CtrlGlobal.GetRotation().Z,
        CtrlGlobal.GetRotation().W);
    UE_LOG(
        LogTemp, Warning,
        TEXT("SavePedalState [HarpGlide]     PivotGlobal: Loc=(%.2f,%.2f,%.2f) "
             "Rot=(%.2f,%.2f,%.2f,%.2f)"),
        PivotGlobal.GetLocation().X, PivotGlobal.GetLocation().Y,
        PivotGlobal.GetLocation().Z, PivotGlobal.GetRotation().X,
        PivotGlobal.GetRotation().Y, PivotGlobal.GetRotation().Z,
        PivotGlobal.GetRotation().W);
    UE_LOG(
        LogTemp, Warning,
        TEXT(
            "SavePedalState [HarpGlide]     LocalInPivot: Loc=(%.2f,%.2f,%.2f) "
            "Rot=(%.2f,%.2f,%.2f,%.2f)"),
        LocalInPivot.GetLocation().X, LocalInPivot.GetLocation().Y,
        LocalInPivot.GetLocation().Z, LocalInPivot.GetRotation().X,
        LocalInPivot.GetRotation().Y, LocalInPivot.GetRotation().Z,
        LocalInPivot.GetRotation().W);

    FHarpGlideRecorderTransform RecT;
    RecT.FromTransform(LocalInPivot);
    HarpGlideActor->RecorderTransforms.FindOrAdd(RecKey) = RecT;

    HarpGlideActor->MarkPackageDirty();
    return true;
}

bool UHarpGlideControlRigProcessor::LoadPedalState(
    AHarpGlideUnreal* HarpGlideActor, EHarpGlidePedalNote Note,
    EHarpGlidePedalState State) {
    if (!ValidateActor(HarpGlideActor, TEXT("LoadPedalState"))) return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        HarpGlideActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during LoadPedalState"));
        ControlRig = GetPerformerControlRig(HarpGlideActor);
        if (!ControlRig) return false;
    }

    // D/C/B → F_L; E/F/G/A → F_R（对应 Blender 插件中的脚部 IK 控制器）
    const FString CtrlName =
        (Note == EHarpGlidePedalNote::D || Note == EHarpGlidePedalNote::C ||
         Note == EHarpGlidePedalNote::B)
            ? TEXT("F_L")
            : TEXT("F_R");

    // 参考 Blender 插件：踏板记录器是 harp_pivot 的子级（局部坐标系），
    // Load 时需要从局部坐标转换回世界坐标
    FRigElementKey PivotKey(TEXT("harp_pivot"), ERigElementType::Control);
    if (!ControlRig->GetHierarchy()->Contains(PivotKey)) {
        UE_LOG(LogTemp, Error,
               TEXT("LoadPedalState [HarpGlide]: harp_pivot not found"));
        return false;
    }
    const FTransform PivotGlobal =
        ControlRig->GetHierarchy()->GetGlobalTransform(PivotKey);

    const FString RecKey = FString::Printf(
        TEXT("pedal_%s_state%d"), *AHarpGlideUnreal::GetPedalNoteString(Note),
        (int32)State);

    const FHarpGlideRecorderTransform* FoundT =
        HarpGlideActor->RecorderTransforms.Find(RecKey);
    if (!FoundT) return false;

    // 重建 harp_pivot 局部空间的变换
    FTransform LocalInPivot;
    LocalInPivot.SetLocation(FoundT->Location);
    LocalInPivot.SetRotation(FoundT->Rotation);

    // 转换回全局（世界）变换
    // 注意：GetRelativeTransform 返回的是 this * Other^(-1)，
    // 因此反向重建需用 LocalInPivot * PivotGlobal（不是 PivotGlobal *
    // LocalInPivot）
    const FTransform TargetWorld = LocalInPivot * PivotGlobal;

    UE_LOG(LogTemp, Warning, TEXT("LoadPedalState [HarpGlide] <<< %s"),
           *RecKey);
    UE_LOG(
        LogTemp, Warning,
        TEXT("LoadPedalState [HarpGlide]     PivotGlobal: Loc=(%.2f,%.2f,%.2f) "
             "Rot=(%.2f,%.2f,%.2f,%.2f)"),
        PivotGlobal.GetLocation().X, PivotGlobal.GetLocation().Y,
        PivotGlobal.GetLocation().Z, PivotGlobal.GetRotation().X,
        PivotGlobal.GetRotation().Y, PivotGlobal.GetRotation().Z,
        PivotGlobal.GetRotation().W);
    UE_LOG(
        LogTemp, Warning,
        TEXT("LoadPedalState [HarpGlide]     StoredLocal: Loc=(%.2f,%.2f,%.2f) "
             "Rot=(%.2f,%.2f,%.2f,%.2f)"),
        LocalInPivot.GetLocation().X, LocalInPivot.GetLocation().Y,
        LocalInPivot.GetLocation().Z, LocalInPivot.GetRotation().X,
        LocalInPivot.GetRotation().Y, LocalInPivot.GetRotation().Z,
        LocalInPivot.GetRotation().W);
    UE_LOG(
        LogTemp, Warning,
        TEXT("LoadPedalState [HarpGlide]     TargetWorld: Loc=(%.2f,%.2f,%.2f) "
             "Rot=(%.2f,%.2f,%.2f,%.2f)"),
        TargetWorld.GetLocation().X, TargetWorld.GetLocation().Y,
        TargetWorld.GetLocation().Z, TargetWorld.GetRotation().X,
        TargetWorld.GetRotation().Y, TargetWorld.GetRotation().Z,
        TargetWorld.GetRotation().W);

    // 使用 SetGlobalTransform 设置全局变换
    FRigElementKey CtrlKey(*CtrlName, ERigElementType::Control);
    ControlRig->GetHierarchy()->SetGlobalTransform(CtrlKey, TargetWorld);

    // 再次读取控制器的全局变换，验证实际结果
    const FTransform AfterGlobal =
        ControlRig->GetHierarchy()->GetGlobalTransform(CtrlKey);
    UE_LOG(
        LogTemp, Warning,
        TEXT(
            "LoadPedalState [HarpGlide]     AfterGlobal : Loc=(%.2f,%.2f,%.2f) "
            "Rot=(%.2f,%.2f,%.2f,%.2f)"),
        AfterGlobal.GetLocation().X, AfterGlobal.GetLocation().Y,
        AfterGlobal.GetLocation().Z, AfterGlobal.GetRotation().X,
        AfterGlobal.GetRotation().Y, AfterGlobal.GetRotation().Z,
        AfterGlobal.GetRotation().W);

    ControlRig->Evaluate_AnyThread();
    return true;
}

// ============================================================
// SaveHarpTiltState / LoadHarpTiltState
// ============================================================

bool UHarpGlideControlRigProcessor::SaveHarpTiltState(
    AHarpGlideUnreal* HarpGlideActor, EHarpGlideTiltState State) {
    if (!ValidateActor(HarpGlideActor, TEXT("SaveHarpTiltState"))) return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        HarpGlideActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveHarpTiltState"));
        ControlRig = GetPerformerControlRig(HarpGlideActor);
        if (!ControlRig) return false;
    }

    FString RecKey = FString::Printf(
        TEXT("harp_pivot_%s"), *AHarpGlideUnreal::GetTiltStateString(State));

    FTransform T;
    if (!FInstrumentControlRigUtility::GetControlLocalTransform(
            ControlRig->GetHierarchy(), TEXT("harp_pivot"), T))
        return false;

    FHarpGlideRecorderTransform RecT;
    RecT.FromTransform(T);
    HarpGlideActor->RecorderTransforms.FindOrAdd(RecKey) = RecT;

    HarpGlideActor->MarkPackageDirty();
    return true;
}

bool UHarpGlideControlRigProcessor::LoadHarpTiltState(
    AHarpGlideUnreal* HarpGlideActor, EHarpGlideTiltState State) {
    if (!ValidateActor(HarpGlideActor, TEXT("LoadHarpTiltState"))) return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        HarpGlideActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during LoadHarpTiltState"));
        ControlRig = GetPerformerControlRig(HarpGlideActor);
        if (!ControlRig) return false;
    }

    FString RecKey = FString::Printf(
        TEXT("harp_pivot_%s"), *AHarpGlideUnreal::GetTiltStateString(State));

    const FHarpGlideRecorderTransform* FoundT =
        HarpGlideActor->RecorderTransforms.Find(RecKey);
    if (!FoundT) return false;

    FTransform NewT;
    NewT.SetLocation(FoundT->Location);
    NewT.SetRotation(FoundT->Rotation);

    FInstrumentControlRigUtility::SetControlLocalTransform(
        ControlRig->GetHierarchy(), TEXT("harp_pivot"), NewT);
    ControlRig->Evaluate_AnyThread();
    return true;
}

// ============================================================
// SaveFootRestState / LoadFootRestState
// ============================================================

bool UHarpGlideControlRigProcessor::SaveFootRestState(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!ValidateActor(HarpGlideActor, TEXT("SaveFootRestState"))) return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        HarpGlideActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveFootRestState"));
        ControlRig = GetPerformerControlRig(HarpGlideActor);
        if (!ControlRig) return false;
    }

    // F_L → F_rest_L, F_R → F_rest_R
    struct FootPair {
        FString CtrlName;
        FString RecKey;
    };
    const TArray<FootPair> Pairs = {{TEXT("F_L"), TEXT("F_rest_L")},
                                    {TEXT("F_R"), TEXT("F_rest_R")}};

    int32 Saved = 0;
    for (const auto& Pair : Pairs) {
        FTransform T;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                ControlRig->GetHierarchy(), Pair.CtrlName, T))
            continue;

        FHarpGlideRecorderTransform RecT;
        RecT.FromTransform(T);
        HarpGlideActor->RecorderTransforms.FindOrAdd(Pair.RecKey) = RecT;
        Saved++;
    }

    HarpGlideActor->MarkPackageDirty();
    return Saved > 0;
}

bool UHarpGlideControlRigProcessor::LoadFootRestState(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!ValidateActor(HarpGlideActor, TEXT("LoadFootRestState"))) return false;

    UControlRig* ControlRig = GetPerformerControlRig(HarpGlideActor);
    if (!ControlRig) {
        HarpGlideActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during LoadFootRestState"));
        ControlRig = GetPerformerControlRig(HarpGlideActor);
        if (!ControlRig) return false;
    }

    struct FootPair {
        FString CtrlName;
        FString RecKey;
    };
    const TArray<FootPair> Pairs = {{TEXT("F_L"), TEXT("F_rest_L")},
                                    {TEXT("F_R"), TEXT("F_rest_R")}};

    int32 Loaded = 0;
    for (const auto& Pair : Pairs) {
        const FHarpGlideRecorderTransform* FoundT =
            HarpGlideActor->RecorderTransforms.Find(Pair.RecKey);
        if (!FoundT) continue;

        FTransform NewT;
        NewT.SetLocation(FoundT->Location);
        NewT.SetRotation(FoundT->Rotation);

        if (FInstrumentControlRigUtility::SetControlLocalTransform(
                ControlRig->GetHierarchy(), Pair.CtrlName, NewT)) {
            Loaded++;
        }
    }

    if (Loaded > 0) ControlRig->Evaluate_AnyThread();
    return Loaded > 0;
}

#undef LOCTEXT_NAMESPACE
