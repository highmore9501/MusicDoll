#include "FretDanceMusicInstrumentProcessor.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "FretDanceTransformSyncProcessor.h"
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

    // 0. 初始化吉他同步关系（controller_root <-> guitar_root）
    // 这会在开始每帧同步前计算并缓存相对变换矩阵
    UFretDanceTransformSyncProcessor::InitializeGuitarSync(FretDanceActor);

    // 1. 清理现有动画数据
    CleanupExistingGuitarAnimations(FretDanceActor);

    // 2. 初始化弦材质
    InitializeStringMaterials(FretDanceActor);

    // 3. 初始化弦材质参数动画轨道
    int32 MaterialTracksInitialized =
        InitializeStringMaterialAnimationTracks(FretDanceActor);

    // 4. 初始化弦振动动画通道（Morph Target）
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
    // TODO: 需要根据吉他的骨骼名称调整参数
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

    // todo:使用 Common 模块的通用方法初始化材质参数轨道
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

    // 生成所有需要的通道名称
    TArray<FString> ChannelNamesToCreate;

    const int32 MaxStringIndex = 5;  // 吉他 6 根弦 (0-5)
    const int32 MinFretNumber = 0;   // 从第 0 品格开始
    const int32 MaxFretNumber = 20;  // 到第 20 品格
    const TArray<FString> Directions = {TEXT("up"),
                                        TEXT("down")};  // 两个振动方向

    for (int32 StringIndex = 0; StringIndex <= MaxStringIndex; ++StringIndex) {
        for (int32 FretNumber = MinFretNumber; FretNumber <= MaxFretNumber;
             ++FretNumber) {
            for (const FString& Direction : Directions) {
                FString ChannelStr = FString::Printf(
                    TEXT("s%dfret%d%s"), StringIndex, FretNumber, *Direction);
                ChannelNamesToCreate.Add(ChannelStr);
            }
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("Creating vibration animation channels for %d channel names..."),
        ChannelNamesToCreate.Num());

    // 使用 Common 模块的通用方法：检查 Root Control 是否存在
    if (!UInstrumentMorphTargetUtility::EnsureRootControlExists(
            ControlRigBlueprint, TEXT("guitar_root"))) {
        UE_LOG(LogTemp, Error, TEXT("====== INITIALIZATION FAILED ======"));
        UE_LOG(LogTemp, Error,
               TEXT("Root Control 'guitar_root' does not exist in Control Rig "
                    "Blueprint"));
        UE_LOG(LogTemp, Error, TEXT(""));
        UE_LOG(LogTemp, Error,
               TEXT("Please manually create the Root Control 'guitar_root' in "
                    "your Control Rig Blueprint:"));
        UE_LOG(LogTemp, Error, TEXT("  1. Open the Control Rig Blueprint"));
        UE_LOG(LogTemp, Error, TEXT("  2. Go to the Hierarchy panel"));
        UE_LOG(LogTemp, Error,
               TEXT("  3. Right-click and create a new Control named "
                    "'guitar_root'"));
        UE_LOG(LogTemp, Error,
               TEXT("  4. Set the Control Type to 'Transform'"));
        UE_LOG(LogTemp, Error, TEXT("  5. Save the Blueprint and try again"));
        UE_LOG(LogTemp, Error, TEXT("====== END OF ERROR REPORT ======"));
        return;
    }

    // 获取 Root Control 的 Key 用于后续操作
    FRigElementKey RootControlKey(TEXT("guitar_root"),
                                  ERigElementType::Control);

    // 使用 Common 模块的通用方法：批量添加动画通道
    if (ChannelNamesToCreate.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("ChannelNamesToCreate is empty"));
        return;
    }

    int32 ChannelsAdded = UInstrumentMorphTargetUtility::AddAnimationChannels(
        ControlRigBlueprint, RootControlKey, ChannelNamesToCreate);

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels "
                "Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully created/verified: %d channels"),
           ChannelsAdded);
    UE_LOG(LogTemp, Warning,
           TEXT("Expected total: %d channels (%d strings × %d frets × %d "
                "directions)"),
           ChannelsAdded, MaxStringIndex + 1, MaxFretNumber - MinFretNumber + 1,
           Directions.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels "
                "Completed =========="));
}

void UFretDanceMusicInstrumentProcessor::GenerateInstrumentAnimation(
    AFretDanceUnreal* FretDanceActor,
    const FString& StringVibrationDataPath) {
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

    if (StringVibrationDataPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("StringVibrationDataPath is empty in GenerateInstrumentAnimation"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Started =========="));
    UE_LOG(LogTemp, Warning, TEXT("Generating from: %s"),
           *StringVibrationDataPath);

#if WITH_EDITOR
    // 清理乐器动画轨道
    UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
        FretDanceActor->Guitar);

    // 使用新的 Morph Target 生成方法
    TMap<FString, TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>
        VibrationKeyframeData;

    if (!LoadAndGenerateStringVibrationAnimation(
            FretDanceActor, StringVibrationDataPath, VibrationKeyframeData)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to load and generate string vibration animation"));
        return;
    }

    // 获取 LevelSequence 和 Sequencer
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

    // 计算帧范围
    FFrameNumber MinFrame = FFrameNumber(INT_MAX);
    FFrameNumber MaxFrame = FFrameNumber(INT_MIN);

    for (const auto& Pair : VibrationKeyframeData) {
        const TArray<FFrameNumber>& FrameNumbers = Pair.Value.Key;
        if (FrameNumbers.Num() > 0) {
            MinFrame = FMath::Min(MinFrame, FrameNumbers[0]);
            MaxFrame = FMath::Max(MaxFrame, FrameNumbers.Last());
        }
    }

    if (MinFrame.Value == INT_MAX || MaxFrame.Value == INT_MIN) {
        UE_LOG(LogTemp, Error, TEXT("Invalid frame range"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Report =========="));
    UE_LOG(LogTemp, Warning,
           TEXT("Successfully processed string vibration data"));
    UE_LOG(LogTemp, Warning, TEXT("Processed %d morph target channels"),
           VibrationKeyframeData.Num());
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

    // TODO - Phase 7: 实现材质动画生成
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
