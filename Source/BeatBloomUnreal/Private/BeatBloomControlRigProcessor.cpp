#include "BeatBloomControlRigProcessor.h"

#include "BeatBloomUnreal.h"
#include "BoneControlMappingUtility.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "LevelSequenceEditorBlueprintLibrary.h"

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
    UControlRig* ControlRig =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
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
    UControlRig* ControlRig =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
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

void UBeatBloomControlRigProcessor::SaveBilinearHelperState(
    ABeatBloomUnreal* BeatBloomActor, const FString& StateSuffix) {
    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor for SaveBilinearHelperState"));
        return;
    }

    if (StateSuffix.IsEmpty() ||
        (StateSuffix != TEXT("A") && StateSuffix != TEXT("B") &&
         StateSuffix != TEXT("C") && StateSuffix != TEXT("D"))) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: Invalid state suffix '%s'. Must be A/B/C/D"),
               *StateSuffix);
        return;
    }

    UControlRig* ControlRig =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(
            LogTemp, Error,
            TEXT("BeatBloom: Failed to get ControlRig instance for Performer"));
        return;
    }

    int32 SavedCount = 0;

    // ========== 1. 读取并保存左右手位置和旋转，计算 Middle_Hand 位置
    // ==========
    FVector LeftHandLocation, RightHandLocation;
    FQuat LeftHandRotation, RightHandRotation;

    bool bHasLeftHand = ReadControllerTransform(
        BeatBloomActor, TEXT("H_L"), LeftHandLocation, LeftHandRotation);
    bool bHasRightHand = ReadControllerTransform(
        BeatBloomActor, TEXT("H_R"), RightHandLocation, RightHandRotation);

    if (!bHasLeftHand || !bHasRightHand) {
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Cannot calculate Middle_Hand - missing hand "
                    "controllers"));
        return;
    }

    // 保存左手位置和旋转到 Left_Hand_{StateSuffix} 记录器
    FString LeftHandRecorderName = TEXT("Left_Hand_") + StateSuffix;
    FBeatBloomRecorderTransform LeftHandRecT;
    LeftHandRecT.Location = LeftHandLocation;
    LeftHandRecT.Rotation = LeftHandRotation;
    BeatBloomActor->RecorderTransforms.Add(LeftHandRecorderName, LeftHandRecT);
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Saved Left_Hand_%s at Loc(%.2f, %.2f, %.2f)"),
           *StateSuffix, LeftHandLocation.X, LeftHandLocation.Y,
           LeftHandLocation.Z);
    SavedCount++;

    // 保存右手位置和旋转到 Right_Hand_{StateSuffix} 记录器
    FString RightHandRecorderName = TEXT("Right_Hand_") + StateSuffix;
    FBeatBloomRecorderTransform RightHandRecT;
    RightHandRecT.Location = RightHandLocation;
    RightHandRecT.Rotation = RightHandRotation;
    BeatBloomActor->RecorderTransforms.Add(RightHandRecorderName,
                                           RightHandRecT);
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Saved Right_Hand_%s at Loc(%.2f, %.2f, %.2f)"),
           *StateSuffix, RightHandLocation.X, RightHandLocation.Y,
           RightHandLocation.Z);
    SavedCount++;

    // Middle_Hand = (H_L + H_R) / 2
    FVector MiddleHandLocation = (LeftHandLocation + RightHandLocation) / 2.0f;

    // 保存到 Middle_Hand_{StateSuffix} 记录器
    FString MiddleHandRecorderName = TEXT("Middle_Hand_") + StateSuffix;
    FBeatBloomRecorderTransform MiddleHandRecT;
    MiddleHandRecT.Location = MiddleHandLocation;
    MiddleHandRecT.Rotation = FQuat::Identity;
    BeatBloomActor->RecorderTransforms.Add(MiddleHandRecorderName,
                                           MiddleHandRecT);

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Saved Middle_Hand_%s at Loc(%.2f, %.2f, %.2f)"),
           *StateSuffix, MiddleHandLocation.X, MiddleHandLocation.Y,
           MiddleHandLocation.Z);
    SavedCount++;

    // ========== 2. 读取 Head_Control 位置 ==========
    FVector HeadControlLocation;
    FQuat DummyRotation;
    if (ReadControllerTransform(BeatBloomActor, TEXT("Head_Control"),
                                HeadControlLocation, DummyRotation)) {
        FString HeadControlRecorderName = TEXT("Head_Control_") + StateSuffix;
        FBeatBloomRecorderTransform HeadControlRecT;
        HeadControlRecT.Location = HeadControlLocation;
        HeadControlRecT.Rotation = FQuat::Identity;
        BeatBloomActor->RecorderTransforms.Add(HeadControlRecorderName,
                                               HeadControlRecT);

        UE_LOG(
            LogTemp, Warning,
            TEXT("BeatBloom: Saved Head_Control_%s at Loc(%.2f, %.2f, %.2f)"),
            *StateSuffix, HeadControlLocation.X, HeadControlLocation.Y,
            HeadControlLocation.Z);
        SavedCount++;
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: Failed to read Head_Control position"));
    }

    // Look_At 不需要保存(通过父子关系跟随 Middle_Hand)

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: SaveBilinearHelperState(%s) completed - Saved: %d"),
           *StateSuffix, SavedCount);
}

void UBeatBloomControlRigProcessor::LoadBilinearHelperState(
    ABeatBloomUnreal* BeatBloomActor, const FString& StateSuffix) {
    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor for LoadBilinearHelperState"));
        return;
    }

    if (StateSuffix.IsEmpty() ||
        (StateSuffix != TEXT("A") && StateSuffix != TEXT("B") &&
         StateSuffix != TEXT("C") && StateSuffix != TEXT("D"))) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: Invalid state suffix '%s'. Must be A/B/C/D"),
               *StateSuffix);
        return;
    }

    UControlRig* ControlRig =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(
            LogTemp, Error,
            TEXT("BeatBloom: Failed to get ControlRig instance for Performer"));
        return;
    }

    // ========== 1. 还原左手控制器（H_L）==========
    FString LeftHandRecorderName = TEXT("Left_Hand_") + StateSuffix;
    const FBeatBloomRecorderTransform* LeftHandData =
        BeatBloomActor->RecorderTransforms.Find(LeftHandRecorderName);
    if (LeftHandData) {
        WriteControllerTransform(BeatBloomActor, TEXT("H_L"),
                                 LeftHandData->Location,
                                 LeftHandData->Rotation);
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Loaded H_L from state '%s' at Loc(%.2f, %.2f, %.2f)"),
               *StateSuffix, LeftHandData->Location.X,
               LeftHandData->Location.Y, LeftHandData->Location.Z);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Left_Hand_%s not found in RecorderTransforms"),
               *StateSuffix);
    }

    // ========== 2. 还原右手控制器（H_R）==========
    FString RightHandRecorderName = TEXT("Right_Hand_") + StateSuffix;
    const FBeatBloomRecorderTransform* RightHandData =
        BeatBloomActor->RecorderTransforms.Find(RightHandRecorderName);
    if (RightHandData) {
        WriteControllerTransform(BeatBloomActor, TEXT("H_R"),
                                 RightHandData->Location,
                                 RightHandData->Rotation);
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Loaded H_R from state '%s' at Loc(%.2f, %.2f, %.2f)"),
               *StateSuffix, RightHandData->Location.X,
               RightHandData->Location.Y, RightHandData->Location.Z);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Right_Hand_%s not found in RecorderTransforms"),
               *StateSuffix);
    }

    // ========== 3. 还原 Head_Control 控制器 ==========
    FString HeadControlRecorderName = TEXT("Head_Control_") + StateSuffix;
    const FBeatBloomRecorderTransform* HeadControlData =
        BeatBloomActor->RecorderTransforms.Find(HeadControlRecorderName);
    if (HeadControlData) {
        FRigElementKey HeadControlKey(TEXT("Head_Control"),
                                      ERigElementType::Control);
        if (ControlRig->GetHierarchy()->Contains(HeadControlKey)) {
            FRigControlElement* HeadControlElem =
                ControlRig->GetHierarchy()->Find<FRigControlElement>(HeadControlKey);
            if (HeadControlElem) {
                FTransform NewT;
                NewT.SetLocation(HeadControlData->Location);
                FRigControlValue NewVal;
                NewVal.SetFromTransform(NewT,
                                        HeadControlElem->Settings.ControlType,
                                        HeadControlElem->Settings.PrimaryAxis);
                ControlRig->GetHierarchy()->SetControlValue(
                    HeadControlElem, NewVal, ERigControlValueType::Current);
                UE_LOG(LogTemp, Warning,
                       TEXT("BeatBloom: Loaded Head_Control from state '%s' at Loc(%.2f, %.2f, %.2f)"),
                       *StateSuffix, HeadControlData->Location.X,
                       HeadControlData->Location.Y, HeadControlData->Location.Z);
            }
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("BeatBloom: Head_Control controller not found in rig"));
        }
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Head_Control_%s not found in RecorderTransforms"),
               *StateSuffix);
    }

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: LoadBilinearHelperState(%s) completed"),
           *StateSuffix);
}

void UBeatBloomControlRigProcessor::SaveHeadControlState(
    ABeatBloomUnreal* BeatBloomActor) {
    // 保存 Head_Control 控制器的位置到对应的 Head_Control 记录器
    // 参考 Blender 版 transfer_state 中第四步的 Head_Control 处理

    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor for SaveHeadControlState"));
        return;
    }

    UControlRig* ControlRig =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(
            LogTemp, Error,
            TEXT("BeatBloom: Failed to get ControlRig instance for Performer"));
        return;
    }

    // 读取当前 Head_Control 控制器的位置
    FVector HeadControlLocation;
    FQuat DummyRotation;
    if (!ReadControllerTransform(BeatBloomActor, TEXT("Head_Control"),
                                 HeadControlLocation, DummyRotation)) {
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Cannot read Head_Control position"));
        return;
    }

    int32 SavedCount = 0;

    // 收集对应的 Head_Control 记录器名称（基于左手和右手的鼓件/状态）
    TSet<FString> HCRecorderNames;

    if (BeatBloomActor->CurrentLeftHandDrumKit == TEXT("Rest")) {
        HCRecorderNames.Add(TEXT("Head_Control_Rest"));
    } else if (!BeatBloomActor->CurrentLeftHandDrumKit.IsEmpty()) {
        FString RecorderName = BeatBloomActor->CurrentLeftHandDrumKit +
                               TEXT("_") +
                               ABeatBloomUnreal::GetStateString(
                                   BeatBloomActor->CurrentLeftHandState) +
                               TEXT("_Head_Control");
        HCRecorderNames.Add(RecorderName);
    }

    if (BeatBloomActor->CurrentRightHandDrumKit == TEXT("Rest")) {
        HCRecorderNames.Add(TEXT("Head_Control_Rest"));
    } else if (!BeatBloomActor->CurrentRightHandDrumKit.IsEmpty()) {
        FString RecorderName = BeatBloomActor->CurrentRightHandDrumKit +
                               TEXT("_") +
                               ABeatBloomUnreal::GetStateString(
                                   BeatBloomActor->CurrentRightHandState) +
                               TEXT("_Head_Control");
        HCRecorderNames.Add(RecorderName);
    }

    // 将 Head_Control 位置保存到对应记录器
    for (const FString& HCRecorderName : HCRecorderNames) {
        FBeatBloomRecorderTransform RecorderTransform;
        RecorderTransform.Location = HeadControlLocation;
        RecorderTransform.Rotation = FQuat::Identity;
        BeatBloomActor->RecorderTransforms.Add(HCRecorderName,
                                               RecorderTransform);

        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "BeatBloom: Saved Head_Control -> %s at Loc(%.2f, %.2f, %.2f)"),
            *HCRecorderName, HeadControlLocation.X, HeadControlLocation.Y,
            HeadControlLocation.Z);
        SavedCount++;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: SaveHeadControlState completed - Saved: %d"),
           SavedCount);
}

void UBeatBloomControlRigProcessor::SaveAllState(
    ABeatBloomUnreal* BeatBloomActor) {
    // 保存所有状态（手部 + 脚部 + 目标 + Head_Control）

    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor for SaveAllState"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("BeatBloom: Starting SaveAllState..."));

    // 依次调用各个保存方法
    SaveHandState(BeatBloomActor);
    SaveFootState(BeatBloomActor);
    SaveHeadControlState(BeatBloomActor);

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
    UControlRig* ControlRig =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
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
        } else if (ControllerName == TEXT("Head_Control")) {
            // Head_Control：仅加载位置，不加载旋转
            bLocationOnly = true;
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

    // 重新评估 Control Rig 以传播变更（约束、IK 等）
    // 注意：不能调用 ForceEvaluate / RefreshCurrentLevelSequence，
    // 否则 Sequencer 会重新从轨道读取关键帧数据，覆盖刚写入的值
    if (ControlRig) {
        ControlRig->Evaluate_AnyThread();
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

    UControlRig* ControlRig =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("ReadControllerTransform: Failed to get ControlRig "
                    "instance for Performer"));
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

    UControlRig* ControlRig =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("WriteControllerTransform: Failed to get ControlRig "
                    "instance for Performer"));
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
    // 手指前缀：T_(thumb),I_(index), M_(middle), R_(ring), P_(pinky)
    // 极向量后缀：_pole
    TArray<FString> FingerPrefixes = {TEXT("T"), TEXT("I"), TEXT("M"),
                                      TEXT("R"), TEXT("P")};
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

    // 6. 创建目标控制器（新的三控制器系统）
    // Middle_Hand 和 Head_Control 挂在 controller_root 下
    if (!ControlExists(RigHierarchy, TEXT("Middle_Hand"))) {
        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, TEXT("Middle_Hand"), TEXT("controller_root"));
        CreatedCount++;
        UE_LOG(LogTemp, Warning, TEXT("Created controller: Middle_Hand"));
    }

    if (!ControlExists(RigHierarchy, TEXT("Head_Control"))) {
        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, TEXT("Head_Control"), TEXT("controller_root"));
        CreatedCount++;
        UE_LOG(LogTemp, Warning, TEXT("Created controller: Head_Control"));
    }

    // Look_At 挂在 Middle_Hand 下
    if (!ControlExists(RigHierarchy, TEXT("Look_At"))) {
        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, TEXT("Look_At"), TEXT("Middle_Hand"));
        CreatedCount++;
        UE_LOG(LogTemp, Warning,
               TEXT("Created controller: Look_At (parent: Middle_Hand)"));
    }

    // 注意: 双线性映射辅助记录器(Middle_Hand_A/B/C/D, Head_Control_A/B/C/D)
    // 不需要创建 Control Rig 控制器,它们只在 RecorderTransforms 中作为数据存储

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

    // 手部控制器 (6个)
    ExpectedControllers.Append({TEXT("H_L"), TEXT("HP_L"), TEXT("H_rotation_L"),
                                TEXT("H_R"), TEXT("HP_R"),
                                TEXT("H_rotation_R")});

    // 脚部控制器 (4个)
    ExpectedControllers.Append(
        {TEXT("F_L"), TEXT("F_rotation_L"), TEXT("F_R"), TEXT("F_rotation_R")});

    // 目标控制器（新的三控制器系统）
    ExpectedControllers.Append(
        {TEXT("Middle_Hand"), TEXT("Look_At"), TEXT("Head_Control")});

    // 注意: 双线性映射辅助记录器(Middle_Hand_A/B/C/D, Head_Control_A/B/C/D)
    // 不需要创建 Control Rig 控制器,只在 RecorderTransforms 中作为数据存储

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
        UE_LOG(
            LogTemp, Error,
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

    // 调用 CheckObjectsStatus 验证完整性
    CheckObjectsStatus(BeatBloomActor);

    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: All objects have been set up successfully"));
}

#undef LOCTEXT_NAMESPACE
