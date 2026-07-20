#include "WindRiseAnimationProcessor.h"

#include "ControlRig.h"
#include "Dom/JsonObject.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "LevelSequence.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "WindRiseControlRigProcessor.h"
#include "WindRiseMusicInstrumentProcessor.h"
#include "WindRiseUnreal.h"

// ============================================================
// GenerateAnimationFromWindRise（入口）
// ============================================================

void UWindRiseAnimationProcessor::GenerateAnimationFromWindRise(
    AWindRiseUnreal* WindRiseActor, const FString& WindRiseFilePath) {
    if (!WindRiseActor) {
        UE_LOG(LogTemp, Error,
               TEXT("WindRiseAnimProcessor: WindRiseActor is null"));
        return;
    }

    // ── 读取并解析 .wind_rise 汇总文件 ──
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *WindRiseFilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("WindRiseAnimProcessor: Failed to load .wind_rise file: "
                    "%s"),
               *WindRiseFilePath);
        return;
    }

    TSharedPtr<FJsonObject> RootObj;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, RootObj)) {
        UE_LOG(LogTemp, Error,
               TEXT("WindRiseAnimProcessor: Failed to parse .wind_rise JSON"));
        return;
    }

    FString LeftHandPath, RightHandPath, CharSKPath, InstSKPath, ActivityPath;
    RootObj->TryGetStringField(TEXT("left_hand_animation_file"), LeftHandPath);
    RootObj->TryGetStringField(TEXT("right_hand_animation_file"),
                               RightHandPath);
    RootObj->TryGetStringField(TEXT("character_sk_animation_file"), CharSKPath);
    RootObj->TryGetStringField(TEXT("instrument_sk_animation_file"),
                               InstSKPath);
    RootObj->TryGetStringField(TEXT("activity_curve_file"), ActivityPath);

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("WindRiseAnimProcessor: No active Level Sequence"));
        return;
    }

    // ── ① 生成手部 Control Rig 动画 ──
    GenerateHandAnimation(WindRiseActor, LeftHandPath, RightHandPath,
                          LevelSequence);

    // ── ② 生成人物 MT 动画（写入 Breath_Control，共用一个 Section） ──
    GenerateCharacterMorphTargetAnimation(WindRiseActor, CharSKPath,
                                          LevelSequence);

    // ── ③ 生成乐器 MT 动画（写入 wind_root，独立的 ControlRig） ──
    GenerateInstrumentMorphTargetAnimation(WindRiseActor, InstSKPath,
                                           LevelSequence);

    // ── ④ 写入活动曲线（如果有） ──
    // WindRise 专用逻辑：写入 controller_root_offset 的 Transform
    //   曲线值=1 → FTransform::Identity（活动，位置归零，回到绑定姿势）
    //   曲线值=0 → RestOffset（不活动，回到休息偏移状态）
    if (!ActivityPath.IsEmpty()) {
        FString ActJson;
        if (FFileHelper::LoadFileToString(ActJson, *ActivityPath)) {
            TArray<TSharedPtr<FJsonValue>> ActFrames;
            TSharedRef<TJsonReader<>> ActReader =
                TJsonReaderFactory<>::Create(ActJson);
            if (FJsonSerializer::Deserialize(ActReader, ActFrames) &&
                ActFrames.Num() > 0) {
                UControlRig* PerformerCR =
                    WindRiseActor->GetCachedControlRig(TEXT("Performer"));
                if (PerformerCR) {
                    TMap<FString, TArray<FAnimationKeyframe>> OffsetData;
                    TArray<FAnimationKeyframe>& OffsetKeys =
                        OffsetData.FindOrAdd(TEXT("controller_root_offset"));

                    for (const auto& EntryVal : ActFrames) {
                        TSharedPtr<FJsonObject> Entry = EntryVal->AsObject();
                        if (!Entry.IsValid()) continue;

                        double FrameDouble = 0.0, Value = 0.0;
                        Entry->TryGetNumberField(TEXT("frame"), FrameDouble);
                        Entry->TryGetNumberField(TEXT("value"), Value);

                        FAnimationKeyframe KF;
                        KF.FrameNumber = FMath::RoundToInt((float)FrameDouble);

                        if (FMath::IsNearlyEqual((float)Value, 1.0f)) {
                            // 活动 → 零变换（归零到绑定姿势）
                            KF.Translation = FVector::ZeroVector;
                            KF.Rotation = FQuat::Identity;
                        } else {
                            // 不活动 → RestOffset（休息偏移）
                            KF.Translation =
                                WindRiseActor->RestOffset.GetLocation();
                            KF.Rotation =
                                WindRiseActor->RestOffset.GetRotation();
                        }
                        KF.bHasLocation = true;
                        KF.bHasRotation = true;

                        OffsetKeys.Add(KF);
                    }

                    if (OffsetKeys.Num() > 0) {
                        FBatchInsertKeyframesSettings Settings;
                        UInstrumentAnimationUtility::BatchInsertControlRigKeys(
                            LevelSequence, PerformerCR, OffsetData, Settings);
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Log,
           TEXT("WindRiseAnimProcessor: Animation generated from %s"),
           *WindRiseFilePath);
}

// ============================================================
// 手部动画生成
// ============================================================

bool UWindRiseAnimationProcessor::GenerateHandAnimation(
    AWindRiseUnreal* WindRiseActor, const FString& LeftHandPath,
    const FString& RightHandPath, ULevelSequence* LevelSequence) {
    if (!WindRiseActor || LeftHandPath.IsEmpty() || RightHandPath.IsEmpty())
        return false;

    // 收集所有手部控制器名称
    TMap<FString, TArray<FAnimationKeyframe>> HandKeyframes;

    auto LoadHandAnimFile = [&](const FString& FilePath) {
        FString AnimJson;
        if (!FFileHelper::LoadFileToString(AnimJson, *FilePath)) return;

        TArray<TSharedPtr<FJsonValue>> Frames;
        TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(AnimJson);
        if (!FJsonSerializer::Deserialize(R, Frames)) return;

        for (const auto& FrameVal : Frames) {
            const TSharedPtr<FJsonObject>* FrameObj = nullptr;
            if (!FrameVal->TryGetObject(FrameObj)) continue;

            double FrameNum = 0;
            (*FrameObj)->TryGetNumberField(TEXT("frame"), FrameNum);

            const TSharedPtr<FJsonObject>* HandInfos = nullptr;
            if (!(*FrameObj)->TryGetObjectField(TEXT("hand_infos"), HandInfos))
                continue;

            for (const auto& InfoPair : HandInfos->Get()->Values) {
                const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                if (!InfoPair.Value->TryGetArray(Values) || Values->Num() < 7)
                    continue;

                FAnimationKeyframe Keyframe;
                Keyframe.FrameNumber = FMath::RoundToInt((float)FrameNum);
                Keyframe.Translation.X = (float)(*Values)[0]->AsNumber();
                Keyframe.Translation.Y = (float)(*Values)[1]->AsNumber();
                Keyframe.Translation.Z = (float)(*Values)[2]->AsNumber();
                Keyframe.Rotation.W = (float)(*Values)[3]->AsNumber();
                Keyframe.Rotation.X = (float)(*Values)[4]->AsNumber();
                Keyframe.Rotation.Y = (float)(*Values)[5]->AsNumber();
                Keyframe.Rotation.Z = (float)(*Values)[6]->AsNumber();
                Keyframe.bHasLocation = true;
                Keyframe.bHasRotation = true;

                HandKeyframes.FindOrAdd(InfoPair.Key).Add(Keyframe);
            }
        }
    };

    LoadHandAnimFile(LeftHandPath);
    LoadHandAnimFile(RightHandPath);

    UControlRig* CR = WindRiseActor->GetCachedControlRig(TEXT("Performer"));
    if (CR && HandKeyframes.Num() > 0) {
        UInstrumentAnimationUtility::BatchInsertControlRigKeys(
            LevelSequence, CR, HandKeyframes);
        return true;
    }

    return false;
}

// ============================================================
// 人物 Morph Target 动画生成
// ============================================================

bool UWindRiseAnimationProcessor::GenerateCharacterMorphTargetAnimation(
    AWindRiseUnreal* WindRiseActor, const FString& CharSKPath,
    ULevelSequence* LevelSequence) {
    if (!WindRiseActor || CharSKPath.IsEmpty() ||
        !WindRiseActor->SkeletalMeshActor)
        return false;

    FString AnimJson;
    if (!FFileHelper::LoadFileToString(AnimJson, *CharSKPath)) return false;

    TArray<TSharedPtr<FJsonValue>> Keyframes;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(AnimJson);
    if (!FJsonSerializer::Deserialize(R, Keyframes)) return false;

    // 整理为 FMorphTargetKeyframeData
    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("WindRiseAnimProcessor: MovieScene is null"));
        return false;
    }
    FFrameRate TickRes = MovieScene->GetTickResolution();
    FFrameRate DispRate = MovieScene->GetDisplayRate();

    TMap<FString, FMorphTargetKeyframeData> MTDataMap;

    for (const auto& KF : Keyframes) {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!KF->TryGetObject(Obj)) continue;

        FString SKName;
        double SKValue = 0, Frame = 0;
        (*Obj)->TryGetStringField(TEXT("shape_key_name"), SKName);
        (*Obj)->TryGetNumberField(TEXT("value"), SKValue);
        (*Obj)->TryGetNumberField(TEXT("frame"), Frame);

        int32 ScaledFrame =
            FMath::RoundToInt(Frame * TickRes.Numerator * DispRate.Denominator /
                              (TickRes.Denominator * DispRate.Numerator));

        FMorphTargetKeyframeData& Data = MTDataMap.FindOrAdd(SKName);
        Data.MorphTargetName = SKName;
        Data.FrameNumbers.Add(FFrameNumber(ScaledFrame));
        Data.Values.Add((float)SKValue);
    }

    TArray<FMorphTargetKeyframeData> FinalData;
    for (auto& Pair : MTDataMap) {
        FinalData.Add(Pair.Value);
    }

    // 写入 Breath_Control
    UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig(
        WindRiseActor->SkeletalMeshActor, FinalData, LevelSequence,
        TEXT("Breath_Control"));

    return true;
}

// ============================================================
// 乐器 Morph Target 动画生成
// ============================================================

bool UWindRiseAnimationProcessor::GenerateInstrumentMorphTargetAnimation(
    AWindRiseUnreal* WindRiseActor, const FString& InstSKPath,
    ULevelSequence* LevelSequence) {
    if (!WindRiseActor || InstSKPath.IsEmpty() ||
        !WindRiseActor->InstrumentMesh)
        return false;

    FString AnimJson;
    if (!FFileHelper::LoadFileToString(AnimJson, *InstSKPath)) return false;

    TArray<TSharedPtr<FJsonValue>> Keyframes;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(AnimJson);
    if (!FJsonSerializer::Deserialize(R, Keyframes)) return false;

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("WindRiseAnimProcessor: MovieScene is null"));
        return false;
    }
    FFrameRate TickRes = MovieScene->GetTickResolution();
    FFrameRate DispRate = MovieScene->GetDisplayRate();

    TMap<FString, FMorphTargetKeyframeData> MTDataMap;

    for (const auto& KF : Keyframes) {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!KF->TryGetObject(Obj)) continue;

        FString SKName;
        double SKValue = 0, Frame = 0;
        (*Obj)->TryGetStringField(TEXT("shape_key_name"), SKName);
        (*Obj)->TryGetNumberField(TEXT("value"), SKValue);
        (*Obj)->TryGetNumberField(TEXT("frame"), Frame);

        int32 ScaledFrame =
            FMath::RoundToInt(Frame * TickRes.Numerator * DispRate.Denominator /
                              (TickRes.Denominator * DispRate.Numerator));

        FMorphTargetKeyframeData& Data = MTDataMap.FindOrAdd(SKName);
        Data.MorphTargetName = SKName;
        Data.FrameNumbers.Add(FFrameNumber(ScaledFrame));
        Data.Values.Add((float)SKValue);
    }

    TArray<FMorphTargetKeyframeData> FinalData;
    for (auto& Pair : MTDataMap) {
        FinalData.Add(Pair.Value);
    }

    // 写入 wind_root
    UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig(
        WindRiseActor->InstrumentMesh, FinalData, LevelSequence,
        TEXT("wind_root"));

    return true;
}
