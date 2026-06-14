#include "ZhengDriftMusicInstrumentProcessor.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentMaterialUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "Json.h"
#include "LevelSequence.h"
#include "Misc/FileHelper.h"
#include "MovieScene.h"
#include "Sections/MovieSceneComponentMaterialParameterSection.h"
#include "Serialization/JsonSerializer.h"
#include "Tracks/MovieSceneMaterialTrack.h"

#define LOCTEXT_NAMESPACE "ZhengDriftMusicInstrumentProcessor"

// ============================================================
// InitializeZhengInstrument
// ============================================================

void UZhengDriftMusicInstrumentProcessor::InitializeZhengInstrument(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeZhengInstrument: Actor is null"));
        return;
    }
    if (!ZhengDriftActor->Zheng) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeZhengInstrument: Zheng is null"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeZhengInstrument Started =========="));

    ZhengDriftActor->RegisterAllControlRigs();
    CleanupExistingZhengAnimations(ZhengDriftActor);
    InitializeStringMaterials(ZhengDriftActor);

    int32 MatTracks = InitializeStringMaterialAnimationTracks(ZhengDriftActor);
    InitializeStringVibrationAnimationChannels(ZhengDriftActor);

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeZhengInstrument Completed. "
                "Material tracks: %d =========="),
           MatTracks);
}

// ============================================================
// InitializeStringMaterials
// ============================================================

void UZhengDriftMusicInstrumentProcessor::InitializeStringMaterials(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor || !ZhengDriftActor->Zheng) return;

    USkeletalMeshComponent* SkeletalMeshComp =
        ZhengDriftActor->Zheng->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp || SkeletalMeshComp->GetNumMaterials() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("InitializeStringMaterials [ZhengDrift]: "
                    "Zheng has no SkeletalMeshComponent or materials"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterials [ZhengDrift] Started "
                "=========="));

    FMaterialUpdateSettings Settings;
    Settings.bSkipAnimatedMaterials = true;

    Settings.MaterialSelector = [ZhengDriftActor, SkeletalMeshComp](
        const FString& SlotName, int32 SlotIndex) -> UMaterialInterface* {
        if (SlotIndex < 0 || SlotIndex >= SkeletalMeshComp->GetNumMaterials())
            return nullptr;

        // 只处理弦相关槽（名称包含 "string"）
        if (!SlotName.Contains(TEXT("string"), ESearchCase::IgnoreCase))
            return nullptr;

        UMaterialInterface* CurrentMaterial =
            SkeletalMeshComp->GetMaterial(SlotIndex);
        if (!CurrentMaterial) return nullptr;

        // 如果已经是生成的材质就直接返回
        FString CurrentName = CurrentMaterial->GetName();
        if (CurrentName.StartsWith(TEXT("MAT_ZhengString_"),
                                    ESearchCase::IgnoreCase))
            return CurrentMaterial;

        FString MatName = FString::Printf(TEXT("MAT_ZhengString_%d"), SlotIndex);
        FString PackagePath =
            FString::Printf(TEXT("/Game/Materials/%s"), *MatName);

        return UInstrumentMaterialUtility::CreateOrGetMaterialInstance(
            MatName, PackagePath, CurrentMaterial,
            ZhengDriftActor->GeneratedMaterials);
    };

    int32 UpdatedCount = UInstrumentMaterialUtility::UpdateSkeletalMeshMaterials(
        SkeletalMeshComp, Settings, ZhengDriftActor->GeneratedMaterials);

    UE_LOG(LogTemp, Warning,
           TEXT("InitializeStringMaterials [ZhengDrift]: Updated %d materials"),
           UpdatedCount);
}

// ============================================================
// InitializeStringMaterialAnimationTracks
// ============================================================

int32 UZhengDriftMusicInstrumentProcessor::
    InitializeStringMaterialAnimationTracks(
        AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor || !ZhengDriftActor->Zheng) return 0;

    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringMaterialAnimationTracks [ZhengDrift]: "
                    "No active LevelSequence"));
        return 0;
    }

    USkeletalMeshComponent* SkeletalMeshComp =
        ZhengDriftActor->Zheng->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) return 0;

    FGuid BindingID = UInstrumentAnimationUtility::GetOrCreateComponentBinding(
        Sequencer, SkeletalMeshComp, true);
    if (!BindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringMaterialAnimationTracks [ZhengDrift]: "
                    "Failed to get component binding"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterialAnimationTracks "
                "[ZhengDrift] Started =========="));

    int32 SuccessCount = 0;
    int32 NumMaterials = SkeletalMeshComp->GetNumMaterials();

    for (int32 SlotIdx = 0; SlotIdx < NumMaterials; ++SlotIdx) {
        UMaterialInterface* Mat = SkeletalMeshComp->GetMaterial(SlotIdx);
        if (!Mat) continue;

        if (!UInstrumentMaterialUtility::MaterialHasParameter(
                Mat, TEXT("Vibration")))
            continue;

        UMovieSceneComponentMaterialTrack* MaterialTrack =
            UInstrumentAnimationUtility::FindOrCreateComponentMaterialTrack(
                LevelSequence, BindingID, SlotIdx);

        if (MaterialTrack &&
            UInstrumentAnimationUtility::AddMaterialParameter(
                MaterialTrack, TEXT("Vibration"), 0.0f)) {
            SuccessCount++;
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("InitializeStringMaterialAnimationTracks [ZhengDrift]: "
                "Created %d tracks (expected up to %d)"),
           SuccessCount, NumMaterials);

    return SuccessCount;
}

// ============================================================
// InitializeStringVibrationAnimationChannels
// ============================================================

void UZhengDriftMusicInstrumentProcessor::
    InitializeStringVibrationAnimationChannels(
        AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringVibrationAnimationChannels: Actor is null"));
        return;
    }
    if (!ZhengDriftActor->Zheng) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringVibrationAnimationChannels: Zheng is null"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels "
                "[ZhengDrift] Started =========="));

    // 获取 Control Rig Blueprint
    UControlRigBlueprint* Blueprint =
        ZhengDriftActor->GetCachedControlRigBlueprint(TEXT("Zheng"));
    if (!Blueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringVibrationAnimationChannels: "
                    "Failed to get ControlRigBlueprint for Zheng"));
        return;
    }

    // 使用 Common 模块的统一方法：从 ControlRig Blueprint 的 Curve Container 读取曲线并创建通道
    int32 ChannelsAdded = UInstrumentMorphTargetUtility::InitializeMorphTargetChannels(
        Blueprint,
        TEXT("zheng_root")
    );

    if (ChannelsAdded == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to initialize morph target channels for Zheng"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels "
                "[ZhengDrift] Completed =========="));
}

// ============================================================
// GenerateInstrumentAnimation
// ============================================================

void UZhengDriftMusicInstrumentProcessor::GenerateInstrumentAnimation(
    AZhengDriftUnreal* ZhengDriftActor,
    const FString& StringAnimationDataPath) {
    if (!ZhengDriftActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation [ZhengDrift]: Actor is null"));
        return;
    }
    if (!ZhengDriftActor->Zheng) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation [ZhengDrift]: Zheng is null"));
        return;
    }
    if (StringAnimationDataPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation [ZhengDrift]: Path is empty"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== ZhengDrift GenerateInstrumentAnimation Started "
                "========== \nPath: %s"),
           *StringAnimationDataPath);

#if WITH_EDITOR
    // 清理乐器动画轨道
    UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
        ZhengDriftActor->Zheng);

    TMap<FString, TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>
        VibrationData;

    if (!LoadAndGenerateStringVibrationAnimation(
            ZhengDriftActor, StringAnimationDataPath, VibrationData)) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation [ZhengDrift]: "
                    "Failed to load/generate vibration animation"));
        return;
    }

    // 统计帧范围
    FFrameNumber MinFrame(INT_MAX);
    FFrameNumber MaxFrame(INT_MIN);
    for (const auto& Pair : VibrationData) {
        const TArray<FFrameNumber>& Frames = Pair.Value.Key;
        if (Frames.Num() > 0) {
            MinFrame = FMath::Min(MinFrame, Frames[0]);
            MaxFrame = FMath::Max(MaxFrame, Frames.Last());
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift GenerateInstrumentAnimation: "
                "Channels=%d FrameRange=[%d, %d]"),
           VibrationData.Num(),
           MinFrame.Value == INT_MAX  ? 0 : MinFrame.Value,
           MaxFrame.Value == INT_MIN  ? 0 : MaxFrame.Value);

    UE_LOG(LogTemp, Warning,
           TEXT("========== ZhengDrift GenerateInstrumentAnimation Completed =========="));
#endif
}

// ============================================================
// LoadAndGenerateStringVibrationAnimation
// ============================================================

bool UZhengDriftMusicInstrumentProcessor::
    LoadAndGenerateStringVibrationAnimation(
        AZhengDriftUnreal* ZhengDriftActor,
        const FString& StringAnimationDataPath,
        TMap<FString,
             TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
            OutVibrationKeyframeData) {
    OutVibrationKeyframeData.Empty();

    if (!ZhengDriftActor || StringAnimationDataPath.IsEmpty()) return false;

#if WITH_EDITOR
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error,
               TEXT("LoadAndGenerateStringVibrationAnimation [ZhengDrift]: "
                    "No active LevelSequence"));
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) return false;

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate    = MovieScene->GetDisplayRate();

    // 读取 JSON
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent,
                                       *StringAnimationDataPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("LoadAndGenerateStringVibrationAnimation [ZhengDrift]: "
                    "Failed to load '%s'"),
               *StringAnimationDataPath);
        return false;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) {
        UE_LOG(LogTemp, Error,
               TEXT("LoadAndGenerateStringVibrationAnimation [ZhengDrift]: "
                    "JSON parse failed"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift: Loaded %d vibration entries"), JsonArray.Num());

    // 解析 JSON → FMorphTargetKeyframeData
    TMap<FString, FMorphTargetKeyframeData> ChannelDataMap;

    for (const auto& Val : JsonArray) {
        TSharedPtr<FJsonObject> Entry = Val->AsObject();
        if (!Entry.IsValid()) continue;

        int32 StringIndex = 0;
        Entry->TryGetNumberField(TEXT("string_index"), StringIndex);

        double FrameDouble = 0.0;
        Entry->TryGetNumberField(TEXT("frame"), FrameDouble);

        double Value = 0.0;
        Entry->TryGetNumberField(TEXT("value"), Value);

        FString ShapeKeyType;
        Entry->TryGetStringField(TEXT("shape_key_type"), ShapeKeyType);

        // 通道命名：string{N}_press 或 string{N}_vib
        FString TypeSuffix = (ShapeKeyType == TEXT("Press"))
                                 ? TEXT("press")
                                 : TEXT("vib");
        FString MorphName = FString::Printf(
            TEXT("string%d_%s"), StringIndex, *TypeSuffix);

        // 转换帧编号：将秒数转换为 Tick 单位
        float ScaledFrameNumberFloat =
            FrameDouble * TickResolution.AsDecimal() / DisplayRate.AsDecimal();
        int32 ScaledFrameNumber = static_cast<int32>(
            FMath::RoundToInt(ScaledFrameNumberFloat));
        FFrameNumber FrameNumber(ScaledFrameNumber);

        FMorphTargetKeyframeData* Data = ChannelDataMap.Find(MorphName);
        if (!Data) {
            Data = &ChannelDataMap.Add(
                MorphName, FMorphTargetKeyframeData(MorphName));
        }

        Data->FrameNumbers.Add(FrameNumber);
        Data->Values.Add(static_cast<float>(Value));
    }

    TArray<FMorphTargetKeyframeData> KeyframeData;
    for (auto& Pair : ChannelDataMap) {
        KeyframeData.Add(Pair.Value);
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("LoadAndGenerateStringVibrationAnimation [ZhengDrift]: "
                    "No vibration data"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift: %d unique morph target channels"),
           KeyframeData.Num());

    // 写入 Morph Target 动画（Root: zheng_root）
    int32 Written =
        UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig(
            ZhengDriftActor->Zheng, KeyframeData, LevelSequence,
            TEXT("zheng_root"));

    if (Written == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("ZhengDrift: Failed to write morph target animations"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift: Successfully wrote %d channels"), Written);

    // 转换为输出格式
    for (const FMorphTargetKeyframeData& Data : KeyframeData) {
        TArray<FMovieSceneFloatValue> FloatValues;
        for (float v : Data.Values) {
            FloatValues.Add(FMovieSceneFloatValue(v));
        }
        OutVibrationKeyframeData.Add(
            Data.MorphTargetName,
            TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>(
                Data.FrameNumbers, FloatValues));
    }

    return true;
#else
    return false;
#endif
}

// ============================================================
// GenerateInstrumentMaterialAnimation
// ============================================================

void UZhengDriftMusicInstrumentProcessor::GenerateInstrumentMaterialAnimation(
    AZhengDriftUnreal* ZhengDriftActor,
    const FString& InstrumentAnimationDataPath) {
    if (!ZhengDriftActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentMaterialAnimation [ZhengDrift]: "
                    "Actor is null"));
        return;
    }
    if (!ZhengDriftActor->Zheng) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentMaterialAnimation [ZhengDrift]: "
                    "Zheng is null"));
        return;
    }
    if (InstrumentAnimationDataPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentMaterialAnimation [ZhengDrift]: "
                    "Path is empty"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== ZhengDrift GenerateInstrumentMaterialAnimation "
                "Started ==========\nPath: %s"),
           *InstrumentAnimationDataPath);

#if WITH_EDITOR
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentMaterialAnimation [ZhengDrift]: "
                    "No active LevelSequence"));
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) return;

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate    = MovieScene->GetDisplayRate();

    // 读取 JSON
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent,
                                       *InstrumentAnimationDataPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentMaterialAnimation [ZhengDrift]: "
                    "Failed to load '%s'"),
               *InstrumentAnimationDataPath);
        return;
    }

    // 格式： [{"string_index":N, "frame":F, "value":V}, ...]
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentMaterialAnimation [ZhengDrift]: "
                    "JSON parse failed"));
        return;
    }

    // 按弦索引分组关键帧 (slot 索引 = string_index)
    TMap<int32, FMaterialParameterKeyframeData> SlotKeyframeMap;

    for (const auto& Val : JsonArray) {
        TSharedPtr<FJsonObject> Entry = Val->AsObject();
        if (!Entry.IsValid()) continue;

        int32  StringIndex = 0;
        double FrameDouble = 0.0;
        double Value       = 0.0;

        Entry->TryGetNumberField(TEXT("string_index"), StringIndex);
        Entry->TryGetNumberField(TEXT("frame"),        FrameDouble);
        Entry->TryGetNumberField(TEXT("value"),        Value);

        // 直接使用原始帧号（秒数），由 WriteMaterialParameterKeyframes 直接使用
        FFrameNumber FrameNumber(static_cast<int32>(FMath::RoundToInt(FrameDouble)));

        FMaterialParameterKeyframeData* Data = SlotKeyframeMap.Find(StringIndex);
        if (!Data) {
            Data = &SlotKeyframeMap.Add(
                StringIndex, FMaterialParameterKeyframeData(TEXT("Vibration")));
        }
        Data->FrameNumbers.Add(FrameNumber);
        Data->Values.Add(static_cast<float>(Value));
    }

    if (SlotKeyframeMap.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("GenerateInstrumentMaterialAnimation [ZhengDrift]: "
                    "No material keyframe data found"));
        return;
    }

    USkeletalMeshComponent* SkeletalMeshComp =
        ZhengDriftActor->Zheng->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) return;

    FGuid BindingID = UInstrumentAnimationUtility::GetOrCreateComponentBinding(
        Sequencer, SkeletalMeshComp, false);
    if (!BindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentMaterialAnimation [ZhengDrift]: "
                    "Component binding not found"));
        return;
    }

    FFrameNumber MinFrame(MAX_int32), MaxFrame(MIN_int32);
    int32 WrittenTracks = 0;

    for (auto& SlotPair : SlotKeyframeMap) {
        int32 SlotIdx = SlotPair.Key;
        FMaterialParameterKeyframeData& ParamData = SlotPair.Value;

        UMovieSceneComponentMaterialTrack* MaterialTrack =
            UInstrumentAnimationUtility::FindOrCreateComponentMaterialTrack(
                LevelSequence, BindingID, SlotIdx);
        if (!MaterialTrack) continue;

        UMovieSceneSection* NewSection =
            UInstrumentAnimationUtility::ResetTrackSections(MaterialTrack);
        if (!NewSection) continue;

        UMovieSceneComponentMaterialParameterSection* ParamSection =
            Cast<UMovieSceneComponentMaterialParameterSection>(NewSection);
        if (!ParamSection) continue;

        TArray<FMaterialParameterKeyframeData> DataArr = {ParamData};
        int32 Written = UInstrumentAnimationUtility::
            WriteMaterialParameterKeyframes(ParamSection, DataArr);

        if (Written > 0) {
            WrittenTracks++;

            for (const FFrameNumber& FN : ParamData.FrameNumbers) {
                MinFrame = FMath::Min(MinFrame, FN);
                MaxFrame = FMath::Max(MaxFrame, FN);
            }

            if (MinFrame.Value != MAX_int32 && MaxFrame.Value != MIN_int32) {
                ParamSection->SetRange(
                    TRange<FFrameNumber>(MinFrame, MaxFrame + 1));
            }
        }
    }

    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

    UInstrumentAnimationUtility::SyncMaterialParameterKeyframesAfterWrite(
        nullptr, nullptr, LevelSequence);

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift GenerateInstrumentMaterialAnimation: "
                "Written %d material tracks, FrameRange=[%d, %d]"),
           WrittenTracks,
           MinFrame.Value == MAX_int32 ? 0 : MinFrame.Value,
           MaxFrame.Value == MIN_int32 ? 0 : MaxFrame.Value);

    UE_LOG(LogTemp, Warning,
           TEXT("========== ZhengDrift GenerateInstrumentMaterialAnimation "
                "Completed =========="));
#endif
}

// ============================================================
// CleanupExistingZhengAnimations
// ============================================================

void UZhengDriftMusicInstrumentProcessor::CleanupExistingZhengAnimations(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor) return;

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift: Cleaning up existing animations..."));

    if (ZhengDriftActor->SkeletalMeshActor) {
        UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
            ZhengDriftActor->SkeletalMeshActor);
    }

    if (ZhengDriftActor->Zheng) {
        UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
            ZhengDriftActor->Zheng);
    }
}

#undef LOCTEXT_NAMESPACE
