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
    AZhengDriftUnreal* ZhengDriftActor,
    FString& OutPerformanceAnimationPath,
    FString& OutTargetAnimationPath,
    FString& OutStringAnimationPath) {
    OutPerformanceAnimationPath.Empty();
    OutTargetAnimationPath.Empty();
    OutStringAnimationPath.Empty();

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
    if (!FFileHelper::LoadFileToString(
            FileContent, *ZhengDriftActor->AnimationFilePath)) {
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
    FString ConfigDir =
        FPaths::GetPath(ZhengDriftActor->AnimationFilePath);

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
    ResolvePath(TEXT("target_animation"),      OutTargetAnimationPath);
    ResolvePath(TEXT("string_animation"),      OutStringAnimationPath);

    bool bAnyValid = !OutPerformanceAnimationPath.IsEmpty() ||
                     !OutTargetAnimationPath.IsEmpty() ||
                     !OutStringAnimationPath.IsEmpty();

    UE_LOG(LogTemp, Warning,
           TEXT("ParseZhengDriftConfigFile: performance='%s' target='%s' string='%s'"),
           *OutPerformanceAnimationPath, *OutTargetAnimationPath,
           *OutStringAnimationPath);

    return bAnyValid;
}

// ============================================================
// GeneratePerformerAnimation
// ============================================================

void UZhengDriftAnimationProcessor::GeneratePerformerAnimation(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor) return;

    FString PerformancePath, TargetPath, StringPath;
    if (!ParseZhengDriftConfigFile(ZhengDriftActor, PerformancePath,
                                    TargetPath, StringPath)) {
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
            ZhengDriftActor->SkeletalMeshActor);
    }

    if (!PerformancePath.IsEmpty()) {
        MakePerformerAnimation(ZhengDriftActor, PerformancePath, LevelSequence);
    }

    if (!TargetPath.IsEmpty()) {
        MakeTargetAnimation(ZhengDriftActor, TargetPath, LevelSequence);
    }
}

// ============================================================
// GenerateInstrumentAnimation
// ============================================================

void UZhengDriftAnimationProcessor::GenerateInstrumentAnimation(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor) return;

    FString PerformancePath, TargetPath, StringPath;
    if (!ParseZhengDriftConfigFile(ZhengDriftActor, PerformancePath,
                                    TargetPath, StringPath)) {
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

    FString PerformancePath, TargetPath, StringPath;
    if (!ParseZhengDriftConfigFile(ZhengDriftActor, PerformancePath,
                                    TargetPath, StringPath)) {
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

    UE_LOG(LogTemp, Warning,
           TEXT("========== ZhengDrift GenerateAllAnimation Started =========="));

    // 1. 演奏者手部动画
    if (!PerformancePath.IsEmpty()) {
        if (ZhengDriftActor->SkeletalMeshActor) {
            UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
                ZhengDriftActor->SkeletalMeshActor);
        }
        MakePerformerAnimation(ZhengDriftActor, PerformancePath, LevelSequence);
    }

    // 2. Target 动画
    if (!TargetPath.IsEmpty()) {
        MakeTargetAnimation(ZhengDriftActor, TargetPath, LevelSequence);
    }

    // 3. 弦振动动画
    if (!StringPath.IsEmpty()) {
        UZhengDriftMusicInstrumentProcessor::GenerateInstrumentAnimation(
            ZhengDriftActor, StringPath);
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== ZhengDrift GenerateAllAnimation Completed =========="));
}

// ============================================================
// MakePerformerAnimation
// ============================================================

void UZhengDriftAnimationProcessor::MakePerformerAnimation(
    AZhengDriftUnreal* ZhengDriftActor,
    const FString& AnimationFilePath,
    ULevelSequence* LevelSequence) {
    if (!ZhengDriftActor || AnimationFilePath.IsEmpty() || !LevelSequence)
        return;

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *AnimationFilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("MakePerformerAnimation: Failed to load '%s'"),
               *AnimationFilePath);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("MakePerformerAnimation: JSON parse failed for '%s'"),
               *AnimationFilePath);
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) return;

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate    = MovieScene->GetDisplayRate();

    // 收集关键帧数据
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;

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
            int32 FrameNumber = static_cast<int32>(FMath::RoundToInt(FrameDouble));

            // hand_position → H_{Suffix}
            if (FrameObj->HasField(TEXT("hand_position"))) {
                TSharedPtr<FJsonValue> PosVal = FrameObj->TryGetField(TEXT("hand_position"));
                if (PosVal.IsValid() && !PosVal->IsNull()) {
                    TArray<TSharedPtr<FJsonValue>> Arr =
                        FrameObj->GetArrayField(TEXT("hand_position"));
                    if (Arr.Num() == 3) {
                        // 检查是否有 null 值
                        bool bHasNull = false;
                        for (const auto& Elem : Arr) {
                            if (!Elem.IsValid() || Elem->IsNull()) {
                                bHasNull = true;
                                break;
                            }
                        }
                        if (!bHasNull) {
                            FVector Loc(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                                        Arr[2]->AsNumber());
                            FString CtrlName = TEXT("H_") + Suffix;
                            FAnimationKeyframe KF;
                            KF.FrameNumber  = FrameNumber;
                            KF.Translation  = Loc;          // NOTE: field is Translation
                            KF.bHasLocation = true;
                            ControlKeyframeData.FindOrAdd(CtrlName).Add(KF);
                        }
                    }
                }
            }

            // hand_rotation → H_{Suffix}
            if (FrameObj->HasField(TEXT("hand_rotation"))) {
                TSharedPtr<FJsonValue> RotVal = FrameObj->TryGetField(TEXT("hand_rotation"));
                if (RotVal.IsValid() && !RotVal->IsNull()) {
                    TArray<TSharedPtr<FJsonValue>> Arr =
                        FrameObj->GetArrayField(TEXT("hand_rotation"));
                    if (Arr.Num() == 4) {
                        // JSON 中的四元数是 WXYZ 顺序（全端统一约定）
                        FQuat Q;
                        Q.W = Arr[0]->AsNumber();
                        Q.X = Arr[1]->AsNumber();
                        Q.Y = Arr[2]->AsNumber();
                        Q.Z = Arr[3]->AsNumber();
                        
                        FString CtrlName = TEXT("H_") + Suffix;
                        TArray<FAnimationKeyframe>& KFs =
                            ControlKeyframeData.FindOrAdd(CtrlName);
                        if (KFs.Num() > 0 &&
                            KFs.Last().FrameNumber == FrameNumber) {
                            KFs.Last().Rotation     = Q;
                            KFs.Last().bHasRotation = true;
                        }
                    }
                }
            }

            // hand_pole_target → HP_{Suffix}
            if (FrameObj->HasField(TEXT("hand_pole_target"))) {
                TSharedPtr<FJsonValue> PoleVal = FrameObj->TryGetField(TEXT("hand_pole_target"));
                if (PoleVal.IsValid() && !PoleVal->IsNull()) {
                    TArray<TSharedPtr<FJsonValue>> Arr =
                        FrameObj->GetArrayField(TEXT("hand_pole_target"));
                    if (Arr.Num() == 3) {
                        // 检查是否有 null 值
                        bool bHasNull = false;
                        for (const auto& Elem : Arr) {
                            if (!Elem.IsValid() || Elem->IsNull()) {
                                bHasNull = true;
                                break;
                            }
                        }
                        if (!bHasNull) {
                            FVector Loc(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                                        Arr[2]->AsNumber());
                            FString CtrlName = TEXT("HP_") + Suffix;
                            FAnimationKeyframe KF;
                            KF.FrameNumber  = FrameNumber;
                            KF.Translation  = Loc;
                            KF.bHasLocation = true;
                            ControlKeyframeData.FindOrAdd(CtrlName).Add(KF);
                        }
                    }
                }
            }

            // finger_positions → T/I/M/R/P_{Suffix}
            if (FrameObj->HasField(TEXT("finger_positions"))) {
                TSharedPtr<FJsonValue> FingersVal = FrameObj->TryGetField(TEXT("finger_positions"));
                if (FingersVal.IsValid() && !FingersVal->IsNull()) {
                    TSharedPtr<FJsonObject> Fingers =
                        FrameObj->GetObjectField(TEXT("finger_positions"));

                    TArray<TPair<FString, FString>> FingerMap = {
                        {TEXT("thumb"),  TEXT("T_")},
                        {TEXT("index"),  TEXT("I_")},
                        {TEXT("middle"), TEXT("M_")},
                        {TEXT("ring"),   TEXT("R_")},
                        {TEXT("pinky"),  TEXT("P_")},
                    };

                    for (const auto& FM : FingerMap) {
                        if (!Fingers->HasField(*FM.Key)) continue;
                        TSharedPtr<FJsonValue> FingerVal = Fingers->TryGetField(*FM.Key);
                        if (!FingerVal.IsValid() || FingerVal->IsNull()) continue;
                        
                        TArray<TSharedPtr<FJsonValue>> Arr =
                            Fingers->GetArrayField(*FM.Key);
                        if (Arr.Num() != 3) continue;

                        // 检查是否有 null 值
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
                        KF.FrameNumber  = FrameNumber;
                        KF.Translation  = Loc;
                        KF.bHasLocation = true;
                        ControlKeyframeData.FindOrAdd(CtrlName).Add(KF);
                    }
                }
            }

            ProcessedFrames++;
        }

        UE_LOG(LogTemp, Warning,
               TEXT("MakePerformerAnimation: Processed %d frames for hand '%s'"),
               ProcessedFrames, *Suffix);
    };

    if (Root->HasField(TEXT("left_hand")))
        ProcessHand(Root->GetArrayField(TEXT("left_hand")), TEXT("L"));

    if (Root->HasField(TEXT("right_hand")))
        ProcessHand(Root->GetArrayField(TEXT("right_hand")), TEXT("R"));

    // 写入关键帧
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
// MakeTargetAnimation
// ============================================================

void UZhengDriftAnimationProcessor::MakeTargetAnimation(
    AZhengDriftUnreal* ZhengDriftActor,
    const FString& TargetAnimationPath,
    ULevelSequence* LevelSequence) {
    if (!ZhengDriftActor || TargetAnimationPath.IsEmpty() || !LevelSequence)
        return;

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *TargetAnimationPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("MakeTargetAnimation: Failed to load '%s'"),
               *TargetAnimationPath);
        return;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) {
        UE_LOG(LogTemp, Error,
               TEXT("MakeTargetAnimation: JSON parse failed for '%s'"),
               *TargetAnimationPath);
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) return;

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate    = MovieScene->GetDisplayRate();

    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;

    for (const auto& Val : JsonArray) {
        TSharedPtr<FJsonObject> FrameObj = Val->AsObject();
        if (!FrameObj.IsValid()) continue;

        double FrameDouble = 0.0;
        FrameObj->TryGetNumberField(TEXT("frame"), FrameDouble);

        // 直接使用原始帧号（秒数），由 BatchInsertControlRigKeys 统一转换
        int32 FrameNumber = static_cast<int32>(FMath::RoundToInt(FrameDouble));

        // Head_Control 有动画数据，Middle_Hand 由 Control Rig 自动计算，Look_At 通过父子关系跟随
        TArray<TPair<FString, FString>> TargetMap = {
            {TEXT("head_control_position"), TEXT("Head_Control")},
        };

        for (const auto& TM : TargetMap) {
            if (!FrameObj->HasField(*TM.Key)) continue;
            TArray<TSharedPtr<FJsonValue>> Arr =
                FrameObj->GetArrayField(*TM.Key);
            if (Arr.Num() != 3) continue;

            // 检查是否有 null 值
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
            KF.FrameNumber  = FrameNumber;
            KF.Translation  = Loc;
            KF.bHasLocation = true;
            ControlKeyframeData.FindOrAdd(TM.Value).Add(KF);
        }
    }

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
