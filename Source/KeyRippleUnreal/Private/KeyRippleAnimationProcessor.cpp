#include "KeyRippleAnimationProcessor.h"

#include "Dom/JsonObject.h"  // 包含FJsonObject
#include "Dom/JsonValue.h"   // 包含FJsonValue
#include "Engine/World.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "KeyRippleControlRigProcessor.h"
#include "KeyRipplePianoProcessor.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"

#define LOCTEXT_NAMESPACE "KeyRippleAnimationProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations - Static helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Safeguard helper to detect and warn about potential duplicate tracks
static bool ValidateNoExistingTracks(ULevelSequence* LevelSequence,
                                     UControlRig* ControlRigInstance,
                                     bool bAutoFix = true);

// Debug helper functions
static void LogAvailableChannels(UMovieSceneSection* Section);

// Rotation unwrapping methods - solving euler angle interpolation jumps
static void UnwrapRotationSequence(
    TArray<FMovieSceneFloatValue>& RotationValues);
static void ProcessRotationChannelsUnwrap(
    TArray<FMovieSceneFloatValue>& RotationXValues,
    TArray<FMovieSceneFloatValue>& RotationYValues,
    TArray<FMovieSceneFloatValue>& RotationZValues);

// Utility methods for reducing code duplication
static FString ValidateAndCorrectControllerName(const FString& InputName);
static void ProcessAnimationFrame(
    TSharedPtr<FJsonObject> FrameObject,
    TMap<FString, TArray<FControlKeyframe>>& ControlKeyframeData,
    int32 FrameIndex, int32& FailedFrames, int32& KeyframesAdded);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Static helper function implementations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// === Safeguard helper to detect and warn about potential duplicate tracks ===
static bool ValidateNoExistingTracks(ULevelSequence* LevelSequence,
                                     UControlRig* ControlRigInstance,
                                     bool bAutoFix) {
    if (!LevelSequence || !ControlRigInstance) {
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        return false;
    }

    // Find all Control Rig tracks in the movie scene
    TArray<UMovieSceneTrack*> AllTracks = MovieScene->GetTracks();
    int32 ControlRigTrackCount = 0;

    for (UMovieSceneTrack* Track : AllTracks) {
        if (Track && Track->IsA<UMovieSceneControlRigParameterTrack>()) {
            ControlRigTrackCount++;
        }
    }

    if (ControlRigTrackCount > 1) {
        UE_LOG(LogTemp, Error,
               TEXT("WARNING: Found %d Control Rig Parameter Tracks in the "
                    "sequence. This may cause duplicate corrupted controls. "
                    "Expected only 1."),
               ControlRigTrackCount);

        if (bAutoFix) {
            // Keep first track, remove duplicates
            bool bSkipFirst = true;
            int32 RemovedCount = 0;

            for (UMovieSceneTrack* Track : AllTracks) {
                if (Track &&
                    Track->IsA<UMovieSceneControlRigParameterTrack>()) {
                    if (bSkipFirst) {
                        bSkipFirst = false;
                        continue;
                    }
                    MovieScene->RemoveTrack(*Track);
                    RemovedCount++;
                }
            }

            UE_LOG(LogTemp, Warning,
                   TEXT("Auto-fixed: Removed %d duplicate Control Rig tracks"),
                   RemovedCount);
        }

        return ControlRigTrackCount > 1;  // Return true if duplicates found
    }

    return false;  // No duplicates
}

// 添加调试辅助函数
static void LogAvailableChannels(UMovieSceneSection* Section) {
    if (!Section) return;

    FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
    TArrayView<const FMovieSceneChannelEntry> AllEntries =
        ChannelProxy.GetAllEntries();

    UE_LOG(LogTemp, Warning, TEXT("=== Available Channels Debug ==="));
    for (const FMovieSceneChannelEntry& Entry : AllEntries) {
#if WITH_EDITOR
        TArrayView<const FMovieSceneChannelMetaData> MetaDataArray =
            Entry.GetMetaData();
        for (int32 i = 0; i < MetaDataArray.Num(); ++i) {
            const FMovieSceneChannelMetaData& MetaData = MetaDataArray[i];
            UE_LOG(LogTemp, Warning, TEXT("Channel: %s"),
                   *MetaData.Name.ToString());
        }
#endif
    }
}

// 角度展开方法 - 解决欧拉角插值跳变问题
static void UnwrapRotationSequence(
    TArray<FMovieSceneFloatValue>& RotationValues) {
    if (RotationValues.Num() < 2) {
        return;
    }

    for (int32 i = 1; i < RotationValues.Num(); ++i) {
        float PrevAngle = RotationValues[i - 1].Value;
        float CurrAngle = RotationValues[i].Value;

        // 计算最短角度差 (使用Unreal的FindDeltaAngleDegrees)
        float Delta = FMath::FindDeltaAngleDegrees(PrevAngle, CurrAngle);

        // 应用累积的角度，确保连续性
        RotationValues[i].Value = PrevAngle + Delta;
    }
}

// 批量处理所有旋转通道的展开
static void ProcessRotationChannelsUnwrap(
    TArray<FMovieSceneFloatValue>& RotationXValues,
    TArray<FMovieSceneFloatValue>& RotationYValues,
    TArray<FMovieSceneFloatValue>& RotationZValues) {
    // 分别处理每个旋转轴
    UnwrapRotationSequence(RotationXValues);
    UnwrapRotationSequence(RotationYValues);
    UnwrapRotationSequence(RotationZValues);
}

// 辅助方法：验证控制器名称
static FString ValidateAndCorrectControllerName(const FString& InputName) {
    // 预定义的有效控制器名称列表
    static const TSet<FString> ValidControllerNames = {
        TEXT("H_L"),          TEXT("H_R"),       TEXT("H_rotation_L"),
        TEXT("H_rotation_R"), TEXT("HP_L"),      TEXT("HP_R"),
        TEXT("0_L"),          TEXT("1_L"),       TEXT("2_L"),
        TEXT("3_L"),          TEXT("4_L"),       TEXT("5_R"),
        TEXT("6_R"),          TEXT("7_R"),       TEXT("8_R"),
        TEXT("9_R"),          TEXT("S_L"),       TEXT("S_R"),
        TEXT("Tar_Body"),     TEXT("Tar_Chest"), TEXT("Tar_Butt")};

    // 检查名称是否在有效列表中
    if (ValidControllerNames.Contains(InputName)) {
        return InputName;
    }

    // 无法纠正
    UE_LOG(LogTemp, Error,
           TEXT("INVALID CONTROLLER: '%s' - Valid: H_L, H_R, 0_L-4_L, 5_R-9_R, "
                "S_L, S_R, Tar_Body/Chest/Butt"),
           *InputName);
    return FString();
}

// 辅助方法：处理单个动画帧对象（前置处理 H_L/H_rotation_L 和
// H_R/H_rotation_R的合并）
static void ProcessAnimationFrame(
    TSharedPtr<FJsonObject> FrameObject,
    TMap<FString, TArray<FControlKeyframe>>& ControlKeyframeData,
    int32 FrameIndex, int32& FailedFrames, int32& KeyframesAdded) {
    if (!FrameObject.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("Frame %d is not a valid JSON object"),
               FrameIndex);
        FailedFrames++;
        return;
    }

    // 获取帧数
    float FrameNumber = 0.0f;
    if (FrameObject->HasField(TEXT("frame"))) {
        FrameNumber = FrameObject->GetNumberField(TEXT("frame"));
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Frame %d does not have 'frame' field"),
               FrameIndex);
        FrameNumber = FrameIndex;
    }

    // 获取 hand_infos 对象
    TSharedPtr<FJsonObject> HandInfos = nullptr;
    if (FrameObject->HasField(TEXT("hand_infos"))) {
        HandInfos = FrameObject->GetObjectField(TEXT("hand_infos"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Frame %d does not have 'hand_infos' field"), FrameIndex);
        FailedFrames++;
        return;
    }

    if (!HandInfos.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("Frame %d hand_infos is not valid"),
               FrameIndex);
        FailedFrames++;
        return;
    }

    // === 第一步：提前提取旋转数据（用于后续合并） ===
    FQuat H_L_Rotation = FQuat::Identity;
    FQuat H_R_Rotation = FQuat::Identity;
    bool bHasH_L_Rotation = false;
    bool bHasH_R_Rotation = false;

    // 检查 H_rotation_L
    if (HandInfos->HasField(TEXT("H_rotation_L"))) {
        TSharedPtr<FJsonValue> RotationDataValue =
            HandInfos->TryGetField(TEXT("H_rotation_L"));
        if (RotationDataValue.IsValid()) {
            TArray<TSharedPtr<FJsonValue>> DataArray =
                RotationDataValue->AsArray();
            if (DataArray.Num() == 4) {
                H_L_Rotation.W = DataArray[0]->AsNumber();
                H_L_Rotation.X = DataArray[1]->AsNumber();
                H_L_Rotation.Y = DataArray[2]->AsNumber();
                H_L_Rotation.Z = DataArray[3]->AsNumber();
                bHasH_L_Rotation = true;
            }
        }
    }

    // 检查 H_rotation_R
    if (HandInfos->HasField(TEXT("H_rotation_R"))) {
        TSharedPtr<FJsonValue> RotationDataValue =
            HandInfos->TryGetField(TEXT("H_rotation_R"));
        if (RotationDataValue.IsValid()) {
            TArray<TSharedPtr<FJsonValue>> DataArray =
                RotationDataValue->AsArray();
            if (DataArray.Num() == 4) {
                H_R_Rotation.W = DataArray[0]->AsNumber();
                H_R_Rotation.X = DataArray[1]->AsNumber();
                H_R_Rotation.Y = DataArray[2]->AsNumber();
                H_R_Rotation.Z = DataArray[3]->AsNumber();
                bHasH_R_Rotation = true;
            }
        }
    }

    // === 第二步：遍历每个控制器的数据，并在必要时合并旋转数据 ===
    for (const auto& Pair : HandInfos->Values) {
        FString RawControlName = Pair.Key;

        // 跳过旋转控制器（已在上面提取）
        if (RawControlName == TEXT("H_rotation_L") ||
            RawControlName == TEXT("H_rotation_R")) {
            continue;
        }

        // 验证和纠正控制器名称
        FString ControlName = ValidateAndCorrectControllerName(RawControlName);

        // 如果验证后名称为空，跳过此控制器
        if (ControlName.IsEmpty()) {
            continue;
        }

        TSharedPtr<FJsonValue> ControlDataValue = Pair.Value;

        if (!ControlDataValue.IsValid()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Frame %.1f control %s has invalid data"), FrameNumber,
                   *ControlName);
            continue;
        }

        // 判断数据维度：3维为位置，4维为旋转
        TArray<TSharedPtr<FJsonValue>> DataArray = ControlDataValue->AsArray();

        if (DataArray.Num() == 0) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Frame %.1f control %s has empty data array"),
                   FrameNumber, *ControlName);
            continue;
        }

        if (DataArray.Num() == 3) {
            // 3维数据 - 位置
            FVector Location;
            Location.X = DataArray[0]->AsNumber();
            Location.Y = DataArray[1]->AsNumber();
            Location.Z = DataArray[2]->AsNumber();

            FControlKeyframe Keyframe;
            Keyframe.FrameNumber = static_cast<int32>(FrameNumber);
            Keyframe.Translation = Location;

            // 如果是 H_L 或 H_R，尝试使用提前提取的旋转数据
            if (ControlName == TEXT("H_L") && bHasH_L_Rotation) {
                Keyframe.Rotation = H_L_Rotation;
            } else if (ControlName == TEXT("H_R") && bHasH_R_Rotation) {
                Keyframe.Rotation = H_R_Rotation;
            } else {
                Keyframe.Rotation = FQuat::Identity;  // 无旋转
            }

            ControlKeyframeData.FindOrAdd(ControlName).Add(Keyframe);

        } else if (DataArray.Num() == 4) {
            // 4维数据 - 旋转（这种情况不应该在这里出现，因为旋转已在上面处理）
            // 但是还添加一个处理在这里，以备后面有其它控制器使用旋转数据
            FQuat Rotation;
            Rotation.W = DataArray[0]->AsNumber();
            Rotation.X = DataArray[1]->AsNumber();
            Rotation.Y = DataArray[2]->AsNumber();
            Rotation.Z = DataArray[3]->AsNumber();

            FControlKeyframe Keyframe;
            Keyframe.FrameNumber = static_cast<int32>(FrameNumber);
            Keyframe.Translation = FVector::ZeroVector;
            Keyframe.Rotation = Rotation;

            ControlKeyframeData.FindOrAdd(ControlName).Add(Keyframe);

        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("Frame %.1f control %s has unexpected data "
                        "dimension: %d"),
                   FrameNumber, *ControlName, DataArray.Num());
            continue;
        }

        KeyframesAdded++;
    }
}

void UKeyRippleAnimationProcessor::MakeAnimation(
    AKeyRippleUnreal* KeyRippleActor, const FString& AnimationFilePath) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error, TEXT("MakeAnimation: KeyRippleActor is null"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Making animation with Control Rig integration: %s"),
           *AnimationFilePath);

    // 1. 读取动画文件
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *AnimationFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load animation file: %s"),
               *AnimationFilePath);
        return;
    }

    // 2. 解析JSON数组
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);

    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse JSON array from animation file: %s"),
               *AnimationFilePath);
        return;
    }

    // 3. 获取 Control Rig Instance
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!UKeyRippleControlRigProcessor::GetControlRigFromSkeletalMeshActor(
            KeyRippleActor->SkeletalMeshActor, ControlRigInstance,
            ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRigInstance is null in MakeAnimation"));
        return;
    }

    // 4. 获取 Sequencer 和 Level Sequence
    TSharedPtr<ISequencer> Sequencer = nullptr;
    ULevelSequence* LevelSequence = nullptr;

    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
        TArray<TWeakPtr<ISequencer>> WeakSequencers =
            FLevelEditorSequencerIntegration::Get().GetSequencers();

        for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
            if (TSharedPtr<ISequencer> CurrentSequencer = WeakSequencer.Pin()) {
                UMovieSceneSequence* RootSequence =
                    CurrentSequencer->GetRootMovieSceneSequence();

                if (!RootSequence) {
                    continue;
                }

                LevelSequence = Cast<ULevelSequence>(RootSequence);
                if (LevelSequence) {
                    Sequencer = CurrentSequencer;
                    break;
                }
            }
        }
    }

    if (!Sequencer.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("No Sequencer is open. Cannot add keyframes to Level "
                    "Sequence."));
        return;
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("No Level Sequence found. Cannot add keyframes."));
        return;
    }

    // === FIX #5: Validate no duplicate tracks before clearing ===
    bool bHasDuplicateTracks =
        ValidateNoExistingTracks(LevelSequence, ControlRigInstance, true);
    if (bHasDuplicateTracks) {
        UE_LOG(LogTemp, Warning,
               TEXT("Duplicate Control Rig tracks detected and auto-fixed. "
                    "Proceeding with animation generation."));
    }

    // 5. 清空Control Rig轨道上的原有关键帧数据
    UE_LOG(LogTemp, Warning,
           TEXT("Clearing existing Control Rig keyframes before adding new "
                "keyframes"));
    ClearControlRigKeyframes(LevelSequence, ControlRigInstance, KeyRippleActor);

    UE_LOG(LogTemp, Warning, TEXT("Starting to process %d animation frames"),
           JsonArray.Num());

    // 准备批量插入的关键帧数据
    TMap<FString, TArray<FControlKeyframe>> ControlKeyframeData;

    int32 ProcessedFrames = 0;
    int32 FailedFrames = 0;
    int32 KeyframesAdded = 0;

    // 6. 处理每一帧并收集关键帧数据
    for (int32 FrameIndex = 0; FrameIndex < JsonArray.Num(); ++FrameIndex) {
        TSharedPtr<FJsonObject> FrameObject = JsonArray[FrameIndex]->AsObject();

        // 使用辅助方法处理单个帧
        ProcessAnimationFrame(FrameObject, ControlKeyframeData, FrameIndex,
                              FailedFrames, KeyframesAdded);

        ProcessedFrames++;
    }

    // 7. 批量插入关键帧
    BatchInsertControlRigKeys(LevelSequence, ControlRigInstance,
                              ControlKeyframeData);

    // 8. 通知 Sequencer 更新显示
    if (Sequencer.IsValid()) {
        Sequencer->NotifyMovieSceneDataChanged(
            EMovieSceneDataChangeType::RefreshAllImmediately);
        UE_LOG(LogTemp, Warning, TEXT("Notified Sequencer to refresh display"));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== MakeAnimation Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed: %d frames"),
           ProcessedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Failed frames: %d"), FailedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Total keyframes added to Sequencer: %d"),
           KeyframesAdded);
    UE_LOG(LogTemp, Warning,
           TEXT("========== MakeAnimation Completed =========="));
}

// 批量插入控制关键帧
void UKeyRippleAnimationProcessor::BatchInsertControlRigKeys(
    ULevelSequence* LevelSequence, UControlRig* ControlRigInstance,
    const TMap<FString, TArray<FControlKeyframe>>& ControlKeyframeData) {
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence is null"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error, TEXT("ControlRigInstance is null"));
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return;
    }

    UMovieSceneControlRigParameterTrack* TargetControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!TargetControlRigTrack) {
        UE_LOG(
            LogTemp, Error,
            TEXT("Failed to find ControlRigParameterTrack for ControlRig: %s"),
            *ControlRigInstance->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Found ControlRigParameterTrack for ControlRig: %s"),
           *ControlRigInstance->GetName());

    // 获取Track中的所有Section
    TArray<UMovieSceneSection*> Sections =
        TargetControlRigTrack->GetAllSections();
    if (Sections.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("ControlRig Track has no sections"));

        // 创建新的Section
        UMovieSceneSection* NewSection =
            TargetControlRigTrack->CreateNewSection();
        if (NewSection) {
            TargetControlRigTrack->AddSection(*NewSection);
            Sections = TargetControlRigTrack->GetAllSections();
        }

        if (Sections.Num() == 0) {
            UE_LOG(LogTemp, Error, TEXT("Failed to create section"));
            return;
        }
    }

    // 取第一个Section（目前只处理一个Section的情况）
    UMovieSceneSection* Section = Sections[0];
    if (!Section) {
        UE_LOG(LogTemp, Error, TEXT("Section is null"));
        return;
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    // 初始化 MinFrame 和 MaxFrame
    FFrameNumber MinFrame(MAX_int32);
    FFrameNumber MaxFrame(MIN_int32);

    UE_LOG(LogTemp, Warning, TEXT("[PERFORMER] ===== FRAME RATE INFO ====="));
    UE_LOG(LogTemp, Warning, TEXT("[PERFORMER] Tick Resolution: %d/%d = %.4f"),
           TickResolution.Numerator, TickResolution.Denominator,
           (float)TickResolution.Numerator / TickResolution.Denominator);
    UE_LOG(LogTemp, Warning, TEXT("[PERFORMER] Display Rate: %d/%d = %.4f"),
           DisplayRate.Numerator, DisplayRate.Denominator,
           (float)DisplayRate.Numerator / DisplayRate.Denominator);
    float ScalingFactor = (float)TickResolution.Numerator *
                          DisplayRate.Denominator /
                          (TickResolution.Denominator * DisplayRate.Numerator);
    UE_LOG(LogTemp, Warning, TEXT("[PERFORMER] Scaling Factor: %.4f"),
           ScalingFactor);

    UE_LOG(LogTemp, Warning, TEXT("[DEBUG] Total controls to process: %d"),
           ControlKeyframeData.Num());

    // 对每个控制对象
    for (const auto& ControlPair : ControlKeyframeData) {
        const FString& ControlName = ControlPair.Key;
        const TArray<FControlKeyframe>& Keyframes = ControlPair.Value;
        FString Prefix = ControlName + TEXT(".");

        UE_LOG(LogTemp, Warning,
               TEXT("[DEBUG] Processing control '%s' with %d keyframes"),
               *ControlName, Keyframes.Num());

        // 查找位置通道
        FMovieSceneFloatChannel* LocationX = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sLocation.X"), *Prefix));
        FMovieSceneFloatChannel* LocationY = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sLocation.Y"), *Prefix));
        FMovieSceneFloatChannel* LocationZ = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sLocation.Z"), *Prefix));

        // 查找旋转通道（Rotation.X, Rotation.Y, Rotation.Z）
        FMovieSceneFloatChannel* RotationX = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sRotation.X"), *Prefix));
        FMovieSceneFloatChannel* RotationY = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sRotation.Y"), *Prefix));
        FMovieSceneFloatChannel* RotationZ = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sRotation.Z"), *Prefix));

        if (!LocationX || !LocationY || !LocationZ || !RotationX ||
            !RotationY || !RotationZ) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Missing channel in control '%s', skipping keyframes"),
                   *ControlName);
            continue;
        }

        TArray<FKeyHandle> KeyHandlesToDelete;

        // 准备批量插入的数据
        TArray<FFrameNumber> Times;
        TArray<FMovieSceneFloatValue> LocationXValues, LocationYValues,
            LocationZValues;
        TArray<FMovieSceneFloatValue> RotationXValues, RotationYValues,
            RotationZValues;

        // 遍历关键帧并添加到批量数据数组
        for (int32 KeyIdx = 0; KeyIdx < Keyframes.Num(); ++KeyIdx) {
            const FControlKeyframe& Keyframe = Keyframes[KeyIdx];

            // 转换帧数，确保基于TickResolution
            int32 ScaledFrameNumber =
                Keyframe.FrameNumber * TickResolution.Numerator *
                DisplayRate.Denominator /
                (TickResolution.Denominator * DisplayRate.Numerator);

            FFrameNumber FrameNum(ScaledFrameNumber);
            Times.Add(FrameNum);

            // 更新 MinFrame 和 MaxFrame
            if (FrameNum < MinFrame) {
                MinFrame = FrameNum;
            }
            if (FrameNum > MaxFrame) {
                MaxFrame = FrameNum;
            }

            // 为每个通道准备值
            LocationXValues.Add(FMovieSceneFloatValue(Keyframe.Translation.X));
            LocationYValues.Add(FMovieSceneFloatValue(Keyframe.Translation.Y));
            LocationZValues.Add(FMovieSceneFloatValue(Keyframe.Translation.Z));

            // 仅在插入时才将四元数转换为欧拉角
            // 这样可以避免提前转换导致的精度丢失(其实没有起作用)
            FRotator EulerRotation = Keyframe.Rotation.Rotator();
            RotationXValues.Add(
                FMovieSceneFloatValue(EulerRotation.Roll));  // X轴旋转 = Pitch
            RotationYValues.Add(
                FMovieSceneFloatValue(EulerRotation.Pitch));  // Y轴旋转 = Yaw
            RotationZValues.Add(
                FMovieSceneFloatValue(EulerRotation.Yaw));  // Z轴旋转 = Roll
        }

        UE_LOG(LogTemp, Warning,
               TEXT("[DEBUG] Control '%s': Prepared %d times, %d location "
                    "values, %d rotation values"),
               *ControlName, Times.Num(), LocationXValues.Num(),
               RotationXValues.Num());

        // === 预处理旋转数据，解决欧拉角插值跳变问题 ===
        UE_LOG(
            LogTemp, Warning,
            TEXT("[DEBUG] Control '%s': Starting rotation unwrap processing"),
            *ControlName);
        ProcessRotationChannelsUnwrap(RotationXValues, RotationYValues,
                                      RotationZValues);
        UE_LOG(
            LogTemp, Warning,
            TEXT("[DEBUG] Control '%s': Rotation unwrap processing completed"),
            *ControlName);

        // Check if this is a Tar_ control (only insert X-axis for displacement)
        bool bIsTarControl =
            ControlName.Contains(TEXT("Tar_"), ESearchCase::IgnoreCase);

        // 批量设置关键帧
        if (bIsTarControl) {
            // For Tar_ controls, only add X-axis keyframes (displacement)
            LocationX->AddKeys(Times, LocationXValues);
            UE_LOG(LogTemp, Warning,
                   TEXT("[DEBUG] Tar_ control '%s': Only X-axis keys added (Y "
                        "and Z skipped)"),
                   *ControlName);
        } else {
            // For normal controls, add all channels
            LocationX->AddKeys(Times, LocationXValues);
            LocationY->AddKeys(Times, LocationYValues);
            LocationZ->AddKeys(Times, LocationZValues);

            RotationX->AddKeys(Times, RotationXValues);
            RotationY->AddKeys(Times, RotationYValues);
            RotationZ->AddKeys(Times, RotationZValues);
        }

        UE_LOG(LogTemp, Warning,
               TEXT("[DEBUG] Control '%s': Keys added successfully"),
               *ControlName);
    }

    // === FIX #4: Properly validate frame range before setting ===
    if (MinFrame != MAX_int32 && MaxFrame != MIN_int32 &&
        MinFrame <= MaxFrame) {
        // Add 1 frame padding to MaxFrame (exclusive end)
        Section->SetRange(TRange<FFrameNumber>(MinFrame, MaxFrame + 1));
        UE_LOG(LogTemp, Warning, TEXT("Set section range to %d - %d"),
               MinFrame.Value, (MaxFrame + 1).Value);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Warning: Invalid frame range detected. MinFrame=%d, "
                    "MaxFrame=%d. Section range not updated."),
               MinFrame.Value, MaxFrame.Value);
    }

    // 修改后刷新
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();
#if WITH_EDITOR
    // 如果在编辑器中，可能需要刷新Sequencer UI
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif
    UE_LOG(LogTemp, Warning, TEXT("Batch keyframe insertion finished."));
}

FMovieSceneFloatChannel* UKeyRippleAnimationProcessor::FindFloatChannel(
    UMovieSceneSection* Section, const FString& ChannelName) {
    if (!Section) {
        UE_LOG(LogTemp, Warning, TEXT("FindFloatChannel: Section is null"));
        return nullptr;
    }

    FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
    FName ChannelNameAsFName(*ChannelName);

    TMovieSceneChannelHandle<FMovieSceneFloatChannel> ChannelHandle =
        ChannelProxy.GetChannelByName<FMovieSceneFloatChannel>(
            ChannelNameAsFName);

    if (ChannelHandle.Get()) {
        return ChannelHandle.Get();
    }

    // === ENHANCED: Better diagnostics for missing channels ===
    UE_LOG(LogTemp, Error,
           TEXT("FindFloatChannel: ✗ Failed to find channel '%s'"),
           *ChannelName);

    // Log all available channel names for debugging
    LogAvailableChannels(Section);

    return nullptr;
}

bool UKeyRippleAnimationProcessor::ParseKeyRippleFile(
    AKeyRippleUnreal* KeyRippleActor, FString& OutAnimationPath,
    FString& OutKeyAnimationPath) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("ParseKeyRippleFile: KeyRippleActor is null"));
        return false;
    }

    const FString& KeyRippleFilePath = KeyRippleActor->AnimationFilePath;

    if (KeyRippleFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleFilePath is empty in ParseKeyRippleFile"));
        return false;
    }

    // 读取文件内容
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *KeyRippleFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load KeyRipple file: %s"),
               *KeyRippleFilePath);
        return false;
    }

    // 解析JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) ||
        !JsonObject.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse JSON from KeyRipple file: %s"),
               *KeyRippleFilePath);
        return false;
    }

    // 提取动画路径
    if (JsonObject->HasField(TEXT("animation_path"))) {
        OutAnimationPath = JsonObject->GetStringField(TEXT("animation_path"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("No animation_path field found in KeyRipple file"));
        OutAnimationPath = FString();
    }

    // 提取钢琴键动画路径
    if (JsonObject->HasField(TEXT("key_animation_path"))) {
        OutKeyAnimationPath =
            JsonObject->GetStringField(TEXT("key_animation_path"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("No key_animation_path field found in KeyRipple file"));
        OutKeyAnimationPath = FString();
    }

    UE_LOG(LogTemp, Warning, TEXT("ParseKeyRippleFile succeeded"));
    UE_LOG(LogTemp, Warning, TEXT("  Animation Path: %s"), *OutAnimationPath);
    UE_LOG(LogTemp, Warning, TEXT("  Key Animation Path: %s"),
           *OutKeyAnimationPath);

    return true;
}

void UKeyRippleAnimationProcessor::GeneratePerformerAnimation(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GeneratePerformerAnimation: KeyRippleActor is null"));
        return;
    }

    FString AnimationPath;
    FString KeyAnimationPath;

    // 解析KeyRipple文件
    if (!ParseKeyRippleFile(KeyRippleActor, AnimationPath, KeyAnimationPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse KeyRipple file in "
                    "GeneratePerformerAnimation"));
        return;
    }

    if (AnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("Animation path is empty in GeneratePerformerAnimation"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Generating performer animation from: %s"),
           *AnimationPath);

    // 调用MakeAnimation生成演奏动画
    MakeAnimation(KeyRippleActor, AnimationPath);
}

void UKeyRippleAnimationProcessor::GeneratePianoKeyAnimation(
    AKeyRippleUnreal* KeyRippleActor, const FString& PianoKeyAnimationPath) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GeneratePianoKeyAnimation: KeyRippleActor is null"));
        return;
    }

    if (PianoKeyAnimationPath.IsEmpty()) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "PianoKeyAnimationPath is empty in GeneratePianoKeyAnimation"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("GeneratePianoKeyAnimation: Starting animation generation "
                "from: %s"),
           *PianoKeyAnimationPath);

    // Call the new Level Sequencer method
    UKeyRipplePianoProcessor::GenerateMorphTargetAnimationInLevelSequencer(
        KeyRippleActor, PianoKeyAnimationPath);

    UE_LOG(LogTemp, Warning, TEXT("GeneratePianoKeyAnimation completed"));
}

void UKeyRippleAnimationProcessor::GenerateAllAnimation(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateAllAnimation: KeyRippleActor is null"));
        return;
    }

    FString AnimationPath;
    FString KeyAnimationPath;

    // 解析KeyRipple文件
    if (!ParseKeyRippleFile(KeyRippleActor, AnimationPath, KeyAnimationPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse KeyRipple file in GenerateAllAnimation"));
        return;
    }

    // 生成演奏动画
    if (!AnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating performer animation from: %s"), *AnimationPath);
        MakeAnimation(KeyRippleActor, AnimationPath);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Animation path is empty, skipping performer animation "
                    "generation"));
    }

    // 生成钢琴键动画
    if (!KeyAnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating piano key animation from: %s"),
               *KeyAnimationPath);
        GeneratePianoKeyAnimation(KeyRippleActor, KeyAnimationPath);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Key animation path is empty, skipping piano key animation "
                    "generation"));
    }

    UE_LOG(LogTemp, Warning, TEXT("GenerateAllAnimation completed"));
}

void UKeyRippleAnimationProcessor::ClearControlRigKeyframes(
    ULevelSequence* LevelSequence, UControlRig* ControlRigInstance,
    AKeyRippleUnreal* KeyRippleActor) {
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence is null"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error, TEXT("ControlRigInstance is null"));
        return;
    }

    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error, TEXT("KeyRippleActor is null"));
        return;
    }

    // 检查给定的ControlRig是否已绑定到任何轨道
    UMovieSceneControlRigParameterTrack* TargetTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!TargetTrack) {
        UE_LOG(LogTemp, Warning,
               TEXT("ControlRig %s is not bound to any track in the sequence"),
               *ControlRigInstance->GetName());
        return;
    }

    // 收集需要清理的所有控制器名称
    TSet<FString> ControlNamesToClean;

    // 添加 FingerControllers 的值
    for (const auto& Pair : KeyRippleActor->FingerControllers) {
        ControlNamesToClean.Add(Pair.Value);
    }

    // 添加 HandControllers 的值
    for (const auto& Pair : KeyRippleActor->HandControllers) {
        ControlNamesToClean.Add(Pair.Value);
    }

    // 添加 TargetPoints 的值
    for (const auto& Pair : KeyRippleActor->TargetPoints) {
        ControlNamesToClean.Add(Pair.Value);
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Identified %d control names to clean from animation tracks"),
           ControlNamesToClean.Num());

    // 获取Track中的所有Section
    TArray<UMovieSceneSection*> AllSections = TargetTrack->GetAllSections();

    if (AllSections.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("ControlRig Track has no sections"));
        return;
    }

    int32 ClearedChannelsCount = 0;

    // 遍历所有 sections，清空指定控制器的通道
    for (UMovieSceneSection* Section : AllSections) {
        if (!Section) {
            continue;
        }

        // 遍历需要清理的每个控制器
        for (const FString& ControlName : ControlNamesToClean) {
            FString Prefix = ControlName + TEXT(".");

            // 查找并清空位置通道
            FMovieSceneFloatChannel* LocationX = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sLocation.X"), *Prefix));
            FMovieSceneFloatChannel* LocationY = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sLocation.Y"), *Prefix));
            FMovieSceneFloatChannel* LocationZ = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sLocation.Z"), *Prefix));

            // 查找并清空旋转通道
            FMovieSceneFloatChannel* RotationX = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sRotation.X"), *Prefix));
            FMovieSceneFloatChannel* RotationY = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sRotation.Y"), *Prefix));
            FMovieSceneFloatChannel* RotationZ = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sRotation.Z"), *Prefix));

            // 清空通道数据
            if (LocationX) {
                LocationX->Reset();
                ClearedChannelsCount++;
            }
            if (LocationY) {
                LocationY->Reset();
                ClearedChannelsCount++;
            }
            if (LocationZ) {
                LocationZ->Reset();
                ClearedChannelsCount++;
            }

            if (RotationX) {
                RotationX->Reset();
                ClearedChannelsCount++;
            }
            if (RotationY) {
                RotationY->Reset();
                ClearedChannelsCount++;
            }
            if (RotationZ) {
                RotationZ->Reset();
                ClearedChannelsCount++;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Cleared %d channels from Control Rig track"),
           ClearedChannelsCount);

    // 标记LevelSequence为已修改
    LevelSequence->MarkPackageDirty();

    UE_LOG(LogTemp, Warning,
           TEXT("Control Rig keyframes cleared for specified controls"));
}

#undef LOCTEXT_NAMESPACE