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

// ============================================================
// GetCommonSuffix
// ============================================================

FString FControlRigCreationUtility::GetCommonSuffix(const FString& NameA,
                                                     const FString& NameB) {
    int32 MinLen = FMath::Min(NameA.Len(), NameB.Len());
    int32 SuffixLen = 0;
    for (int32 i = 1; i <= MinLen; ++i) {
        if (NameA[NameA.Len() - i] == NameB[NameB.Len() - i]) {
            SuffixLen = i;
        } else {
            break;
        }
    }
    if (SuffixLen == 0) return TEXT("");
    return NameA.Right(SuffixLen);
}

// ============================================================
// ParseControlIndex
// ============================================================

int32 FControlRigCreationUtility::ParseControlIndex(const FString& Name,
                                                     const FString& Suffix) {
    if (Suffix.IsEmpty() || !Name.EndsWith(Suffix)) return -1;
    FString Prefix = Name.LeftChop(Suffix.Len());
    // Prefix must end with digits, optionally start with 's'
    if (Prefix.IsEmpty()) return -1;
    // Strip leading 's' or 'S'
    FString NumStr = Prefix;
    if (NumStr.StartsWith(TEXT("s")) || NumStr.StartsWith(TEXT("S"))) {
        NumStr = NumStr.RightChop(1);
    }
    if (NumStr.IsEmpty()) return -1;
    for (TCHAR Ch : NumStr) {
        if (!FChar::IsDigit(Ch)) return -1;
    }
    return FCString::Atoi(*NumStr);
}

// ============================================================
// LinearDistributeControls
// ============================================================

int32 FControlRigCreationUtility::LinearDistributeControls(
    UControlRig* ControlRig) {
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls: ControlRig is null"));
        return -1;
    }

    URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls: Failed to get RigHierarchy"));
        return -1;
    }

    // 收集当前选中的控制器
    TArray<FRigElementKey> SelectedKeys = Hierarchy->GetSelectedKeys();
    TArray<FRigElementKey> SelectedControls;
    for (const FRigElementKey& Key : SelectedKeys) {
        if (Key.Type == ERigElementType::Control) {
            SelectedControls.Add(Key);
        }
    }

    if (SelectedControls.Num() != 2) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls: Please select exactly 2 "
                    "controls (selected %d)"),
               SelectedControls.Num());
        return -1;
    }

    FString NameA = SelectedControls[0].Name.ToString();
    FString NameB = SelectedControls[1].Name.ToString();

    FString Suffix = GetCommonSuffix(NameA, NameB);
    if (Suffix.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls: Selected controls '%s' and '%s' "
                    "have no common suffix"),
               *NameA, *NameB);
        return -1;
    }

    int32 IndexA = ParseControlIndex(NameA, Suffix);
    int32 IndexB = ParseControlIndex(NameB, Suffix);

    if (IndexA < 0 || IndexB < 0) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls: Cannot parse numeric index from "
                    "'%s' and '%s' with suffix '%s'"),
               *NameA, *NameB, *Suffix);
        return -1;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("LinearDistributeControls: suffix='%s', A=%d(%s), B=%d(%s)"),
           *Suffix, IndexA, *NameA, IndexB, *NameB);

    // 收集场景中所有符合 s{N}{Suffix} 模式的控制器
    TArray<FRigElementKey> AllControlKeys = Hierarchy->GetAllKeys(false);
    TArray<TPair<int32, FRigElementKey>> Candidates;

    for (const FRigElementKey& Key : AllControlKeys) {
        if (Key.Type != ERigElementType::Control) continue;
        FString Name = Key.Name.ToString();
        int32 Idx = ParseControlIndex(Name, Suffix);
        if (Idx < 0) continue;
        Candidates.Add({Idx, Key});
    }

    if (Candidates.Num() < 2) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls: Not enough controls with suffix "
                    "'%s'"),
               *Suffix);
        return -1;
    }

    Candidates.Sort([](const TPair<int32, FRigElementKey>& A,
                       const TPair<int32, FRigElementKey>& B) {
        return A.Key < B.Key;
    });

    int32 MinIdx = FMath::Min(IndexA, IndexB);
    int32 MaxIdx = FMath::Max(IndexA, IndexB);

    // 筛选范围内的控制器
    TArray<TPair<int32, FRigElementKey>> Targets;
    for (const auto& Pair : Candidates) {
        if (Pair.Key >= MinIdx && Pair.Key <= MaxIdx) {
            Targets.Add(Pair);
        }
    }

    if (Targets.Num() < 2) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls: Not enough controls in range "
                    "[%d, %d]"),
               MinIdx, MaxIdx);
        return -1;
    }

    // 获取端点位置（当前值）
    auto GetControlLocation = [&](const FRigElementKey& Key) -> FVector {
        FRigControlElement* Elem = Hierarchy->Find<FRigControlElement>(Key);
        if (!Elem) return FVector::ZeroVector;
        FRigControlValue Val = Hierarchy->GetControlValue(
            Elem, ERigControlValueType::Current);
        FTransform T = Val.GetAsTransform(Elem->Settings.ControlType,
                                          Elem->Settings.PrimaryAxis);
        return T.GetLocation();
    };

    // 找到选中的两个端点 Key
    FRigElementKey KeyA = SelectedControls[0];
    FRigElementKey KeyB = SelectedControls[1];
    FVector PosA = GetControlLocation(KeyA);
    FVector PosB = GetControlLocation(KeyB);

    int32 StartNum = Targets[0].Key;
    int32 EndNum = Targets.Last().Key;
    int32 NumSpan = EndNum - StartNum;

    if (NumSpan == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("LinearDistributeControls: Index span is zero"));
        return -1;
    }

    // 以 StartNum/EndNum 对应 PosA/PosB（按实际索引线性插值）
    // 先确定哪端对应小索引
    FVector PosStart = (IndexA < IndexB) ? PosA : PosB;
    FVector PosEnd = (IndexA < IndexB) ? PosB : PosA;

    int32 DistributedCount = 0;
    for (const auto& Pair : Targets) {
        float T = (float)(Pair.Key - StartNum) / (float)NumSpan;
        FVector NewLocation = FMath::Lerp(PosStart, PosEnd, T);

        FRigControlElement* Elem =
            Hierarchy->Find<FRigControlElement>(Pair.Value);
        if (!Elem) continue;

        FRigControlValue CurrentVal = Hierarchy->GetControlValue(
            Elem, ERigControlValueType::Current);
        FTransform CurrentTransform = CurrentVal.GetAsTransform(
            Elem->Settings.ControlType, Elem->Settings.PrimaryAxis);
        CurrentTransform.SetLocation(NewLocation);

        FRigControlValue NewVal;
        NewVal.SetFromTransform(CurrentTransform, Elem->Settings.ControlType,
                                Elem->Settings.PrimaryAxis);
        Hierarchy->SetControlValue(Elem, NewVal, ERigControlValueType::Current);
        DistributedCount++;
    }

    ControlRig->Evaluate_AnyThread();

    UE_LOG(LogTemp, Warning,
           TEXT("LinearDistributeControls: Distributed %d controls"),
           DistributedCount);
    return DistributedCount;
}