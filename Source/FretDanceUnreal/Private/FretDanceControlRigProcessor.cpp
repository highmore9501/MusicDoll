#include "FretDanceControlRigProcessor.h"

#include "Animation/SkeletalMeshActor.h"
#include "BoneControlMappingUtility.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
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

        // 2. 创建控制器根节点
        if (CreateController(ControlRigBlueprint, TEXT("controller_root"),
                             TEXT("base_root"))) {
            CreatedCount++;
        }
    }

    // 3. 创建左手控制器
    TArray<FString> LeftControllers = {
        TEXT("H_L"),  TEXT("HP_L"), TEXT("T_L"),
        TEXT("TP_L"), TEXT("I_L"),  TEXT("M_L"),
        TEXT("R_L"),  TEXT("P_L"),  TEXT("H_rotation_L")};

    for (const FString& ControllerName : LeftControllers) {
        FString ParentName = TEXT("controller_root");
        // 旋转控制器作为controller_root的直接子级
        if (ControllerName == TEXT("H_rotation_L")) {
            ParentName = TEXT("controller_root");
        }

        if (CreateController(ControlRigBlueprint, ControllerName, ParentName)) {
            CreatedCount++;
        }
    }

    // 4. 创建右手控制器
    TArray<FString> RightControllers = {TEXT("H_R"), TEXT("HP_R"),
                                        TEXT("H_rotation_R")};

    FString ControllerRootName = TEXT("controller_root");

    for (const FString& ControllerName : RightControllers) {
        if (CreateController(ControlRigBlueprint, ControllerName,
                             ControllerRootName)) {
            CreatedCount++;
        }
    }

    // 5. 根据乐器类型创建右手手指控制器
    TMap<FString, FString> RightFingerHierarchy =
        GetRightHandControllerHierarchy(FretDanceActor->InstrumentType,
                                        *ControllerRootName);

    for (const auto& FingerPair : RightFingerHierarchy) {
        const FString& FingerName = FingerPair.Key;
        const FString& ParentName = FingerPair.Value;

        // 只创建非空的控制器名称
        // 排除已经在 RightControllers 中创建的控制器
        if (!FingerName.IsEmpty() && FingerName != TEXT("H_R") &&
            FingerName != TEXT("HP_R") && FingerName != TEXT("H_rotation_R")) {
            if (CreateController(ControlRigBlueprint, FingerName, ParentName)) {
                CreatedCount++;
            }
        }
    }

    // 6. 创建辅助线控制器（在 Control Rig 中创建箭头形状的 Control）
    for (const auto& GuidePair : FretDanceActor->GuideLines) {
        const FString& GuideName = GuidePair.Value;
        if (CreateController(ControlRigBlueprint, GuideName,
                             TEXT("controller_root"))) {
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

    // 步骤2: 验证控制器状态
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
// 4. SaveState - 保存控制器状态
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UFretDanceControlRigProcessor::SaveState(
    AFretDanceUnreal* FretDanceActor, TMap<FString, FTransform>& OutStateData) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("SaveState"))) {
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("========== SaveState Started =========="));

    OutStateData.Empty();

    // 分别保存左右手状态
    bool bLeftSaved = SaveLeftHandState(FretDanceActor, OutStateData);
    bool bRightSaved = SaveRightHandState(FretDanceActor, OutStateData);

    // 保存辅助线状态
    bool bGuidelinesSaved = SaveGuidelinesState(FretDanceActor, OutStateData);

    UE_LOG(LogTemp, Warning,
           TEXT("✅ SaveState completed. Saved %d controller states."),
           OutStateData.Num());

    return bLeftSaved || bRightSaved || bGuidelinesSaved;
}

bool UFretDanceControlRigProcessor::SaveLeftHandState(
    AFretDanceUnreal* FretDanceActor, TMap<FString, FTransform>& OutStateData) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("SaveLeftHandState"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("SaveLeftHandState: Failed to get ControlRig"));
        return false;
    }

    // 获取当前左手状态
    EFretDanceBasePosition Position = FretDanceActor->CurrentBasePosition;
    EFretDanceLeftHandState State = FretDanceActor->CurrentLeftHandState;

    UE_LOG(LogTemp, Warning,
           TEXT("Saving left hand state: Position=%s, State=%s"),
           *UEnum::GetValueAsString(Position), *UEnum::GetValueAsString(State));

    // 获取映射关系
    TMap<FString, FString> RecorderMapping =
        FretDanceActor->GetLeftHandControllerToRecorderMapping(Position, State);

    int32 SavedCount = 0;

    // 遍历映射，保存每个 Controller 到 Recorder
    for (const auto& Pair : RecorderMapping) {
        const FString& ControllerName = Pair.Key;
        const FString& RecorderName = Pair.Value;

        FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
            FTransform Transform =
                ControlRig->GetHierarchy()->GetGlobalTransform(ElementKey);
            OutStateData.Add(RecorderName, Transform);
            SavedCount++;
            UE_LOG(LogTemp, Verbose, TEXT("Saved left hand: %s -> %s"),
                   *ControllerName, *RecorderName);
        } else {
            UE_LOG(LogTemp, Warning, TEXT("Controller not found: %s"),
                   *ControllerName);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ SaveLeftHandState completed. Saved %d controllers."),
           SavedCount);
    return SavedCount > 0;
}

bool UFretDanceControlRigProcessor::SaveRightHandState(
    AFretDanceUnreal* FretDanceActor, TMap<FString, FTransform>& OutStateData) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("SaveRightHandState"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("SaveRightHandState: Failed to get ControlRig"));
        return false;
    }

    // 获取当前右手状态
    EFretDanceRightHandState State = FretDanceActor->CurrentRightHandState;

    UE_LOG(LogTemp, Warning, TEXT("Saving right hand state: State=%s"),
           *UEnum::GetValueAsString(State));

    // 获取映射关系
    TMap<FString, FString> RecorderMapping =
        FretDanceActor->GetRightHandControllerToRecorderMapping(State);

    int32 SavedCount = 0;

    // 遍历映射，保存每个 Controller 到 Recorder
    for (const auto& Pair : RecorderMapping) {
        const FString& ControllerName = Pair.Key;
        const FString& RecorderName = Pair.Value;

        FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
            FTransform Transform =
                ControlRig->GetHierarchy()->GetGlobalTransform(ElementKey);
            OutStateData.Add(RecorderName, Transform);
            SavedCount++;
            UE_LOG(LogTemp, Verbose, TEXT("Saved right hand: %s -> %s"),
                   *ControllerName, *RecorderName);
        } else {
            UE_LOG(LogTemp, Warning, TEXT("Controller not found: %s"),
                   *ControllerName);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ SaveRightHandState completed. Saved %d controllers."),
           SavedCount);
    return SavedCount > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 5. LoadState - 加载控制器状态
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
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("========== LoadState Started =========="));

    int32 LoadedCount = 0;
    int32 FailedCount = 0;

    // === 加载左手 ===
    // 获取当前左手状态
    EFretDanceBasePosition LeftPosition = FretDanceActor->CurrentBasePosition;
    EFretDanceLeftHandState LeftState = FretDanceActor->CurrentLeftHandState;

    UE_LOG(LogTemp, Warning,
           TEXT("Loading left hand state: Position=%s, State=%s"),
           *UEnum::GetValueAsString(LeftPosition),
           *UEnum::GetValueAsString(LeftState));

    // 获取反向映射（Recorder -> Controller）
    TMap<FString, FString> LeftReverseMapping =
        FretDanceActor->GetLeftHandRecorderToControllerMapping(LeftPosition,
                                                               LeftState);

    // 遍历反向映射，从 Recorder 加载到 Controller
    for (const auto& Pair : LeftReverseMapping) {
        const FString& RecorderName = Pair.Key;
        const FString& ControllerName = Pair.Value;

        const FTransform* FoundTransform = StateData.Find(RecorderName);
        if (FoundTransform) {
            FRigElementKey ElementKey(*ControllerName,
                                      ERigElementType::Control);
            if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
                ControlRig->GetHierarchy()->SetGlobalTransform(
                    ElementKey, *FoundTransform, true, true, true);
                LoadedCount++;
                UE_LOG(LogTemp, Verbose, TEXT("Loaded left hand: %s <- %s"),
                       *ControllerName, *RecorderName);
            } else {
                FailedCount++;
                UE_LOG(LogTemp, Warning, TEXT("Controller not found: %s"),
                       *ControllerName);
            }
        } else {
            FailedCount++;
            UE_LOG(LogTemp, Warning,
                   TEXT("Recorder data not found in StateData: %s"),
                   *RecorderName);
        }
    }

    // === 加载右手 ===
    // 获取当前右手状态
    EFretDanceRightHandState RightState = FretDanceActor->CurrentRightHandState;

    UE_LOG(LogTemp, Warning, TEXT("Loading right hand state: State=%s"),
           *UEnum::GetValueAsString(RightState));

    // 获取反向映射（Recorder -> Controller）
    TMap<FString, FString> RightReverseMapping =
        FretDanceActor->GetRightHandRecorderToControllerMapping(RightState);

    // 遍历反向映射，从 Recorder 加载到 Controller
    for (const auto& Pair : RightReverseMapping) {
        const FString& RecorderName = Pair.Key;
        const FString& ControllerName = Pair.Value;

        const FTransform* FoundTransform = StateData.Find(RecorderName);
        if (FoundTransform) {
            FRigElementKey ElementKey(*ControllerName,
                                      ERigElementType::Control);
            if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
                ControlRig->GetHierarchy()->SetGlobalTransform(
                    ElementKey, *FoundTransform, true, true, true);
                LoadedCount++;
                UE_LOG(LogTemp, Verbose, TEXT("Loaded right hand: %s <- %s"),
                       *ControllerName, *RecorderName);
            } else {
                FailedCount++;
                UE_LOG(LogTemp, Warning, TEXT("Controller not found: %s"),
                       *ControllerName);
            }
        } else {
            FailedCount++;
            UE_LOG(LogTemp, Warning,
                   TEXT("Recorder data not found in StateData: %s"),
                   *RecorderName);
        }
    }

    // === 加载辅助线 ===
    bool bGuidelinesLoaded = LoadGuidelinesState(FretDanceActor, StateData);

    UE_LOG(LogTemp, Warning,
           TEXT("✅ LoadState completed. Loaded: %d, Failed: %d"), LoadedCount,
           FailedCount);
    return LoadedCount > FailedCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 辅助线状态保存和加载
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UFretDanceControlRigProcessor::SaveGuidelinesState(
    AFretDanceUnreal* FretDanceActor, TMap<FString, FTransform>& OutStateData) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("SaveGuidelinesState"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("SaveGuidelinesState: Failed to get ControlRig"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Saving guidelines state..."));

    int32 SavedCount = 0;

    // 遍历所有辅助线，保存其 Transform
    for (const auto& GuidePair : FretDanceActor->GuideLines) {
        const FString& GuideName = GuidePair.Value;

        FRigElementKey ElementKey(*GuideName, ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
            FTransform Transform =
                ControlRig->GetHierarchy()->GetGlobalTransform(ElementKey);
            OutStateData.Add(GuideName, Transform);
            SavedCount++;
            UE_LOG(LogTemp, Verbose, TEXT("Saved guideline: %s"), *GuideName);
        } else {
            UE_LOG(LogTemp, Warning, TEXT("Guideline controller not found: %s"),
                   *GuideName);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ SaveGuidelinesState completed. Saved %d guidelines."),
           SavedCount);
    return SavedCount > 0;
}

bool UFretDanceControlRigProcessor::LoadGuidelinesState(
    AFretDanceUnreal* FretDanceActor,
    const TMap<FString, FTransform>& StateData) {
    if (!ValidateFretDanceActor(FretDanceActor, TEXT("LoadGuidelinesState"))) {
        return false;
    }

    UControlRig* ControlRig = GetControlRig(FretDanceActor);
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("LoadGuidelinesState: Failed to get ControlRig"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Loading guidelines state..."));

    int32 LoadedCount = 0;
    int32 FailedCount = 0;

    // 遍历所有辅助线，从 StateData 加载 Transform
    for (const auto& GuidePair : FretDanceActor->GuideLines) {
        const FString& GuideName = GuidePair.Value;

        const FTransform* FoundTransform = StateData.Find(GuideName);
        if (FoundTransform) {
            FRigElementKey ElementKey(*GuideName, ERigElementType::Control);
            if (ControlRig->GetHierarchy()->Contains(ElementKey)) {
                ControlRig->GetHierarchy()->SetGlobalTransform(
                    ElementKey, *FoundTransform, true, true, true);
                LoadedCount++;
                UE_LOG(LogTemp, Verbose, TEXT("Loaded guideline: %s"),
                       *GuideName);
            } else {
                FailedCount++;
                UE_LOG(LogTemp, Warning,
                       TEXT("Guideline controller not found: %s"), *GuideName);
            }
        } else {
            FailedCount++;
            UE_LOG(LogTemp, Warning,
                   TEXT("Guideline data not found in StateData: %s"),
                   *GuideName);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ LoadGuidelinesState completed. Loaded: %d, Failed: %d"),
           LoadedCount, FailedCount);
    return LoadedCount > FailedCount;
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
    ExpectedControllers.Add(TEXT("H_rotation_L"));

    // 右手控制器
    ExpectedControllers.Add(TEXT("H_R"));
    ExpectedControllers.Add(TEXT("HP_R"));
    ExpectedControllers.Add(TEXT("H_rotation_R"));

    FString ControllerRootName = TEXT("controller_root");

    // 右手手指控制器（根据乐器类型）
    TMap<FString, FString> RightFingerHierarchy =
        GetRightHandControllerHierarchy(FretDanceActor->InstrumentType,
                                        ControllerRootName);
    for (const auto& FingerPair : RightFingerHierarchy) {
        const FString& FingerName = FingerPair.Key;
        // 排除已包含的基础控制器
        if (!FingerName.IsEmpty() && FingerName != TEXT("H_R") &&
            FingerName != TEXT("HP_R") && FingerName != TEXT("H_rotation_R")) {
            ExpectedControllers.Add(FingerName);
        }
    }

    // 添加辅助线控制器
    for (const auto& GuidePair : FretDanceActor->GuideLines) {
        ExpectedControllers.Add(GuidePair.Value);
    }

    return ExpectedControllers;
}

TMap<FString, FString>
UFretDanceControlRigProcessor::GetRightHandControllerHierarchy(
    EFretDanceInstrumentType InstrumentType, FString ControllerRootName) {
    TMap<FString, FString> Hierarchy;

    // 所有乐器类型都包含基本的右手控制器
    Hierarchy.Add(TEXT("H_R"), ControllerRootName);   // 右手掌（根级）
    Hierarchy.Add(TEXT("HP_R"), ControllerRootName);  // 右手掌枢轴
    Hierarchy.Add(TEXT("T_R"), ControllerRootName);   // 右手拇指
    Hierarchy.Add(TEXT("TP_R"), ControllerRootName);  // 右手拇指枢轴

    // 电吉他的特殊层级结构
    if (InstrumentType == EFretDanceInstrumentType::ELECTRIC_GUITAR) {
        // 右手手指控制器
        Hierarchy.Add(TEXT("I_R"), TEXT("H_R"));  // 右手食指
        Hierarchy.Add(TEXT("M_R"), TEXT("H_R"));  // 右手中指
        Hierarchy.Add(TEXT("R_R"), TEXT("H_R"));  // 右手无名指
        Hierarchy.Add(TEXT("P_R"), TEXT("H_R"));  // 右手小指
    } else {
        // 其他乐器类型的层级结构（所有手指直接挂在 controller_root 下）
        Hierarchy.Add(TEXT("I_R"), ControllerRootName);  // 右手食指
        Hierarchy.Add(TEXT("M_R"), ControllerRootName);  // 右手中指
        Hierarchy.Add(TEXT("R_R"), ControllerRootName);  // 右手无名指
        Hierarchy.Add(TEXT("P_R"), ControllerRootName);  // 右手小指
    }

    return Hierarchy;
}

#undef LOCTEXT_NAMESPACE