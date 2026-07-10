#include "FretDanceControlRigProcessor.h"

#include "Animation/SkeletalMeshActor.h"
#include "BoneControlMappingUtility.h"
#include "ControlRig.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelEditor.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Logging/MessageLog.h"
#include "MovieSceneSequence.h"
#include "Rigs/RigHierarchyController.h"

#define LOCTEXT_NAMESPACE "FretDanceControlRigProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 1. SetupControllers - 创建所有控制器
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32 UFretDanceControlRigProcessor::SetupControllers(
    AFretDanceUnreal* FretDanceActor) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("SetupControllers"))) {
        return 0;
    }

    // 通过 Subsystem 获取 ControlRig 和 ControlRigBlueprint
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupControllers: GEngine is not available"));
        return 0;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupControllers: ControlRig Cache Subsystem is not "
                    "available"));
        return 0;
    }

    // 获取当前 LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("SetupControllers: No Level Sequence is currently open"));
        return 0;
    }

    // 现在可以安全地从缓存中获取ControlRigBlueprint

    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            FretDanceActor->SkeletalMeshActor, LevelSequence);

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupControllers: Failed to get ControlRig or "
                    "ControlRigBlueprint from Subsystem"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== SetupControllers Started =========="));

    int32 CreatedCount = 0;

    // 创建控制器层次结构
    // 注意：这里只创建控制器，不创建记录器
    // 记录器在InitializeRecorderTransforms中处理

    // 1. 创建基础控制器根节点
    if (CreateController(ControlRigBlueprint, TEXT("base_root"))) {
        CreatedCount++;

        // 2. 创建控制器根节点（绑定到胸骨）
        if (CreateController(ControlRigBlueprint, TEXT("controller_root"),
                             TEXT("base_root"))) {
            CreatedCount++;

            // 2.1 创建 controller_root_offset（用于接收动画数据）
            if (CreateController(ControlRigBlueprint,
                                 TEXT("controller_root_offset"),
                                 TEXT("controller_root"))) {
                CreatedCount++;
            }
        }
    }

    // 3. 创建左手控制器（改为挂在 controller_root_offset 下）
    TArray<FString> LeftControllers = {
        TEXT("H_L"), TEXT("HP_L"), TEXT("T_L"), TEXT("TP_L"),
        TEXT("I_L"), TEXT("M_L"),  TEXT("R_L"), TEXT("P_L"),
    };

    for (const FString& ControllerName : LeftControllers) {
        FString ParentName = TEXT("controller_root_offset");
        if (ControllerName == TEXT("TP_L")) {
            ParentName = TEXT("H_L");
        }

        if (CreateController(ControlRigBlueprint, ControllerName, ParentName)) {
            CreatedCount++;
        }
    }

    // 4. 创建右手控制器（改为挂在 controller_root_offset 下）
    TArray<FString> RightControllers = {
        TEXT("H_R"),
        TEXT("HP_R"),
    };

    FString ControllerRootName = TEXT("controller_root_offset");

    for (const FString& ControllerName : RightControllers) {
        if (CreateController(ControlRigBlueprint, ControllerName,
                             ControllerRootName)) {
            CreatedCount++;
        }
    }

    // 5. 根据乐器类型创建右手手指控制器
    TMap<FString, FString> RightFingerHierarchy =
        GetRightHandControllerHierarchy(FretDanceActor->InstrumentType,
                                        ControllerRootName);

    for (const auto& FingerPair : RightFingerHierarchy) {
        const FString& FingerName = FingerPair.Key;
        const FString& ParentName = FingerPair.Value;

        // 只创建非空的控制器名称
        // 排除已经在 RightControllers 中创建的控制器
        if (!FingerName.IsEmpty() && FingerName != TEXT("H_R") &&
            FingerName != TEXT("HP_R")) {
            if (CreateController(ControlRigBlueprint, FingerName, ParentName)) {
                CreatedCount++;
            }
        }
    }

    // 6. 创建指板位置控制器（改为挂在 controller_root_offset 下）
    for (const auto& FretPair : FretDanceActor->GuitarFretPositions) {
        const FString& RecorderName = FretPair.Value;
        if (CreateController(ControlRigBlueprint, RecorderName,
                             TEXT("controller_root_offset"))) {
            CreatedCount++;
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ SetupControllers completed. Created %d controllers."),
           CreatedCount);
    return CreatedCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 2. CheckObjectsStatus - 验证控制器存在性
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UFretDanceControlRigProcessor::CheckObjectsStatus(
    AFretDanceUnreal* FretDanceActor) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("CheckObjectsStatus"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("CheckObjectsStatus: Failed to get ControlRig"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== CheckObjectsStatus Started =========="));

    TArray<FString> ExpectedControllers =
        GetExpectedControllerNames(FretDanceActor);
    int32 FoundCount = 0;
    int32 MissingCount = 0;

    for (const FString& ControllerName : ExpectedControllers) {
        FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
            FoundCount++;
            UE_LOG(LogTemp, Verbose, TEXT("✅ Found controller: %s"),
                   *ControllerName);
        } else {
            MissingCount++;
            UE_LOG(LogTemp, Warning, TEXT("❌ Missing controller: %s"),
                   *ControllerName);
        }
    }

    bool bAllFound = (MissingCount == 0);

    UE_LOG(LogTemp, Warning, TEXT("CheckObjectsStatus Results:"));
    UE_LOG(LogTemp, Warning, TEXT("  Expected: %d"), ExpectedControllers.Num());
    UE_LOG(LogTemp, Warning, TEXT("  Found: %d"), FoundCount);
    UE_LOG(LogTemp, Warning, TEXT("  Missing: %d"), MissingCount);
    UE_LOG(LogTemp, Warning, TEXT("  Status: %s"),
           bAllFound ? TEXT("PASS") : TEXT("FAIL"));

    return bAllFound;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 3. SetupAllObjects - 一键初始化
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UFretDanceControlRigProcessor::SetupAllObjects(
    AFretDanceUnreal* FretDanceActor) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("SetupAllObjects"))) {
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== SetupAllObjects Started =========="));

    // 使用统一的注册方法注册所有 ControlRig（演奏者 + 吉他）
    FretDanceActor->RegisterAllControlRigs();

    // 步骤 1: 创建控制器
    int32 CreatedCount = SetupControllers(FretDanceActor);
    if (CreatedCount == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("SetupAllObjects: Failed to create any controllers"));
        return false;
    }

    // 步骤 2: 更新记录器键名（与当前命名逻辑对齐，只增删不丢数据）
    FretDanceActor->UpdateRecorderKeys();

    // 步骤 3: 验证控制器状态
    bool bValidationPassed = CheckObjectsStatus(FretDanceActor);

    if (bValidationPassed) {
        UE_LOG(LogTemp, Warning,
               TEXT("✅ SetupAllObjects completed successfully"));
    } else {
        UE_LOG(LogTemp, Error, TEXT("❌ SetupAllObjects validation failed"));
    }

    return bValidationPassed;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 4. SaveState - 保存控制器状态到 RecorderTransforms
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UFretDanceControlRigProcessor::SaveState(
    AFretDanceUnreal* FretDanceActor, TMap<FString, FTransform>& OutStateData) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("SaveState"))) {
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("========== SaveState Started =========="));
    UE_LOG(LogTemp, Warning, TEXT("Current Left Hand: Position=%s, State=%s"),
           *UEnum::GetValueAsString(FretDanceActor->CurrentBasePosition),
           *UEnum::GetValueAsString(FretDanceActor->CurrentLeftHandState));
    UE_LOG(LogTemp, Warning, TEXT("Current Right Hand: State=%s"),
           *UEnum::GetValueAsString(FretDanceActor->CurrentRightHandState));

    // 先重新初始化所有记录器键名（确保新增加的键名也被加入）
    // 这样即使旧实例没有保存过某些状态，RecorderTransforms 中也存在默认值
    FretDanceActor->RecorderTransforms.Empty();

    // 初始化左手记录器
    for (const auto& Pair : FretDanceActor->LeftHandPositionRecorders) {
        for (int32 i = 0; i < Pair.Value.Num(); ++i) {
            FString KeyName = Pair.Value[i];
            if (!FretDanceActor->RecorderTransforms.Contains(KeyName)) {
                FretDanceActor->RecorderTransforms.Add(
                    KeyName, FFretDanceRecorderTransform());
            }
        }
    }

    // 初始化右手记录器
    for (const auto& Pair : FretDanceActor->RightHandPositionRecorders) {
        for (int32 i = 0; i < Pair.Value.Num(); ++i) {
            FString KeyName = Pair.Value[i];
            if (!FretDanceActor->RecorderTransforms.Contains(KeyName)) {
                FretDanceActor->RecorderTransforms.Add(
                    KeyName, FFretDanceRecorderTransform());
            }
        }
    }

    // 初始化指板位置记录器
    for (const auto& FretPair : FretDanceActor->GuitarFretPositions) {
        const FString& RecorderName = FretPair.Value;
        if (!FretDanceActor->RecorderTransforms.Contains(RecorderName)) {
            FretDanceActor->RecorderTransforms.Add(
                RecorderName, FFretDanceRecorderTransform());
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Initialized RecorderTransforms with %d default entries"),
           FretDanceActor->RecorderTransforms.Num());

    // 分别保存左右手状态到 RecorderTransforms（覆盖当前状态的真实数据）
    bool bLeftSaved = SaveLeftHandState(FretDanceActor);
    bool bRightSaved = SaveRightHandState(FretDanceActor);

    // 保存指板位置状态到 RecorderTransforms（与左手状态无关，始终保存）
    bool bFretPositionsSaved = SaveFretPositionsState(FretDanceActor);

    UE_LOG(LogTemp, Warning, TEXT("=== SaveState Summary ==="));
    UE_LOG(LogTemp, Warning, TEXT("Left Hand Saved: %s"),
           bLeftSaved ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Warning, TEXT("Right Hand Saved: %s"),
           bRightSaved ? TEXT("YES") : TEXT("NO"));

    UE_LOG(LogTemp, Warning, TEXT("Fret Positions Saved: %s"),
           bFretPositionsSaved ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Warning, TEXT("Total RecorderTransforms: %d"),
           FretDanceActor->RecorderTransforms.Num());

    for (const auto& Pair : FretDanceActor->RecorderTransforms) {
        UE_LOG(LogTemp, Warning,
               TEXT("  [%s] Location=(%.2f,%.2f,%.2f) "
                    "Rotation=(%.2f,%.2f,%.2f,%.2f)"),
               *Pair.Key, Pair.Value.Location.X, Pair.Value.Location.Y,
               Pair.Value.Location.Z, Pair.Value.Rotation.X,
               Pair.Value.Rotation.Y, Pair.Value.Rotation.Z,
               Pair.Value.Rotation.W);
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ SaveState completed."));

    return bLeftSaved || bRightSaved || bFretPositionsSaved;
}

bool UFretDanceControlRigProcessor::SaveLeftHandState(
    AFretDanceUnreal* FretDanceActor) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("SaveLeftHandState"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("SaveLeftHandState: Failed to get ControlRig"));
        // 尝试重新注册 ControlRig
        UE_LOG(
            LogTemp, Warning,
            TEXT("SaveLeftHandState: Attempting to re-register ControlRig..."));
        FretDanceActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveLeftHandState"));

        // 重新尝试获取 ControlRig
        ControlRig = GetControlRig(FretDanceActor);
        if (!ControlRig) {
            UE_LOG(LogTemp, Error,
                   TEXT("SaveLeftHandState: Still failed to get ControlRig "
                        "after re-registration"));
            return false;
        }
    }

    // 获取当前左手状态
    EFretDanceBasePosition Position = FretDanceActor->CurrentBasePosition;
    EFretDanceLeftHandState State = FretDanceActor->CurrentLeftHandState;

    UE_LOG(LogTemp, Warning, TEXT("=== SaveLeftHandState ==="));
    UE_LOG(LogTemp, Warning, TEXT("Position=%s, State=%s"),
           *UEnum::GetValueAsString(Position), *UEnum::GetValueAsString(State));

    // 获取映射关系
    TMap<FString, FString> RecorderMapping =
        FretDanceActor->GetLeftHandControllerToRecorderMapping(Position, State);
    UE_LOG(LogTemp, Warning, TEXT("Controller->Recorder Mapping Count: %d"),
           RecorderMapping.Num());

    int32 SavedCount = 0;
    int32 NotFoundCount = 0;

    // 遍历映射，保存每个 Controller 到 RecorderTransforms
    for (const auto& Pair : RecorderMapping) {
        const FString& ControllerName = Pair.Key;
        const FString& RecorderName = Pair.Value;

        FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
            FRigControlElement* ControlElement =
                ControlRig->GetHierarchy()->Find<FRigControlElement>(
                    ElementKey);
            if (ControlElement) {
                // 使用 GetControlValue 获取控制器值（与 StringFlow 一致）
                FRigControlValue CurrentValue =
                    ControlRig->GetHierarchy()->GetControlValue(
                        ControlElement, ERigControlValueType::Current);
                FTransform CurrentTransform = CurrentValue.GetAsTransform(
                    ControlElement->Settings.ControlType,
                    ControlElement->Settings.PrimaryAxis);

                // 保存到 RecorderTransforms
                FFretDanceRecorderTransform RecorderTransform;
                RecorderTransform.FromTransform(CurrentTransform);

                // 使用 FindOrAdd 而不是 Add，确保覆盖默认值
                FretDanceActor->RecorderTransforms.FindOrAdd(RecorderName) =
                    RecorderTransform;

                SavedCount++;
                UE_LOG(
                    LogTemp, Warning,
                    TEXT("  ✅ SAVED: %s -> %s | Loc(%.2f,%.2f,%.2f) "
                         "Rot(%.4f,%.4f,%.4f,%.4f)"),
                    *ControllerName, *RecorderName,
                    RecorderTransform.Location.X, RecorderTransform.Location.Y,
                    RecorderTransform.Location.Z, RecorderTransform.Rotation.X,
                    RecorderTransform.Rotation.Y, RecorderTransform.Rotation.Z,
                    RecorderTransform.Rotation.W);
            } else {
                NotFoundCount++;
                UE_LOG(LogTemp, Error,
                       TEXT("  ❌ CONTROL ELEMENT NULL: %s (Recorder: %s)"),
                       *ControllerName, *RecorderName);
            }
        } else {
            NotFoundCount++;
            UE_LOG(LogTemp, Error,
                   TEXT("  ❌ CONTROLLER NOT FOUND: %s (Recorder: %s)"),
                   *ControllerName, *RecorderName);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ SaveLeftHandState completed. Saved: %d, Not Found: %d"),
           SavedCount, NotFoundCount);

    // 保存指板位置状态（与左手状态无关，始终保存）
    SaveFretPositionsState(FretDanceActor);

    return true;
}

bool UFretDanceControlRigProcessor::SaveRightHandState(
    AFretDanceUnreal* FretDanceActor) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("SaveRightHandState"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("SaveRightHandState: Failed to get ControlRig"));
        // 尝试重新注册 ControlRig
        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "SaveRightHandState: Attempting to re-register ControlRig..."));
        FretDanceActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveRightHandState"));

        // 重新尝试获取 ControlRig
        ControlRig = GetControlRig(FretDanceActor);
        if (!ControlRig) {
            UE_LOG(LogTemp, Error,
                   TEXT("SaveRightHandState: Still failed to get ControlRig "
                        "after re-registration"));
            return false;
        }
    }

    // 获取当前右手状态
    EFretDanceRightHandState State = FretDanceActor->CurrentRightHandState;

    UE_LOG(LogTemp, Warning, TEXT("=== SaveRightHandState ==="));
    UE_LOG(LogTemp, Warning, TEXT("State=%s"), *UEnum::GetValueAsString(State));

    // 获取映射关系
    TMap<FString, FString> RecorderMapping =
        FretDanceActor->GetRightHandControllerToRecorderMapping(State);
    UE_LOG(LogTemp, Warning, TEXT("Controller->Recorder Mapping Count: %d"),
           RecorderMapping.Num());

    int32 SavedCount = 0;
    int32 NotFoundCount = 0;

    // 遍历映射，保存每个 Controller 到 RecorderTransforms
    for (const auto& Pair : RecorderMapping) {
        const FString& ControllerName = Pair.Key;
        const FString& RecorderName = Pair.Value;

        FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
            FRigControlElement* ControlElement =
                ControlRig->GetHierarchy()->Find<FRigControlElement>(
                    ElementKey);
            if (ControlElement) {
                // 使用 GetControlValue 获取控制器值（与 StringFlow 一致）
                FRigControlValue CurrentValue =
                    ControlRig->GetHierarchy()->GetControlValue(
                        ControlElement, ERigControlValueType::Current);
                FTransform CurrentTransform = CurrentValue.GetAsTransform(
                    ControlElement->Settings.ControlType,
                    ControlElement->Settings.PrimaryAxis);

                // 保存到 RecorderTransforms
                FFretDanceRecorderTransform RecorderTransform;
                RecorderTransform.FromTransform(CurrentTransform);

                // 保存到 RecorderTransforms（JSON 键名与内部键名已统一）
                FretDanceActor->RecorderTransforms.FindOrAdd(RecorderName) =
                    RecorderTransform;

                SavedCount++;
                UE_LOG(
                    LogTemp, Warning,
                    TEXT("  ✅ SAVED: %s -> %s | Loc(%.2f,%.2f,%.2f) "
                         "Rot(%.4f,%.4f,%.4f,%.4f)"),
                    *ControllerName, *RecorderName,
                    RecorderTransform.Location.X, RecorderTransform.Location.Y,
                    RecorderTransform.Location.Z, RecorderTransform.Rotation.X,
                    RecorderTransform.Rotation.Y, RecorderTransform.Rotation.Z,
                    RecorderTransform.Rotation.W);
            } else {
                NotFoundCount++;
                UE_LOG(LogTemp, Error,
                       TEXT("  ❌ CONTROL ELEMENT NULL: %s (Recorder: %s)"),
                       *ControllerName, *RecorderName);
            }
        } else {
            NotFoundCount++;
            UE_LOG(LogTemp, Error,
                   TEXT("  ❌ CONTROLLER NOT FOUND: %s (Recorder: %s)"),
                   *ControllerName, *RecorderName);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ SaveRightHandState completed. Saved: %d, Not Found: %d"),
           SavedCount, NotFoundCount);

    // 保存指板位置状态（与右手状态无关，始终保存）
    SaveFretPositionsState(FretDanceActor);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 5. LoadState - 从 RecorderTransforms 加载控制器状态
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UFretDanceControlRigProcessor::LoadState(
    AFretDanceUnreal* FretDanceActor,
    const TMap<FString, FTransform>& StateData) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("LoadState"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error, TEXT("LoadState: Failed to get ControlRig"));
        // 尝试重新注册 ControlRig
        UE_LOG(LogTemp, Warning,
               TEXT("LoadState: Attempting to re-register ControlRig..."));
        FretDanceActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during LoadState"));

        // 重新尝试获取 ControlRig
        ControlRig = GetControlRig(FretDanceActor);
        if (!ControlRig) {
            UE_LOG(LogTemp, Error,
                   TEXT("LoadState: Still failed to get ControlRig after "
                        "re-registration"));
            return false;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("========== LoadState Started =========="));
    UE_LOG(LogTemp, Warning, TEXT("Current Left Hand: Position=%s, State=%s"),
           *UEnum::GetValueAsString(FretDanceActor->CurrentBasePosition),
           *UEnum::GetValueAsString(FretDanceActor->CurrentLeftHandState));
    UE_LOG(LogTemp, Warning, TEXT("Current Right Hand: State=%s"),
           *UEnum::GetValueAsString(FretDanceActor->CurrentRightHandState));
    UE_LOG(LogTemp, Warning, TEXT("Total RecorderTransforms in Actor: %d"),
           FretDanceActor->RecorderTransforms.Num());

    int32 LoadedCount = 0;
    int32 FailedCount = 0;
    int32 NotFoundInMapCount = 0;
    int32 ControllerMissingCount = 0;

    // === 加载左手 ===
    // 获取当前左手状态
    EFretDanceBasePosition LeftPosition = FretDanceActor->CurrentBasePosition;
    EFretDanceLeftHandState LeftState = FretDanceActor->CurrentLeftHandState;

    UE_LOG(LogTemp, Warning, TEXT("=== Load Left Hand ==="));
    UE_LOG(LogTemp, Warning, TEXT("Position=%s, State=%s"),
           *UEnum::GetValueAsString(LeftPosition),
           *UEnum::GetValueAsString(LeftState));

    // 获取反向映射（Recorder -> Controller）
    TMap<FString, FString> LeftReverseMapping =
        FretDanceActor->GetLeftHandRecorderToControllerMapping(LeftPosition,
                                                               LeftState);
    UE_LOG(LogTemp, Warning, TEXT("Recorder->Controller Mapping Count: %d"),
           LeftReverseMapping.Num());

    // 遍历反向映射，从 RecorderTransforms 加载到 Controller
    for (const auto& Pair : LeftReverseMapping) {
        const FString& RecorderName = Pair.Key;
        const FString& ControllerName = Pair.Value;

        // JSON 键名与内部键名已统一，直接查找
        const FFretDanceRecorderTransform* FoundTransform =
            FretDanceActor->RecorderTransforms.Find(RecorderName);

        if (FoundTransform) {
            FRigElementKey ElementKey(*ControllerName,
                                      ERigElementType::Control);
            if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
                FRigControlElement* ControlElement =
                    ControlRig->GetHierarchy()->Find<FRigControlElement>(
                        ElementKey);
                if (ControlElement) {
                    FTransform NewTransform;
                    NewTransform.SetLocation(FoundTransform->Location);
                    NewTransform.SetRotation(FoundTransform->Rotation);

                    FRigControlValue NewValue;
                    NewValue.SetFromTransform(
                        NewTransform, ControlElement->Settings.ControlType,
                        ControlElement->Settings.PrimaryAxis);

                    ControlRig->GetHierarchy()->SetControlValue(
                        ControlElement, NewValue,
                        ERigControlValueType::Current);

                    LoadedCount++;
                    UE_LOG(
                        LogTemp, Warning,
                        TEXT("  ✅ LOADED: %s <- %s | "
                             "Loc(%.2f,%.2f,%.2f) Rot(%.4f,%.4f,%.4f,%.4f)"),
                        *ControllerName, *RecorderName,
                        FoundTransform->Location.X, FoundTransform->Location.Y,
                        FoundTransform->Location.Z, FoundTransform->Rotation.X,
                        FoundTransform->Rotation.Y, FoundTransform->Rotation.Z,
                        FoundTransform->Rotation.W);
                } else {
                    ControllerMissingCount++;
                    UE_LOG(LogTemp, Error,
                           TEXT("  ❌ CONTROL ELEMENT NULL: %s (Recorder: %s)"),
                           *ControllerName, *RecorderName);
                }
            } else {
                ControllerMissingCount++;
                UE_LOG(LogTemp, Error,
                       TEXT("  ❌ CONTROLLER NOT FOUND: %s (Recorder: %s)"),
                       *ControllerName, *RecorderName);
            }
        } else {
            NotFoundInMapCount++;
            UE_LOG(LogTemp, Error,
                   TEXT("  ❌ DATA NOT IN MAP: Recorder %s (Controller: %s)"),
                   *RecorderName, *ControllerName);
        }
    }

    // === 加载右手 ===
    // 获取当前右手状态
    EFretDanceRightHandState RightState = FretDanceActor->CurrentRightHandState;

    UE_LOG(LogTemp, Warning, TEXT("=== Load Right Hand ==="));
    UE_LOG(LogTemp, Warning, TEXT("State=%s"),
           *UEnum::GetValueAsString(RightState));

    // 获取反向映射（Recorder -> Controller）
    TMap<FString, FString> RightReverseMapping =
        FretDanceActor->GetRightHandRecorderToControllerMapping(RightState);
    UE_LOG(LogTemp, Warning, TEXT("Recorder->Controller Mapping Count: %d"),
           RightReverseMapping.Num());

    // 遍历反向映射，从 RecorderTransforms 加载到 Controller
    for (const auto& Pair : RightReverseMapping) {
        const FString& RecorderName = Pair.Key;
        const FString& ControllerName = Pair.Value;

        // JSON 键名与内部键名已统一，直接查找
        const FFretDanceRecorderTransform* FoundTransform =
            FretDanceActor->RecorderTransforms.Find(RecorderName);

        if (FoundTransform) {
            FRigElementKey ElementKey(*ControllerName,
                                      ERigElementType::Control);
            if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
                FRigControlElement* ControlElement =
                    ControlRig->GetHierarchy()->Find<FRigControlElement>(
                        ElementKey);
                if (ControlElement) {
                    FTransform NewTransform;
                    NewTransform.SetLocation(FoundTransform->Location);
                    NewTransform.SetRotation(FoundTransform->Rotation);

                    FRigControlValue NewValue;
                    NewValue.SetFromTransform(
                        NewTransform, ControlElement->Settings.ControlType,
                        ControlElement->Settings.PrimaryAxis);

                    ControlRig->GetHierarchy()->SetControlValue(
                        ControlElement, NewValue,
                        ERigControlValueType::Current);

                    LoadedCount++;
                    UE_LOG(
                        LogTemp, Warning,
                        TEXT("  ✅ LOADED: %s <- %s | "
                             "Loc(%.2f,%.2f,%.2f) Rot(%.4f,%.4f,%.4f,%.4f)"),
                        *ControllerName, *RecorderName,
                        FoundTransform->Location.X, FoundTransform->Location.Y,
                        FoundTransform->Location.Z, FoundTransform->Rotation.X,
                        FoundTransform->Rotation.Y, FoundTransform->Rotation.Z,
                        FoundTransform->Rotation.W);
                } else {
                    ControllerMissingCount++;
                    UE_LOG(LogTemp, Error,
                           TEXT("  ❌ CONTROL ELEMENT NULL: %s (Recorder: %s)"),
                           *ControllerName, *RecorderName);
                }
            } else {
                ControllerMissingCount++;
                UE_LOG(LogTemp, Error,
                       TEXT("  ❌ CONTROLLER NOT FOUND: %s (Recorder: %s)"),
                       *ControllerName, *RecorderName);
            }
        } else {
            NotFoundInMapCount++;
            UE_LOG(LogTemp, Error,
                   TEXT("  ❌ DATA NOT IN MAP: Recorder %s (Controller: %s)"),
                   *RecorderName, *ControllerName);
        }
    }

    // === 加载指板位置 ===
    bool bFretPositionsLoaded = LoadFretPositionsState(FretDanceActor);
    if (bFretPositionsLoaded) {
        UE_LOG(LogTemp, Warning, TEXT("✅ Fret positions loaded successfully"));
    } else {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No fret positions loaded"));
    }

    // 重新评估 Control Rig 以传播变更（约束、IK 等）
    // 注意：不能调用 ForceEvaluate / RefreshCurrentLevelSequence，
    // 否则 Sequencer 会重新从轨道读取关键帧数据，覆盖刚写入的值
    if (ControlRig) {
        ControlRig->Evaluate_AnyThread();
    }

    UE_LOG(LogTemp, Warning, TEXT("=== LoadState Summary ==="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully Loaded: %d"), LoadedCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed - Data Not In Map: %d"),
           NotFoundInMapCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed - Controller Missing: %d"),
           ControllerMissingCount);
    UE_LOG(LogTemp, Warning, TEXT("✅ LoadState completed."));

    return LoadedCount > (NotFoundInMapCount + ControllerMissingCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 指板位置状态保存和加载
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UFretDanceControlRigProcessor::SaveFretPositionsState(
    AFretDanceUnreal* FretDanceActor) {
    if (!ValidateFretDanceActor(FretDanceActor,
                                TEXT("SaveFretPositionsState"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("SaveFretPositionsState: Failed to get ControlRig"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== SaveFretPositionsState ==="));
    UE_LOG(LogTemp, Warning, TEXT("Total GuitarFretPositions: %d"),
           FretDanceActor->GuitarFretPositions.Num());

    int32 SavedCount = 0;
    int32 NotFoundCount = 0;

    // 遍历所有指板位置记录器，保存其 Transform 到
    // RecorderTransforms（与左手状态无关）
    for (const auto& FretPair : FretDanceActor->GuitarFretPositions) {
        const FString& PositionKey = FretPair.Key;     // P0, P1, P2, P3, P4
        const FString& RecorderName = FretPair.Value;  // P0, P1, P2, P3, P4

        FRigElementKey ElementKey(*RecorderName, ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
            FRigControlElement* ControlElement =
                ControlRig->GetHierarchy()->Find<FRigControlElement>(
                    ElementKey);
            if (ControlElement) {
                // 使用 GetControlValue 获取控制器值（与 StringFlow 一致）
                FRigControlValue CurrentValue =
                    ControlRig->GetHierarchy()->GetControlValue(
                        ControlElement, ERigControlValueType::Current);
                FTransform CurrentTransform = CurrentValue.GetAsTransform(
                    ControlElement->Settings.ControlType,
                    ControlElement->Settings.PrimaryAxis);

                // 保存到 RecorderTransforms
                FFretDanceRecorderTransform RecorderTransform;
                RecorderTransform.FromTransform(CurrentTransform);

                // 使用 FindOrAdd 而不是 Add，确保覆盖默认值
                FretDanceActor->RecorderTransforms.FindOrAdd(RecorderName) =
                    RecorderTransform;

                SavedCount++;
                UE_LOG(
                    LogTemp, Warning,
                    TEXT("  ✅ SAVED: %s -> %s | Loc(%.2f,%.2f,%.2f) "
                         "Rot(%.4f,%.4f,%.4f,%.4f)"),
                    *PositionKey, *RecorderName, RecorderTransform.Location.X,
                    RecorderTransform.Location.Y, RecorderTransform.Location.Z,
                    RecorderTransform.Rotation.X, RecorderTransform.Rotation.Y,
                    RecorderTransform.Rotation.Z, RecorderTransform.Rotation.W);
            } else {
                NotFoundCount++;
                UE_LOG(LogTemp, Error,
                       TEXT("  ❌ CONTROL ELEMENT NULL: %s (Key: %s)"),
                       *RecorderName, *PositionKey);
            }
        } else {
            NotFoundCount++;
            UE_LOG(LogTemp, Error,
                   TEXT("  ❌ CONTROLLER NOT FOUND: %s (Key: %s)"),
                   *RecorderName, *PositionKey);
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("✅ SaveFretPositionsState completed. Saved: %d, Not Found: %d"),
        SavedCount, NotFoundCount);
    return SavedCount > 0;
}

bool UFretDanceControlRigProcessor::LoadFretPositionsState(
    AFretDanceUnreal* FretDanceActor) {
    if (!ValidateFretDanceActor(FretDanceActor,
                                TEXT("LoadFretPositionsState"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("LoadFretPositionsState: Failed to get ControlRig"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== LoadFretPositionsState ==="));
    UE_LOG(LogTemp, Warning, TEXT("Total GuitarFretPositions: %d"),
           FretDanceActor->GuitarFretPositions.Num());

    int32 LoadedCount = 0;
    int32 NotFoundInMapCount = 0;
    int32 ControllerMissingCount = 0;

    // 遍历所有指板位置记录器，从 RecorderTransforms 加载到
    // Controller（与左手状态无关）
    for (const auto& FretPair : FretDanceActor->GuitarFretPositions) {
        const FString& PositionKey = FretPair.Key;
        const FString& RecorderName = FretPair.Value;

        // 从 RecorderTransforms 查找数据
        const FFretDanceRecorderTransform* FoundTransform =
            FretDanceActor->RecorderTransforms.Find(RecorderName);

        if (FoundTransform) {
            FRigElementKey ElementKey(*RecorderName, ERigElementType::Control);
            if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
                FRigControlElement* ControlElement =
                    ControlRig->GetHierarchy()->Find<FRigControlElement>(
                        ElementKey);
                if (ControlElement) {
                    FTransform NewTransform;
                    NewTransform.SetLocation(FoundTransform->Location);
                    NewTransform.SetRotation(FoundTransform->Rotation);

                    FRigControlValue NewValue;
                    NewValue.SetFromTransform(
                        NewTransform, ControlElement->Settings.ControlType,
                        ControlElement->Settings.PrimaryAxis);

                    ControlRig->GetHierarchy()->SetControlValue(
                        ControlElement, NewValue,
                        ERigControlValueType::Current);

                    LoadedCount++;
                    UE_LOG(
                        LogTemp, Warning,
                        TEXT("  ✅ LOADED: %s <- %s | Loc(%.2f,%.2f,%.2f) "
                             "Rot(%.4f,%.4f,%.4f,%.4f)"),
                        *RecorderName, *PositionKey, FoundTransform->Location.X,
                        FoundTransform->Location.Y, FoundTransform->Location.Z,
                        FoundTransform->Rotation.X, FoundTransform->Rotation.Y,
                        FoundTransform->Rotation.Z, FoundTransform->Rotation.W);
                } else {
                    ControllerMissingCount++;
                    UE_LOG(LogTemp, Error,
                           TEXT("  ❌ CONTROL ELEMENT NULL: %s (Key: %s)"),
                           *RecorderName, *PositionKey);
                }
            } else {
                ControllerMissingCount++;
                UE_LOG(LogTemp, Error,
                       TEXT("  ❌ CONTROLLER NOT FOUND: %s (Key: %s)"),
                       *RecorderName, *PositionKey);
            }
        } else {
            NotFoundInMapCount++;
            UE_LOG(LogTemp, Error, TEXT("  ❌ DATA NOT IN MAP: %s (Key: %s)"),
                   *RecorderName, *PositionKey);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ LoadFretPositionsState completed. Loaded: %d, Data "
                "Missing: %d, Controller Missing: %d"),
           LoadedCount, NotFoundInMapCount, ControllerMissingCount);
    return LoadedCount > (NotFoundInMapCount + ControllerMissingCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 私有辅助方法实现
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UFretDanceControlRigProcessor::ValidateFretDanceActor(
    AFretDanceUnreal* FretDanceActor, const FString& FunctionName) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error, TEXT("%s: FretDanceActor is null"),
               *FunctionName);
        return false;
    }

    if (!FretDanceActor->Guitar) {
        UE_LOG(LogTemp, Error, TEXT("%s: Guitar is not assigned"),
               *FunctionName);
        return false;
    }

    return true;
}

UControlRig* UFretDanceControlRigProcessor::GetControlRig(
    AFretDanceUnreal* FretDanceActor) {
    // SetupControllers 应该操作 Performer（演奏者）的 Control Rig
    // 因为所有手部控制器（H_L, H_R, I_L, I_R 等）都在演奏者身上
    // FretDance 继承自 AInstrumentBase，其 SkeletalMeshActor 属性指向演奏者
    return FretDanceActor->GetCachedControlRig(TEXT("Performer"));
}

bool UFretDanceControlRigProcessor::CreateController(
    UControlRigBlueprint* ControlRigBlueprint, const FString& ControllerName,
    const FString& ParentName, const FTransform& Transform) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("CreateController: ControlRigBlueprint is null"));
        return false;
    }

    if (ControllerName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("CreateController: ControllerName is empty"));
        return false;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("CreateController: Failed to get RigHierarchy from "
                    "ControlRigBlueprint"));
        return false;
    }

    // 检查控制器是否已存在
    FRigElementKey ExistingKey(*ControllerName, ERigElementType::Control);
    if (RigHierarchy->Contains(ExistingKey)) {
        UE_LOG(LogTemp, Verbose,
               TEXT("Controller %s already exists, skipping creation"),
               *ControllerName);
        return true;
    }

    // 使用新的接口创建控制器
    UE_LOG(LogTemp, Warning,
           TEXT("[DEBUG] Attempting to create control '%s' with parent='%s'"),
           *ControllerName, *ParentName);

    bool bSuccess = FControlRigCreationUtility::CreateControl(
        ControlRigBlueprint, ControllerName, ParentName);

    return bSuccess;
}

TArray<FString> UFretDanceControlRigProcessor::GetExpectedControllerNames(
    AFretDanceUnreal* FretDanceActor) {
    TArray<FString> ExpectedControllers;

    // 基础控制器
    ExpectedControllers.Add(TEXT("base_root"));
    ExpectedControllers.Add(TEXT("controller_root"));

    // 左手控制器
    ExpectedControllers.Add(TEXT("H_L"));
    ExpectedControllers.Add(TEXT("HP_L"));
    ExpectedControllers.Add(TEXT("T_L"));
    ExpectedControllers.Add(TEXT("TP_L"));
    ExpectedControllers.Add(TEXT("I_L"));
    ExpectedControllers.Add(TEXT("M_L"));
    ExpectedControllers.Add(TEXT("R_L"));
    ExpectedControllers.Add(TEXT("P_L"));

    // 右手控制器
    ExpectedControllers.Add(TEXT("H_R"));
    ExpectedControllers.Add(TEXT("HP_R"));

    FString ControllerRootName = TEXT("controller_root");

    // 右手手指控制器（根据乐器类型）
    TMap<FString, FString> RightFingerHierarchy =
        GetRightHandControllerHierarchy(FretDanceActor->InstrumentType,
                                        ControllerRootName);
    for (const auto& FingerPair : RightFingerHierarchy) {
        const FString& FingerName = FingerPair.Key;
        // 排除已包含的基础控制器
        if (!FingerName.IsEmpty() && FingerName != TEXT("H_R") &&
            FingerName != TEXT("HP_R")) {
            ExpectedControllers.Add(FingerName);
        }
    }

    // 添加指板位置控制器
    for (const auto& FretPair : FretDanceActor->GuitarFretPositions) {
        ExpectedControllers.Add(FretPair.Value);
    }

    return ExpectedControllers;
}

TMap<FString, FString>
UFretDanceControlRigProcessor::GetRightHandControllerHierarchy(
    EFretDanceInstrumentType InstrumentType, FString ControllerRootName) {
    TMap<FString, FString> Hierarchy;

    // 所有乐器类型都包含基本的右手控制器
    // 注意：ControllerRootName 现在是 "controller_root_offset"
    Hierarchy.Add(TEXT("H_R"), ControllerRootName);   // 右手掌（根级）
    Hierarchy.Add(TEXT("HP_R"), ControllerRootName);  // 右手掌枢轴
    Hierarchy.Add(TEXT("T_R"), ControllerRootName);   // 右手拇指
    Hierarchy.Add(TEXT("TP_R"), TEXT("H_R"));         // 右手拇指枢轴

    // 电吉他的特殊层级结构
    if (InstrumentType == EFretDanceInstrumentType::ELECTRIC_GUITAR) {
        // 右手手指控制器
        Hierarchy.Add(TEXT("I_R"), TEXT("T_R"));  // 右手食指
        Hierarchy.Add(TEXT("M_R"), TEXT("H_R"));  // 右手中指
        Hierarchy.Add(TEXT("R_R"), TEXT("H_R"));  // 右手无名指
        Hierarchy.Add(TEXT("P_R"), TEXT("H_R"));  // 右手小指
    } else {
        // 其他乐器类型的层级结构（所有手指直接挂在 controller_root_offset 下）
        Hierarchy.Add(TEXT("I_R"), ControllerRootName);  // 右手食指
        Hierarchy.Add(TEXT("M_R"), ControllerRootName);  // 右手中指
        Hierarchy.Add(TEXT("R_R"), ControllerRootName);  // 右手无名指
        Hierarchy.Add(TEXT("P_R"), ControllerRootName);  // 右手小指
    }

    // 为其它右手手指添加pole_target
    Hierarchy.Add(TEXT("I_R_pole"), TEXT("H_R"));  // 右手食指
    Hierarchy.Add(TEXT("M_R_pole"), TEXT("H_R"));  // 右手中指
    Hierarchy.Add(TEXT("R_R_pole"), TEXT("H_R"));  // 右手无名指
    Hierarchy.Add(TEXT("P_R_pole"), TEXT("H_R"));  // 右手小指

    // 顺便在这里也为每个左手手指添加pole_target
    Hierarchy.Add(TEXT("I_L_pole"), TEXT("H_L"));  // 左手食指
    Hierarchy.Add(TEXT("M_L_pole"), TEXT("H_L"));  // 左手中指
    Hierarchy.Add(TEXT("R_L_pole"), TEXT("H_L"));  // 左手无名指
    Hierarchy.Add(TEXT("P_L_pole"), TEXT("H_L"));  // 左手小指

    return Hierarchy;
}

#undef LOCTEXT_NAMESPACE