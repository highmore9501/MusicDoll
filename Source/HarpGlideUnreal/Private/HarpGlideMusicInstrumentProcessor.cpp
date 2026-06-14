#include "HarpGlideMusicInstrumentProcessor.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentMaterialUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "Json.h"
#include "LevelSequence.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MovieScene.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "HarpGlideMusicInstrumentProcessor"

// ============================================================
// InitializeHarpInstrument
// ============================================================

void UHarpGlideMusicInstrumentProcessor::InitializeHarpInstrument(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!HarpGlideActor || !HarpGlideActor->Harp) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeHarpInstrument: Actor or Harp mesh is null"));
        return;
    }

    // 注册竖琴 CR（必须先于清理和通道初始化，与其他乐器模块保持一致）
    HarpGlideActor->RegisterAllControlRigs();

    CleanupExistingHarpAnimations(HarpGlideActor);

    // 初始化弦材质
    InitializeStringMaterials(HarpGlideActor);

    // 创建弦振动 Morph Target 通道
    InitializeStringVibrationAnimationChannels(HarpGlideActor);

    UE_LOG(LogTemp, Warning,
           TEXT("HarpGlide InitializeHarpInstrument completed"));
}

// ============================================================
// InitializeStringMaterials
// ============================================================

void UHarpGlideMusicInstrumentProcessor::InitializeStringMaterials(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!HarpGlideActor || !HarpGlideActor->Harp) return;

    USkeletalMeshComponent* SkeletalMeshComp =
        HarpGlideActor->Harp->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp || SkeletalMeshComp->GetNumMaterials() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("InitializeStringMaterials [HarpGlide]: "
                    "Harp has no SkeletalMeshComponent or materials"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterials [HarpGlide] Started "
                "=========="));

    FMaterialUpdateSettings Settings;
    Settings.bSkipAnimatedMaterials = true;

    Settings.MaterialSelector = [HarpGlideActor, SkeletalMeshComp](
                                    const FString& SlotName,
                                    int32 SlotIndex) -> UMaterialInterface* {
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
        if (CurrentName.StartsWith(TEXT("MAT_HarpString_"),
                                   ESearchCase::IgnoreCase))
            return CurrentMaterial;

        FString MatName = FString::Printf(TEXT("MAT_HarpString_%d"), SlotIndex);
        FString PackagePath =
            FString::Printf(TEXT("/Game/Materials/%s"), *MatName);

        return UInstrumentMaterialUtility::CreateOrGetMaterialInstance(
            MatName, PackagePath, CurrentMaterial,
            HarpGlideActor->GeneratedMaterials);
    };

    int32 UpdatedCount =
        UInstrumentMaterialUtility::UpdateSkeletalMeshMaterials(
            SkeletalMeshComp, Settings, HarpGlideActor->GeneratedMaterials);

    UE_LOG(LogTemp, Warning,
           TEXT("InitializeStringMaterials [HarpGlide]: Updated %d materials"),
           UpdatedCount);
}

// ============================================================
// InitializeStringVibrationAnimationChannels
// ============================================================

void UHarpGlideMusicInstrumentProcessor::
    InitializeStringVibrationAnimationChannels(
        AHarpGlideUnreal* HarpGlideActor) {
    if (!HarpGlideActor || !HarpGlideActor->Harp) return;

    // 使用 UInstrumentMorphTargetUtility
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels "
                "[HarpGlide] Started =========="));

    UControlRigBlueprint* Blueprint =
        HarpGlideActor->GetCachedControlRigBlueprint(TEXT("Harp"));
    if (!Blueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeStringVibrationAnimationChannels: "
                    "Failed to get ControlRigBlueprint for Harp"));
        return;
    }

    int32 ChannelsAdded =
        UInstrumentMorphTargetUtility::InitializeMorphTargetChannels(
            Blueprint, TEXT("harp_root"));

    if (ChannelsAdded == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to initialize morph target channels for Harp"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels "
                "[HarpGlide] Completed =========="));
}

// ============================================================
// GenerateInstrumentAnimation（弦振动 + 踏板 Shape Key 合并写入）
// ============================================================

void UHarpGlideMusicInstrumentProcessor::GenerateInstrumentAnimation(
    AHarpGlideUnreal* HarpGlideActor, const FString& StringAnimationDataPath,
    const FString& PedalShapeAnimationDataPath) {
    if (!HarpGlideActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation [HarpGlide]: Actor is null"));
        return;
    }
    if (!HarpGlideActor->Harp) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation [HarpGlide]: Harp is null"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== HarpGlide GenerateInstrumentAnimation Started "
                "=========="
                "\nString: %s\nPedal:  %s"),
           *StringAnimationDataPath, *PedalShapeAnimationDataPath);

#if WITH_EDITOR
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation [HarpGlide]: No "
                    "LevelSequence"));
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) return;

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    // 帧转换 lambda
    auto ToFrameNumber = [&](double FrameDouble) -> FFrameNumber {
        int32 Scaled = static_cast<int32>(
            FMath::RoundToInt(FrameDouble * TickResolution.AsDecimal() /
                              DisplayRate.AsDecimal()));
        return FFrameNumber(Scaled);
    };

    // 用 Map 聚合所有 Morph Target 关键帧（弦振动 + 踏板共用同一个 Map）
    TMap<FString, FMorphTargetKeyframeData> ChannelDataMap;

    // ============================================================
    // 阶段 A：收集弦振动数据
    // ============================================================
    if (!StringAnimationDataPath.IsEmpty()) {
        FString JsonContent;
        if (FFileHelper::LoadFileToString(JsonContent,
                                          *StringAnimationDataPath)) {
            TArray<TSharedPtr<FJsonValue>> JsonArray;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(JsonContent);
            if (FJsonSerializer::Deserialize(Reader, JsonArray)) {
                UE_LOG(LogTemp, Warning,
                       TEXT("HarpGlide: Loaded %d string vibration entries"),
                       JsonArray.Num());

                for (const auto& Val : JsonArray) {
                    TSharedPtr<FJsonObject> Entry = Val->AsObject();
                    if (!Entry.IsValid()) continue;

                    int32 StringIndex = 0;
                    Entry->TryGetNumberField(TEXT("string_index"), StringIndex);

                    double FrameDouble = 0.0;
                    Entry->TryGetNumberField(TEXT("frame"), FrameDouble);

                    double Value = 0.0;
                    Entry->TryGetNumberField(TEXT("value"), Value);

                    bool bIsThumb = false;
                    Entry->TryGetBoolField(TEXT("is_thumb"), bIsThumb);

                    FString TypeSuffix =
                        bIsThumb ? TEXT("outer") : TEXT("inner");
                    FString MorphName = FString::Printf(
                        TEXT("string%d_%s"), StringIndex, *TypeSuffix);

                    FMorphTargetKeyframeData* Data =
                        ChannelDataMap.Find(MorphName);
                    if (!Data) {
                        Data = &ChannelDataMap.Add(
                            MorphName, FMorphTargetKeyframeData(MorphName));
                    }
                    Data->FrameNumbers.Add(ToFrameNumber(FrameDouble));
                    Data->Values.Add(static_cast<float>(Value));
                }
            } else {
                UE_LOG(LogTemp, Error,
                       TEXT("GenerateInstrumentAnimation [HarpGlide]: String "
                            "JSON parse failed"));
            }
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("GenerateInstrumentAnimation [HarpGlide]: Failed to "
                        "load string JSON '%s'"),
                   *StringAnimationDataPath);
        }
    }

    // ============================================================
    // 阶段 B：收集踏板 Shape Key 数据
    //
    // Rust 端 pedal_shape_key.rs 输出格式（PedalShapeKeyEvent）：
    //   {
    //     "pedal_state": "pedal_A_state0",
    //     "data": { "frame": 0.0, "value": 0.0 }
    //   }
    //
    // "pedal_state" 就是完整的 Morph Target 名称，
    // 例如 pedal_A_state0 ~ pedal_A_state4（共 7 踏板 × 5 状态 = 35 个）。
    // 直接作为 ChannelDataMap 的 key 使用，无需额外拼接。
    // ============================================================
    if (!PedalShapeAnimationDataPath.IsEmpty()) {
        FString JsonContent;
        if (FFileHelper::LoadFileToString(JsonContent,
                                          *PedalShapeAnimationDataPath)) {
            TArray<TSharedPtr<FJsonValue>> JsonArray;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(JsonContent);
            if (FJsonSerializer::Deserialize(Reader, JsonArray)) {
                UE_LOG(LogTemp, Warning,
                       TEXT("HarpGlide: Loaded %d pedal shape entries"),
                       JsonArray.Num());

                for (const auto& Val : JsonArray) {
                    TSharedPtr<FJsonObject> Entry = Val->AsObject();
                    if (!Entry.IsValid()) continue;

                    // 读取完整的 Morph Target 名称（如 "pedal_A_state0"）
                    FString PedalStateName;
                    if (!Entry->TryGetStringField(TEXT("pedal_state"),
                                                  PedalStateName))
                        continue;

                    // 读取嵌套的 data 对象
                    const TSharedPtr<FJsonObject>* DataObj = nullptr;
                    if (!Entry->TryGetObjectField(TEXT("data"), DataObj) ||
                        !DataObj || !DataObj->IsValid())
                        continue;

                    double FrameDouble = 0.0;
                    (*DataObj)->TryGetNumberField(TEXT("frame"), FrameDouble);

                    double Value = 0.0;
                    (*DataObj)->TryGetNumberField(TEXT("value"), Value);

                    FMorphTargetKeyframeData* Data =
                        ChannelDataMap.Find(PedalStateName);
                    if (!Data) {
                        Data = &ChannelDataMap.Add(
                            PedalStateName,
                            FMorphTargetKeyframeData(PedalStateName));
                    }
                    Data->FrameNumbers.Add(ToFrameNumber(FrameDouble));
                    Data->Values.Add(static_cast<float>(Value));
                }
            } else {
                UE_LOG(LogTemp, Error,
                       TEXT("GenerateInstrumentAnimation [HarpGlide]: Pedal "
                            "JSON parse failed"));
            }
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("GenerateInstrumentAnimation [HarpGlide]: Failed to "
                        "load pedal JSON '%s'"),
                   *PedalShapeAnimationDataPath);
        }
    }

    // ============================================================
    // 阶段 C：一次性写入所有关键帧
    // ============================================================
    TArray<FMorphTargetKeyframeData> KeyframeData;
    for (auto& Pair : ChannelDataMap) {
        KeyframeData.Add(Pair.Value);
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation [HarpGlide]: No data"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("HarpGlide: %d total morph target channels (string + pedal)"),
           KeyframeData.Num());

    int32 Written =
        UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig(
            HarpGlideActor->Harp, KeyframeData, LevelSequence,
            TEXT("harp_root"));

    if (Written == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("HarpGlide: Failed to write morph target animations"));
        return;
    }

    // 与 FretDance 对齐：写入关键帧后触发 ControlRig 重新注册，
    // 确保 Sequencer 正确识别新增的轨道数据
    HarpGlideActor->TriggerControlRigReregistration(
        TEXT("Generate Instrument Animation"));
#endif

    UE_LOG(LogTemp, Warning,
           TEXT("========== HarpGlide GenerateInstrumentAnimation Completed "
                "=========="));
}

// ============================================================
// CleanupExistingHarpAnimations
// ============================================================

void UHarpGlideMusicInstrumentProcessor::CleanupExistingHarpAnimations(
    AHarpGlideUnreal* HarpGlideActor) {
    if (!HarpGlideActor || !HarpGlideActor->Harp) return;

    // 清理竖琴网格上的旧动画轨道
    UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
        HarpGlideActor->Harp);

    UE_LOG(LogTemp, Verbose, TEXT("CleanupExistingHarpAnimations done"));
}

#undef LOCTEXT_NAMESPACE
