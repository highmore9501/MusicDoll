#include "KeyRippleControlRigProcessor.h"

#include "BoneControlMappingUtility.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "KeyRippleControlRigHelper.h"
#include "KeyRipplePianoProcessor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Rigs/RigHierarchyController.h"

#define LOCTEXT_NAMESPACE "KeyRippleControlRigProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UKeyRippleControlRigProcessor implementations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FString UKeyRippleControlRigProcessor::GetRecorderNameForControl(
    AKeyRippleUnreal* KeyRippleActor, const FString& ControlName,
    bool bIsFingerControl) {
    bool bIsLeftHand = ControlName.EndsWith(TEXT("_L"));

    EPositionType PositionType = bIsLeftHand
                                     ? KeyRippleActor->LeftHandPositionType
                                     : KeyRippleActor->RightHandPositionType;

    FString PositionTypeStr =
        KeyRippleActor->GetPositionTypeString(PositionType);

    EKeyType KeyType = bIsLeftHand ? KeyRippleActor->LeftHandKeyType
                                   : KeyRippleActor->RightHandKeyType;

    FString KeyTypeStr = KeyRippleActor->GetKeyTypeString(KeyType);

    FString RecorderName = FString::Printf(TEXT("%s_%s_%s"), *PositionTypeStr,
                                           *KeyTypeStr, *ControlName);

    UE_LOG(LogTemp, Warning,
           TEXT("GetRecorderNameForControl: %s -> %s | Hand: %s | Position: "
                "%s | KeyType: %s"),
           *ControlName, *RecorderName,
           bIsLeftHand ? TEXT("LEFT") : TEXT("RIGHT"), *PositionTypeStr,
           *KeyTypeStr);

    return RecorderName;
}

FString UKeyRippleControlRigProcessor::GetControlNameFromRecorder(
    const FString& RecorderName) {
    int32 FirstUnderscore = RecorderName.Find(TEXT("_"));
    if (FirstUnderscore == INDEX_NONE) {
        return RecorderName;
    }

    int32 SecondUnderscore =
        RecorderName.Find(TEXT("_"), ESearchCase::CaseSensitive,
                          ESearchDir::FromStart, FirstUnderscore + 1);
    if (SecondUnderscore == INDEX_NONE) {
        return RecorderName;
    }

    return RecorderName.Mid(SecondUnderscore + 1);
}

void UKeyRippleControlRigProcessor::CheckObjectsStatus(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!FKeyRippleControlRigHelper::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("CheckObjectsStatus"))) {
        return;
    }

    // 通过Subsystem获取ControlRig
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: GEngine is not available"));
        return;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: ControlRig Cache Subsystem is not "
                    "available"));
        return;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("ControlRig access: No Level Sequence is currently open"));
        return;
    }

    UControlRig* ControlRigInstance = CacheSubsystem->GetControlRig(
        KeyRippleActor->SkeletalMeshActor, LevelSequence);
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence);

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    TSet<FString> ExpectedObjects =
        FKeyRippleControlRigHelper::GetAllControllerNames(KeyRippleActor);

    TArray<FString> ExistingObjects;
    TArray<FString> MissingObjects;

    if (ControlRigBlueprint) {
        URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
        if (RigHierarchy) {
            for (const FString& ObjectName : ExpectedObjects) {
                bool bFound = false;
                FRigElementKey ElementKey(*ObjectName,
                                          ERigElementType::Control);

                if (RigHierarchy->Contains(ElementKey)) {
                    ExistingObjects.Add(ObjectName);
                    bFound = true;
                } else {
                    ElementKey.Type = ERigElementType::Bone;
                    if (RigHierarchy->Contains(ElementKey)) {
                        ExistingObjects.Add(ObjectName);
                        bFound = true;
                    }
                }

                if (!bFound) {
                    MissingObjects.Add(ObjectName);
                }
            }
        }

        UE_LOG(LogTemp, Warning,
               TEXT("KeyRipple 对象状态报告 (Control Rig 版本)"));
        UE_LOG(LogTemp, Warning, TEXT("========================"));
        UE_LOG(LogTemp, Warning, TEXT("预期对象总数: %d"),
               ExpectedObjects.Num());
        UE_LOG(LogTemp, Warning, TEXT("存在的对象数量: %d"),
               ExistingObjects.Num());
        UE_LOG(LogTemp, Warning, TEXT("缺失的对象数量: %d"),
               MissingObjects.Num());

        if (ExistingObjects.Num() > 0) {
            UE_LOG(LogTemp, Warning, TEXT("存在的对象:"));
            for (const FString& ObjName : ExistingObjects) {
                UE_LOG(LogTemp, Warning, TEXT("  - %s"), *ObjName);
            }
        }

        if (MissingObjects.Num() > 0) {
            UE_LOG(LogTemp, Warning, TEXT("缺失的对象:"));
            for (const FString& ObjName : MissingObjects) {
                UE_LOG(LogTemp, Warning, TEXT("  - %s"), *ObjName);
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("========================"));
    }
}

void UKeyRippleControlRigProcessor::SetupAllObjects(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!FKeyRippleControlRigHelper::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("SetupAllObjects"))) {
        return;
    }

    // 通过Subsystem获取ControlRig
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: GEngine is not available"));
        return;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: ControlRig Cache Subsystem is not "
                    "available"));
        return;
    }

    // 获取当前 LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("ControlRig access: No Level Sequence is currently open"));
        return;
    }

    // 关键修复：先触发注册机制，确保两个 ControlRig 都被注册到缓存子系统
    KeyRippleActor->RegisterAllControlRigs(CacheSubsystem, LevelSequence);

    // 现在可以安全地获取 ControlRig 了
    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(KeyRippleActor->SkeletalMeshActor,
                                      LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence,
            TEXT("controller_root"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    SetupControllers(KeyRippleActor);

    // 添加Bone Control Mapping变量
    if (ControlRigBlueprint) {
        FBoneControlMappingUtility::AddBoneControlMappingVariable(
            ControlRigBlueprint, KeyRippleActor);
    }

    FKeyRippleControlRigHelper::InitializeRecorderTransforms(KeyRippleActor);

    UE_LOG(LogTemp, Warning, TEXT("All KeyRipple objects have been set up"));
}

void UKeyRippleControlRigProcessor::SaveState(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!FKeyRippleControlRigHelper::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("SaveState"))) {
        return;
    }

    // 通过Subsystem获取ControlRig
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: GEngine is not available"));
        return;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: ControlRig Cache Subsystem is not "
                    "available"));
        return;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("ControlRig access: No Level Sequence is currently open"));
        return;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(KeyRippleActor->SkeletalMeshActor,
                                      LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence,
            TEXT("controller_root"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(
            LogTemp, Error,
            TEXT("Failed to get Control Rig Instance from SkeletalMeshActor"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigInstance"));
        return;
    }

    FKeyRippleControlRigHelper::LogStandardStart(TEXT("SaveState"));

    UE_LOG(LogTemp, Warning,
           TEXT("========== KeyRippleUnreal Current Status =========="));
    UE_LOG(LogTemp, Warning, TEXT("Left Hand:"));
    UE_LOG(LogTemp, Warning, TEXT("  Key Type: %s"),
           KeyRippleActor->LeftHandKeyType == EKeyType::WHITE ? TEXT("WHITE")
                                                              : TEXT("BLACK"));
    UE_LOG(LogTemp, Warning, TEXT("  Position Type: %s"),
           KeyRippleActor->LeftHandPositionType == EPositionType::HIGH
               ? TEXT("HIGH")
               : (KeyRippleActor->LeftHandPositionType == EPositionType::LOW
                      ? TEXT("LOW")
                      : TEXT("MIDDLE")));

    UE_LOG(LogTemp, Warning, TEXT("Right Hand:"));
    UE_LOG(LogTemp, Warning, TEXT("  Key Type: %s"),
           KeyRippleActor->RightHandKeyType == EKeyType::WHITE ? TEXT("WHITE")
                                                               : TEXT("BLACK"));
    UE_LOG(LogTemp, Warning, TEXT("  Position Type: %s"),
           KeyRippleActor->RightHandPositionType == EPositionType::HIGH
               ? TEXT("HIGH")
               : (KeyRippleActor->RightHandPositionType == EPositionType::LOW
                      ? TEXT("LOW")
                      : TEXT("MIDDLE")));
    UE_LOG(LogTemp, Warning, TEXT("========== End Status =========="));

    int32 SavedCount = 0;
    int32 FailedCount = 0;

    UE_LOG(LogTemp, Warning, TEXT("Processing state-dependent controllers..."));

    ControlRigInstance->Evaluate_AnyThread();

    FKeyRippleControlRigHelper::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->FingerControllers,
        SavedCount, FailedCount, true, true);

    FKeyRippleControlRigHelper::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->HandControllers,
        SavedCount, FailedCount, false, true);

    {
        // TargetPoints 中仅 Head_Control 需要保存状态，
        // Mid_Hand 由 Control Rig 根据 H_L/H_R 自动计算，
        // Look_At 通过父子关系跟随 Mid_Hand，均不需要保存
        TMap<FString, FString> HeadControlOnly;
        if (const FString* HeadCtrlName =
                KeyRippleActor->TargetPoints.Find(TEXT("head_position"))) {
            HeadControlOnly.Add(TEXT("head_position"), *HeadCtrlName);
        }
        FKeyRippleControlRigHelper::SaveControllers(
            KeyRippleActor, RigHierarchy, HeadControlOnly, SavedCount,
            FailedCount, false, true);
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Processing state-independent controllers..."));

    FKeyRippleControlRigHelper::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->KeyBoardPositions,
        SavedCount, FailedCount, false, false);

    FKeyRippleControlRigHelper::LogStandardEnd(
        TEXT("SaveState"), SavedCount, FailedCount,
        KeyRippleActor->RecorderTransforms.Num());

    // 在 Sequencer 中为已保存控制器写入关键帧，防止后续操作导致控件复位
    {  // 影响 Finger: {0..N-1}_L, {0..N-1}_R (默认 10 个)
        //       Hand: H_L,HP_L,H_R,HP_R (4)
        //       Target: Head_Control (仅，Mid_Hand/Look_At 不需要)
        //       KeyBoard: black_key,highest_white_key,lowest_white_key,
        //                 lowest_white_key_end,normal_hand_expand_position,
        //                 wide_expand_hand_position (6) — 默认共 21 个
        if (ControlRigInstance) {
            TSet<FString> CtrlNames;
            for (const auto& P : KeyRippleActor->FingerControllers)
                CtrlNames.Add(P.Value);
            for (const auto& P : KeyRippleActor->HandControllers)
                CtrlNames.Add(P.Value);
            // TargetPoints: 仅 Head_Control 需要写入关键帧
            if (const FString* HeadCtrlName =
                    KeyRippleActor->TargetPoints.Find(TEXT("head_position"))) {
                CtrlNames.Add(*HeadCtrlName);
            }
            for (const auto& P : KeyRippleActor->KeyBoardPositions)
                CtrlNames.Add(P.Value);
            UInstrumentAnimationUtility::InsertCurrentPoseKeyframes(
                ControlRigInstance, CtrlNames.Array());
        }
    }

    if (KeyRippleActor) {
        KeyRippleActor->MarkPackageDirty();
    }
}

void UKeyRippleControlRigProcessor::LoadState(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!FKeyRippleControlRigHelper::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("LoadState"))) {
        return;
    }

    // 通过Subsystem获取ControlRig
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: GEngine is not available"));
        return;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: ControlRig Cache Subsystem is not "
                    "available"));
        return;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("ControlRig access: No Level Sequence is currently open"));
        return;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(KeyRippleActor->SkeletalMeshActor,
                                      LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence,
            TEXT("controller_root"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigInstance"));
        return;
    }

    FKeyRippleControlRigHelper::LogStandardStart(TEXT("LoadState"));

    UE_LOG(LogTemp, Warning,
           TEXT("========== KeyRippleUnreal Current Status =========="));
    UE_LOG(LogTemp, Warning, TEXT("Left Hand:"));
    UE_LOG(LogTemp, Warning, TEXT("  Key Type: %s"),
           KeyRippleActor->LeftHandKeyType == EKeyType::WHITE ? TEXT("WHITE")
                                                              : TEXT("BLACK"));
    UE_LOG(LogTemp, Warning, TEXT("  Position Type: %s"),
           KeyRippleActor->LeftHandPositionType == EPositionType::HIGH
               ? TEXT("HIGH")
               : (KeyRippleActor->LeftHandPositionType == EPositionType::LOW
                      ? TEXT("LOW")
                      : TEXT("MIDDLE")));

    UE_LOG(LogTemp, Warning, TEXT("Right Hand:"));
    UE_LOG(LogTemp, Warning, TEXT("  Key Type: %s"),
           KeyRippleActor->RightHandKeyType == EKeyType::WHITE ? TEXT("WHITE")
                                                               : TEXT("BLACK"));
    UE_LOG(LogTemp, Warning, TEXT("  Position Type: %s"),
           KeyRippleActor->RightHandPositionType == EPositionType::HIGH
               ? TEXT("HIGH")
               : (KeyRippleActor->RightHandPositionType == EPositionType::LOW
                      ? TEXT("LOW")
                      : TEXT("MIDDLE")));
    UE_LOG(LogTemp, Warning, TEXT("========== End Status =========="));

    int32 LoadedCount = 0;
    int32 FailedCount = 0;

    UE_LOG(LogTemp, Warning, TEXT("Loading state-dependent controllers..."));

    FKeyRippleControlRigHelper::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->FingerControllers,
        LoadedCount, FailedCount, true, true);

    FKeyRippleControlRigHelper::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->HandControllers,
        LoadedCount, FailedCount, false, true);

    {
        // TargetPoints 中仅 Head_Control 需要加载状态
        TMap<FString, FString> HeadControlOnly;
        if (const FString* HeadCtrlName =
                KeyRippleActor->TargetPoints.Find(TEXT("head_position"))) {
            HeadControlOnly.Add(TEXT("head_position"), *HeadCtrlName);
        }
        FKeyRippleControlRigHelper::LoadControllers(
            KeyRippleActor, RigHierarchy, HeadControlOnly, LoadedCount,
            FailedCount, false, true);
    }

    UE_LOG(LogTemp, Warning, TEXT("Loading state-independent controllers..."));

    FKeyRippleControlRigHelper::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->KeyBoardPositions,
        LoadedCount, FailedCount, false, false);

    // 注意：这里不能调用 Evaluate_AnyThread() / ForceEvaluate，
    // 否则 Sequencer 会用当前帧的旧关键帧覆盖刚写入的目标值。
    // 值的传播与最终求值由 InsertCurrentPoseKeyframes 末尾的 ForceEvaluate
    // 完成。

    // 在 Sequencer 中为已恢复控制器写入关键帧，防止后续操作导致控件复位
    {  // 影响 Finger: {0..N-1}_L, {0..N-1}_R (默认 10 个)
        //       Hand: H_L,HP_L,H_R,HP_R (4)
        //       Target: Head_Control (仅，Mid_Hand/Look_At 不需要)
        //       KeyBoard: black_key,highest_white_key,lowest_white_key,
        //                 lowest_white_key_end,normal_hand_expand_position,
        //                 wide_expand_hand_position (6) — 默认共 21 个
        if (ControlRigInstance) {
            TSet<FString> CtrlNames;
            for (const auto& P : KeyRippleActor->FingerControllers)
                CtrlNames.Add(P.Value);
            for (const auto& P : KeyRippleActor->HandControllers)
                CtrlNames.Add(P.Value);
            // TargetPoints: 仅 Head_Control 需要写入关键帧
            if (const FString* HeadCtrlName =
                    KeyRippleActor->TargetPoints.Find(TEXT("head_position"))) {
                CtrlNames.Add(*HeadCtrlName);
            }
            for (const auto& P : KeyRippleActor->KeyBoardPositions)
                CtrlNames.Add(P.Value);
            UInstrumentAnimationUtility::InsertCurrentPoseKeyframes(
                ControlRigInstance, CtrlNames.Array());
        }
    }

    FKeyRippleControlRigHelper::LogStandardEnd(
        TEXT("LoadState"), LoadedCount, FailedCount,
        KeyRippleActor->RecorderTransforms.Num());
}

void UKeyRippleControlRigProcessor::SetupControllers(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!FKeyRippleControlRigHelper::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("SetupControllers"))) {
        return;
    }

    // 通过Subsystem获取ControlRig
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: GEngine is not available"));
        return;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: ControlRig Cache Subsystem is not "
                    "available"));
        return;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("ControlRig access: No Level Sequence is currently open"));
        return;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(KeyRippleActor->SkeletalMeshActor,
                                      LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence,
            TEXT("controller_root"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigBlueprint"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Setting up controllers with Control Rig integration"));

    // 步骤 0 - 在开始之前清理任何重复的Controls
    TSet<FString> AllControllerNames =
        FKeyRippleControlRigHelper::GetAllControllerNames(KeyRippleActor);
    FKeyRippleControlRigHelper::CleanupDuplicateControls(
        KeyRippleActor, RigHierarchy, AllControllerNames);

    // 第1步：创建 base_root（最上层的根控制器）
    if (!FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, TEXT("base_root"), TEXT(""))) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create base_root"));
        return;
    }

    // 第2步：创建 controller_root（乐器演奏层级的根控制器，父级为base_root）
    // EnsureControl：已存在时也会校验父级是否为 base_root
    if (!FControlRigCreationUtility::EnsureControl(
            ControlRigBlueprint, TEXT("controller_root"), TEXT("base_root"))) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create controller_root"));
        return;
    }

    // 第3步：在 base_root → controller_root 层级下，按依赖顺序创建各子控制器
    // 创建顺序：手掌控制器 → Mid_Hand → 手指控制器 → 辅助控件(ext_) →
    // 其余目标点 → 极向量控制器(pole_) → 键盘位置
    // 注意：base_root 和 controller_root 已在第1、2步中创建完成

    // 3a. 创建手掌控制器（H_L, H_R, HP_L, HP_R）— 作为手指和 pole 的父级
    //     EnsureControl：已存在时校验并修正父级为 controller_root
    for (const auto& Pair : KeyRippleActor->HandControllers) {
        const FString& ControllerName = Pair.Value;
        FControlRigCreationUtility::EnsureControl(
            ControlRigBlueprint, ControllerName, TEXT("controller_root"));
    }

    // 3b. 创建 Mid_Hand（作为 Look_At 的父级，必须在 Look_At 之前创建）
    for (const auto& Pair : KeyRippleActor->TargetPoints) {
        if (!Pair.Value.Equals(TEXT("Mid_Hand"))) continue;
        FControlRigCreationUtility::EnsureControl(
            ControlRigBlueprint, Pair.Value, TEXT("controller_root"));
        break;
    }

    // 3c. 创建手指控制器（0_L, 0_R, ...）— 挂在对应手掌下（H_L 或 H_R）
    for (const auto& FingerPair : KeyRippleActor->FingerControllers) {
        const FString& ControllerName = FingerPair.Value;
        FString HandSuffix =
            ControllerName.EndsWith(TEXT("_L")) ? TEXT("_L") : TEXT("_R");
        FString ParentName = FString::Printf(TEXT("H%s"), *HandSuffix);
        FControlRigCreationUtility::EnsureControl(
            ControlRigBlueprint, ControllerName, ParentName);
    }

    // 3d. 创建辅助控件（ext_）— 每个手指一个，与手指控件一样挂在对应手掌下
    //     极向量控件（pole_）将绑定在对应的 ext_ 控件下面（见 3f）
    for (const auto& FingerPair : KeyRippleActor->FingerControllers) {
        const FString& FingerControllerName = FingerPair.Value;
        FString ExtControllerName =
            FString::Printf(TEXT("ext_%s"), *FingerControllerName);
        FString HandSuffix = FingerControllerName.EndsWith(TEXT("_L"))
                                 ? TEXT("_L")
                                 : TEXT("_R");
        FString ParentName = FString::Printf(TEXT("H%s"), *HandSuffix);
        FControlRigCreationUtility::EnsureControl(
            ControlRigBlueprint, ExtControllerName, ParentName);
    }

    // 3e. 创建其余目标点控制器（Mid_Hand 已在 3b 中创建，此处跳过）
    //     Look_At 挂在 Mid_Hand 下，其余挂在 controller_root 下
    for (const auto& Pair : KeyRippleActor->TargetPoints) {
        const FString& ControllerName = Pair.Value;
        if (ControllerName.Equals(TEXT("Mid_Hand"))) continue;  // 已在 3b 创建

        FString ParentName = ControllerName.Equals(TEXT("Look_At"))
                                 ? TEXT("Mid_Hand")
                                 : TEXT("controller_root");
        FControlRigCreationUtility::EnsureControl(
            ControlRigBlueprint, ControllerName, ParentName);
    }

    // 3f. 创建极向量控制器（pole_0, pole_1, ...）— 挂在对应的 ext_ 控件下面
    for (const FString& ControllerName : AllControllerNames) {
        if (!ControllerName.StartsWith(TEXT("pole_"))) continue;

        FString PoleFingerNumber =
            ControllerName.Mid(5);  // 去掉 "pole_" 前缀
        FString RelatedFingerControllerName;

        for (const auto& FingerPair : KeyRippleActor->FingerControllers) {
            if (FingerPair.Key == PoleFingerNumber) {
                RelatedFingerControllerName = FingerPair.Value;
                break;
            }
        }

        if (!RelatedFingerControllerName.IsEmpty()) {
            FString ParentName = FString::Printf(
                TEXT("ext_%s"), *RelatedFingerControllerName);
            FControlRigCreationUtility::EnsureControl(
                ControlRigBlueprint, ControllerName, ParentName);
        }
    }

    // 3g. 创建键盘位置控制器（KeyBoardPositions）— 挂在 controller_root 下
    for (const auto& Pair : KeyRippleActor->KeyBoardPositions) {
        const FString& ControllerName = Pair.Value;
        FControlRigCreationUtility::EnsureControl(
            ControlRigBlueprint, ControllerName, TEXT("controller_root"));
    }

    UE_LOG(LogTemp, Warning, TEXT("Finished setting up controllers"));
}

AActor* UKeyRippleControlRigProcessor::CreateController(
    AKeyRippleUnreal* KeyRippleActor, const FString& ControllerName,
    const FString& ParentControllerName) {
    if (!FKeyRippleControlRigHelper::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("CreateController"))) {
        return nullptr;
    }

    // 通过Subsystem获取ControlRig
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: GEngine is not available"));
        return nullptr;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig access: ControlRig Cache Subsystem is not "
                    "available"));
        return nullptr;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("ControlRig access: No Level Sequence is currently open"));
        return nullptr;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(KeyRippleActor->SkeletalMeshActor,
                                      LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence,
            TEXT("controller_root"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return nullptr;
    }

    // 使用 EnsureControl：已存在时校验并修正父级，不存在则创建
    if (FControlRigCreationUtility::EnsureControl(
            ControlRigBlueprint, ControllerName, ParentControllerName)) {
        UE_LOG(LogTemp, Warning,
               TEXT("✅ Successfully ensured controller %s with parent %s"),
               *ControllerName, *ParentControllerName);
        return nullptr;
    } else {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to ensure controller %s"),
               *ControllerName);
        return nullptr;
    }
}

void UKeyRippleControlRigProcessor::SetupTargetActorDriver(
    AKeyRippleUnreal* KeyRippleActor, AActor* TargetActor) {
    if (!FKeyRippleControlRigHelper::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("SetupTargetActorDriver"))) {
        return;
    }

    UClass* KeyRippleActorClass = KeyRippleActor->GetClass();
    bool bIsControlRigBlueprint =
        KeyRippleActorClass->IsChildOf(UControlRigBlueprint::StaticClass());
    if (!bIsControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleActor is not a UControlRigBlueprint type in "
                    "SetupTargetActorDriver, actual type: %s"),
               *KeyRippleActorClass->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Setting up target actor driver with Control Rig integration"));
}

void UKeyRippleControlRigProcessor::CleanupUnusedActors(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!FKeyRippleControlRigHelper::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("CleanupUnusedActors"))) {
        return;
    }

    UClass* KeyRippleActorClass = KeyRippleActor->GetClass();
    bool bIsControlRigBlueprint =
        KeyRippleActorClass->IsChildOf(UControlRigBlueprint::StaticClass());
    if (!bIsControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleActor is not a UControlRigBlueprint type in "
                    "CleanupUnusedActors, actual type: %s"),
               *KeyRippleActorClass->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Cleaning up unused actors with Control Rig integration"));
}

#undef LOCTEXT_NAMESPACE