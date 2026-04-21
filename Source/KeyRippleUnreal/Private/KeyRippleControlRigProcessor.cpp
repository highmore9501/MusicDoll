#include "KeyRippleControlRigProcessor.h"

#include "BoneControlMappingUtility.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
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
    UControlRig* ControlRigInstance = CacheSubsystem->GetControlRig(
        KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));

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

    UControlRig* ControlRigInstance = CacheSubsystem->GetControlRig(
        KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));

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

    FKeyRippleControlRigHelper::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->ShoulderControllers,
        SavedCount, FailedCount, false, true);

    FKeyRippleControlRigHelper::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->TargetPoints, SavedCount,
        FailedCount, false, true);

    UE_LOG(LogTemp, Warning,
           TEXT("Processing state-independent controllers..."));

    FKeyRippleControlRigHelper::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->KeyBoardPositions,
        SavedCount, FailedCount, false, false);

    FKeyRippleControlRigHelper::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->Guidelines, SavedCount,
        FailedCount, false, false);

    FKeyRippleControlRigHelper::LogStandardEnd(
        TEXT("SaveState"), SavedCount, FailedCount,
        KeyRippleActor->RecorderTransforms.Num());

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

    UControlRig* ControlRigInstance = CacheSubsystem->GetControlRig(
        KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));

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

    FKeyRippleControlRigHelper::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->ShoulderControllers,
        LoadedCount, FailedCount, false, true);

    FKeyRippleControlRigHelper::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->TargetPoints, LoadedCount,
        FailedCount, false, true);

    UE_LOG(LogTemp, Warning, TEXT("Loading state-independent controllers..."));

    FKeyRippleControlRigHelper::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->KeyBoardPositions,
        LoadedCount, FailedCount, false, false);

    FKeyRippleControlRigHelper::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->Guidelines, LoadedCount,
        FailedCount, false, false);

#if WITH_EDITOR
    // 通知 Sequencer 强制重新求値，使控制器位置在当前帧持久化
    {
        ULevelSequence* ActiveLevelSequence = nullptr;
        TSharedPtr<ISequencer> ActiveSequencer = nullptr;
        if (UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
                ActiveLevelSequence, ActiveSequencer)) {
            if (ActiveSequencer.IsValid()) {
                ActiveSequencer->ForceEvaluate();
            }
        }
    }
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif

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

    UControlRig* ControlRigInstance = CacheSubsystem->GetControlRig(
        KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));

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
    if (!FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, TEXT("controller_root"), TEXT("base_root"))) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create controller_root"));
        return;
    }

    // 第3步：遍历所有其他控制器，创建父级为controller_root的控制器
    // 将TSet转换为TArray并排序，确保pole控制器最后处理
    TArray<FString> SortedControllerNames = AllControllerNames.Array();
    SortedControllerNames.Sort([](const FString& A, const FString& B) {
        // pole控制器排在后面
        bool bAIsPole = A.StartsWith(TEXT("pole_"));
        bool bBIsPole = B.StartsWith(TEXT("pole_"));

        if (bAIsPole && !bBIsPole) return false;  // A是pole，B不是，A排后面
        if (!bAIsPole && bBIsPole) return true;   // A不是pole，B是，A排前面

        return A < B;  // 都是pole或都不是pole，按字典序排列
    });

    // 遍历所有其他控制器名称，检查是否存在，如果不存在则创建
    for (const FString& ControllerName : SortedControllerNames) {
        FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);
        bool bExists = RigHierarchy->Contains(ElementKey);

        if (!bExists) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Controller %s does not exist, creating as child of "
                        "controller_root..."),
                   *ControllerName);

            // 确定父控制器并创建控制器
            FString ParentControllerName = TEXT("controller_root");

            // 检查是否为 pole 控制器
            if (ControllerName.StartsWith(TEXT("pole_"))) {
                // 这是一个 pole
                // 控制器，需要找到对应的手指控制器作为参考，然后找到对应的手掌
                FString PoleFingerNumber =
                    ControllerName.Mid(5);  // 去掉 "pole_" 前缀

                // 查找对应的手指控制器
                for (const auto& FingerPair :
                     KeyRippleActor->FingerControllers) {
                    if (FingerPair.Key == PoleFingerNumber) {
                        FString FingerControllerName =
                            FingerPair.Value;  // 例如 "0_L" 或 "5_R"
                        // 根据手指控制器的后缀（_L或_R）确定对应的手掌
                        FString HandSuffix =
                            FingerControllerName.EndsWith(TEXT("_L"))
                                ? TEXT("_L")
                                : TEXT("_R");
                        ParentControllerName = FString::Printf(
                            TEXT("H%s"), *HandSuffix);  // 例如 "H_L" 或 "H_R"

                        UE_LOG(LogTemp, Warning,
                               TEXT("Found finger controller %s, setting hand "
                                    "controller %s as parent for pole %s"),
                               *FingerControllerName, *ParentControllerName,
                               *ControllerName);
                        break;
                    }
                }
            }

            FControlRigCreationUtility::CreateControl(
                ControlRigBlueprint, ControllerName, ParentControllerName);
        } else {
            UE_LOG(LogTemp, Warning, TEXT("✅ Controller %s already exists"),
                   *ControllerName);
        }
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

    UControlRig* ControlRigInstance = CacheSubsystem->GetControlRig(
        KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence, TEXT("controller_root"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return nullptr;
    }

    // 使用新的统一接口
    if (FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, ControllerName, ParentControllerName)) {
        UE_LOG(LogTemp, Warning,
               TEXT("✅ Successfully created controller %s with parent %s"),
               *ControllerName, *ParentControllerName);
        return nullptr;
    } else {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create controller %s"),
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