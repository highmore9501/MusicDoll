#include "FretDanceMusicInstrumentProcessor.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "FretDanceUnreal.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "InstrumentMaterialUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "LevelSequence.h"
#include "MovieScene.h"

#define LOCTEXT_NAMESPACE "FretDanceMusicInstrumentProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public methods implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UFretDanceMusicInstrumentProcessor::InitializeGuitarInstrument(
    AFretDanceUnreal* FretDanceActor) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeGuitarInstrument: FretDanceActor is null"));
        return;
    }

    if (!FretDanceActor->Guitar) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeGuitarInstrument: Guitar is null"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeGuitarInstrument Started =========="));

    // 使用统一的注册方法注册所有 ControlRig（演奏者 + 吉他）
    FretDanceActor->RegisterAllControlRigs();

    // 1. 清理现有动画数据
    CleanupExistingGuitarAnimations(FretDanceActor);

    // 2. 初始化弦材质
    InitializeStringMaterials(FretDanceActor);

    // 3. 初始化弦材质参数动画轨道
    int32 MaterialTracksInitialized =
        InitializeStringMaterialAnimationTracks(FretDanceActor);

    // 4. 初始化弦振动动画通道（Morph Target）
    // 内部会通过 EnsureRootControlExists 创建 guitar_root（如果不存在），
    // 然后在其下批量添加 animation channels，修改 Blueprint Hierarchy。
    InitializeStringVibrationAnimationChannels(FretDanceActor);

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeGuitarInstrument Completed =========="));
    UE_LOG(LogTemp, Warning, TEXT("Initialized %d material animation tracks"),
           MaterialTracksInitialized);
}

void UFretDanceMusicInstrumentProcessor::InitializeStringMaterials(
    AFretDanceUnreal* FretDanceActor) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringMaterials: FretDanceActor is null"));
        return;
    }

    if (!FretDanceActor->Guitar) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringMaterials: Guitar is null"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterials Started =========="));

    // 使用 Common 模块的通用方法初始化弦材质
    // 假设吉他的弦骨骼命名为：string_0, string_1, ..., string_5

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterials Completed =========="));
}

int32 UFretDanceMusicInstrumentProcessor::
    InitializeStringMaterialAnimationTracks(AFretDanceUnreal* FretDanceActor) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringMaterialAnimationTracks: FretDanceActor "
                    "is null"));
        return 0;
    }

    // 获取 LevelSequence
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error, TEXT("请确保已打开 Level Sequence"));
        return 0;
    }

    if (!FretDanceActor->Guitar) {
        UE_LOG(
            LogTemp, Error,
            TEXT("Guitar is null in InitializeStringMaterialAnimationTracks"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterialAnimationTracks Started "
                "=========="));

    // 获取 Control Rig Instance - 使用缓存机制
    UControlRig* ControlRigInstance =
        FretDanceActor->GetCachedControlRig(TEXT("Guitar"));
    UControlRigBlueprint* ControlRigBlueprint =
        FretDanceActor->GetCachedControlRigBlueprint(TEXT("Guitar"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ControlRig for Guitar - cache miss"));
        return 0;
    }

    // 使用 Common 模块的通用方法初始化材质参数轨道
    int32 TracksInitialized = 0;

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterialAnimationTracks Summary "
                "=========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully initialized: %d tracks"),
           TracksInitialized);

    return TracksInitialized;
}

void UFretDanceMusicInstrumentProcessor::
    InitializeStringVibrationAnimationChannels(
        AFretDanceUnreal* FretDanceActor) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringVibrationAnimationChannels: "
                    "FretDanceActor is null"));
        return;
    }

    if (!FretDanceActor->Guitar) {
        UE_LOG(
            LogTemp, Error,
            TEXT("InitializeStringVibrationAnimationChannels: Guitar is null"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels Started "
                "=========="));

    // 获取 Control Rig Instance 和 Blueprint - 使用缓存机制
    UControlRig* ControlRigInstance =
        FretDanceActor->GetCachedControlRig(TEXT("Guitar"));
    UControlRigBlueprint* ControlRigBlueprint =
        FretDanceActor->GetCachedControlRigBlueprint(TEXT("Guitar"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("FretDanceMusicInstrumentProcessor: Failed to get "
                    "ControlRig for Guitar - cache miss"));
        return;
    }

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRigBlueprint is null in "
                    "InitializeStringVibrationAnimationChannels"));
        return;
    }

    // 使用 Common 模块的统一方法：从 ControlRig Blueprint 的 Curve Container
    // 读取曲线并创建通道
    int32 ChannelsAdded =
        UInstrumentMorphTargetUtility::InitializeMorphTargetChannels(
            ControlRigBlueprint, TEXT("guitar_root"));

    if (ChannelsAdded == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to initialize morph target channels for Guitar"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels "
                "Completed =========="));
}

void UFretDanceMusicInstrumentProcessor::GenerateInstrumentAnimation(
    AFretDanceUnreal* FretDanceActor, const FString& StringVibrationDataPath,
    const FString& VibratoShapeKeyDataPath) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation: FretDanceActor is null"));
        return;
    }

    if (!FretDanceActor->Guitar) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation: Guitar is null"));
        return;
    }

    if (StringVibrationDataPath.IsEmpty() &&
        VibratoShapeKeyDataPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation: Both data paths are empty"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Started =========="
                "\nString:  %s\nVibrato: %s"),
           *StringVibrationDataPath, *VibratoShapeKeyDataPath);

#if WITH_EDITOR
    // 清理乐器动画轨道
    UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
        FretDanceActor->Guitar);

    // 获取 LevelSequence
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error, TEXT("请确保已打开 Level Sequence"));
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return;
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    // 帧转换 lambda
    auto ToFrameNumber = [&](double FrameDouble) -> FFrameNumber {
        int32 Scaled = static_cast<int32>(
            FMath::RoundToInt(FrameDouble * TickResolution.AsDecimal() /
                              DisplayRate.AsDecimal()));
        return FFrameNumber(Scaled);
    };

    // ============================================================
    // 阶段 A：用 Map 聚合所有 Morph Target 关键帧
    // （弦振动 + 摇把 shape key 共用同一个 Map，避免互相覆盖）
    // ============================================================
    TMap<FString, FMorphTargetKeyframeData> ChannelDataMap;

    // ----- A1：收集弦振动数据 -----
    if (!StringVibrationDataPath.IsEmpty()) {
        FString JsonContent;
        if (!FFileHelper::LoadFileToString(JsonContent,
                                           *StringVibrationDataPath)) {
            UE_LOG(LogTemp, Error,
                   TEXT("Failed to load string vibration JSON: %s"),
                   *StringVibrationDataPath);
        } else {
            TArray<TSharedPtr<FJsonValue>> JsonArray;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(JsonContent);

            if (FJsonSerializer::Deserialize(Reader, JsonArray)) {
                UE_LOG(LogTemp, Warning,
                       TEXT("Loaded %d string vibration entries"),
                       JsonArray.Num());

                for (const auto& Value : JsonArray) {
                    TSharedPtr<FJsonObject> EntryObj = Value->AsObject();
                    if (!EntryObj.IsValid()) continue;

                    double FrameDouble =
                        EntryObj->GetNumberField(TEXT("frame"));
                    int32 FretNumber = static_cast<int32>(
                        EntryObj->GetIntegerField(TEXT("fret")));
                    float Influence = static_cast<float>(
                        EntryObj->GetNumberField(TEXT("influence")));
                    bool bIsUpDirection =
                        EntryObj->GetBoolField(TEXT("isUpDirection"));
                    int32 StringIndex = static_cast<int32>(
                        EntryObj->GetIntegerField(TEXT("stringIndex")));

                    FString DirectionStr =
                        bIsUpDirection ? TEXT("up") : TEXT("down");
                    FString MorphTargetName =
                        FString::Printf(TEXT("s%dfret%d%s"), StringIndex,
                                        FretNumber, *DirectionStr);

                    FFrameNumber FrameNumber = ToFrameNumber(FrameDouble);

                    FMorphTargetKeyframeData* ChannelData =
                        ChannelDataMap.Find(MorphTargetName);
                    if (!ChannelData) {
                        ChannelData = &ChannelDataMap.Add(
                            MorphTargetName,
                            FMorphTargetKeyframeData(MorphTargetName));
                    }
                    ChannelData->FrameNumbers.Add(FrameNumber);
                    ChannelData->Values.Add(Influence);
                }
            } else {
                UE_LOG(LogTemp, Error,
                       TEXT("Failed to parse string vibration JSON"));
            }
        }
    }

    // ----- A2：收集摇把 shape key 数据 -----
    // Rust 端 VibratoShapeKeyFrame 输出格式：
    //   { "frame": 63.346, "state": "release", "vibrato_up": 0.0,
    //   "vibrato_down": 0.0 }
    // Morph Target 名称直接使用字段名 "vibrato_up" 和 "vibrato_down"
    if (!VibratoShapeKeyDataPath.IsEmpty()) {
        FString JsonContent;
        if (!FFileHelper::LoadFileToString(JsonContent,
                                           *VibratoShapeKeyDataPath)) {
            UE_LOG(LogTemp, Error,
                   TEXT("Failed to load vibrato shape key JSON: %s"),
                   *VibratoShapeKeyDataPath);
        } else {
            TArray<TSharedPtr<FJsonValue>> JsonArray;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(JsonContent);

            if (FJsonSerializer::Deserialize(Reader, JsonArray)) {
                UE_LOG(LogTemp, Warning,
                       TEXT("Loaded %d vibrato shape key entries"),
                       JsonArray.Num());

                for (const auto& Value : JsonArray) {
                    TSharedPtr<FJsonObject> EntryObj = Value->AsObject();
                    if (!EntryObj.IsValid()) continue;

                    double FrameDouble =
                        EntryObj->GetNumberField(TEXT("frame"));

                    double VibratoUp = 0.0;
                    EntryObj->TryGetNumberField(TEXT("vibrato_up"), VibratoUp);

                    double VibratoDown = 0.0;
                    EntryObj->TryGetNumberField(TEXT("vibrato_down"),
                                                VibratoDown);

                    FFrameNumber FrameNumber = ToFrameNumber(FrameDouble);

                    // vibrato_up 通道
                    {
                        FMorphTargetKeyframeData* ChannelData =
                            ChannelDataMap.Find(TEXT("vibrato_up"));
                        if (!ChannelData) {
                            ChannelData = &ChannelDataMap.Add(
                                TEXT("vibrato_up"),
                                FMorphTargetKeyframeData(TEXT("vibrato_up")));
                        }
                        ChannelData->FrameNumbers.Add(FrameNumber);
                        ChannelData->Values.Add(static_cast<float>(VibratoUp));
                    }

                    // vibrato_down 通道
                    {
                        FMorphTargetKeyframeData* ChannelData =
                            ChannelDataMap.Find(TEXT("vibrato_down"));
                        if (!ChannelData) {
                            ChannelData = &ChannelDataMap.Add(
                                TEXT("vibrato_down"),
                                FMorphTargetKeyframeData(TEXT("vibrato_down")));
                        }
                        ChannelData->FrameNumbers.Add(FrameNumber);
                        ChannelData->Values.Add(
                            static_cast<float>(VibratoDown));
                    }
                }
            } else {
                UE_LOG(LogTemp, Error,
                       TEXT("Failed to parse vibrato shape key JSON"));
            }
        }
    }

    // ============================================================
    // 阶段 B：一次性写入所有 Morph Target 关键帧
    // ============================================================
    TArray<FMorphTargetKeyframeData> KeyframeData;
    for (auto& Pair : ChannelDataMap) {
        KeyframeData.Add(Pair.Value);
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation: No data to write"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Writing %d total morph target channels (string + vibrato)"),
           KeyframeData.Num());

    int32 Written =
        UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig(
            FretDanceActor->Guitar, KeyframeData, LevelSequence,
            TEXT("guitar_root"));

    if (Written == 0) {
        UE_LOG(LogTemp, Error, TEXT("Failed to write morph target animations"));
        return;
    }

    // 计算帧范围
    FFrameNumber MinFrame = FFrameNumber(INT_MAX);
    FFrameNumber MaxFrame = FFrameNumber(INT_MIN);

    for (const auto& Data : KeyframeData) {
        if (Data.FrameNumbers.Num() > 0) {
            MinFrame = FMath::Min(MinFrame, Data.FrameNumbers[0]);
            MaxFrame = FMath::Max(MaxFrame, Data.FrameNumbers.Last());
        }
    }

    if (MinFrame.Value == INT_MAX || MaxFrame.Value == INT_MIN) {
        UE_LOG(LogTemp, Error, TEXT("Invalid frame range"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Report =========="));
    UE_LOG(LogTemp, Warning,
           TEXT("Successfully processed %d channels (string + vibrato)"),
           KeyframeData.Num());
    UE_LOG(LogTemp, Warning, TEXT("Frame range: %d - %d"), MinFrame.Value,
           MaxFrame.Value);
    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Completed =========="));

#endif
}

bool UFretDanceMusicInstrumentProcessor::
    LoadAndGenerateStringVibrationAnimation(
        AFretDanceUnreal* FretDanceActor,
        const FString& StringVibrationDataPath,
        TMap<FString,
             TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
            OutVibrationKeyframeData) {
    OutVibrationKeyframeData.Empty();

    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("LoadAndGenerateStringVibrationAnimation: FretDanceActor "
                    "is null"));
        return false;
    }

    if (StringVibrationDataPath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("StringVibrationDataPath is empty"));
        return false;
    }

#if WITH_EDITOR
    // 获取 LevelSequence
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error, TEXT("请确保已打开 Level Sequence"));
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return false;
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    // ========== FretDance 特定的 JSON 读取逻辑 ==========
    // 读取 JSON 文件
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *StringVibrationDataPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("[FretDanceMusicInstrumentProcessor] Failed to load JSON "
                    "file: %s"),
               *StringVibrationDataPath);
        return false;
    }

    // 解析 JSON 数组（FretDance 是数组格式）
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);

    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON array from file: %s"),
               *StringVibrationDataPath);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Loaded %d vibration entries from JSON"),
           JsonArray.Num());

    // ========== 处理 FretDance 特定的数据格式 ==========
    // FretDance JSON 格式:
    // [
    //   {
    //     "frame": 5.625,
    //     "fret": 3,
    //     "influence": 1.0,
    //     "isUpDirection": true,
    //     "stringIndex": 2
    //   }
    // ]

    // 转换为 FMorphTargetKeyframeData 数组
    TMap<FString, FMorphTargetKeyframeData> ChannelDataMap;

    for (const auto& Value : JsonArray) {
        TSharedPtr<FJsonObject> EntryObj = Value->AsObject();
        if (!EntryObj.IsValid()) {
            continue;
        }

        // 提取字段
        double FrameDouble = EntryObj->GetNumberField(TEXT("frame"));
        int32 FretNumber =
            static_cast<int32>(EntryObj->GetIntegerField(TEXT("fret")));
        float Influence =
            static_cast<float>(EntryObj->GetNumberField(TEXT("influence")));
        bool bIsUpDirection = EntryObj->GetBoolField(TEXT("isUpDirection"));
        int32 StringIndex =
            static_cast<int32>(EntryObj->GetIntegerField(TEXT("stringIndex")));

        // 生成 Morph Target 名称：s{弦}fret{品}{方向}
        FString DirectionStr = bIsUpDirection ? TEXT("up") : TEXT("down");
        FString MorphTargetName = FString::Printf(
            TEXT("s%dfret%d%s"), StringIndex, FretNumber, *DirectionStr);

        // 转换为帧编号
        FFrameNumber FrameNumber(static_cast<int32>(
            FMath::RoundToInt(FrameDouble * TickResolution.AsDecimal() /
                              DisplayRate.AsDecimal())));

        // 添加到对应的通道数据中
        FMorphTargetKeyframeData* ChannelData =
            ChannelDataMap.Find(MorphTargetName);
        if (!ChannelData) {
            ChannelData = &ChannelDataMap.Add(
                MorphTargetName, FMorphTargetKeyframeData(MorphTargetName));
        }

        ChannelData->FrameNumbers.Add(FrameNumber);
        ChannelData->Values.Add(Influence);
    }

    // 转换为数组格式以便写入
    TArray<FMorphTargetKeyframeData> KeyframeData;
    for (auto& Pair : ChannelDataMap) {
        KeyframeData.Add(Pair.Value);
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("No vibration data found"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Processed %d unique morph target channels"),
           KeyframeData.Num());

    // ========== 使用通用方法写入 Morph Target 动画 ==========
    int32 WrittenTargets =
        UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig(
            FretDanceActor->Guitar, KeyframeData, LevelSequence,
            TEXT("guitar_root"));  // FretDance 使用 guitar_root

    if (WrittenTargets == 0) {
        UE_LOG(LogTemp, Error, TEXT("Failed to write morph target animations"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✓ Successfully wrote keyframes for %d channels"),
           WrittenTargets);

    // ========== 转换数据格式供 Material 动画使用 ==========
    for (const FMorphTargetKeyframeData& Data : KeyframeData) {
        TArray<FMovieSceneFloatValue> FloatValues;
        FloatValues.Reserve(Data.Values.Num());
        for (float Value : Data.Values) {
            FloatValues.Add(FMovieSceneFloatValue(Value));
        }
        OutVibrationKeyframeData.Add(
            Data.MorphTargetName,
            TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>(
                Data.FrameNumbers, FloatValues));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== LoadAndGenerateStringVibrationAnimation Completed "
                "=========="));

    return true;
#else
    return false;
#endif
}

void UFretDanceMusicInstrumentProcessor::GenerateInstrumentMaterialAnimation(
    AFretDanceUnreal* FretDanceActor,
    const FString& InstrumentAnimationDataPath) {
    if (!FretDanceActor) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "GenerateInstrumentMaterialAnimation: FretDanceActor is null"));
        return;
    }

    if (InstrumentAnimationDataPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("InstrumentAnimationDataPath is empty in "
                    "GenerateInstrumentMaterialAnimation"));
        return;
    }

    // Phase 7: 实现材质动画生成
    UE_LOG(LogTemp, Warning,
           TEXT("GenerateInstrumentMaterialAnimation: NOT YET IMPLEMENTED "
                "(Phase 7)"));
}

void UFretDanceMusicInstrumentProcessor::CleanupExistingGuitarAnimations(
    AFretDanceUnreal* FretDanceActor) {
    if (!FretDanceActor) {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Cleaning up existing guitar animations..."));

    // 清理 Control Rig 轨道上的旧关键帧
    if (FretDanceActor->SkeletalMeshActor) {
        UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
            FretDanceActor->SkeletalMeshActor);
    }

    // 清理材质参数轨道上的旧关键帧
    if (FretDanceActor->Guitar) {
        UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
            FretDanceActor->Guitar);
    }
}

#undef LOCTEXT_NAMESPACE
