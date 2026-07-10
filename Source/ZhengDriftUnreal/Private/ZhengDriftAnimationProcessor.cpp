#include "ZhengDriftAnimationProcessor.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "InstrumentAnimationUtility.h"
#include "Json.h"
#include "LevelSequence.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MovieScene.h"
#include "Serialization/JsonSerializer.h"
#include "ZhengDriftMusicInstrumentProcessor.h"

#define LOCTEXT_NAMESPACE "ZhengDriftAnimationProcessor"

// ============================================================
// ParseZhengDriftConfigFile
// ============================================================

bool UZhengDriftAnimationProcessor::ParseZhengDriftConfigFile(
    AZhengDriftUnreal* ZhengDriftActor, FString& OutPerformanceAnimationPath,
    FString& OutTargetAnimationPath, FString& OutStringAnimationPath,
    FString& OutActivityCurvePath) {
    OutPerformanceAnimationPath.Empty();
    OutTargetAnimationPath.Empty();
    OutStringAnimationPath.Empty();
    OutActivityCurvePath.Empty();

    if (!ZhengDriftActor) {
        UE_LOG(LogTemp, Error,
               TEXT("ParseZhengDriftConfigFile: Actor is null"));
        return false;
    }

    if (ZhengDriftActor->AnimationFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("ParseZhengDriftConfigFile: AnimationFilePath is empty"));
        return false;
    }

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent,
                                       *ZhengDriftActor->AnimationFilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("ParseZhengDriftConfigFile: Failed to load %s"),
               *ZhengDriftActor->AnimationFilePath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("ParseZhengDriftConfigFile: JSON parse failed for %s"),
               *ZhengDriftActor->AnimationFilePath);
        return false;
    }

    // 配置文件所在目录（用于相对路径解析）
    FString ConfigDir = FPaths::GetPath(ZhengDriftActor->AnimationFilePath);

    auto ResolvePath = [&](const FString& Key, FString& OutPath) {
        FString Raw;
        if (!Root->TryGetStringField(*Key, Raw)) return;
        if (FPaths::IsRelative(Raw)) {
            OutPath = FPaths::Combine(ConfigDir, Raw);
        } else {
            OutPath = Raw;
        }
    };

    ResolvePath(TEXT("performance_animation"), OutPerformanceAnimationPath);
    ResolvePath(TEXT("target_animation"), OutTargetAnimationPath);
    ResolvePath(TEXT("string_animation"), OutStringAnimationPath);
    ResolvePath(TEXT("activity_curve_path"), OutActivityCurvePath);

    bool bAnyValid = !OutPerformanceAnimationPath.IsEmpty() ||
                     !OutTargetAnimationPath.IsEmpty() ||
                     !OutStringAnimationPath.IsEmpty();

    UE_LOG(LogTemp, Warning,
           TEXT("ParseZhengDriftConfigFile: performance='%s' target='%s' "
                "string='%s' activity_curve='%s'"),
           *OutPerformanceAnimationPath, *OutTargetAnimationPath,
           *OutStringAnimationPath, *OutActivityCurvePath);

    return bAnyValid;
}

// ============================================================
// GeneratePerformerAnimation
// ============================================================

void UZhengDriftAnimationProcessor::GeneratePerformerAnimation(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor) return;

    FString PerformancePath, TargetPath, StringPath, ActivityCurvePath;
    if (!ParseZhengDriftConfigFile(ZhengDriftActor, PerformancePath, TargetPath,
                                   StringPath, ActivityCurvePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("GeneratePerformerAnimation: Config parse failed"));
        return;
    }

    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error,
               TEXT("GeneratePerformerAnimation: No active LevelSequence"));
        return;
    }

    // 清理演奏者旧关键帧
    if (ZhengDriftActor->SkeletalMeshActor) {
        UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
            ZhengDriftActor->SkeletalMeshActor,
            TArray<FString>{TEXT("F_L"), TEXT("F_R"), TEXT("F_L_pole"),
                            TEXT("F_R_pole")});
    }

    // ====== 第一步：收集所有数据（先不写入） ======
    TMap<FString, TArray<FAnimationKeyframe>> AllControlKeyframeData;

    if (!PerformancePath.IsEmpty()) {
        CollectPerformerKeyframes(PerformancePath, AllControlKeyframeData);
    }

    if (!TargetPath.IsEmpty()) {
        CollectTargetKeyframes(TargetPath, AllControlKeyframeData);
    }

    // ====== 第二步：一次性写入 Control Rig ======
    if (AllControlKeyframeData.Num() > 0 &&
        ZhengDriftActor->SkeletalMeshActor) {
        UControlRig* ControlRig =
            ZhengDriftActor->GetCachedControlRig(TEXT("Performer"));
        if (ControlRig) {
            FBatchInsertKeyframesSettings Settings;
            UInstrumentAnimationUtility::BatchInsertControlRigKeys(
                LevelSequence, ControlRig, AllControlKeyframeData, Settings);
            UE_LOG(LogTemp, Warning,
                   TEXT("GeneratePerformerAnimation: Wrote keyframes for %d "
                        "controllers (performer + target)"),
                   AllControlKeyframeData.Num());
        }
    }

    // 写入 active curve
    if (!ActivityCurvePath.IsEmpty() && ZhengDriftActor->SkeletalMeshActor) {
        UE_LOG(LogTemp, Warning, TEXT("Writing active curve from: %s"),
               *ActivityCurvePath);
        UInstrumentAnimationUtility::WriteActiveCurveFromFile(
            ZhengDriftActor->SkeletalMeshActor, ActivityCurvePath,
            LevelSequence);
    }
}

// ============================================================
// GenerateInstrumentAnimation
// ============================================================

void UZhengDriftAnimationProcessor::GenerateInstrumentAnimation(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor) return;

    FString PerformancePath, TargetPath, StringPath, ActivityCurvePath;
    if (!ParseZhengDriftConfigFile(ZhengDriftActor, PerformancePath, TargetPath,
                                   StringPath, ActivityCurvePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation: Config parse failed"));
        return;
    }

    if (StringPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("GenerateInstrumentAnimation: No string_animation path"));
        return;
    }

    UZhengDriftMusicInstrumentProcessor::GenerateInstrumentAnimation(
        ZhengDriftActor, StringPath);
}

// ============================================================
// GenerateAllAnimation
// ============================================================

void UZhengDriftAnimationProcessor::GenerateAllAnimation(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor) return;

    FString PerformancePath, TargetPath, StringPath, ActivityCurvePath;
    if (!ParseZhengDriftConfigFile(ZhengDriftActor, PerformancePath, TargetPath,
                                   StringPath, ActivityCurvePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateAllAnimation: Config parse failed"));
        return;
    }

    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateAllAnimation: No active LevelSequence"));
        return;
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("========== ZhengDrift GenerateAllAnimation Started =========="));

    // 1. 演奏者手部动画 — 先收集数据
    TMap<FString, TArray<FAnimationKeyframe>> AllControlKeyframeData;

    if (ZhengDriftActor->SkeletalMeshActor) {
        UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
            ZhengDriftActor->SkeletalMeshActor,
            TArray<FString>{TEXT("F_L"), TEXT("F_R"), TEXT("F_L_pole"),
                            TEXT("F_R_pole")});
    }

    if (!PerformancePath.IsEmpty()) {
        CollectPerformerKeyframes(PerformancePath, AllControlKeyframeData);
    }

    // 2. Target 动画 — 继续收集到同一 map
    if (!TargetPath.IsEmpty()) {
        CollectTargetKeyframes(TargetPath, AllControlKeyframeData);
    }

    // 3. 一次性写入所有控制器的关键帧
    if (AllControlKeyframeData.Num() > 0 &&
        ZhengDriftActor->SkeletalMeshActor) {
        UControlRig* ControlRig =
            ZhengDriftActor->GetCachedControlRig(TEXT("Performer"));
        if (ControlRig) {
            FBatchInsertKeyframesSettings Settings;
            UInstrumentAnimationUtility::BatchInsertControlRigKeys(
                LevelSequence, ControlRig, AllControlKeyframeData, Settings);
            UE_LOG(LogTemp, Warning,
                   TEXT("GenerateAllAnimation: Wrote keyframes for %d "
                        "controllers (performer + target)"),
                   AllControlKeyframeData.Num());
        }
    }

    // 4. 弦振动动画
    if (!StringPath.IsEmpty()) {
        UZhengDriftMusicInstrumentProcessor::GenerateInstrumentAnimation(
            ZhengDriftActor, StringPath);
    }

    // 5. 写入 active curve
    if (!ActivityCurvePath.IsEmpty() && ZhengDriftActor->SkeletalMeshActor) {
        UE_LOG(LogTemp, Warning, TEXT("Writing active curve from: %s"),
               *ActivityCurvePath);
        UInstrumentAnimationUtility::WriteActiveCurveFromFile(
            ZhengDriftActor->SkeletalMeshActor, ActivityCurvePath,
            LevelSequence);
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT(
            "========== ZhengDrift GenerateAllAnimation Completed =========="));
}

// ============================================================
// CollectPerformerKeyframes
// ============================================================

void UZhengDriftAnimationProcessor::CollectPerformerKeyframes(
    const FString& AnimationFilePath,
    TMap<FString, TArray<FAnimationKeyframe>>& OutControlKeyframeData) {
    if (AnimationFilePath.IsEmpty()) return;

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *AnimationFilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("CollectPerformerKeyframes: Failed to load '%s'"),
               *AnimationFilePath);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("CollectPerformerKeyframes: JSON parse failed for '%s'"),
               *AnimationFilePath);
        return;
    }

    // 处理单侧手部数据的 lambda
    auto ProcessHand = [&](const TArray<TSharedPtr<FJsonValue>>& HandArray,
                           const FString& Suffix) {
        int32 ProcessedFrames = 0;

        for (const auto& FrameVal : HandArray) {
            TSharedPtr<FJsonObject> FrameObj = FrameVal->AsObject();
            if (!FrameObj.IsValid()) continue;

            double FrameDouble = 0.0;
            FrameObj->TryGetNumberField(TEXT("frame"), FrameDouble);

            // 直接使用原始帧号（秒数），由 BatchInsertControlRigKeys 统一转换
            int32 FrameNumber =
                static_cast<int32>(FMath::RoundToInt(FrameDouble));

            // hand_infos 字段（新格式）：{"H_L": [x,y,z], "H_rotation_L":
            // [w,x,y,z], ...}
            if (FrameObj->HasField(TEXT("hand_infos"))) {
                TSharedPtr<FJsonObject> HandInfos =
                    FrameObj->GetObjectField(TEXT("hand_infos"));
                for (const auto& Entry : HandInfos->Values) {
                    const FString& CtrlName = Entry.Key;
                    const TArray<TSharedPtr<FJsonValue>>& Arr =
                        Entry.Value->AsArray();

                    if (CtrlName.Contains(TEXT("rotation"))) {
                        // 旋转四元数 [w, x, y, z]
                        // 注意：旋转必须合并到对应的 H_{Suffix} 控件的 keyframe
                        // 中， 而不是写入单独的 "H_rotation_L" 控件。
                        if (Arr.Num() == 4) {
                            FQuat Q;
                            Q.W = Arr[0]->AsNumber();
                            Q.X = Arr[1]->AsNumber();
                            Q.Y = Arr[2]->AsNumber();
                            Q.Z = Arr[3]->AsNumber();

                            // 从 "H_rotation_L" 解析出 "H_L"
                            FString TargetCtrlName = CtrlName;
                            TargetCtrlName.RemoveFromStart(TEXT("H_rotation_"));
                            TargetCtrlName = TEXT("H_") + TargetCtrlName;

                            TArray<FAnimationKeyframe>& KFs =
                                OutControlKeyframeData.FindOrAdd(
                                    TargetCtrlName);
                            // 尝试合并到同帧已有 keyframe
                            bool bFound = false;
                            for (int32 i = KFs.Num() - 1; i >= 0; i--) {
                                if (KFs[i].FrameNumber == FrameNumber) {
                                    KFs[i].Rotation = Q;
                                    KFs[i].bHasRotation = true;
                                    bFound = true;
                                    break;
                                }
                            }
                            // 如果没有同帧 keyframe（rotation 先于 position
                            // 到达），创建占位
                            if (!bFound) {
                                FAnimationKeyframe KF;
                                KF.FrameNumber = FrameNumber;
                                KF.Rotation = Q;
                                KF.bHasRotation = true;
                                KFs.Add(KF);
                            }
                        }
                    } else {
                        // 位置 [x, y, z]
                        if (Arr.Num() == 3) {
                            bool bHasNull = false;
                            for (const auto& Elem : Arr) {
                                if (!Elem.IsValid() || Elem->IsNull()) {
                                    bHasNull = true;
                                    break;
                                }
                            }
                            if (!bHasNull) {
                                FVector Loc(Arr[0]->AsNumber(),
                                            Arr[1]->AsNumber(),
                                            Arr[2]->AsNumber());

                                // 查找是否已有同帧 keyframe（如 rotation 占位）
                                TArray<FAnimationKeyframe>& KFs =
                                    OutControlKeyframeData.FindOrAdd(CtrlName);
                                bool bMerged = false;
                                for (int32 i = KFs.Num() - 1; i >= 0; i--) {
                                    if (KFs[i].FrameNumber == FrameNumber) {
                                        KFs[i].Translation = Loc;
                                        KFs[i].bHasLocation = true;
                                        bMerged = true;
                                        break;
                                    }
                                }
                                if (!bMerged) {
                                    FAnimationKeyframe KF;
                                    KF.FrameNumber = FrameNumber;
                                    KF.Translation = Loc;
                                    KF.bHasLocation = true;
                                    KFs.Add(KF);
                                }
                            }
                        }
                    }
                }
            }
            // 旧格式兼容：hand_position / hand_rotation / hand_pole_target /
            // finger_positions
            else {
                // hand_position → H_{Suffix}（合并到同帧 keyframe，避免覆盖已有
                // rotation）
                if (FrameObj->HasField(TEXT("hand_position"))) {
                    TSharedPtr<FJsonValue> PosVal =
                        FrameObj->TryGetField(TEXT("hand_position"));
                    if (PosVal.IsValid() && !PosVal->IsNull()) {
                        TArray<TSharedPtr<FJsonValue>> Arr =
                            FrameObj->GetArrayField(TEXT("hand_position"));
                        if (Arr.Num() == 3) {
                            bool bHasNull = false;
                            for (const auto& Elem : Arr) {
                                if (!Elem.IsValid() || Elem->IsNull()) {
                                    bHasNull = true;
                                    break;
                                }
                            }
                            if (!bHasNull) {
                                FVector Loc(Arr[0]->AsNumber(),
                                            Arr[1]->AsNumber(),
                                            Arr[2]->AsNumber());
                                FString CtrlName = TEXT("H_") + Suffix;
                                TArray<FAnimationKeyframe>& KFs =
                                    OutControlKeyframeData.FindOrAdd(CtrlName);
                                // 查找同帧 keyframe 合并
                                bool bMerged = false;
                                for (int32 i = KFs.Num() - 1; i >= 0; i--) {
                                    if (KFs[i].FrameNumber == FrameNumber) {
                                        KFs[i].Translation = Loc;
                                        KFs[i].bHasLocation = true;
                                        bMerged = true;
                                        break;
                                    }
                                }
                                if (!bMerged) {
                                    FAnimationKeyframe KF;
                                    KF.FrameNumber = FrameNumber;
                                    KF.Translation = Loc;
                                    KF.bHasLocation = true;
                                    KFs.Add(KF);
                                }
                            }
                        }
                    }
                }

                // hand_rotation → H_{Suffix}（合并到同帧 keyframe）
                if (FrameObj->HasField(TEXT("hand_rotation"))) {
                    TSharedPtr<FJsonValue> RotVal =
                        FrameObj->TryGetField(TEXT("hand_rotation"));
                    if (RotVal.IsValid() && !RotVal->IsNull()) {
                        TArray<TSharedPtr<FJsonValue>> Arr =
                            FrameObj->GetArrayField(TEXT("hand_rotation"));
                        if (Arr.Num() == 4) {
                            FQuat Q;
                            Q.W = Arr[0]->AsNumber();
                            Q.X = Arr[1]->AsNumber();
                            Q.Y = Arr[2]->AsNumber();
                            Q.Z = Arr[3]->AsNumber();

                            FString CtrlName = TEXT("H_") + Suffix;
                            TArray<FAnimationKeyframe>& KFs =
                                OutControlKeyframeData.FindOrAdd(CtrlName);
                            // 遍历查找同帧 keyframe（不依赖 KFs.Last()）
                            bool bFound = false;
                            for (int32 i = KFs.Num() - 1; i >= 0; i--) {
                                if (KFs[i].FrameNumber == FrameNumber) {
                                    KFs[i].Rotation = Q;
                                    KFs[i].bHasRotation = true;
                                    bFound = true;
                                    break;
                                }
                            }
                            // 没有同帧 keyframe 时创建占位
                            if (!bFound) {
                                FAnimationKeyframe KF;
                                KF.FrameNumber = FrameNumber;
                                KF.Rotation = Q;
                                KF.bHasRotation = true;
                                KFs.Add(KF);
                            }
                        }
                    }
                }

                // hand_pole_target → HP_{Suffix}
                if (FrameObj->HasField(TEXT("hand_pole_target"))) {
                    TSharedPtr<FJsonValue> PoleVal =
                        FrameObj->TryGetField(TEXT("hand_pole_target"));
                    if (PoleVal.IsValid() && !PoleVal->IsNull()) {
                        TArray<TSharedPtr<FJsonValue>> Arr =
                            FrameObj->GetArrayField(TEXT("hand_pole_target"));
                        if (Arr.Num() == 3) {
                            bool bHasNull = false;
                            for (const auto& Elem : Arr) {
                                if (!Elem.IsValid() || Elem->IsNull()) {
                                    bHasNull = true;
                                    break;
                                }
                            }
                            if (!bHasNull) {
                                FVector Loc(Arr[0]->AsNumber(),
                                            Arr[1]->AsNumber(),
                                            Arr[2]->AsNumber());
                                FString CtrlName = TEXT("HP_") + Suffix;
                                FAnimationKeyframe KF;
                                KF.FrameNumber = FrameNumber;
                                KF.Translation = Loc;
                                KF.bHasLocation = true;
                                OutControlKeyframeData.FindOrAdd(CtrlName).Add(
                                    KF);
                            }
                        }
                    }
                }

                // finger_positions → T/I/M/R/P_{Suffix}（旧格式）
                if (FrameObj->HasField(TEXT("finger_positions"))) {
                    TSharedPtr<FJsonValue> FingersVal =
                        FrameObj->TryGetField(TEXT("finger_positions"));
                    if (FingersVal.IsValid() && !FingersVal->IsNull()) {
                        TSharedPtr<FJsonObject> Fingers =
                            FrameObj->GetObjectField(TEXT("finger_positions"));

                        TArray<TPair<FString, FString>> FingerMap = {
                            {TEXT("thumb"), TEXT("T_")},
                            {TEXT("index"), TEXT("I_")},
                            {TEXT("middle"), TEXT("M_")},
                            {TEXT("ring"), TEXT("R_")},
                            {TEXT("pinky"), TEXT("P_")},
                        };

                        for (const auto& FM : FingerMap) {
                            if (!Fingers->HasField(*FM.Key)) continue;
                            TSharedPtr<FJsonValue> FingerVal =
                                Fingers->TryGetField(*FM.Key);
                            if (!FingerVal.IsValid() || FingerVal->IsNull())
                                continue;

                            TArray<TSharedPtr<FJsonValue>> Arr =
                                Fingers->GetArrayField(*FM.Key);
                            if (Arr.Num() != 3) continue;

                            bool bHasNull = false;
                            for (const auto& Elem : Arr) {
                                if (!Elem.IsValid() || Elem->IsNull()) {
                                    bHasNull = true;
                                    break;
                                }
                            }
                            if (bHasNull) continue;

                            FVector Loc(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                                        Arr[2]->AsNumber());
                            FString CtrlName = FM.Value + Suffix;
                            FAnimationKeyframe KF;
                            KF.FrameNumber = FrameNumber;
                            KF.Translation = Loc;
                            KF.bHasLocation = true;
                            OutControlKeyframeData.FindOrAdd(CtrlName).Add(KF);
                        }
                    }
                }
            }

            ProcessedFrames++;
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "CollectPerformerKeyframes: Processed %d frames for hand '%s'"),
            ProcessedFrames, *Suffix);
    };

    if (Root->HasField(TEXT("left_hand")))
        ProcessHand(Root->GetArrayField(TEXT("left_hand")), TEXT("L"));

    if (Root->HasField(TEXT("right_hand")))
        ProcessHand(Root->GetArrayField(TEXT("right_hand")), TEXT("R"));
}

// ============================================================
// MakePerformerAnimation（旧接口保留，内部调用 Collector + 写入）
// ============================================================

void UZhengDriftAnimationProcessor::MakePerformerAnimation(
    AZhengDriftUnreal* ZhengDriftActor, const FString& AnimationFilePath,
    ULevelSequence* LevelSequence) {
    if (!ZhengDriftActor || AnimationFilePath.IsEmpty() || !LevelSequence)
        return;

    // Step 1: 收集数据
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;
    CollectPerformerKeyframes(AnimationFilePath, ControlKeyframeData);

    // Step 2: 写入 Control Rig
    if (ControlKeyframeData.Num() > 0 && ZhengDriftActor->SkeletalMeshActor) {
        UControlRig* ControlRig =
            ZhengDriftActor->GetCachedControlRig(TEXT("Performer"));
        if (ControlRig) {
            FBatchInsertKeyframesSettings Settings;
            UInstrumentAnimationUtility::BatchInsertControlRigKeys(
                LevelSequence, ControlRig, ControlKeyframeData, Settings);
            UE_LOG(LogTemp, Warning,
                   TEXT("MakePerformerAnimation: Wrote keyframes for %d "
                        "controllers"),
                   ControlKeyframeData.Num());
        }
    }
}

// ============================================================
// CollectTargetKeyframes
// ============================================================

void UZhengDriftAnimationProcessor::CollectTargetKeyframes(
    const FString& TargetAnimationPath,
    TMap<FString, TArray<FAnimationKeyframe>>& OutControlKeyframeData) {
    if (TargetAnimationPath.IsEmpty()) return;

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *TargetAnimationPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("CollectTargetKeyframes: Failed to load '%s'"),
               *TargetAnimationPath);
        return;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) {
        UE_LOG(LogTemp, Error,
               TEXT("CollectTargetKeyframes: JSON parse failed for '%s'"),
               *TargetAnimationPath);
        return;
    }

    int32 ProcessedFrames = 0;
    for (const auto& Val : JsonArray) {
        TSharedPtr<FJsonObject> FrameObj = Val->AsObject();
        if (!FrameObj.IsValid()) continue;

        double FrameDouble = 0.0;
        FrameObj->TryGetNumberField(TEXT("frame"), FrameDouble);

        int32 FrameNumber = static_cast<int32>(FMath::RoundToInt(FrameDouble));

        // Head_Control 有动画数据
        TArray<TPair<FString, FString>> TargetMap = {
            {TEXT("head_control_position"), TEXT("Head_Control")},
        };

        for (const auto& TM : TargetMap) {
            if (!FrameObj->HasField(*TM.Key)) continue;
            TArray<TSharedPtr<FJsonValue>> Arr =
                FrameObj->GetArrayField(*TM.Key);
            if (Arr.Num() != 3) continue;

            bool bHasNull = false;
            for (const auto& Elem : Arr) {
                if (!Elem.IsValid() || Elem->IsNull()) {
                    bHasNull = true;
                    break;
                }
            }
            if (bHasNull) continue;

            FVector Loc(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                        Arr[2]->AsNumber());
            FAnimationKeyframe KF;
            KF.FrameNumber = FrameNumber;
            KF.Translation = Loc;
            KF.bHasLocation = true;
            OutControlKeyframeData.FindOrAdd(TM.Value).Add(KF);
        }
        ProcessedFrames++;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("CollectTargetKeyframes: Processed %d frames for Head_Control"),
           ProcessedFrames);
}

// ============================================================
// MakeTargetAnimation（旧接口保留，内部调用 Collector + 写入）
// ============================================================

void UZhengDriftAnimationProcessor::MakeTargetAnimation(
    AZhengDriftUnreal* ZhengDriftActor, const FString& TargetAnimationPath,
    ULevelSequence* LevelSequence) {
    if (!ZhengDriftActor || TargetAnimationPath.IsEmpty() || !LevelSequence)
        return;

    // Step 1: 收集数据
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;
    CollectTargetKeyframes(TargetAnimationPath, ControlKeyframeData);

    // Step 2: 写入 Control Rig
    if (ControlKeyframeData.Num() > 0 && ZhengDriftActor->SkeletalMeshActor) {
        UControlRig* ControlRig =
            ZhengDriftActor->GetCachedControlRig(TEXT("Performer"));
        if (ControlRig) {
            FBatchInsertKeyframesSettings Settings;
            UInstrumentAnimationUtility::BatchInsertControlRigKeys(
                LevelSequence, ControlRig, ControlKeyframeData, Settings);
            UE_LOG(LogTemp, Warning,
                   TEXT("MakeTargetAnimation: Wrote keyframes for %d target "
                        "controllers (Head_Control)"),
                   ControlKeyframeData.Num());
        }
    }
}

#undef LOCTEXT_NAMESPACE
