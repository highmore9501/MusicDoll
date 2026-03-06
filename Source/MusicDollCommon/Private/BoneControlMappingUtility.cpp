#include "BoneControlMappingUtility.h"

#include "ControlRigBlueprintLegacy.h"
#include "EdGraphSchema_K2.h"
#include "InstrumentBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Rigs/RigHierarchy.h"

bool FBoneControlMappingUtility::AddBoneControlMappingVariable(
    UControlRigBlueprint* ControlRigBlueprint,
    AInstrumentBase* InstrumentActor) {
    if (!ControlRigBlueprint) {
        UE_LOG(
            LogTemp, Error,
            TEXT("AddBoneControlMappingVariable: ControlRigBlueprint is null"));
        return false;
    }

    const FName VarName = FName(TEXT("BoneControlPairMapping"));

    // 检查变量是否已存在
    int32 ExistingVarIndex = INDEX_NONE;
    for (int32 i = 0; i < ControlRigBlueprint->NewVariables.Num(); ++i) {
        if (ControlRigBlueprint->NewVariables[i].VarName == VarName) {
            ExistingVarIndex = i;
            break;
        }
    }

    // 如果变量已存在，删除它（确保重新生成新的变量）
    if (ExistingVarIndex != INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("AddBoneControlMappingVariable: Variable '%s' already "
                    "exists, removing old variable to recreate it"),
               *VarName.ToString());
        ControlRigBlueprint->NewVariables.RemoveAt(ExistingVarIndex);
    }

    // 标记为已修改
    ControlRigBlueprint->SetFlags(RF_Transactional);

    // 创建新变量描述
    FBPVariableDescription NewVariable;
    NewVariable.VarName = VarName;
    NewVariable.VarGuid = FGuid::NewGuid();
    NewVariable.RepNotifyFunc = NAME_None;
    NewVariable.Category = FText::FromString(TEXT("Bone Control Mapping"));
    NewVariable.FriendlyName =
        FName::NameToDisplayString(VarName.ToString(), false);

    // 设置变量类型为 TArray<FBoneControlPair>
    NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    NewVariable.VarType.PinSubCategoryObject = FBoneControlPair::StaticStruct();
    NewVariable.VarType.ContainerType = EPinContainerType::Array;

    // 设置属性标志 - 默认为蓝图可见和可编辑
    NewVariable.PropertyFlags |=
        (CPF_Edit | CPF_BlueprintVisible | CPF_DisableEditOnInstance);

    // 用户创建的变量不应该有这些属性
    NewVariable.VarType.bIsConst = false;
    NewVariable.VarType.bIsWeakPointer = false;
    NewVariable.VarType.bIsReference = false;

    // 添加变量到蓝图
    ControlRigBlueprint->NewVariables.Add(NewVariable);

    // 标记蓝图为结构化修改，这会触发蓝图重新编译
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
        ControlRigBlueprint);

    UE_LOG(LogTemp, Warning,
           TEXT("AddBoneControlMappingVariable: Successfully added variable "
                "'%s' to NewVariables"),
           *VarName.ToString());

    return true;
}

bool FBoneControlMappingUtility::GetBoneControlMapping(
    UControlRigBlueprint* ControlRigBlueprint,
    TArray<FBoneControlPair>& OutMapping) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("GetBoneControlMapping: ControlRigBlueprint is null"));
        return false;
    }

    OutMapping.Reset();

    // 查找 BoneControlPairMapping 变量
    const FName VarName = FName(TEXT("BoneControlPairMapping"));

    for (const FBPVariableDescription& Var :
         ControlRigBlueprint->NewVariables) {
        if (Var.VarName == VarName) {
            UE_LOG(LogTemp, Warning,
                   TEXT("GetBoneControlMapping: Found variable '%s'"),
                   *VarName.ToString());

            // 获取蓝图的生成类 - 使用正确的方式
            UClass* GeneratedClass = ControlRigBlueprint->GeneratedClass;
            if (!GeneratedClass) {
                // 尝试使用 SkeletonGeneratedClass 作为备选
                GeneratedClass = ControlRigBlueprint->SkeletonGeneratedClass;
            }

            if (!GeneratedClass) {
                UE_LOG(LogTemp, Error,
                       TEXT("GetBoneControlMapping: Failed to get "
                            "GeneratedClass or SkeletonGeneratedClass"));
                return false;
            }

            // 获取默认对象实例
            UObject* DefaultObject = GeneratedClass->GetDefaultObject();
            if (!DefaultObject) {
                UE_LOG(
                    LogTemp, Error,
                    TEXT("GetBoneControlMapping: Failed to get DefaultObject"));
                return false;
            }

            // 通过反射查找属性
            FProperty* Property = GeneratedClass->FindPropertyByName(VarName);
            if (!Property) {
                UE_LOG(
                    LogTemp, Warning,
                    TEXT("GetBoneControlMapping: Property '%s' not found in "
                         "class - variable exists but property not yet "
                         "compiled. GeneratedClass: %s, SkeletonClass: %s"),
                    *VarName.ToString(),
                    *GetNameSafe(ControlRigBlueprint->GeneratedClass),
                    *GetNameSafe(ControlRigBlueprint->SkeletonGeneratedClass));
                return true;  // 变量存在但还没有编译成属性，返回 true
                              // 表示成功获取（空映射）
            }

            // 检查属性是否是 TArray 类型
            FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
            if (!ArrayProperty) {
                UE_LOG(
                    LogTemp, Error,
                    TEXT(
                        "GetBoneControlMapping: Property '%s' is not an array"),
                    *VarName.ToString());
                return false;
            }

            // 检查数组元素是否是 FBoneControlPair 类型
            FStructProperty* StructProperty =
                CastField<FStructProperty>(ArrayProperty->Inner);
            if (!StructProperty ||
                StructProperty->Struct != FBoneControlPair::StaticStruct()) {
                UE_LOG(LogTemp, Error,
                       TEXT("GetBoneControlMapping: Array does not contain "
                            "FBoneControlPair"));
                return false;
            }

            // 获取数组指针
            FScriptArrayHelper ArrayHelper(
                ArrayProperty,
                ArrayProperty->ContainerPtrToValuePtr<void>(DefaultObject));

            UE_LOG(LogTemp, Warning,
                   TEXT("GetBoneControlMapping: Found %d items in "
                        "BoneControlPairMapping"),
                   ArrayHelper.Num());

            // 遍历数组并提取 FBoneControlPair
            for (int32 i = 0; i < ArrayHelper.Num(); i++) {
                FBoneControlPair* PairPtr =
                    (FBoneControlPair*)ArrayHelper.GetRawPtr(i);
                if (PairPtr) {
                    OutMapping.Add(*PairPtr);
                    UE_LOG(LogTemp, Verbose,
                           TEXT("  Loaded pair %d: Bone=%s, Control=%s"), i,
                           *PairPtr->BoneName.ToString(),
                           *PairPtr->ControlName.ToString());
                }
            }

            return true;
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("GetBoneControlMapping: Variable '%s' not found in NewVariables"),
        TEXT("BoneControlPairMapping"));
    return false;
}

bool FBoneControlMappingUtility::SetBoneControlMapping(
    UControlRigBlueprint* ControlRigBlueprint,
    const TArray<FBoneControlPair>& InMapping) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("SetBoneControlMapping: ControlRigBlueprint is null"));
        return false;
    }

    // 查找 BoneControlPairMapping 变量描述
    const FName VarName = FName(TEXT("BoneControlPairMapping"));

    FBPVariableDescription* TargetVar = nullptr;
    for (FBPVariableDescription& Var : ControlRigBlueprint->NewVariables) {
        if (Var.VarName == VarName) {
            TargetVar = &Var;
            break;
        }
    }

    if (!TargetVar) {
        UE_LOG(LogTemp, Error,
               TEXT("SetBoneControlMapping: Variable '%s' not found in "
                    "NewVariables. Please call AddBoneControlMappingVariable "
                    "first to "
                    "initialize it."),
               *VarName.ToString());
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("SetBoneControlMapping: Setting %d mappings to variable '%s'"),
           InMapping.Num(), *VarName.ToString());

    // 获取蓝图的生成类 - 使用正确的方式
    UClass* GeneratedClass = ControlRigBlueprint->GeneratedClass;
    if (!GeneratedClass) {
        // 尝试使用 SkeletonGeneratedClass 作为备选
        GeneratedClass = ControlRigBlueprint->SkeletonGeneratedClass;
    }

    if (!GeneratedClass) {
        UE_LOG(LogTemp, Error,
               TEXT("SetBoneControlMapping: Failed to get GeneratedClass or "
                    "SkeletonGeneratedClass"));
        return false;
    }

    // 获取默认对象实例
    UObject* DefaultObject = GeneratedClass->GetDefaultObject();
    if (!DefaultObject) {
        UE_LOG(LogTemp, Error,
               TEXT("SetBoneControlMapping: Failed to get DefaultObject"));
        return false;
    }

    // 通过反射查找属性
    FProperty* Property = GeneratedClass->FindPropertyByName(VarName);
    if (!Property) {
        UE_LOG(LogTemp, Error,
               TEXT("SetBoneControlMapping: Property '%s' not found in class - "
                    "Blueprint needs to be compiled. GeneratedClass: %s, "
                    "SkeletonClass: %s"),
               *VarName.ToString(),
               *GetNameSafe(ControlRigBlueprint->GeneratedClass),
               *GetNameSafe(ControlRigBlueprint->SkeletonGeneratedClass));
        return false;
    }

    // 检查属性是否是 TArray 类型
    FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
    if (!ArrayProperty) {
        UE_LOG(LogTemp, Error,
               TEXT("SetBoneControlMapping: Property '%s' is not an array"),
               *VarName.ToString());
        return false;
    }

    // 验证数组元素类型
    FStructProperty* StructProperty =
        CastField<FStructProperty>(ArrayProperty->Inner);
    if (!StructProperty ||
        StructProperty->Struct != FBoneControlPair::StaticStruct()) {
        UE_LOG(LogTemp, Error,
               TEXT("SetBoneControlMapping: Array does not contain "
                    "FBoneControlPair"));
        return false;
    }

    // 标记对象为修改中
    DefaultObject->Modify();

    // 获取数组指针
    FScriptArrayHelper ArrayHelper(
        ArrayProperty,
        ArrayProperty->ContainerPtrToValuePtr<void>(DefaultObject));

    // 清空现有数组
    ArrayHelper.EmptyValues();

    UE_LOG(LogTemp, Warning,
           TEXT("SetBoneControlMapping: Cleared existing array, adding %d new "
                "items"),
           InMapping.Num());

    // 添加新的映射
    for (const FBoneControlPair& Pair : InMapping) {
        int32 NewIndex = ArrayHelper.AddValue();
        FBoneControlPair* NewPair =
            (FBoneControlPair*)ArrayHelper.GetRawPtr(NewIndex);
        if (NewPair) {
            *NewPair = Pair;
            UE_LOG(LogTemp, Verbose,
                   TEXT("  Saved pair %d: Bone=%s, Control=%s"), NewIndex,
                   *Pair.BoneName.ToString(), *Pair.ControlName.ToString());
        }
    }

    // 标记蓝图包为已修改，以便保存到磁盘
    ControlRigBlueprint->MarkPackageDirty();

    UE_LOG(LogTemp, Warning,
           TEXT("SetBoneControlMapping: Successfully saved %d mappings to "
                "blueprint"),
           InMapping.Num());

    return true;
}

bool FBoneControlMappingUtility::GetAllBoneNamesFromHierarchy(
    UControlRigBlueprint* ControlRigBlueprint, TArray<FString>& OutBoneNames) {
    if (!ControlRigBlueprint) {
        UE_LOG(
            LogTemp, Error,
            TEXT("GetAllBoneNamesFromHierarchy: ControlRigBlueprint is null"));
        return false;
    }

    URigHierarchy* Hierarchy = ControlRigBlueprint->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("GetAllBoneNamesFromHierarchy: Failed to get hierarchy"));
        return false;
    }

    OutBoneNames.Reset();

    TArray<FRigElementKey> AllKeys = Hierarchy->GetAllKeys();
    for (const FRigElementKey& Key : AllKeys) {
        if (Key.Type == ERigElementType::Bone) {
            OutBoneNames.Add(Key.Name.ToString());
        }
    }

    return true;
}

bool FBoneControlMappingUtility::GetAllControlNamesFromHierarchy(
    UControlRigBlueprint* ControlRigBlueprint,
    TArray<FString>& OutControlNames) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("GetAllControlNamesFromHierarchy: ControlRigBlueprint is "
                    "null"));
        return false;
    }

    URigHierarchy* Hierarchy = ControlRigBlueprint->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(
            LogTemp, Error,
            TEXT("GetAllControlNamesFromHierarchy: Failed to get hierarchy"));
        return false;
    }

    OutControlNames.Reset();

    TArray<FRigElementKey> AllKeys = Hierarchy->GetAllKeys();
    for (const FRigElementKey& Key : AllKeys) {
        if (Key.Type == ERigElementType::Control) {
            OutControlNames.Add(Key.Name.ToString());
        }
    }

    return true;
}

bool FBoneControlMappingUtility::SyncBoneControlPairs(
    UControlRigBlueprint* ControlRigBlueprint, AInstrumentBase* InstrumentActor,
    int32& OutSyncedCount, int32& OutFailedCount) {
    OutSyncedCount = 0;
    OutFailedCount = 0;

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncBoneControlPairs: ControlRigBlueprint is null"));
        return false;
    }

    if (!InstrumentActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncBoneControlPairs: InstrumentActor is null"));
        return false;
    }

    // 获取Hierarchy
    URigHierarchy* Hierarchy = ControlRigBlueprint->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncBoneControlPairs: Failed to get hierarchy"));
        return false;
    }

    // 获取骨骼网格Actor
    ASkeletalMeshActor* SkeletalMeshActor = InstrumentActor->SkeletalMeshActor;
    if (!SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncBoneControlPairs: SkeletalMeshActor is null"));
        return false;
    }

    USkeletalMeshComponent* SkeletalMeshComponent =
        SkeletalMeshActor->GetSkeletalMeshComponent();
    if (!SkeletalMeshComponent) {
        UE_LOG(LogTemp, Error,
               TEXT("SyncBoneControlPairs: SkeletalMeshComponent is null"));
        return false;
    }

    // 获取BoneControlPairMapping变量
    TArray<FBoneControlPair> BoneControlPairs;
    if (!GetBoneControlMapping(ControlRigBlueprint, BoneControlPairs)) {
        UE_LOG(
            LogTemp, Error,
            TEXT("SyncBoneControlPairs: Failed to get BoneControlPairMapping"));
        return false;
    }

    if (BoneControlPairs.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("SyncBoneControlPairs: No BoneControlPairs found"));
        return true;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("SyncBoneControlPairs: Starting sync for %d pairs"),
           BoneControlPairs.Num());

    // 构建 Control 名称到 Pair 的映射，并获取每个 Control 的层级深度
    TArray<TPair<FRigElementKey, FBoneControlPair>> SortedPairs;
    TMap<FString, int32> ControlDepthCache;

    for (const FBoneControlPair& Pair : BoneControlPairs) {
        // 验证 bone 和 control 都不为 empty
        if (Pair.BoneName.IsNone() || Pair.ControlName.IsNone()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("SyncBoneControlPairs: Skipping pair with empty name"));
            continue;
        }

        // 获取 Control 元素
        FRigElementKey ControlKey(*Pair.ControlName.ToString(),
                                  ERigElementType::Control);
        if (!Hierarchy->Contains(ControlKey)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("SyncBoneControlPairs: Control '%s' not found in "
                        "hierarchy"),
                   *Pair.ControlName.ToString());
            OutFailedCount++;
            continue;
        }

        SortedPairs.Add(
            TPair<FRigElementKey, FBoneControlPair>(ControlKey, Pair));
    }

    // 计算每个 Control 的层级深度（从叶子到根）
    auto GetControlDepth = [&](FRigElementKey ControlKey,
                               auto& SelfFunc) -> int32 {
        FString ControlName = ControlKey.Name.ToString();

        // 检查缓存
        int32* CachedDepth = ControlDepthCache.Find(ControlName);
        if (CachedDepth) {
            return *CachedDepth;
        }

        int32 Depth = 0;
        FRigElementKey CurrentKey = ControlKey;

        while (true) {
            FRigElementKey ParentElement =
                Hierarchy->GetFirstParent(CurrentKey);
            if (!ParentElement) {
                break;
            }
            Depth++;
            CurrentKey = ParentElement;
        }

        ControlDepthCache.Add(ControlName, Depth);
        return Depth;
    };

    // 按层级深度排序，先处理父级（深度小的），再处理子级（深度大的）
    SortedPairs.Sort([&](const TPair<FRigElementKey, FBoneControlPair>& A,
                         const TPair<FRigElementKey, FBoneControlPair>& B) {
        int32 DepthA = GetControlDepth(A.Key, GetControlDepth);
        int32 DepthB = GetControlDepth(B.Key, GetControlDepth);
        // 深度大的（子级）排在后面
        return DepthA < DepthB;
    });

    UE_LOG(LogTemp, Warning,
           TEXT("SyncBoneControlPairs: Sorted pairs by depth (parent first):"));
    for (int32 i = 0; i < SortedPairs.Num(); i++) {
        FString ControlName = SortedPairs[i].Key.Name.ToString();
        int32 Depth = GetControlDepth(SortedPairs[i].Key, GetControlDepth);
        UE_LOG(LogTemp, Verbose, TEXT("  [%d] Control=%s, Depth=%d"), i,
               *ControlName, Depth);
    }

    // 遍历排序后的所有的 Bone-Control Pair
    for (const TPair<FRigElementKey, FBoneControlPair>& SortedPair :
         SortedPairs) {
        const FBoneControlPair& Pair = SortedPair.Value;
        FRigElementKey ControlKey = SortedPair.Key;

        // 验证bone和control都不为empty
        if (Pair.BoneName.IsNone() || Pair.ControlName.IsNone()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("SyncBoneControlPairs: Skipping pair with empty name"));
            continue;
        }

        // 获取bone的世界变换
        int32 BoneIndex = SkeletalMeshComponent->GetBoneIndex(Pair.BoneName);
        if (BoneIndex == INDEX_NONE) {
            UE_LOG(LogTemp, Warning,
                   TEXT("SyncBoneControlPairs: Bone '%s' not found in skeletal "
                        "mesh"),
                   *Pair.BoneName.ToString());
            OutFailedCount++;
            continue;
        }

        // 获取 bone 的世界变换
        const FTransform& ComponentSpaceTransform =
            SkeletalMeshComponent->GetComponentSpaceTransforms()[BoneIndex];
        // FTransform BoneWorldTransform =
        //     ComponentSpaceTransform * SkeletalMeshActor->GetActorTransform();

        // Control 元素已经在前面获取并验证过了，直接使用
        FRigControlElement* ControlElement =
            Hierarchy->Find<FRigControlElement>(ControlKey);
        if (!ControlElement) {
            UE_LOG(LogTemp, Warning,
                   TEXT("SyncBoneControlPairs: Failed to find ControlElement "
                        "for '%s'"),
                   *Pair.ControlName.ToString());
            OutFailedCount++;
            continue;
        }

        // 直接使用 Bone 的世界变换（SetGlobalTransform 会自动处理层级关系）
        // 只保留位置和旋转，移除缩放（避免 Control 变得过大）
        FVector Location = ComponentSpaceTransform.GetLocation();
        FRotator Rotation = ComponentSpaceTransform.Rotator();
        FTransform CleanTransform(Rotation, Location, FVector(1.f, 1.f, 1.f));

        // 直接设置 Control 的全局变换
        int32 ControlIndex = Hierarchy->GetIndex(ControlKey);
        if (ControlIndex != INDEX_NONE) {
            // 直接设置全局变换
            constexpr bool bAffectChildren = true;
            constexpr bool bSetupUndo = true;
            constexpr bool bPrintPythonCommand = true;

            // 设置初始全局变换
            Hierarchy->SetInitialGlobalTransform(ControlKey, CleanTransform,
                                                 bAffectChildren, bSetupUndo);

            // 设置当前全局变换
            Hierarchy->SetGlobalTransform(ControlKey, CleanTransform,
                                          bAffectChildren, bSetupUndo,
                                          bPrintPythonCommand);

            OutSyncedCount++;
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("SyncBoneControlPairs: Failed to get control index for "
                        "'%s'"),
                   *Pair.ControlName.ToString());
            OutFailedCount++;
        }
    }

    // 标记蓝图为结构化修改
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
        ControlRigBlueprint);

    UE_LOG(LogTemp, Warning,
           TEXT("SyncBoneControlPairs: Completed - Synced: %d, Failed: %d"),
           OutSyncedCount, OutFailedCount);

    return true;
}
