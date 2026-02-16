#include "KeyRippleControlRigProcessor.h"

#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"  // 包含FJsonObject
#include "Dom/JsonValue.h"   // 包含FJsonValue
#include "ISequencer.h"
#include "InstrumentControlRigUtility.h"
#include "KeyRipplePianoProcessor.h"
#include "LevelEditor.h"
#include "LevelEditorSequencerIntegration.h"
#include "Rigs/RigHierarchyController.h"
#include "Serialization/JsonReader.h"      // 包含TJsonReader
#include "Serialization/JsonSerializer.h"  // 包含FJsonSerializer
#include "Serialization/JsonWriter.h"      // 包含TJsonWriter

#define LOCTEXT_NAMESPACE "KeyRippleControlRigProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Local helper structure - contains all static helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct FKeyRippleControlRigHelpers {
    // ========================================
    // Control existence and validation helpers
    // ========================================

    static bool StrictControlExistenceCheck(URigHierarchy* RigHierarchy,
                                            const FString& ControllerName) {
        if (!RigHierarchy) {
            return false;
        }

        FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);

        if (!RigHierarchy->Contains(ElementKey)) {
            return false;
        }

        FRigControlElement* ControlElement =
            RigHierarchy->Find<FRigControlElement>(ElementKey);
        if (!ControlElement) {
            UE_LOG(
                LogTemp, Warning,
                TEXT("Control '%s' exists in hierarchy but element is null - "
                     "considering as non-existent"),
                *ControllerName);
            return false;
        }

        if (ControlElement->Settings.ControlType == ERigControlType::Bool &&
            ControllerName != TEXT("controller_root")) {
            UE_LOG(
                LogTemp, Warning,
                TEXT(
                    "Control '%s' has unexpected Bool type - may be corrupted"),
                *ControllerName);
            return false;
        }

        return true;
    }

    static bool GetControlRigInstanceAndBlueprint(
        AKeyRippleUnreal* KeyRippleActor, UControlRig*& OutControlRigInstance,
        UControlRigBlueprint*& OutControlRigBlueprint) {
        return UKeyRippleControlRigProcessor::
            GetControlRigFromSkeletalMeshActor(
                KeyRippleActor->SkeletalMeshActor, OutControlRigInstance,
                OutControlRigBlueprint);
    }

    // ========================================
    // Utility and validation helpers
    // ========================================

    static bool ValidateKeyRippleActor(AKeyRippleUnreal* KeyRippleActor,
                                       const FString& FunctionName) {
        if (!KeyRippleActor) {
            UE_LOG(LogTemp, Error, TEXT("%s: KeyRippleActor is null"),
                   *FunctionName);
            return false;
        }
        return true;
    }

    static void LogStandardStart(const FString& OperationName) {
        UE_LOG(LogTemp, Warning, TEXT("========== %s Started =========="),
               *OperationName);
    }

    static void LogStandardEnd(const FString& OperationName, int32 SuccessCount,
                               int32 FailCount, int32 TotalCount) {
        UE_LOG(LogTemp, Warning, TEXT("========== %s Summary =========="),
               *OperationName);
        UE_LOG(LogTemp, Warning, TEXT("Successfully processed: %d items"),
               SuccessCount);
        UE_LOG(LogTemp, Warning, TEXT("Failed to process: %d items"),
               FailCount);
        UE_LOG(LogTemp, Warning, TEXT("Total items: %d"), TotalCount);
        UE_LOG(LogTemp, Warning, TEXT("========== %s Completed =========="),
               *OperationName);
    }

    // ========================================
    // Enum to string conversion helpers
    // ========================================

    // NOTE: These methods are now called from AKeyRippleUnreal public methods
    // instead of being duplicated here. See GetRecorderNameForControl() and
    // GenerateStateDependentRecorders() for usage examples.

    // ========================================
    // Controller name collection
    // ========================================

    static TSet<FString> GetAllControllerNames(
        AKeyRippleUnreal* KeyRippleActor) {
        TSet<FString> AllControllerNames;

        const TMap<FString, FString>* ControllerMaps[] = {
            &KeyRippleActor->FingerControllers,
            &KeyRippleActor->HandControllers,
            &KeyRippleActor->KeyBoardPositions,
            &KeyRippleActor->Guidelines,
            &KeyRippleActor->TargetPoints,
            &KeyRippleActor->ShoulderControllers,
            &KeyRippleActor->PolePoints};

        for (const auto* ControllerMap : ControllerMaps) {
            for (const auto& Pair : *ControllerMap) {
                AllControllerNames.Add(Pair.Value);
            }
        }

        return AllControllerNames;
    }

    // ========================================
    // State-dependent recorder generation
    // ========================================

    static TArray<FString> GenerateStateDependentRecorders(
        AKeyRippleUnreal* KeyRippleActor, const FString& ControllerName) {
        TArray<FString> Result;

        for (EPositionType PositionType :
             {EPositionType::HIGH, EPositionType::LOW, EPositionType::MIDDLE}) {
            for (EKeyType KeyType : {EKeyType::WHITE, EKeyType::BLACK}) {
                FString PositionStr = KeyRippleActor->GetPositionTypeString(PositionType);
                FString KeyStr = KeyRippleActor->GetKeyTypeString(KeyType);

                FString RecorderName = FString::Printf(
                    TEXT("%s_%s_%s"), *PositionStr, *KeyStr, *ControllerName);
                Result.Add(RecorderName);
            }
        }

        return Result;
    }

    // ========================================
    // Recorder initialization
    // ========================================

    static void InitializeControllerRecorderItem(
        AKeyRippleUnreal* KeyRippleActor, const FString& RecorderName) {
        FRecorderTransform DefaultTransform;
        DefaultTransform.Location = FVector::ZeroVector;
        DefaultTransform.Rotation = FQuat::Identity;

        KeyRippleActor->RecorderTransforms.Add(RecorderName, DefaultTransform);
    }

    static void AddControllerRecordersToTransforms(
        AKeyRippleUnreal* KeyRippleActor,
        const TMap<FString, FString>& Controllers, bool bIsStateDependent) {
        for (const auto& ControllerPair : Controllers) {
            FString ControllerName = ControllerPair.Value;

            if (bIsStateDependent) {
                TArray<FString> StateRecorders =
                    GenerateStateDependentRecorders(KeyRippleActor,
                                                    ControllerName);

                for (const FString& RecorderName : StateRecorders) {
                    InitializeControllerRecorderItem(KeyRippleActor,
                                                     RecorderName);
                }
            } else {
                InitializeControllerRecorderItem(KeyRippleActor,
                                                 ControllerName);
            }
        }
    }

    static void InitializeRecorderTransforms(AKeyRippleUnreal* KeyRippleActor) {
        KeyRippleActor->RecorderTransforms.Empty();

        AddControllerRecordersToTransforms(
            KeyRippleActor, KeyRippleActor->FingerControllers, true);

        AddControllerRecordersToTransforms(
            KeyRippleActor, KeyRippleActor->HandControllers, true);

        AddControllerRecordersToTransforms(
            KeyRippleActor, KeyRippleActor->ShoulderControllers, true);

        AddControllerRecordersToTransforms(KeyRippleActor,
                                           KeyRippleActor->TargetPoints, true);

        AddControllerRecordersToTransforms(
            KeyRippleActor, KeyRippleActor->KeyBoardPositions, false);

        AddControllerRecordersToTransforms(KeyRippleActor,
                                           KeyRippleActor->Guidelines, false);
    }

    // ========================================
    // Save/Load transform helpers
    // ========================================

    static void SaveControllerTransform(AKeyRippleUnreal* KeyRippleActor,
                                        URigHierarchy* RigHierarchy,
                                        const FString& ControlName,
                                        const FString& RecorderName,
                                        int32& SavedCount, int32& FailedCount) {
        UE_LOG(LogTemp, Warning,
               TEXT("SaveControllerTransform: Control='%s' -> Recorder='%s'"),
               *ControlName, *RecorderName);

        FRigElementKey ControlKey(*ControlName, ERigElementType::Control);
        if (RigHierarchy->Contains(ControlKey)) {
            FRigControlElement* ControlElement =
                RigHierarchy->Find<FRigControlElement>(ControlKey);
            if (ControlElement) {
                FRigControlValue CurrentValue = RigHierarchy->GetControlValue(
                    ControlElement, ERigControlValueType::Current);
                FTransform CurrentTransform = CurrentValue.GetAsTransform(
                    ControlElement->Settings.ControlType,
                    ControlElement->Settings.PrimaryAxis);

                FRecorderTransform RecorderTransform;
                RecorderTransform.FromTransform(CurrentTransform);

                KeyRippleActor->RecorderTransforms.Add(RecorderName,
                                                       RecorderTransform);

                UE_LOG(LogTemp, Warning,
                       TEXT("  SAVED: '%s' at Pos(%.2f,%.2f,%.2f) "
                            "Rot(%.2f,%.2f,%.2f,%.2f)"),
                       *RecorderName, CurrentTransform.GetLocation().X,
                       CurrentTransform.GetLocation().Y,
                       CurrentTransform.GetLocation().Z,
                       CurrentTransform.GetRotation().W,
                       CurrentTransform.GetRotation().X,
                       CurrentTransform.GetRotation().Y,
                       CurrentTransform.GetRotation().Z);

                SavedCount++;
            } else {
                UE_LOG(LogTemp, Warning,
                       TEXT("  ✗ Failed to get ControlElement for: %s"),
                       *ControlName);
                FailedCount++;
            }
        } else {
            UE_LOG(LogTemp, Warning, TEXT("  ✗ Control not found: %s"),
                   *ControlName);
            FailedCount++;
        }
    }

    static void LoadControllerTransform(AKeyRippleUnreal* KeyRippleActor,
                                        URigHierarchy* RigHierarchy,
                                        const FString& ControlName,
                                        const FString& ExpectedRecorderName,
                                        int32& LoadedCount,
                                        int32& FailedCount) {
        UE_LOG(LogTemp, Warning,
               TEXT("LoadControllerTransform: Control='%s' <- Expected "
                    "Recorder='%s'"),
               *ControlName, *ExpectedRecorderName);

        const FRecorderTransform* FoundTransform =
            KeyRippleActor->RecorderTransforms.Find(ExpectedRecorderName);

        if (!FoundTransform) {
            UE_LOG(LogTemp, Warning,
                   TEXT("MISSING: Expected recorder not in data table: %s"),
                   *ExpectedRecorderName);

            FailedCount++;
            return;
        }

        FTransform LoadTransform = FoundTransform->ToTransform();
        UE_LOG(LogTemp, Warning,
               TEXT("FOUND: '%s' with Pos(%.2f,%.2f,%.2f) "
                    "Rot(%.2f,%.2f,%.2f,%.2f)"),
               *ExpectedRecorderName, LoadTransform.GetLocation().X,
               LoadTransform.GetLocation().Y, LoadTransform.GetLocation().Z,
               LoadTransform.GetRotation().W, LoadTransform.GetRotation().X,
               LoadTransform.GetRotation().Y, LoadTransform.GetRotation().Z);

        FRigElementKey ControlKey(*ControlName, ERigElementType::Control);
        if (RigHierarchy->Contains(ControlKey)) {
            FRigControlElement* ControlElement =
                RigHierarchy->Find<FRigControlElement>(ControlKey);
            if (ControlElement) {
                FTransform NewTransform = FoundTransform->ToTransform();

                FRigControlValue NewValue;
                NewValue.SetFromTransform(NewTransform,
                                          ControlElement->Settings.ControlType,
                                          ControlElement->Settings.PrimaryAxis);

                RigHierarchy->SetControlValue(ControlElement, NewValue,
                                              ERigControlValueType::Current);

                UE_LOG(LogTemp, Warning,
                       TEXT("LOADED: Applied transform to control '%s'"),
                       *ControlName);

                LoadedCount++;
            } else {
                UE_LOG(LogTemp, Warning,
                       TEXT("Failed to get ControlElement for: %s"),
                       *ControlName);
                FailedCount++;
            }
        } else {
            UE_LOG(LogTemp, Warning, TEXT("Control not found: %s"),
                   *ControlName);
            FailedCount++;
        }
    }

    static void SaveControllers(AKeyRippleUnreal* KeyRippleActor,
                                URigHierarchy* RigHierarchy,
                                const TMap<FString, FString>& Controllers,
                                int32& SavedCount, int32& FailedCount,
                                bool bIsFingerControl = true,
                                bool isStateDependent = true) {
        for (const auto& ControllerPair : Controllers) {
            FString ControlName = ControllerPair.Value;
            FString RecorderName =
                isStateDependent
                    ? UKeyRippleControlRigProcessor::GetRecorderNameForControl(
                          KeyRippleActor, ControlName, bIsFingerControl)
                    : ControlName;

            SaveControllerTransform(KeyRippleActor, RigHierarchy, ControlName,
                                    RecorderName, SavedCount, FailedCount);
        }
    }

    static void LoadControllers(AKeyRippleUnreal* KeyRippleActor,
                                URigHierarchy* RigHierarchy,
                                const TMap<FString, FString>& Controllers,
                                int32& LoadedCount, int32& FailedCount,
                                bool bIsFingerControl = true,
                                bool isStateDependent = true) {
        for (const auto& ControllerPair : Controllers) {
            FString ControlName = ControllerPair.Value;

            FString ExpectedRecorderName =
                isStateDependent
                    ? UKeyRippleControlRigProcessor::GetRecorderNameForControl(
                          KeyRippleActor, ControlName, bIsFingerControl)
                    : ControlName;

            LoadControllerTransform(KeyRippleActor, RigHierarchy, ControlName,
                                    ExpectedRecorderName, LoadedCount,
                                    FailedCount);
        }
    }

    // ========================================
    // Duplicate control cleanup
    // ========================================

    static void CleanupDuplicateControls(
        AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
        const TSet<FString>& ExpectedControllerNames) {
        if (!RigHierarchy) {
            return;
        }

        URigHierarchyController* HierarchyController =
            RigHierarchy->GetController();
        if (!HierarchyController) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Cannot get HierarchyController for cleanup"));
            return;
        }

        UE_LOG(LogTemp, Warning,
               TEXT("Starting cleanup of duplicate/corrupted controls..."));

        TArray<FRigElementKey> ExistingControlKeys =
            RigHierarchy->GetAllKeys(false);
        TArray<FRigElementKey> FilteredControlKeys;

        for (const FRigElementKey& Key : ExistingControlKeys) {
            if (Key.Type == ERigElementType::Control) {
                FilteredControlKeys.Add(Key);
            }
        }

        int32 DuplicatesFound = 0;
        TMap<FString, TArray<FRigElementKey>> ControlGroups;

        for (const FRigElementKey& ControlKey : FilteredControlKeys) {
            FString ControlName = ControlKey.Name.ToString();

            if (ExpectedControllerNames.Contains(ControlName) ||
                ControlName == TEXT("controller_root")) {
                ControlGroups.FindOrAdd(ControlName).Add(ControlKey);
            } else {
                UE_LOG(
                    LogTemp, VeryVerbose,
                    TEXT("Skipping non-expected control '%s' during cleanup"),
                    *ControlName);
            }
        }

        for (const auto& GroupPair : ControlGroups) {
            const FString& ControlName = GroupPair.Key;
            const TArray<FRigElementKey>& ControlInstances = GroupPair.Value;

            if (ControlInstances.Num() > 1) {
                UE_LOG(
                    LogTemp, Warning,
                    TEXT("  🔍 Found %d instances of control '%s' - removing "
                         "duplicates"),
                    ControlInstances.Num(), *ControlName);

                for (int32 i = 1; i < ControlInstances.Num(); i++) {
                    bool bRemoved = HierarchyController->RemoveElement(
                        ControlInstances[i], true, false);
                    if (bRemoved) {
                        UE_LOG(LogTemp, Warning,
                               TEXT("    ✅ Removed duplicate control '%s' "
                                    "instance %d"),
                               *ControlName, i + 1);
                        DuplicatesFound++;
                    } else {
                        UE_LOG(LogTemp, Warning,
                               TEXT("    ❌ Failed to remove duplicate control "
                                    "'%s' instance %d"),
                               *ControlName, i + 1);
                    }
                }
            }
        }

        if (DuplicatesFound > 0) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Cleanup completed: Removed %d duplicate control "
                        "instances"),
                   DuplicatesFound);
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("Cleanup completed: No duplicates found"));
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UKeyRippleControlRigProcessor implementations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UKeyRippleControlRigProcessor::GetControlRigFromSkeletalMeshActor(
    ASkeletalMeshActor* SkeletalMeshActor, UControlRig*& OutControlRigInstance,
    UControlRigBlueprint*& OutControlRigBlueprint) {
    return FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
        SkeletalMeshActor, OutControlRigInstance, OutControlRigBlueprint);
}

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
    if (!FKeyRippleControlRigHelpers::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("CheckObjectsStatus"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FKeyRippleControlRigHelpers::GetControlRigInstanceAndBlueprint(
            KeyRippleActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    TSet<FString> ExpectedObjects =
        FKeyRippleControlRigHelpers::GetAllControllerNames(KeyRippleActor);

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
    if (!FKeyRippleControlRigHelpers::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("SetupAllObjects"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FKeyRippleControlRigHelpers::GetControlRigInstanceAndBlueprint(
            KeyRippleActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    SetupControllers(KeyRippleActor);
    FKeyRippleControlRigHelpers::InitializeRecorderTransforms(KeyRippleActor);

    UE_LOG(LogTemp, Warning, TEXT("All KeyRipple objects have been set up"));
}

void UKeyRippleControlRigProcessor::SaveState(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!FKeyRippleControlRigHelpers::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("SaveState"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FKeyRippleControlRigHelpers::GetControlRigInstanceAndBlueprint(
            KeyRippleActor, ControlRigInstance, ControlRigBlueprint)) {
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

    FKeyRippleControlRigHelpers::LogStandardStart(TEXT("SaveState"));

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

    FKeyRippleControlRigHelpers::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->FingerControllers,
        SavedCount, FailedCount, true, true);

    FKeyRippleControlRigHelpers::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->HandControllers,
        SavedCount, FailedCount, false, true);

    FKeyRippleControlRigHelpers::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->ShoulderControllers,
        SavedCount, FailedCount, false, true);

    FKeyRippleControlRigHelpers::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->TargetPoints, SavedCount,
        FailedCount, false, true);

    UE_LOG(LogTemp, Warning,
           TEXT("Processing state-independent controllers..."));

    FKeyRippleControlRigHelpers::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->KeyBoardPositions,
        SavedCount, FailedCount, false, false);

    FKeyRippleControlRigHelpers::SaveControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->Guidelines, SavedCount,
        FailedCount, false, false);

    FKeyRippleControlRigHelpers::LogStandardEnd(
        TEXT("SaveState"), SavedCount, FailedCount,
        KeyRippleActor->RecorderTransforms.Num());

    if (KeyRippleActor) {
        KeyRippleActor->MarkPackageDirty();
    }
}

void UKeyRippleControlRigProcessor::LoadState(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!FKeyRippleControlRigHelpers::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("LoadState"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FKeyRippleControlRigHelpers::GetControlRigInstanceAndBlueprint(
            KeyRippleActor, ControlRigInstance, ControlRigBlueprint)) {
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

    FKeyRippleControlRigHelpers::LogStandardStart(TEXT("LoadState"));

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

    FKeyRippleControlRigHelpers::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->FingerControllers,
        LoadedCount, FailedCount, true, true);

    FKeyRippleControlRigHelpers::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->HandControllers,
        LoadedCount, FailedCount, false, true);

    FKeyRippleControlRigHelpers::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->ShoulderControllers,
        LoadedCount, FailedCount, false, true);

    FKeyRippleControlRigHelpers::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->TargetPoints, LoadedCount,
        FailedCount, false, true);

    UE_LOG(LogTemp, Warning, TEXT("Loading state-independent controllers..."));

    FKeyRippleControlRigHelpers::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->KeyBoardPositions,
        LoadedCount, FailedCount, false, false);

    FKeyRippleControlRigHelpers::LoadControllers(
        KeyRippleActor, RigHierarchy, KeyRippleActor->Guidelines, LoadedCount,
        FailedCount, false, false);

    FKeyRippleControlRigHelpers::LogStandardEnd(
        TEXT("LoadState"), LoadedCount, FailedCount,
        KeyRippleActor->RecorderTransforms.Num());
}

void UKeyRippleControlRigProcessor::SetupControllers(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!FKeyRippleControlRigHelpers::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("SetupControllers"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FKeyRippleControlRigHelpers::GetControlRigInstanceAndBlueprint(
            KeyRippleActor, ControlRigInstance, ControlRigBlueprint)) {
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
        FKeyRippleControlRigHelpers::GetAllControllerNames(KeyRippleActor);
    FKeyRippleControlRigHelpers::CleanupDuplicateControls(
        KeyRippleActor, RigHierarchy, AllControllerNames);

    // 获取层级控制器
    URigHierarchyController* HierarchyController =
        RigHierarchy->GetController();
    if (!HierarchyController) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get hierarchy controller"));
        return;
    }

    // 第1步：创建 base_root（最上层的根控制器）
    if (!FControlRigCreationUtility::CreateRootController(
            HierarchyController, RigHierarchy, TEXT("base_root"),
            TEXT("Cube"))) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create base_root"));
        return;
    }

    // 第2步：创建 controller_root（乐器演奏层级的根控制器，父级为base_root）
    if (!FControlRigCreationUtility::CreateInstrumentRootController(
            HierarchyController, RigHierarchy, TEXT("controller_root"),
            TEXT("base_root"), TEXT("Cube"))) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create controller_root"));
        return;
    }

    // 第3步：遍历所有其他控制器，创建父级为controller_root的控制器
    FRigElementKey RootControllerKey(TEXT("controller_root"),
                                     ERigElementType::Control);

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
        // 🔧 FIX: 使用更严格的存在性检查
        bool bExists = FKeyRippleControlRigHelpers::StrictControlExistenceCheck(
            RigHierarchy, ControllerName);

        if (!bExists) {
            // 如果不存在，则创建控制器
            UE_LOG(LogTemp, Warning,
                   TEXT("Controller %s does not exist, creating as child of "
                        "controller_root..."),
                   *ControllerName);

            // 确定父控制器并创建控制器
            FString ParentControllerName = TEXT("controller_root");

            // 检查是否为 pole 控制器
            if (ControllerName.StartsWith(TEXT("pole_"))) {
                // 这是一个 pole 控制器，需要找到对应的手指控制器作为父级
                FString PoleFingerNumber =
                    ControllerName.Mid(5);  // 去掉 "pole_" 前缀

                // 查找对应的手指控制器
                for (const auto& FingerPair :
                     KeyRippleActor->FingerControllers) {
                    if (FingerPair.Key == PoleFingerNumber) {
                        ParentControllerName =
                            FingerPair.Value;  // 例如 "0_L" 或 "5_R"
                        UE_LOG(LogTemp, Warning,
                               TEXT("Found finger controller %s as parent for "
                                    "pole %s"),
                               *ParentControllerName, *ControllerName);
                        break;
                    }
                }
            }

            CreateController(KeyRippleActor, ControllerName,
                             ParentControllerName);
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
    if (!FKeyRippleControlRigHelpers::ValidateKeyRippleActor(
            KeyRippleActor, TEXT("CreateController"))) {
        return nullptr;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FKeyRippleControlRigHelpers::GetControlRigInstanceAndBlueprint(
            KeyRippleActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return nullptr;
    }

    // 获取层级结构和控制器
    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigBlueprint"));
        return nullptr;
    }

    URigHierarchyController* HierarchyController =
        RigHierarchy->GetController();
    if (!HierarchyController) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get hierarchy controller"));
        return nullptr;
    }

    if (FKeyRippleControlRigHelpers::StrictControlExistenceCheck(
            RigHierarchy, ControllerName)) {
        UE_LOG(LogTemp, Warning,
               TEXT("✅ Controller %s already exists (verified)"),
               *ControllerName);
        return nullptr;  // 如果已存在，返回nullptr，因为不需要创建
    }

    // 确定父控制器键
    FRigElementKey ParentKey;
    if (!ParentControllerName.IsEmpty()) {
        if (FKeyRippleControlRigHelpers::StrictControlExistenceCheck(
                RigHierarchy, ParentControllerName)) {
            FRigElementKey PotentialParentKey(*ParentControllerName,
                                              ERigElementType::Control);
            ParentKey = PotentialParentKey;
            UE_LOG(LogTemp, Warning,
                   TEXT("✅ Using verified parent controller '%s' for '%s'"),
                   *ParentControllerName, *ControllerName);
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("⚠️ Parent controller '%s' does not exist or is "
                        "corrupted, creating child "
                        "controller '%s' without parent"),
                   *ParentControllerName, *ControllerName);
        }
    }

    // 确定控制器类型和形状
    FRigControlSettings ControlSettings;
    ControlSettings.ControlType = ERigControlType::Transform;
    ControlSettings.DisplayName = FName(*ControllerName);

    // 根据控制器名称确定形状
    if (ControllerName.Contains(TEXT("hand"), ESearchCase::IgnoreCase) &&
        !ControllerName.Contains(TEXT("rotation"), ESearchCase::IgnoreCase)) {
        // HandControllers且不包含rotation的使用立方体形状
        ControlSettings.ShapeName = FName(TEXT("Cube"));
    } else if (ControllerName.Contains(TEXT("rotation"),
                                       ESearchCase::IgnoreCase)) {
        // 包含rotation的使用圆形形状
        ControlSettings.ShapeName = FName(TEXT("Circle"));
    } else if (ControllerName.StartsWith(TEXT("pole_"))) {
        // Pole控制器使用钻石形状
        ControlSettings.ShapeName = FName(TEXT("Diamond"));
    } else {
        // 其他控制器使用小球形
        ControlSettings.ShapeName = FName(TEXT("Sphere"));
    }

    FTransform InitialTransform = FTransform::Identity;
    FRigControlValue InitialValue;
    InitialValue.SetFromTransform(InitialTransform, ERigControlType::Transform,
                                  ERigControlAxis::X);

    // 添加控制
    FRigElementKey NewControlKey = HierarchyController->AddControl(
        FName(*ControllerName), ParentKey, ControlSettings, InitialValue,
        FTransform::Identity,  // Offset transform
        FTransform::Identity,  // Shape transform
        true,                  // bSetupUndo
        false                  // bPrintPythonCommand
    );

    if (NewControlKey.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("✅ Successfully created controller: %s"),
               *ControllerName);

        // 🔧 FIX: 创建后验证Control确实存在且正确
        if (!FKeyRippleControlRigHelpers::StrictControlExistenceCheck(
                RigHierarchy, ControllerName)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("⚠️ Created controller '%s' but verification failed - "
                        "may need manual check"),
                   *ControllerName);
        }

        return nullptr;  // 在Control Rig中创建的控制器不需要返回AActor指针
    } else {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create controller: %s"),
               *ControllerName);
        return nullptr;
    }
}

void UKeyRippleControlRigProcessor::SetupTargetActorDriver(
    AKeyRippleUnreal* KeyRippleActor, AActor* TargetActor) {
    if (!FKeyRippleControlRigHelpers::ValidateKeyRippleActor(
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
    if (!FKeyRippleControlRigHelpers::ValidateKeyRippleActor(
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