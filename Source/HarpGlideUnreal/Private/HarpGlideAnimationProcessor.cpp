#include "HarpGlideAnimationProcessor.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HarpGlideMusicInstrumentProcessor.h"
#include "InstrumentAnimationUtility.h"
#include "Json.h"
#include "LevelSequence.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MovieScene.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "HarpGlideAnimationProcessor"

// ============================================================
// ParseHarpGlideConfigFile
// ============================================================

bool UHarpGlideAnimationProcessor::ParseHarpGlideConfigFile(
    AHarpGlideUnreal* HarpGlideActor, FString& OutPerformanceAnimationPath,
    FString& OutHarpAnimationPath, FString& OutStringAnimationPath,
    FString& OutPedalShapeAnimationPath) {
    OutPerformanceAnimationPath.Empty();
    OutHarpAnimationPath.Empty();
    OutStringAnimationPath.Empty();
    OutPedalShapeAnimationPath.Empty();

    if (!HarpGlideActor) {
        UE_LOG(LogTemp, Error, TEXT("ParseHarpGlideConfigFile: Actor is null"));
        return false;
    }

    if (HarpGlideActor->AnimationFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("ParseHarpGlideConfigFile: AnimationFilePath is empty"));
        return false;
    }

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent,
                                       *HarpGlideActor->AnimationFilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("ParseHarpGlideConfigFile: Failed to load %s"),
               *HarpGlideActor->AnimationFilePath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("ParseHarpGlideConfigFile: JSON parse failed for %s"),
               *HarpGlideActor->AnimationFilePath);
        return false;
    }

    // 配置文件所在目录（用于相对路径解析）
    FString ConfigDir = FPaths::GetPath(HarpGlideActor->AnimationFilePath);

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
    ResolvePath(TEXT("harp_animation"), OutHarpAnimationPath);
    ResolvePath(TEXT("string_animation"), OutStringAnimationPath);
    ResolvePath(TEXT("pedal_shape_animation"), OutPedalShapeAnimationPath);

    bool bAnyValid = !OutPerformanceAnimationPath.IsEmpty() ||
                     !OutHarpAnimationPath.IsEmpty() ||
                     !OutStringAnimationPath.IsEmpty() ||
                     !OutPedalShapeAnimationPath.IsEmpty();

    UE_LOG(LogTemp, Warning,
           TEXT("ParseHarpGlideConfigFile: perf='%s' harp='%s' string='%s' "
                "pedal='%s'"),
           *OutPerformanceAnimationPath, *OutHarpAnimationPath,
           *OutStringAnimationPath, *OutPedalShapeAnimationPath);

    return bAnyValid;
}

// ============================================================
// GeneratePerformerAnimation
// ============================================================

void UHarpGlideAnimationProcessor::GeneratePerformerAnimation(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!HarpGlideActor) return;

    FString PerfPath, HarpPath, StringPath, PedalShapePath;
    if (!ParseHarpGlideConfigFile(HarpGlideActor, PerfPath, HarpPath,
                                  StringPath, PedalShapePath)) {
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
    if (HarpGlideActor->SkeletalMeshActor) {
        UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
            HarpGlideActor->SkeletalMeshActor);
    }

    if (!PerfPath.IsEmpty()) {
        MakePerformanceAnimation(HarpGlideActor, PerfPath, LevelSequence);
    }

    if (!HarpPath.IsEmpty()) {
        MakeHarpAnimation(HarpGlideActor, HarpPath, LevelSequence);
    }

    // 与 FretDance 对齐：写入关键帧后触发 ControlRig 重新注册
    HarpGlideActor->TriggerControlRigReregistration(
        TEXT("Generate Performer Animation"));
}

// ============================================================
// GenerateInstrumentAnimation
// ============================================================

void UHarpGlideAnimationProcessor::GenerateInstrumentAnimation(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!HarpGlideActor) return;

    FString PerfPath, HarpPath, StringPath, PedalShapePath;
    if (!ParseHarpGlideConfigFile(HarpGlideActor, PerfPath, HarpPath,
                                  StringPath, PedalShapePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation: Config parse failed"));
        return;
    }

    if (StringPath.IsEmpty() && PedalShapePath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("GenerateInstrumentAnimation: No instrument animation "
                    "paths configured"));
        return;
    }

    // 先清理竖琴旧轨道，再一次性写入所有 Morph Target 动画
    UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
        HarpGlideActor->Harp);

    // 弦振动 + 踏板 Shape Key 合并为一次 Write 调用
    UHarpGlideMusicInstrumentProcessor::GenerateInstrumentAnimation(
        HarpGlideActor, StringPath, PedalShapePath);
}

// ============================================================
// GenerateAllAnimation
// ============================================================

void UHarpGlideAnimationProcessor::GenerateAllAnimation(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!HarpGlideActor) return;

    FString PerfPath, HarpPath, StringPath, PedalShapePath;
    if (!ParseHarpGlideConfigFile(HarpGlideActor, PerfPath, HarpPath,
                                  StringPath, PedalShapePath)) {
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
        TEXT("========== HarpGlide GenerateAllAnimation Started =========="));

    // 1. 演奏者动画（双手 + 双脚 + 头部）
    if (!PerfPath.IsEmpty()) {
        if (HarpGlideActor->SkeletalMeshActor) {
            UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
                HarpGlideActor->SkeletalMeshActor);
        }
        MakePerformanceAnimation(HarpGlideActor, PerfPath, LevelSequence);
    }

    // 2. 竖琴倾斜动画（harp_pivot）
    if (!HarpPath.IsEmpty()) {
        MakeHarpAnimation(HarpGlideActor, HarpPath, LevelSequence);
    }

    // 3. 弦振动 + 踏板 Shape Key 合并为一次写入
    if (!StringPath.IsEmpty() || !PedalShapePath.IsEmpty()) {
        UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
            HarpGlideActor->Harp);

        UHarpGlideMusicInstrumentProcessor::GenerateInstrumentAnimation(
            HarpGlideActor, StringPath, PedalShapePath);
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("========== HarpGlide GenerateAllAnimation Completed =========="));
}

// ============================================================
// MakePerformanceAnimation
//
// 从 performance JSON 生成关键帧：
//   双手: H_L/R (位置+旋转), T/I/M/R/P_L/R (位置), HP_L/R (位置)
//   双脚: F_L, F_R (位置+旋转)
//   头部: Head (位置+旋转)
// ============================================================

void UHarpGlideAnimationProcessor::MakePerformanceAnimation(
    AHarpGlideUnreal* HarpGlideActor, const FString& AnimationFilePath,
    ULevelSequence* LevelSequence) {
    if (!HarpGlideActor || AnimationFilePath.IsEmpty() || !LevelSequence)
        return;

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *AnimationFilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("MakePerformanceAnimation: Failed to load '%s'"),
               *AnimationFilePath);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("MakePerformanceAnimation: JSON parse failed for '%s'"),
               *AnimationFilePath);
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) return;

    // 收集关键帧数据
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;

    // ── 处理单侧手部数据（扁平化 ControllerKeyframe 格式）──────────────
    // Rust 输出格式：{ controller_name, frame, state, transform: { position,
    // rotation } } controller_name 映射：h→H, thumb→T, index→I, middle→M,
    // ring→R, pinky→P, hp→HP
    auto ProcessHand = [&](const TArray<TSharedPtr<FJsonValue>>& HandArray,
                           const FString& Suffix) {
        TMap<FString, FString> CtrlNameMap = {
            {TEXT("h"), TEXT("H_")},     {TEXT("thumb"), TEXT("T_")},
            {TEXT("index"), TEXT("I_")}, {TEXT("middle"), TEXT("M_")},
            {TEXT("ring"), TEXT("R_")},  {TEXT("pinky"), TEXT("P_")},
            {TEXT("hp"), TEXT("HP_")},
        };

        int32 ProcessedFrames = 0;

        for (const auto& FrameVal : HandArray) {
            TSharedPtr<FJsonObject> FrameObj = FrameVal->AsObject();
            if (!FrameObj.IsValid()) continue;

            // 读取 controller_name
            FString ControllerName;
            if (!FrameObj->TryGetStringField(TEXT("controller_name"),
                                             ControllerName))
                continue;

            // 映射到控件前缀
            FString* Prefix = CtrlNameMap.Find(ControllerName);
            if (!Prefix) {
                UE_LOG(LogTemp, Warning,
                       TEXT("ProcessHand: Unknown controller '%s'"),
                       *ControllerName);
                continue;
            }

            // 读取帧号
            double FrameDouble = 0.0;
            FrameObj->TryGetNumberField(TEXT("frame"), FrameDouble);
            int32 FrameNumber =
                static_cast<int32>(FMath::RoundToInt(FrameDouble));

            // 读取 transform 嵌套对象
            if (!FrameObj->HasField(TEXT("transform"))) continue;
            const TSharedPtr<FJsonObject>& TransformObj =
                FrameObj->GetObjectField(TEXT("transform"));
            if (!TransformObj.IsValid()) continue;

            FString CtrlName = *Prefix + Suffix;
            FAnimationKeyframe KF;
            KF.FrameNumber = FrameNumber;
            KF.bHasLocation = false;
            KF.bHasRotation = false;

            // position
            if (TransformObj->HasField(TEXT("position"))) {
                TArray<TSharedPtr<FJsonValue>> Arr =
                    TransformObj->GetArrayField(TEXT("position"));
                if (Arr.Num() == 3) {
                    KF.Translation =
                        FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                                Arr[2]->AsNumber());
                    KF.bHasLocation = true;
                }
            }

            // rotation (WXYZ)
            if (TransformObj->HasField(TEXT("rotation"))) {
                TArray<TSharedPtr<FJsonValue>> Arr =
                    TransformObj->GetArrayField(TEXT("rotation"));
                if (Arr.Num() == 4) {
                    KF.Rotation.W = Arr[0]->AsNumber();
                    KF.Rotation.X = Arr[1]->AsNumber();
                    KF.Rotation.Y = Arr[2]->AsNumber();
                    KF.Rotation.Z = Arr[3]->AsNumber();
                    KF.bHasRotation = true;
                }
            }

            if (KF.bHasLocation || KF.bHasRotation) {
                ControlKeyframeData.FindOrAdd(CtrlName).Add(KF);
                ProcessedFrames++;
            }
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT("MakePerformanceAnimation: Processed %d frames for hand '%s'"),
            ProcessedFrames, *Suffix);
    };

    // ── 处理脚部数据 ──────────────────────────────────────────────
    auto ProcessFoot = [&](const TArray<TSharedPtr<FJsonValue>>& FootArray,
                           const FString& CtrlName) {
        int32 ProcessedFrames = 0;

        for (const auto& FrameVal : FootArray) {
            TSharedPtr<FJsonObject> FrameObj = FrameVal->AsObject();
            if (!FrameObj.IsValid()) continue;

            double FrameDouble = 0.0;
            FrameObj->TryGetNumberField(TEXT("frame"), FrameDouble);
            int32 FrameNumber =
                static_cast<int32>(FMath::RoundToInt(FrameDouble));

            bool bHasLocation = false;
            bool bHasRotation = false;
            FVector Loc(FVector::ZeroVector);
            FQuat Rot(FQuat::Identity);

            // foot_position
            if (FrameObj->HasField(TEXT("foot_position"))) {
                TArray<TSharedPtr<FJsonValue>> Arr =
                    FrameObj->GetArrayField(TEXT("foot_position"));
                if (Arr.Num() == 3) {
                    Loc = FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                                  Arr[2]->AsNumber());
                    bHasLocation = true;
                }
            }

            // foot_rotation (WXYZ)
            if (FrameObj->HasField(TEXT("foot_rotation"))) {
                TArray<TSharedPtr<FJsonValue>> Arr =
                    FrameObj->GetArrayField(TEXT("foot_rotation"));
                if (Arr.Num() == 4) {
                    Rot.W = Arr[0]->AsNumber();
                    Rot.X = Arr[1]->AsNumber();
                    Rot.Y = Arr[2]->AsNumber();
                    Rot.Z = Arr[3]->AsNumber();
                    bHasRotation = true;
                }
            }

            if (bHasLocation || bHasRotation) {
                FAnimationKeyframe KF;
                KF.FrameNumber = FrameNumber;
                KF.Translation = Loc;
                KF.Rotation = Rot;
                KF.bHasLocation = bHasLocation;
                KF.bHasRotation = bHasRotation;
                ControlKeyframeData.FindOrAdd(CtrlName).Add(KF);
                ProcessedFrames++;
            }
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT("MakePerformanceAnimation: Processed %d frames for foot '%s'"),
            ProcessedFrames, *CtrlName);
    };

    // ── 处理头部数据 ──────────────────────────────────────────────
    auto ProcessHead = [&](const TArray<TSharedPtr<FJsonValue>>& HeadArray) {
        int32 ProcessedFrames = 0;

        for (const auto& FrameVal : HeadArray) {
            TSharedPtr<FJsonObject> FrameObj = FrameVal->AsObject();
            if (!FrameObj.IsValid()) continue;

            double FrameDouble = 0.0;
            FrameObj->TryGetNumberField(TEXT("frame"), FrameDouble);
            int32 FrameNumber =
                static_cast<int32>(FMath::RoundToInt(FrameDouble));

            bool bHasLocation = false;
            bool bHasRotation = false;
            FVector Loc(FVector::ZeroVector);
            FQuat Rot(FQuat::Identity);

            // head_position (Rust 输出字段名为 location)
            if (FrameObj->HasField(TEXT("location"))) {
                TArray<TSharedPtr<FJsonValue>> Arr =
                    FrameObj->GetArrayField(TEXT("location"));
                if (Arr.Num() == 3) {
                    Loc = FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                                  Arr[2]->AsNumber());
                    bHasLocation = true;
                }
            }

            // head_rotation (Rust 输出字段名为 rotation, WXYZ)
            if (FrameObj->HasField(TEXT("rotation"))) {
                TArray<TSharedPtr<FJsonValue>> Arr =
                    FrameObj->GetArrayField(TEXT("rotation"));
                if (Arr.Num() == 4) {
                    Rot.W = Arr[0]->AsNumber();
                    Rot.X = Arr[1]->AsNumber();
                    Rot.Y = Arr[2]->AsNumber();
                    Rot.Z = Arr[3]->AsNumber();
                    bHasRotation = true;
                }
            }

            if (bHasLocation || bHasRotation) {
                FAnimationKeyframe KF;
                KF.FrameNumber = FrameNumber;
                KF.Translation = Loc;
                KF.Rotation = Rot;
                KF.bHasLocation = bHasLocation;
                KF.bHasRotation = bHasRotation;
                ControlKeyframeData.FindOrAdd(TEXT("Head")).Add(KF);
                ProcessedFrames++;
            }
        }

        UE_LOG(LogTemp, Warning,
               TEXT("MakePerformanceAnimation: Processed %d frames for Head"),
               ProcessedFrames);
    };

    // ── 执行数据提取 ──────────────────────────────────────────────

    if (Root->HasField(TEXT("left_hand")))
        ProcessHand(Root->GetArrayField(TEXT("left_hand")), TEXT("L"));

    if (Root->HasField(TEXT("right_hand")))
        ProcessHand(Root->GetArrayField(TEXT("right_hand")), TEXT("R"));

    if (Root->HasField(TEXT("left_foot")))
        ProcessFoot(Root->GetArrayField(TEXT("left_foot")), TEXT("F_L"));

    if (Root->HasField(TEXT("right_foot")))
        ProcessFoot(Root->GetArrayField(TEXT("right_foot")), TEXT("F_R"));

    if (Root->HasField(TEXT("head")))
        ProcessHead(Root->GetArrayField(TEXT("head")));

    // ── 批量写入关键帧 ──────────────────────────────────────────────
    if (ControlKeyframeData.Num() > 0 && HarpGlideActor->SkeletalMeshActor) {
        UControlRig* ControlRig =
            HarpGlideActor->GetCachedControlRig(TEXT("Performer"));
        if (ControlRig) {
            FBatchInsertKeyframesSettings Settings;
            UInstrumentAnimationUtility::BatchInsertControlRigKeys(
                LevelSequence, ControlRig, ControlKeyframeData, Settings);
            UE_LOG(LogTemp, Warning,
                   TEXT("MakePerformanceAnimation: Wrote keyframes for %d "
                        "controllers"),
                   ControlKeyframeData.Num());
        }
    }
}

// ============================================================
// MakeHarpAnimation
//
// 从 harp_animation JSON 生成 harp_pivot 关键帧
// JSON 格式：{ "frame": N, "location": [x,y,z], "rotation": [w,x,y,z] }
// ============================================================

void UHarpGlideAnimationProcessor::MakeHarpAnimation(
    AHarpGlideUnreal* HarpGlideActor, const FString& HarpAnimationPath,
    ULevelSequence* LevelSequence) {
    if (!HarpGlideActor || HarpAnimationPath.IsEmpty() || !LevelSequence)
        return;

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *HarpAnimationPath)) {
        UE_LOG(LogTemp, Error, TEXT("MakeHarpAnimation: Failed to load '%s'"),
               *HarpAnimationPath);
        return;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) {
        UE_LOG(LogTemp, Error,
               TEXT("MakeHarpAnimation: JSON parse failed for '%s'"),
               *HarpAnimationPath);
        return;
    }

    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;

    for (const auto& Val : JsonArray) {
        TSharedPtr<FJsonObject> FrameObj = Val->AsObject();
        if (!FrameObj.IsValid()) continue;

        double FrameDouble = 0.0;
        FrameObj->TryGetNumberField(TEXT("frame"), FrameDouble);
        int32 FrameNumber = static_cast<int32>(FMath::RoundToInt(FrameDouble));

        bool bHasLocation = false;
        bool bHasRotation = false;
        FVector Loc(FVector::ZeroVector);
        FQuat Rot(FQuat::Identity);

        if (FrameObj->HasField(TEXT("location"))) {
            TArray<TSharedPtr<FJsonValue>> Arr =
                FrameObj->GetArrayField(TEXT("location"));
            if (Arr.Num() == 3) {
                Loc = FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(),
                              Arr[2]->AsNumber());
                bHasLocation = true;
            }
        }

        if (FrameObj->HasField(TEXT("rotation"))) {
            TArray<TSharedPtr<FJsonValue>> Arr =
                FrameObj->GetArrayField(TEXT("rotation"));
            if (Arr.Num() == 4) {
                Rot.W = Arr[0]->AsNumber();
                Rot.X = Arr[1]->AsNumber();
                Rot.Y = Arr[2]->AsNumber();
                Rot.Z = Arr[3]->AsNumber();
                bHasRotation = true;
            }
        }

        if (bHasLocation || bHasRotation) {
            FAnimationKeyframe KF;
            KF.FrameNumber = FrameNumber;
            KF.Translation = Loc;
            KF.Rotation = Rot;
            KF.bHasLocation = bHasLocation;
            KF.bHasRotation = bHasRotation;
            ControlKeyframeData.FindOrAdd(TEXT("harp_pivot")).Add(KF);
        }
    }

    if (ControlKeyframeData.Num() > 0 && HarpGlideActor->SkeletalMeshActor) {
        UControlRig* ControlRig =
            HarpGlideActor->GetCachedControlRig(TEXT("Performer"));
        if (ControlRig) {
            FBatchInsertKeyframesSettings Settings;
            UInstrumentAnimationUtility::BatchInsertControlRigKeys(
                LevelSequence, ControlRig, ControlKeyframeData, Settings);
            UE_LOG(LogTemp, Warning,
                   TEXT("MakeHarpAnimation: Wrote %d harp_pivot keyframes"),
                   ControlKeyframeData[TEXT("harp_pivot")].Num());
        }
    }
}

#undef LOCTEXT_NAMESPACE
