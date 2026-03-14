#include "BeatBloomControlRigProcessor.h"

#include "BeatBloomTransformSyncProcessor.h"
#include "BeatBloomUnreal.h"
#include "BoneControlMappingUtility.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "InstrumentControlRigUtility.h"

#define LOCTEXT_NAMESPACE "BeatBloomControlRigProcessor"

void UBeatBloomControlRigProcessor::SaveHandState(
    ABeatBloomUnreal* BeatBloomActor) {
    // 保存手部状态（左手 + 右手）
    // 参考设计文档 04_BeatBloom_ControlRigProcessor.md 第五节 5.1

    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor for SaveHandState"));
        return;
    }

    // 验证是否能获取到 ControlRig 实例
    UControlRig* ControlRig = BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(
            LogTemp, Error,
            TEXT("BeatBloom: Failed to get ControlRig instance for Performer"));
        return;
    }

    // 获取控制器到记录器的映射
    TMap<FString, FString> Mapping =
        BeatBloomActor->GetCurrentControllerToRecorderMapping();

    int32 SavedCount = 0;
    int32 FailedCount = 0;

    // 手部控制器列表
    TArray<FString> HandControllers = {
        TEXT("H_L"), TEXT("HP_L"), TEXT("H_rotation_L"),
        TEXT("H_R"), TEXT("HP_R"), TEXT("H_rotation_R")};

    // 遍历所有手部控制器并保存
    for (const FString& ControllerName : HandControllers) {
        FString* RecorderNamePtr = Mapping.Find(ControllerName);
        if (!RecorderNamePtr) {
            UE_LOG(LogTemp, Warning,
                   TEXT("BeatBloom: Controller %s not found in mapping (hand "
                        "may be resting)"),
                   *ControllerName);
            continue;
        }

        // 对于旋转控制器，重定位到对应的位置控制器读取数据
        FString ActualControllerName = ControllerName;
        if (ControllerName == TEXT("H_rotation_L")) {
            ActualControllerName = TEXT("H_L");
        } else if (ControllerName == TEXT("H_rotation_R")) {
            ActualControllerName = TEXT("H_R");
        }

        FVector Location;
        FQuat Rotation;
        if (ReadControllerTransform(BeatBloomActor, ActualControllerName,
                                    Location, Rotation)) {
            // 根据控制器类型决定保存的数据
            FBeatBloomRecorderTransform RecorderTransform;
            
            // 位置控制器（H_*, HP_*）：同时保存位置和旋转
            RecorderTransform.Location = Location;
            RecorderTransform.Rotation = Rotation;

            BeatBloomActor->RecorderTransforms.Add(*RecorderNamePtr,
                                                   RecorderTransform);
            UE_LOG(LogTemp, Warning,
                   TEXT("BeatBloom: Saved %s -> %s at Loc(%.2f, %.2f, %.2f) "
                        "Rot(%.2f, %.2f, %.2f, %.2f)"),
                   *ControllerName, **RecorderNamePtr,
                   RecorderTransform.Location.X, RecorderTransform.Location.Y,
                   RecorderTransform.Location.Z, RecorderTransform.Rotation.X,
                   RecorderTransform.Rotation.Y, RecorderTransform.Rotation.Z,
                   RecorderTransform.Rotation.W);
            SavedCount++;
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("BeatBloom: Failed to read controller %s"),
                   *ActualControllerName);
            FailedCount++;
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: SaveHandState completed - Saved: %d, Failed: %d"),
           SavedCount, FailedCount);
}

void UBeatBloomControlRigProcessor::SaveFootState(
    ABeatBloomUnreal* BeatBloomActor) {
    // 保存脚部状态（左脚 + 右脚）
    // 参考设计文档 04_BeatBloom_ControlRigProcessor.md 第五节 5.2

    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor for SaveFootState"));
        return;
    }

    // 验证是否能获取到 ControlRig 实例
    UControlRig* ControlRig = BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(
            LogTemp, Error,
            TEXT("BeatBloom: Failed to get ControlRig instance for Performer"));
        return;
    }

    // 获取控制器到记录器的映射
    TMap<FString, FString> Mapping =
        BeatBloomActor->GetCurrentControllerToRecorderMapping();

    int32 SavedCount = 0;
    int32 FailedCount = 0;

    // 脚部控制器列表
    TArray<FString> FootControllers = {TEXT("F_L"), TEXT("F_rotation_L"),
                                       TEXT("F_R"), TEXT("F_rotation_R")};

    // 遍历所有脚部控制器并保存
    for (const FString& ControllerName : FootControllers) {
        FString* RecorderNamePtr = Mapping.Find(ControllerName);
        if (!RecorderNamePtr) {
            UE_LOG(LogTemp, Warning,
                   TEXT("BeatBloom: Controller %s not found in mapping (foot "
                        "may not be selected)"),
                   *ControllerName);
            continue;
        }

        // 对于脚部旋转控制器，重定位到对应的位置控制器读取数据
        FString ActualControllerName = ControllerName;
        if (ControllerName == TEXT("F_rotation_L")) {
            ActualControllerName = TEXT("F_L");
        } else if (ControllerName == TEXT("F_rotation_R")) {
            ActualControllerName = TEXT("F_R");
        }

        FVector Location;
        FQuat Rotation;
        if (ReadControllerTransform(BeatBloomActor, ActualControllerName,
                                    Location, Rotation)) {
            // 根据控制器类型决定保存的数据
            FBeatBloomRecorderTransform RecorderTransform;
            
            // 脚部位置控制器（F_*）：同时保存位置和旋转
            RecorderTransform.Location = Location;
            RecorderTransform.Rotation = Rotation;

            BeatBloomActor->RecorderTransforms.Add(*RecorderNamePtr,
                                                   RecorderTransform);
            UE_LOG(LogTemp, Warning,
                   TEXT("BeatBloom: Saved %s -> %s at Loc(%.2f, %.2f, %.2f) "
                        "Rot(%.2f, %.2f, %.2f, %.2f)"),
                   *ControllerName, **RecorderNamePtr,
                   RecorderTransform.Location.X, RecorderTransform.Location.Y,
                   RecorderTransform.Location.Z, RecorderTransform.Rotation.X,
                   RecorderTransform.Rotation.Y, RecorderTransform.Rotation.Z,
                   RecorderTransform.Rotation.W);
            SavedCount++;
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("BeatBloom: Failed to read controller %s"),
                   *ActualControllerName);
            FailedCount++;
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: SaveFootState completed - Saved: %d, Failed: %d"),
           SavedCount, FailedCount);
}

void UBeatBloomControlRigProcessor::SaveTargetState(
    ABeatBloomUnreal* BeatBloomActor) {
    // 保存目标状态（Body / Chest / Head）
    // 从 ControlRig 读取 Tar_Body/Tar_Chest/Tar_Head 的 Z 轴位置
    // 保存到 RecorderTransforms
    // 参考设计文档 04_BeatBloom_ControlRigProcessor.md 第五节 5.3

    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor for SaveTargetState"));
        return;
    }

    // 验证是否能获取到 ControlRig 实例
    UControlRig* ControlRig = BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(
            LogTemp, Error,
            TEXT("BeatBloom: Failed to get ControlRig instance for Performer"));
        return;
    }

    // 获取控制器到记录器的映射
    TMap<FString, FString> Mapping =
        BeatBloomActor->GetCurrentControllerToRecorderMapping();

    int32 SavedCount = 0;
    int32 FailedCount = 0;

    // 目标控制器列表
    TArray<FString> TargetControllers = {TEXT("Tar_Body"), TEXT("Tar_Chest"),
                                         TEXT("Tar_Head")};

    // 遍历所有目标控制器并保存
    for (const FString& ControllerName : TargetControllers) {
        FString* RecorderNamePtr = Mapping.Find(ControllerName);
        if (!RecorderNamePtr) {
            UE_LOG(LogTemp, Warning,
                   TEXT("BeatBloom: Controller %s not found in mapping (target "
                        "may not be selected)"),
                   *ControllerName);
            continue;
        }

        FVector Location;
        FQuat Rotation;
        if (ReadControllerTransform(BeatBloomActor, ControllerName, Location,
                                    Rotation)) {
            // 目标控制器：仅保存 Z 轴位置
            FBeatBloomRecorderTransform RecorderTransform;
            RecorderTransform.Location = FVector(0.0f, 0.0f, Location.Z);
            RecorderTransform.Rotation = FQuat::Identity;

            BeatBloomActor->RecorderTransforms.Add(*RecorderNamePtr,
                                                   RecorderTransform);
            UE_LOG(LogTemp, Warning,
                   TEXT("BeatBloom: Saved %s -> %s at Z=%.2f"), *ControllerName,
                   **RecorderNamePtr, Location.Z);
            SavedCount++;
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("BeatBloom: Failed to read controller %s"),
                   *ControllerName);
            FailedCount++;
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: SaveTargetState completed - Saved: %d, Failed: %d"),
           SavedCount, FailedCount);
}

void UBeatBloomControlRigProcessor::SaveAllState(
    ABeatBloomUnreal* BeatBloomActor) {
    // 保存所有状态（手部 + 脚部 + 目标）

    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor for SaveAllState"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("BeatBloom: Starting SaveAllState..."));

    // 依次调用各个保存方法
    SaveHandState(BeatBloomActor);
    SaveFootState(BeatBloomActor);
    SaveTargetState(BeatBloomActor);

    UE_LOG(LogTemp, Warning, TEXT("BeatBloom: SaveAllState completed"));
}

void UBeatBloomControlRigProcessor::LoadState(
    ABeatBloomUnreal* BeatBloomActor) {
    // 加载状态：根据当前界面选择，将 RecorderTransforms 中对应的值写入
    // ControlRig 控制器 参考设计文档 04_BeatBloom_ControlRigProcessor.md
    // 第五节 5.4

    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor for LoadState"));
        return;
    }

    // 验证是否能获取到 ControlRig 实例
    UControlRig* ControlRig = BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(
            LogTemp, Error,
            TEXT("BeatBloom: Failed to get ControlRig instance for Performer"));
        return;
    }

    // 获取控制器到记录器的映射
    TMap<FString, FString> Mapping =
        BeatBloomActor->GetCurrentControllerToRecorderMapping();

    int32 LoadedCount = 0;
    int32 FailedCount = 0;

    // 遍历所有映射对并加载
    for (const auto& MappingPair : Mapping) {
        const FString& ControllerName = MappingPair.Key;
        const FString& RecorderName = MappingPair.Value;

        // 从 RecorderTransforms 查找记录器数据
        const FBeatBloomRecorderTransform* FoundTransform =
            BeatBloomActor->RecorderTransforms.Find(RecorderName);

        if (!FoundTransform) {
            UE_LOG(
                LogTemp, Warning,
                TEXT("BeatBloom: Recorder %s not found in RecorderTransforms"),
                *RecorderName);
            FailedCount++;
            continue;
        }

        // 根据控制器类型决定加载的数据
        bool bLocationOnly = false;
        bool bRotationOnly = false;
        bool bZOnly = false;

        if (ControllerName.StartsWith(TEXT("H_rotation")) ||
            ControllerName.StartsWith(TEXT("F_rotation"))) {
            // 旋转控制器不需要真实加载
            continue;
        } else if (ControllerName.StartsWith(TEXT("Tar_"))) {
            // 目标控制器：仅加载 Z 轴
            bZOnly = true;
        } else {
            // 位置控制器（H_*, HP_*, F_*）：同时加载位置和旋转
            // 不需要特殊标志，默认行为即可
        }

        if (WriteControllerTransform(BeatBloomActor, ControllerName,
                                     FoundTransform->Location,
                                     FoundTransform->Rotation, bLocationOnly,
                                     bRotationOnly, bZOnly)) {
            if (bZOnly) {
                UE_LOG(LogTemp, Warning,
                       TEXT("BeatBloom: Loaded %s <- %s at Z=%.2f"),
                       *ControllerName, *RecorderName,
                       FoundTransform->Location.Z);
            } else {
                UE_LOG(LogTemp, Warning,
                       TEXT("BeatBloom: Loaded %s <- %s at Loc(%.2f, %.2f, "
                            "%.2f) Rot(%.2f, %.2f, %.2f, %.2f)"),
                       *ControllerName, *RecorderName,
                       FoundTransform->Location.X, FoundTransform->Location.Y,
                       FoundTransform->Location.Z, FoundTransform->Rotation.X,
                       FoundTransform->Rotation.Y, FoundTransform->Rotation.Z,
                       FoundTransform->Rotation.W);
            }
            LoadedCount++;
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("BeatBloom: Failed to write controller %s"),
                   *ControllerName);
            FailedCount++;
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: LoadState completed - Loaded: %d, Failed: %d"),
           LoadedCount, FailedCount);
}

TMap<FString, FString>
UBeatBloomControlRigProcessor::GetCurrentControllerToRecorderMapping(
    ABeatBloomUnreal* BeatBloomActor) {
    // 委托给 BeatBloomActor->GetCurrentControllerToRecorderMapping()
    return BeatBloomActor
               ? BeatBloomActor->GetCurrentControllerToRecorderMapping()
               : TMap<FString, FString>();
}

bool UBeatBloomControlRigProcessor::ReadControllerTransform(
    ABeatBloomUnreal* BeatBloomActor, const FString& ControllerName,
    FVector& OutLocation, FQuat& OutRotation) {
    // 使用 UInstrumentControlRigUtility 从 ControlRig 实例读取
    if (!BeatBloomActor) return false;

    UControlRig* ControlRig = BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(LogTemp, Error, TEXT("ReadControllerTransform: Failed to get ControlRig instance for Performer"));
        return false;
    }

    URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
    if (!RigHierarchy) return false;

    FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);
    if (!RigHierarchy->Contains(ElementKey)) return false;

    FRigControlElement* ControlElement =
        RigHierarchy->Find<FRigControlElement>(ElementKey);
    if (!ControlElement) return false;

    FRigControlValue CurrentValue = RigHierarchy->GetControlValue(
        ControlElement, ERigControlValueType::Current);
    FTransform CurrentTransform =
        CurrentValue.GetAsTransform(ControlElement->Settings.ControlType,
                                    ControlElement->Settings.PrimaryAxis);

    OutLocation = CurrentTransform.GetLocation();
    OutRotation = CurrentTransform.GetRotation();
    return true;
}

bool UBeatBloomControlRigProcessor::WriteControllerTransform(
    ABeatBloomUnreal* BeatBloomActor, const FString& ControllerName,
    const FVector& Location, const FQuat& Rotation, bool bLocationOnly,
    bool bRotationOnly, bool bZOnly) {
    // 使用 UInstrumentControlRigUtility 写入 ControlRig 实例
    if (!BeatBloomActor) return false;

    UControlRig* ControlRig = BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(LogTemp, Error, TEXT("WriteControllerTransform: Failed to get ControlRig instance for Performer"));
        return false;
    }

    URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
    if (!RigHierarchy) return false;

    FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);
    if (!RigHierarchy->Contains(ElementKey)) return false;

    FRigControlElement* ControlElement =
        RigHierarchy->Find<FRigControlElement>(ElementKey);
    if (!ControlElement) return false;

    FRigControlValue CurrentValue = RigHierarchy->GetControlValue(
        ControlElement, ERigControlValueType::Current);
    FTransform CurrentTransform =
        CurrentValue.GetAsTransform(ControlElement->Settings.ControlType,
                                    ControlElement->Settings.PrimaryAxis);

    // 构建新 Transform
    FTransform NewTransform = CurrentTransform;

    if (bZOnly) {
        // 仅更新 Z 轴（用于目标控制器）
        FVector CurrentLocation = CurrentTransform.GetLocation();
        CurrentLocation.Z = Location.Z;
        NewTransform.SetLocation(CurrentLocation);
    } else if (bLocationOnly) {
        NewTransform.SetLocation(Location);
    } else if (bRotationOnly) {
        NewTransform.SetRotation(Rotation);
    } else {
        // 默认同时更新位置和旋转
        NewTransform.SetLocation(Location);
        NewTransform.SetRotation(Rotation);
    }

    FRigControlValue NewValue;
    NewValue.SetFromTransform(NewTransform,
                              ControlElement->Settings.ControlType,
                              ControlElement->Settings.PrimaryAxis);

    RigHierarchy->SetControlValue(ControlElement, NewValue,
                                  ERigControlValueType::Current);
    return true;
}

bool UBeatBloomControlRigProcessor::ControlExists(URigHierarchy* RigHierarchy,
                                                  const FString& ControlName) {
    FRigElementKey ElementKey(*ControlName, ERigElementType::Control);
    return RigHierarchy->Contains(ElementKey);
}

int32 UBeatBloomControlRigProcessor::SetupControllers(
    ABeatBloomUnreal* BeatBloomActor,
    UControlRigBlueprint* ControlRigBlueprint) {
    if (!BeatBloomActor || !ControlRigBlueprint) {
        return 0;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    URigHierarchyController* HierarchyController =
        RigHierarchy->GetController();

    int32 CreatedCount = 0;

    // 1. 创建 base_root（如果没有）
    if (!ControlExists(RigHierarchy, TEXT("base_root"))) {
        FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                  TEXT("base_root"), TEXT(""));
        CreatedCount++;
        UE_LOG(LogTemp, Warning, TEXT("Created controller: base_root"));
    }

    // 2. 创建 controller_root
    if (!ControlExists(RigHierarchy, TEXT("controller_root"))) {
        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, TEXT("controller_root"), TEXT("base_root"));
        CreatedCount++;
        UE_LOG(LogTemp, Warning, TEXT("Created controller: controller_root"));
    }

    // 3. 创建手部控制器
    TArray<FString> HandControllers = {
        TEXT("H_L"), TEXT("HP_L"), TEXT("H_rotation_L"),
        TEXT("H_R"), TEXT("HP_R"), TEXT("H_rotation_R")};

    for (const FString& ControllerName : HandControllers) {
        if (!ControlExists(RigHierarchy, ControllerName)) {
            FControlRigCreationUtility::CreateControl(
                ControlRigBlueprint, ControllerName, TEXT("controller_root"));
            CreatedCount++;
            UE_LOG(LogTemp, Warning, TEXT("Created controller: %s"),
                   *ControllerName);
        }
    }

    // 4. 创建手指控制器和极向量控制器
    // 手指前缀：I_(index), M_(middle), R_(ring), P_(pinky)
    // 极向量后缀：_pole
    TArray<FString> FingerPrefixes = {TEXT("I"), TEXT("M"), TEXT("R"),
                                      TEXT("P")};
    TArray<FString> HandSuffixes = {TEXT("_L"), TEXT("_R")};

    for (const FString& FingerPrefix : FingerPrefixes) {
        for (const FString& HandSuffix : HandSuffixes) {
            // 创建手指控制器，父级为 H_
            FString FingerControllerName = FingerPrefix + HandSuffix;
            FString ParentControllerName = TEXT("H") + HandSuffix;

            if (!ControlExists(RigHierarchy, FingerControllerName)) {
                FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                          FingerControllerName,
                                                          ParentControllerName);
                CreatedCount++;
                UE_LOG(LogTemp, Warning,
                       TEXT("Created finger controller: %s (parent: %s)"),
                       *FingerControllerName, *ParentControllerName);
            }

            // 创建手指极向量控制器
            FString FingerPoleControllerName =
                FingerPrefix + TEXT("_pole") + HandSuffix;

            if (!ControlExists(RigHierarchy, FingerPoleControllerName)) {
                FControlRigCreationUtility::CreateControl(
                    ControlRigBlueprint, FingerPoleControllerName,
                    ParentControllerName);
                CreatedCount++;
                UE_LOG(LogTemp, Warning,
                       TEXT("Created finger pole controller: %s (parent: %s)"),
                       *FingerPoleControllerName, *ParentControllerName);
            }
        }
    }

    // 5. 创建脚部控制器
    TArray<FString> FootControllers = {
        TEXT("F_L"), TEXT("F_rotation_L"), TEXT("F_pole_L"),
        TEXT("F_R"), TEXT("F_rotation_R"), TEXT("F_pole_R")};

    for (const FString& ControllerName : FootControllers) {
        if (!ControlExists(RigHierarchy, ControllerName)) {
            FControlRigCreationUtility::CreateControl(
                ControlRigBlueprint, ControllerName, TEXT("controller_root"));
            CreatedCount++;
            UE_LOG(LogTemp, Warning, TEXT("Created controller: %s"),
                   *ControllerName);
        }
    }

    // 6. 创建目标控制器
    TArray<FString> TargetControllers = {TEXT("Tar_Body"), TEXT("Tar_Chest"),
                                         TEXT("Tar_Head")};

    for (const FString& ControllerName : TargetControllers) {
        if (!ControlExists(RigHierarchy, ControllerName)) {
            FControlRigCreationUtility::CreateControl(
                ControlRigBlueprint, ControllerName, TEXT("controller_root"));
            CreatedCount++;
            UE_LOG(LogTemp, Warning, TEXT("Created controller: %s"),
                   *ControllerName);
        }
    }

    return CreatedCount;
}

void UBeatBloomControlRigProcessor::CheckObjectsStatus(
    ABeatBloomUnreal* BeatBloomActor) {
    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for check objects status"));
        return;
    }

    UControlRigBlueprint* ControlRigBlueprint =
        BeatBloomActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ControlRigBlueprint for Performer"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigBlueprint"));
        return;
    }

    // 收集所有预期的控制器名称
    TSet<FString> ExpectedControllers;
    ExpectedControllers.Add(TEXT("base_root"));
    ExpectedControllers.Add(TEXT("controller_root"));
    ExpectedControllers.Append({TEXT("H_L"), TEXT("HP_L"), TEXT("H_rotation_L"),
                                TEXT("H_R"), TEXT("HP_R"), TEXT("H_rotation_R"),
                                TEXT("F_L"), TEXT("F_rotation_L"), TEXT("F_R"),
                                TEXT("F_rotation_R"), TEXT("Tar_Body"),
                                TEXT("Tar_Chest"), TEXT("Tar_Head")});

    // 验证层次结构中的所有控制器
    TArray<FString> ExistingControllers;
    TArray<FString> MissingControllers;

    for (const FString& ControllerName : ExpectedControllers) {
        if (ControlExists(RigHierarchy, ControllerName)) {
            ExistingControllers.Add(ControllerName);
        } else {
            MissingControllers.Add(ControllerName);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("BeatBloom 对象状态报告"));
    UE_LOG(LogTemp, Warning, TEXT("========================"));
    UE_LOG(LogTemp, Warning, TEXT("预期控制器总数：%d"),
           ExpectedControllers.Num());
    UE_LOG(LogTemp, Warning, TEXT("存在的控制器数量：%d"),
           ExistingControllers.Num());
    UE_LOG(LogTemp, Warning, TEXT("缺失的控制器数量：%d"),
           MissingControllers.Num());

    if (ExistingControllers.Num() > 0) {
        UE_LOG(LogTemp, Warning, TEXT("存在的控制器:"));
        for (const FString& CtrlName : ExistingControllers) {
            UE_LOG(LogTemp, Warning, TEXT("  ✓ %s"), *CtrlName);
        }
    }

    if (MissingControllers.Num() > 0) {
        UE_LOG(LogTemp, Warning, TEXT("缺失的控制器:"));
        for (const FString& CtrlName : MissingControllers) {
            UE_LOG(LogTemp, Warning, TEXT("  ✗ %s"), *CtrlName);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("========================"));
}

void UBeatBloomControlRigProcessor::SetupAllObjects(
    ABeatBloomUnreal* BeatBloomActor) {
    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for setup drummer control rig"));
        return;
    }

    // 检查是否已加载 drumkit 配置
    if (BeatBloomActor->DrumKitConfig.Components.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: Please load .drumkit config file first!"));
        return;
    }

    // 获取 ControlRigBlueprint
    UControlRigBlueprint* ControlRigBlueprint =
        BeatBloomActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ControlRigBlueprint for Performer"));
        return;
    }

    // 创建控制器
    int32 CreatedCount = SetupControllers(BeatBloomActor, ControlRigBlueprint);
    UE_LOG(LogTemp, Warning, TEXT("BeatBloom: Created %d controllers"),
           CreatedCount);

    // 添加 Bone Control Mapping 变量
    FBoneControlMappingUtility::AddBoneControlMappingVariable(
        ControlRigBlueprint, BeatBloomActor);
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Added Bone Control Mapping variable"));

    // RecorderTransforms 已经在 LoadDrumKitConfig 时初始化，这里不再重复

    // 初始化鼓组同步关系（controller_root <-> drumkit_control）
    // 这会在开始每帧同步前计算并缓存相对变换矩阵
    UBeatBloomTransformSyncProcessor::InitializeDrumKitSync(BeatBloomActor);

    // 调用 CheckObjectsStatus 验证完整性
    CheckObjectsStatus(BeatBloomActor);

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: All objects have been set up successfully"));
}

#undef LOCTEXT_NAMESPACE
