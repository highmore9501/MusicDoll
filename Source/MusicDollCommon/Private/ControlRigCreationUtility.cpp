#include "ControlRigCreationUtility.h"

#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"

bool FControlRigCreationUtility::GetHierarchyAndController(
    UControlRigBlueprint* ControlRigBlueprint, URigHierarchy*& OutRigHierarchy,
    URigHierarchyController*& OutHierarchyController) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("GetHierarchyAndController: ControlRigBlueprint is null"));
        return false;
    }

    OutRigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!OutRigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("GetHierarchyAndController: Failed to get RigHierarchy "
                    "from ControlRigBlueprint"));
        return false;
    }

    OutHierarchyController = OutRigHierarchy->GetController();
    if (!OutHierarchyController) {
        UE_LOG(LogTemp, Error,
               TEXT("GetHierarchyAndController: Failed to get "
                    "HierarchyController"));
        return false;
    }

    return true;
}

bool FControlRigCreationUtility::CreateControl(
    UControlRigBlueprint* ControlRigBlueprint, const FString& ControlName,
    const FString& ParentName) {
    if (!ControlRigBlueprint || ControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("CreateControl: Invalid parameters - ControlRigBlueprint: "
                    "%d, ControlName: %s"),
               ControlRigBlueprint != nullptr, *ControlName);
        return false;
    }

    URigHierarchy* RigHierarchy = nullptr;
    URigHierarchyController* HierarchyController = nullptr;

    if (!GetHierarchyAndController(ControlRigBlueprint, RigHierarchy,
                                   HierarchyController)) {
        return false;
    }

    // 严格检查 Control 是否已存在
    if (StrictControlExistenceCheck(RigHierarchy, ControlName)) {
        UE_LOG(LogTemp, Warning, TEXT("✓ Control '%s' already exists"),
               *ControlName);
        return true;
    }

    // 额外的预防措施 - 再次检查可能的损坏 Control
    FRigElementKey ExistingElementKey(*ControlName, ERigElementType::Control);
    if (RigHierarchy->Contains(ExistingElementKey)) {
        UE_LOG(LogTemp, Warning,
               TEXT("⚠️ Found potentially corrupted control '%s' - attempting "
                    "cleanup before creation"),
               *ControlName);

        if (HierarchyController->RemoveElement(ExistingElementKey, true,
                                               false)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("🧹 Successfully removed corrupted control '%s'"),
                   *ControlName);
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("❌ Failed to remove corrupted control '%s' - aborting "
                        "creation"),
                   *ControlName);
            return false;
        }
    }

    // 验证父 Control 如果指定的话
    FRigElementKey VerifiedParentKey;
    if (!ParentName.IsEmpty()) {
        if (!StrictControlExistenceCheck(RigHierarchy, ParentName)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("⚠️ Parent control '%s' does not exist or is corrupted "
                        "- creating '%s' without parent"),
                   *ParentName, *ControlName);
        } else {
            VerifiedParentKey =
                FRigElementKey(*ParentName, ERigElementType::Control);
        }
    }

    // 自动确定 ShapeName
    FString ShapeName = DetermineShapeName(ControlName);

    // 创建 Control
    bool bSuccess =
        CreateControlInternal(HierarchyController, RigHierarchy, ControlName,
                              VerifiedParentKey, ShapeName);

    // 标记脏数据
    if (bSuccess) {
        ControlRigBlueprint->MarkPackageDirty();
    }

    return bSuccess;
}

bool FControlRigCreationUtility::CreateControlInternal(
    URigHierarchyController* HierarchyController, URigHierarchy* RigHierarchy,
    const FString& ControlName, const FRigElementKey& ParentKey,
    const FString& ShapeName) {
    if (!HierarchyController || !RigHierarchy || ControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("CreateControlInternal: Invalid parameters"));
        return false;
    }

    // 创建 Control 设置
    FRigControlSettings ControlSettings;
    ControlSettings.ControlType = ERigControlType::Transform;
    ControlSettings.DisplayName = FName(*ControlName);
    ControlSettings.ShapeName = FName(*ShapeName);

    // 创建初始变换
    FTransform InitialTransform = FTransform::Identity;
    FRigControlValue InitialValue;
    InitialValue.SetFromTransform(InitialTransform, ERigControlType::Transform,
                                  ERigControlAxis::X);

    // 添加 Control
    FRigElementKey NewControlKey = HierarchyController->AddControl(
        FName(*ControlName), ParentKey, ControlSettings, InitialValue,
        FTransform::Identity, FTransform::Identity, true, false);

    if (NewControlKey.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("✅ Successfully created control: %s"),
               *ControlName);

        if (!StrictControlExistenceCheck(RigHierarchy, ControlName)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("⚠️ Created control '%s' but verification failed - may "
                        "need manual check"),
                   *ControlName);
        }

        return true;
    } else {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create control: %s"),
               *ControlName);
        return false;
    }
}

bool FControlRigCreationUtility::StrictControlExistenceCheck(
    URigHierarchy* RigHierarchy, const FString& ControllerName) {
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
        UE_LOG(LogTemp, Warning,
               TEXT("Control '%s' exists in hierarchy but element is null - "
                    "considering as non-existent"),
               *ControllerName);
        return false;
    }

    return true;
}

FString FControlRigCreationUtility::DetermineShapeName(
    const FString& ControlName) {
    if (ControlName.Contains(TEXT("hand"), ESearchCase::IgnoreCase) &&
        !ControlName.Contains(TEXT("rotation"), ESearchCase::IgnoreCase)) {
        return TEXT("Box_Thick");
    } else if (ControlName.Contains(TEXT("rotation"),
                                    ESearchCase::IgnoreCase)) {
        return TEXT("Circle_Thin");
    } else if (ControlName.Contains(TEXT("pole"), ESearchCase::IgnoreCase)) {
        return TEXT("QuarterCircle_Thin");
    } else if (ControlName.Contains(TEXT("line"), ESearchCase::IgnoreCase)) {
        return TEXT("Arrow_Solid");
    } else {
        return TEXT("Default");
    }
}

int32 FControlRigCreationUtility::CleanupDuplicateControls(
    URigHierarchy* RigHierarchy, const TSet<FString>& ExpectedControllerNames,
    bool bLogVerbose) {
    if (!RigHierarchy) {
        return 0;
    }

    URigHierarchyController* HierarchyController =
        RigHierarchy->GetController();
    if (!HierarchyController) {
        UE_LOG(LogTemp, Warning,
               TEXT("Cannot get HierarchyController for cleanup"));
        return 0;
    }

    if (bLogVerbose) {
        UE_LOG(LogTemp, Warning,
               TEXT("Starting cleanup of duplicate/corrupted controls..."));
    }

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

        if (ExpectedControllerNames.Contains(ControlName)) {
            ControlGroups.FindOrAdd(ControlName).Add(ControlKey);
        }
    }

    for (const auto& GroupPair : ControlGroups) {
        const FString& ControlName = GroupPair.Key;
        const TArray<FRigElementKey>& ControlInstances = GroupPair.Value;

        if (ControlInstances.Num() > 1) {
            if (bLogVerbose) {
                UE_LOG(LogTemp, Warning,
                       TEXT("  🔍 Found %d instances of control '%s' - "
                            "removing duplicates"),
                       ControlInstances.Num(), *ControlName);
            }

            for (int32 i = 1; i < ControlInstances.Num(); i++) {
                bool bRemoved = HierarchyController->RemoveElement(
                    ControlInstances[i], true, false);
                if (bRemoved) {
                    DuplicatesFound++;
                    if (bLogVerbose) {
                        UE_LOG(LogTemp, Warning,
                               TEXT("    ✅ Removed duplicate control '%s' "
                                    "instance %d"),
                               *ControlName, i + 1);
                    }
                } else if (bLogVerbose) {
                    UE_LOG(LogTemp, Warning,
                           TEXT("    ❌ Failed to remove duplicate control "
                                "'%s' instance %d"),
                           *ControlName, i + 1);
                }
            }
        }
    }

    if (bLogVerbose) {
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

    return DuplicatesFound;
}

// 辅助函数：获取所有可用的 Shape 名称列表
TArray<FName> FControlRigCreationUtility::GetAvailableShapeNames(
    const UControlRig* InControlRig) {
    TArray<FName> AvailableShapeNames;

    if (!InControlRig) {
        return AvailableShapeNames;
    }

    // 获取所有 Shape Libraries
    const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& ShapeLibraries =
        InControlRig->GetShapeLibraries();

    const TMap<FString, FString>& LibraryNameMap =
        InControlRig->GetShapeLibraryNameMap();

    // 遍历所有 Shape Library
    for (const TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibrary :
         ShapeLibraries) {
        if (!ShapeLibrary.IsValid()) {
            ShapeLibrary.LoadSynchronous();
        }

        if (!ShapeLibrary.IsValid()) {
            continue;
        }

        // 确定是否使用命名空间（当有多个 Shape Library 时）
        const bool bUseNameSpace = ShapeLibraries.Num() > 1;

        FString LibraryName = ShapeLibrary->GetName();
        if (const FString* RemappedName = LibraryNameMap.Find(LibraryName)) {
            LibraryName = *RemappedName;
        }

        const FString NameSpace =
            bUseNameSpace ? LibraryName + TEXT(".") : FString();

        // 添加默认形状
        AvailableShapeNames.Add(*UControlRigShapeLibrary::GetShapeName(
            ShapeLibrary.Get(), bUseNameSpace, LibraryNameMap,
            ShapeLibrary->DefaultShape));

        // 添加所有自定义形状
        for (const FControlRigShapeDefinition& Shape : ShapeLibrary->Shapes) {
            AvailableShapeNames.Add(*UControlRigShapeLibrary::GetShapeName(
                ShapeLibrary.Get(), bUseNameSpace, LibraryNameMap, Shape));
        }
    }

    return AvailableShapeNames;
}