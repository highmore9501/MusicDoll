#include "BeatBloomAnimationProcessor.h"

#include "BeatBloomDrumKitProcessor.h"
#include "BeatBloomUnreal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "InstrumentAnimationUtility.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "LevelSequence.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "BeatBloomAnimationProcessor"

void UBeatBloomAnimationProcessor::GeneratePerformerAnimation(
    ABeatBloomUnreal* BeatBloomActor) {
    // 解析 .beatbloom 文件获取动画路径
    FString PerformerAnimationPath;
    FString DrumKitAnimationPath;

    if (!ABeatBloomUnreal::ParseBeatBloomFile(BeatBloomActor->AnimationFilePath,
                                              PerformerAnimationPath,
                                              DrumKitAnimationPath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse .beatbloom file: %s"),
               *BeatBloomActor->AnimationFilePath);
        return;
    }

    // 读取 .animation JSON 文件
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *PerformerAnimationPath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load animation file: %s"),
               *PerformerAnimationPath);
        return;
    }

    // 解析 JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse animation JSON: %s"),
               *PerformerAnimationPath);
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

    // 获取 ControlRig 缓存
    UControlRig* ControlRigInstance =
        BeatBloomActor->GetCachedControlRig(TEXT("Performer"));
    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ControlRig instance for Performer"));
        return;
    }

    // 构建关键帧数据
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;
    int32 ProcessedFrames = 0;
    int32 KeyframesAdded = 0;

    // 处理左手动画
    if (JsonObject->HasField(TEXT("left_hand_animation"))) {
        auto LeftHandArray =
            JsonObject->GetArrayField(TEXT("left_hand_animation"));
        ProcessHandAnimation(LeftHandArray, TEXT("L"), ControlKeyframeData,
                             ProcessedFrames, KeyframesAdded);
    }

    // 处理右手动画
    if (JsonObject->HasField(TEXT("right_hand_animation"))) {
        auto RightHandArray =
            JsonObject->GetArrayField(TEXT("right_hand_animation"));
        ProcessHandAnimation(RightHandArray, TEXT("R"), ControlKeyframeData,
                             ProcessedFrames, KeyframesAdded);
    }

    // 处理左脚动画
    if (JsonObject->HasField(TEXT("left_foot_animation"))) {
        auto LeftFootArray =
            JsonObject->GetArrayField(TEXT("left_foot_animation"));
        ProcessFootAnimation(LeftFootArray, TEXT("L"), ControlKeyframeData,
                             ProcessedFrames, KeyframesAdded);
    }

    // 处理右脚动画
    if (JsonObject->HasField(TEXT("right_foot_animation"))) {
        auto RightFootArray =
            JsonObject->GetArrayField(TEXT("right_foot_animation"));
        ProcessFootAnimation(RightFootArray, TEXT("R"), ControlKeyframeData,
                             ProcessedFrames, KeyframesAdded);
    }

    // 处理头部控制器动画
    if (JsonObject->HasField(TEXT("head_control_animation"))) {
        auto HeadControlArray =
            JsonObject->GetArrayField(TEXT("head_control_animation"));
        ProcessHeadControlAnimation(HeadControlArray, ControlKeyframeData,
                                    ProcessedFrames, KeyframesAdded);
    }

    // 配置批量插入设置
    FBatchInsertKeyframesSettings Settings;

    // 批量插入关键帧
    UInstrumentAnimationUtility::BatchInsertControlRigKeys(
        LevelSequence, ControlRigInstance, ControlKeyframeData, Settings);

    // 刷新显示
    BeatBloomActor->TriggerControlRigReregistration(
        TEXT("Generate Performer Animation"));

    UE_LOG(LogTemp, Warning,
           TEXT("========== GeneratePerformerAnimation Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed: %d frames"),
           ProcessedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Total keyframes added: %d"), KeyframesAdded);
    UE_LOG(LogTemp, Warning,
           TEXT("========== GeneratePerformerAnimation Completed =========="));
}

void UBeatBloomAnimationProcessor::GenerateDrumKitAnimation(
    ABeatBloomUnreal* BeatBloomActor) {
    // 解析 .beatbloom 文件获取动画路径
    FString PerformerAnimationPath;
    FString DrumKitAnimationPath;

    if (!ABeatBloomUnreal::ParseBeatBloomFile(BeatBloomActor->AnimationFilePath,
                                              PerformerAnimationPath,
                                              DrumKitAnimationPath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse .beatbloom file: %s"),
               *BeatBloomActor->AnimationFilePath);
        return;
    }

    // 委托给 UBeatBloomDrumKitProcessor::GenerateDrumKitAnimationFromPath
    UBeatBloomDrumKitProcessor::GenerateDrumKitAnimationFromPath(
        BeatBloomActor, DrumKitAnimationPath);
}

void UBeatBloomAnimationProcessor::GenerateAllAnimation(
    ABeatBloomUnreal* BeatBloomActor) {
    // 依次调用 GeneratePerformerAnimation 和 GenerateDrumKitAnimation
    GeneratePerformerAnimation(BeatBloomActor);
    GenerateDrumKitAnimation(BeatBloomActor);
}

void UBeatBloomAnimationProcessor::ProcessHandAnimation(
    const TArray<TSharedPtr<FJsonValue>>& AnimationArray,
    const FString& HandSuffix,
    TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
    int32& OutProcessedFrames, int32& OutKeyframesAdded) {
    // 遍历帧数组，提取 position/rotation/pivot_position
    // 映射到 H_{Suffix}, HP_{Suffix}

    for (const auto& FrameValue : AnimationArray) {
        TSharedPtr<FJsonObject> FrameObject = FrameValue->AsObject();
        if (!FrameObject.IsValid()) {
            continue;
        }

        // 获取帧编号
        double FrameNumberDouble = FrameObject->GetNumberField(TEXT("frame"));
        int32 FrameNumber = FMath::RoundToInt(FrameNumberDouble);

        // 处理 position -> H_{Suffix}（位置 + 旋转）
        if (FrameObject->HasField(TEXT("position"))) {
            auto PositionArray = FrameObject->GetArrayField(TEXT("position"));
            FVector Location;
            Location.X = PositionArray[0]->AsNumber();
            Location.Y = PositionArray[1]->AsNumber();
            Location.Z = PositionArray[2]->AsNumber();

            FAnimationKeyframe Keyframe;
            Keyframe.FrameNumber = FrameNumber;
            Keyframe.Translation = Location;
            Keyframe.bHasLocation = true;

            // 提取 rotation -> H_{Suffix} 的旋转部分
            if (FrameObject->HasField(TEXT("rotation"))) {
                auto RotationArray =
                    FrameObject->GetArrayField(TEXT("rotation"));
                FQuat Rotation;
                Rotation.W = RotationArray[0]->AsNumber();
                Rotation.X = RotationArray[1]->AsNumber();
                Rotation.Y = RotationArray[2]->AsNumber();
                Rotation.Z = RotationArray[3]->AsNumber();
                Rotation.Normalize();

                Keyframe.Rotation = Rotation;
                Keyframe.bHasRotation = true;
            }

            ControlKeyframeData
                .FindOrAdd(FString::Printf(TEXT("H_%s"), *HandSuffix))
                .Add(Keyframe);
            OutKeyframesAdded++;
        }

        // 处理 pivot_position -> HP_{Suffix}（只位置）
        if (FrameObject->HasField(TEXT("pivot_position"))) {
            auto PivotPositionArray =
                FrameObject->GetArrayField(TEXT("pivot_position"));
            FVector PivotLocation;
            PivotLocation.X = PivotPositionArray[0]->AsNumber();
            PivotLocation.Y = PivotPositionArray[1]->AsNumber();
            PivotLocation.Z = PivotPositionArray[2]->AsNumber();

            FAnimationKeyframe PivotKeyframe;
            PivotKeyframe.FrameNumber = FrameNumber;
            PivotKeyframe.Translation = PivotLocation;
            PivotKeyframe.bHasLocation = true;
            PivotKeyframe.bHasRotation = false;

            ControlKeyframeData
                .FindOrAdd(FString::Printf(TEXT("HP_%s"), *HandSuffix))
                .Add(PivotKeyframe);
            OutKeyframesAdded++;
        }

        OutProcessedFrames++;
    }
}

void UBeatBloomAnimationProcessor::ProcessFootAnimation(
    const TArray<TSharedPtr<FJsonValue>>& AnimationArray,
    const FString& FootSuffix,
    TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
    int32& OutProcessedFrames, int32& OutKeyframesAdded) {
    // 遍历帧数组，提取 position/rotation
    // 映射到 F_{Suffix}（位置 + 旋转）
    // 注意：脚部不使用 pivot_position

    for (const auto& FrameValue : AnimationArray) {
        TSharedPtr<FJsonObject> FrameObject = FrameValue->AsObject();
        if (!FrameObject.IsValid()) {
            continue;
        }

        // 获取帧编号
        double FrameNumberDouble = FrameObject->GetNumberField(TEXT("frame"));
        int32 FrameNumber = FMath::RoundToInt(FrameNumberDouble);

        // 处理 position -> F_{Suffix}（位置 + 旋转）
        if (FrameObject->HasField(TEXT("position"))) {
            auto PositionArray = FrameObject->GetArrayField(TEXT("position"));
            FVector Location;
            Location.X = PositionArray[0]->AsNumber();
            Location.Y = PositionArray[1]->AsNumber();
            Location.Z = PositionArray[2]->AsNumber();

            FAnimationKeyframe Keyframe;
            Keyframe.FrameNumber = FrameNumber;
            Keyframe.Translation = Location;
            Keyframe.bHasLocation = true;

            // 提取 rotation -> F_{Suffix} 的旋转部分
            if (FrameObject->HasField(TEXT("rotation"))) {
                auto RotationArray =
                    FrameObject->GetArrayField(TEXT("rotation"));
                FQuat Rotation;
                Rotation.W = RotationArray[0]->AsNumber();
                Rotation.X = RotationArray[1]->AsNumber();
                Rotation.Y = RotationArray[2]->AsNumber();
                Rotation.Z = RotationArray[3]->AsNumber();
                Rotation.Normalize();

                Keyframe.Rotation = Rotation;
                Keyframe.bHasRotation = true;
            }

            ControlKeyframeData
                .FindOrAdd(FString::Printf(TEXT("F_%s"), *FootSuffix))
                .Add(Keyframe);
            OutKeyframesAdded++;
        }

        OutProcessedFrames++;
    }
}

void UBeatBloomAnimationProcessor::ProcessHeadControlAnimation(
    const TArray<TSharedPtr<FJsonValue>>& AnimationArray,
    TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
    int32& OutProcessedFrames, int32& OutKeyframesAdded) {
    // 处理 Head_Control 动画（完整XYZ位置）
    // JSON 结构：{ "frame": N, "head_control_position": [x, y, z] }

    for (const auto& FrameValue : AnimationArray) {
        TSharedPtr<FJsonObject> FrameObject = FrameValue->AsObject();
        if (!FrameObject.IsValid()) {
            continue;
        }

        // 获取帧编号
        double FrameNumberDouble = FrameObject->GetNumberField(TEXT("frame"));
        int32 FrameNumber = FMath::RoundToInt(FrameNumberDouble);

        // 处理 head_control_position -> Head_Control（使用完整 XYZ）
        if (FrameObject->HasField(TEXT("head_control_position"))) {
            auto PositionArray =
                FrameObject->GetArrayField(TEXT("head_control_position"));
            FVector Location;
            Location.X = PositionArray[0]->AsNumber();
            Location.Y = PositionArray[1]->AsNumber();
            Location.Z = PositionArray[2]->AsNumber();

            FAnimationKeyframe Keyframe;
            Keyframe.FrameNumber = FrameNumber;
            Keyframe.Translation = Location;
            Keyframe.bHasLocation = true;
            Keyframe.bHasRotation = false;

            ControlKeyframeData.FindOrAdd(TEXT("Head_Control")).Add(Keyframe);
            OutKeyframesAdded++;
        }

        OutProcessedFrames++;
    }
}

const TSet<FString>&
UBeatBloomAnimationProcessor::GetValidBeatBloomControllerNames() {
    static const TSet<FString> ValidControllers = []() {
        TSet<FString> ValidSet;
        // 手部 (4个)
        ValidSet.Append({TEXT("H_L"), TEXT("HP_L"), TEXT("H_R"), TEXT("HP_R")});
        // 脚部 (2个)
        ValidSet.Append({TEXT("F_L"), TEXT("F_R")});
        // 头部控制器
        ValidSet.Append({TEXT("Head_Control")});
        return ValidSet;
    }();
    return ValidControllers;
}

#undef LOCTEXT_NAMESPACE
