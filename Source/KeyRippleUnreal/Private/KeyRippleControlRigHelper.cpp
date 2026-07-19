#include "KeyRippleControlRigHelper.h"

#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "InstrumentControlRigUtility.h"
#include "KeyRippleControlRigProcessor.h"
#include "Rigs/RigHierarchyController.h"

#define LOCTEXT_NAMESPACE "KeyRippleControlRigHelper"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FKeyRippleControlRigHelper 实现
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FKeyRippleControlRigHelper::ValidateKeyRippleActor(
    AKeyRippleUnreal* KeyRippleActor, const FString& FunctionName) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error, TEXT("%s: KeyRippleActor is null"),
               *FunctionName);
        return false;
    }
    return true;
}

void FKeyRippleControlRigHelper::LogStandardStart(
    const FString& OperationName) {
    UE_LOG(LogTemp, Warning, TEXT("========== %s Started =========="),
           *OperationName);
}

void FKeyRippleControlRigHelper::LogStandardEnd(const FString& OperationName,
                                                int32 SuccessCount,
                                                int32 FailCount,
                                                int32 TotalCount) {
    UE_LOG(LogTemp, Warning, TEXT("========== %s Summary =========="),
           *OperationName);
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed: %d items"),
           SuccessCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed to process: %d items"), FailCount);
    UE_LOG(LogTemp, Warning, TEXT("Total items: %d"), TotalCount);
    UE_LOG(LogTemp, Warning, TEXT("========== %s Completed =========="),
           *OperationName);
}

TSet<FString> FKeyRippleControlRigHelper::GetAllControllerNames(
    AKeyRippleUnreal* KeyRippleActor) {
    TSet<FString> AllControllerNames;

    const TMap<FString, FString>* ControllerMaps[] = {
        &KeyRippleActor->FingerControllers, &KeyRippleActor->HandControllers,
        &KeyRippleActor->KeyBoardPositions, &KeyRippleActor->TargetPoints,
        &KeyRippleActor->PolePoints};

    for (const auto* ControllerMap : ControllerMaps) {
        for (const auto& Pair : *ControllerMap) {
            AllControllerNames.Add(Pair.Value);
        }
    }

    return AllControllerNames;
}

TArray<FString> FKeyRippleControlRigHelper::GenerateStateDependentRecorders(
    AKeyRippleUnreal* KeyRippleActor, const FString& ControllerName) {
    TArray<FString> Result;

    for (EPositionType PositionType :
         {EPositionType::HIGH, EPositionType::LOW, EPositionType::MIDDLE}) {
        for (EKeyType KeyType : {EKeyType::WHITE, EKeyType::BLACK}) {
            FString PositionStr =
                KeyRippleActor->GetPositionTypeString(PositionType);
            FString KeyStr = KeyRippleActor->GetKeyTypeString(KeyType);

            FString RecorderName = FString::Printf(
                TEXT("%s_%s_%s"), *PositionStr, *KeyStr, *ControllerName);
            Result.Add(RecorderName);
        }
    }

    return Result;
}

void FKeyRippleControlRigHelper::InitializeControllerRecorderItem(
    AKeyRippleUnreal* KeyRippleActor, const FString& RecorderName) {
    FRecorderTransform DefaultTransform;
    DefaultTransform.Location = FVector::ZeroVector;
    DefaultTransform.Rotation = FQuat::Identity;

    KeyRippleActor->RecorderTransforms.Add(RecorderName, DefaultTransform);
}

void FKeyRippleControlRigHelper::AddControllerRecordersToTransforms(
    AKeyRippleUnreal* KeyRippleActor, const TMap<FString, FString>& Controllers,
    bool bIsStateDependent) {
    for (const auto& ControllerPair : Controllers) {
        FString ControllerName = ControllerPair.Value;

        if (bIsStateDependent) {
            TArray<FString> StateRecorders =
                GenerateStateDependentRecorders(KeyRippleActor, ControllerName);

            for (const FString& RecorderName : StateRecorders) {
                InitializeControllerRecorderItem(KeyRippleActor, RecorderName);
            }
        } else {
            InitializeControllerRecorderItem(KeyRippleActor, ControllerName);
        }
    }
}

void FKeyRippleControlRigHelper::InitializeRecorderTransforms(
    AKeyRippleUnreal* KeyRippleActor) {
    KeyRippleActor->RecorderTransforms.Empty();

    AddControllerRecordersToTransforms(KeyRippleActor,
                                       KeyRippleActor->FingerControllers, true);

    AddControllerRecordersToTransforms(KeyRippleActor,
                                       KeyRippleActor->HandControllers, true);

    AddControllerRecordersToTransforms(KeyRippleActor,
                                       KeyRippleActor->TargetPoints, true);

    AddControllerRecordersToTransforms(
        KeyRippleActor, KeyRippleActor->KeyBoardPositions, false);
}

void FKeyRippleControlRigHelper::SaveControllerTransform(
    AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
    const FString& ControlName, const FString& RecorderName, int32& SavedCount,
    int32& FailedCount) {
    UE_LOG(LogTemp, Warning,
           TEXT("SaveControllerTransform: Control='%s' -> Recorder='%s'"),
           *ControlName, *RecorderName);

    FTransform CurrentTransform;
    if (FInstrumentControlRigUtility::GetControlLocalTransform(
            RigHierarchy, ControlName, CurrentTransform)) {
        FRecorderTransform RecorderTransform;
        RecorderTransform.FromTransform(CurrentTransform);

        KeyRippleActor->RecorderTransforms.Add(RecorderName, RecorderTransform);

        UE_LOG(
            LogTemp, Warning,
            TEXT("  SAVED: '%s' at Pos(%.2f,%.2f,%.2f) "
                 "Rot(%.2f,%.2f,%.2f,%.2f)"),
            *RecorderName, CurrentTransform.GetLocation().X,
            CurrentTransform.GetLocation().Y, CurrentTransform.GetLocation().Z,
            CurrentTransform.GetRotation().W, CurrentTransform.GetRotation().X,
            CurrentTransform.GetRotation().Y, CurrentTransform.GetRotation().Z);

        SavedCount++;
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("  ✗ Failed to get ControlElement for: %s"), *ControlName);
        FailedCount++;
    }
}

void FKeyRippleControlRigHelper::LoadControllerTransform(
    AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
    const FString& ControlName, const FString& ExpectedRecorderName,
    int32& LoadedCount, int32& FailedCount) {
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

    if (FInstrumentControlRigUtility::SetControlLocalTransform(
            RigHierarchy, ControlName, FoundTransform->ToTransform())) {
        UE_LOG(LogTemp, Warning,
               TEXT("LOADED: Applied transform to control '%s'"), *ControlName);

        LoadedCount++;
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Failed to get ControlElement for: %s"),
               *ControlName);
        FailedCount++;
    }
}

void FKeyRippleControlRigHelper::SaveControllers(
    AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
    const TMap<FString, FString>& Controllers, int32& SavedCount,
    int32& FailedCount, bool bIsFingerControl, bool isStateDependent) {
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

void FKeyRippleControlRigHelper::LoadControllers(
    AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
    const TMap<FString, FString>& Controllers, int32& LoadedCount,
    int32& FailedCount, bool bIsFingerControl, bool isStateDependent) {
    for (const auto& ControllerPair : Controllers) {
        FString ControlName = ControllerPair.Value;

        FString ExpectedRecorderName =
            isStateDependent
                ? UKeyRippleControlRigProcessor::GetRecorderNameForControl(
                      KeyRippleActor, ControlName, bIsFingerControl)
                : ControlName;

        LoadControllerTransform(KeyRippleActor, RigHierarchy, ControlName,
                                ExpectedRecorderName, LoadedCount, FailedCount);
    }
}

void FKeyRippleControlRigHelper::CleanupDuplicateControls(
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
            UE_LOG(LogTemp, VeryVerbose,
                   TEXT("Skipping non-expected control '%s' during cleanup"),
                   *ControlName);
        }
    }

    for (const auto& GroupPair : ControlGroups) {
        const FString& ControlName = GroupPair.Key;
        const TArray<FRigElementKey>& ControlInstances = GroupPair.Value;

        if (ControlInstances.Num() > 1) {
            UE_LOG(LogTemp, Warning,
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

#undef LOCTEXT_NAMESPACE
