#include "StringFlowAnimationProcessor.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "StringFlowControlRigProcessor.h"
#include "StringFlowMusicInstrumentProcessor.h"
#include "StringFlowUnreal.h"

#define LOCTEXT_NAMESPACE "StringFlowAnimationProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// StringFlow-specific static helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace StringFlowAnimationHelper {
/**
 * 控制器过滤类型
 */
enum class EControllerFilterType {
    All,       // 所有控制器
    LeftHand,  // 仅左手控制器
    RightHand  // 仅右手控制器
};

/**
 * 获取有效的 StringFlow 控制器名称集合
 * @param FilterType 过滤类型：所有、仅左手、仅右手
 */
static const TSet<FString> GetValidStringFlowControllerNames(
    EControllerFilterType FilterType = EControllerFilterType::All) {
    static const TSet<FString> LeftHandControllers = {
        TEXT("H_L"), TEXT("HP_L"), TEXT("T_L"), TEXT("1_L"),
        TEXT("2_L"), TEXT("3_L"),  TEXT("4_L")};

    static const TSet<FString> RightHandControllers = {
        TEXT("H_R"),
        TEXT("HP_R"),
        TEXT("T_R"),
        TEXT("1_R"),
        TEXT("2_R"),
        TEXT("3_R"),
        TEXT("4_R"),
        TEXT("String_Touch_Point"),
        TEXT("Bow_Controller")};

    static const TSet<FString> AllControllers = []() {
        TSet<FString> Combined;
        Combined.Append(LeftHandControllers);
        Combined.Append(RightHandControllers);
        return Combined;
    }();

    switch (FilterType) {
        case EControllerFilterType::LeftHand:
            return LeftHandControllers;
        case EControllerFilterType::RightHand:
            return RightHandControllers;
        case EControllerFilterType::All:
        default:
            return AllControllers;
    }
}

/**
 * 处理单个动画帧 - StringFlow 特定的 JSON 结构解析
 * 假设 JSON 结构为: { "frame": N, "hand_infos": {...} }
 */
static void ProcessStringFlowAnimationFrame(
    TSharedPtr<FJsonObject> FrameObject,
    TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
    int32 FrameIndex, int32& OutFailedFrames, int32& OutKeyframesAdded) {
    if (!FrameObject.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("Frame %d is not a valid JSON object"),
               FrameIndex);
        OutFailedFrames++;
        return;
    }

    // 获取帧数（StringFlow 特定字段）
    int32 FrameNumber = FrameIndex;
    if (FrameObject->HasField(TEXT("frame"))) {
        FrameNumber =
            static_cast<int32>(FrameObject->GetNumberField(TEXT("frame")));
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Frame %d does not have 'frame' field"),
               FrameIndex);
    }

    // 获取 hand_infos 对象（StringFlow 特定字段）
    TSharedPtr<FJsonObject> HandInfos = nullptr;
    if (FrameObject->HasField(TEXT("hand_infos"))) {
        HandInfos = FrameObject->GetObjectField(TEXT("hand_infos"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Frame %d does not have 'hand_infos' field"), FrameIndex);
        OutFailedFrames++;
        return;
    }

    if (!HandInfos.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("Frame %d hand_infos is not valid"),
               FrameIndex);
        OutFailedFrames++;
        return;
    }

    // StringFlow 控件数据格式：每个控制器值为直接数组
    //   - 3 元素 = 位置 [x, y, z]
    //   - 4 元素 = 四元数旋转 [w, x, y, z]
    //   - 7 元素 = 合并 [x, y, z, w, qx, qy, qz]（H_ 手掌控制器位置+旋转）
    for (const auto& Pair : HandInfos->Values) {
        FString RawControlName = Pair.Key;

        FString ControlName =
            UInstrumentAnimationUtility::ValidateControllerName(
                RawControlName, GetValidStringFlowControllerNames(),
                TEXT("StringFlow"));

        if (ControlName.IsEmpty()) {
            continue;
        }

        TSharedPtr<FJsonValue> ControlDataValue = Pair.Value;
        if (!ControlDataValue.IsValid()) {
            continue;
        }

        const TArray<TSharedPtr<FJsonValue>>* DataArrayPtr = nullptr;
        if (!ControlDataValue->TryGetArray(DataArrayPtr) || !DataArrayPtr) {
            continue;
        }

        const TArray<TSharedPtr<FJsonValue>>& DataArray = *DataArrayPtr;

        FAnimationKeyframe Keyframe;
        Keyframe.FrameNumber = FrameNumber;

        if (DataArray.Num() == 3) {
            // 位置数据 [x, y, z] → 写入原控制器
            FVector Location;
            Location.X = DataArray[0]->AsNumber();
            Location.Y = DataArray[1]->AsNumber();
            Location.Z = DataArray[2]->AsNumber();
            Keyframe.Translation = Location;
            Keyframe.bHasLocation = true;
            ControlKeyframeData.FindOrAdd(ControlName).Add(Keyframe);
            OutKeyframesAdded++;
        } else if (DataArray.Num() == 4) {
            // 四元数旋转 [w, x, y, z] → 写入原控制器的旋转通道
            FQuat Rotation;
            Rotation.W = DataArray[0]->AsNumber();
            Rotation.X = DataArray[1]->AsNumber();
            Rotation.Y = DataArray[2]->AsNumber();
            Rotation.Z = DataArray[3]->AsNumber();
            Rotation.Normalize();
            Keyframe.Rotation = Rotation;
            Keyframe.bHasRotation = true;
            ControlKeyframeData.FindOrAdd(ControlName).Add(Keyframe);
            OutKeyframesAdded++;
        } else if (DataArray.Num() == 7) {
            // 合并格式 [x, y, z, w, qx, qy, qz]：前3为位置，后4为四元数
            FVector Location;
            Location.X = DataArray[0]->AsNumber();
            Location.Y = DataArray[1]->AsNumber();
            Location.Z = DataArray[2]->AsNumber();
            Keyframe.Translation = Location;
            Keyframe.bHasLocation = true;

            FQuat Rotation;
            Rotation.W = DataArray[3]->AsNumber();
            Rotation.X = DataArray[4]->AsNumber();
            Rotation.Y = DataArray[5]->AsNumber();
            Rotation.Z = DataArray[6]->AsNumber();
            Rotation.Normalize();
            Keyframe.Rotation = Rotation;
            Keyframe.bHasRotation = true;

            ControlKeyframeData.FindOrAdd(ControlName).Add(Keyframe);
            OutKeyframesAdded++;
        } else {
            continue;
        }
    }
}

}  // namespace StringFlowAnimationHelper

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public methods implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UStringFlowAnimationProcessor::GeneratePerformerAnimation(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GeneratePerformerAnimation: StringFlowActor is null"));
        return;
    }

    // 获取LevelSequence
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error, TEXT("请确保已打开Level Sequence"));
        return;
    }

    FString LeftHandAnimationPath;
    FString RightHandAnimationPath;
    FString StringVibrationPath;
    FString ActivityCurvePath;

    // 解析配置文件
    if (!ParseStringFlowConfigFile(StringFlowActor, LeftHandAnimationPath,
                                   RightHandAnimationPath, StringVibrationPath,
                                   ActivityCurvePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse StringFlow config file in "
                    "GeneratePerformerAnimation"));
        return;
    }

    // 生成左手动画
    if (!LeftHandAnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating left hand animation from: %s"),
               *LeftHandAnimationPath);
        MakePerformerAnimation(StringFlowActor, LeftHandAnimationPath,
                               LevelSequence);
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Left hand animation path is empty"));
    }

    // 生成右手动画
    if (!RightHandAnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating right hand animation from: %s"),
               *RightHandAnimationPath);
        MakePerformerAnimation(StringFlowActor, RightHandAnimationPath,
                               LevelSequence);
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Right hand animation path is empty"));
    }

    // 写入 active curve
    if (!ActivityCurvePath.IsEmpty() && StringFlowActor->SkeletalMeshActor) {
        UE_LOG(LogTemp, Warning, TEXT("Writing active curve from: %s"),
               *ActivityCurvePath);
        UInstrumentAnimationUtility::WriteActiveCurveFromFile(
            StringFlowActor->SkeletalMeshActor, ActivityCurvePath,
            LevelSequence);
    }
}

void UStringFlowAnimationProcessor::GenerateInstrumentAnimation(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentAnimation: StringFlowActor is null"));
        return;
    }

    // 调用StringFlowMusicInstrumentProcessor的实际实现
    UStringFlowMusicInstrumentProcessor::GenerateInstrumentAnimation(
        StringFlowActor);
}

void UStringFlowAnimationProcessor::GenerateAllAnimation(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateAllAnimation: StringFlowActor is null"));
        return;
    }

    FString LeftHandAnimationPath;
    FString RightHandAnimationPath;
    FString StringVibrationPath;
    FString ActivityCurvePath;

    // 解析配置文件
    if (!ParseStringFlowConfigFile(StringFlowActor, LeftHandAnimationPath,
                                   RightHandAnimationPath, StringVibrationPath,
                                   ActivityCurvePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse StringFlow config file in "
                    "GenerateAllAnimation"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateAllAnimation Started =========="));

    // 生成演奏动画
    GeneratePerformerAnimation(StringFlowActor);

    // 生成乐器动画
    if (!StringVibrationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating instrument animation from: %s"),
               *StringVibrationPath);
        GenerateInstrumentAnimation(StringFlowActor);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Instrument animation path is empty, skipping instrument "
                    "animation"));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateAllAnimation Completed =========="));
}

void UStringFlowAnimationProcessor::MakeStringAnimation(
    AStringFlowUnreal* StringFlowActor, const FString& AnimationFilePath,
    ULevelSequence* LevelSequence) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("MakeStringAnimation: StringFlowActor is null"));
        return;
    }

    if (AnimationFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("AnimationFilePath is empty in MakeStringAnimation"));
        return;
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("MakeStringAnimation: LevelSequence is null"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== MakeStringAnimation Started: %s =========="),
           *AnimationFilePath);

#if WITH_EDITOR
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
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON array from file: %s"),
               *AnimationFilePath);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Loaded %d animation frames"),
           JsonArray.Num());

    // 3. 获取 Control Rig Instance (弦乐器模型) - 使用缓存机制
    UControlRig* ControlRigInstance =
        StringFlowActor->GetCachedControlRig(TEXT("StringInstrument"));
    UControlRigBlueprint* ControlRigBlueprint =
        StringFlowActor->GetCachedControlRigBlueprint(TEXT("StringInstrument"));

    // 不再提供后备查询，如果缓存未命中则直接失败
    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowAnimationProcessor: Failed to get ControlRig "
                    "for StringInstrument - cache miss"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRigInstance is null in MakeStringAnimation"));
        return;
    }

    // 4. 验证并修复重复的轨道
    bool bHasDuplicateTracks =
        UInstrumentAnimationUtility::ValidateNoExistingTracks(
            LevelSequence, ControlRigInstance, true);
    if (bHasDuplicateTracks) {
        UE_LOG(LogTemp, Warning,
               TEXT("Duplicate Control Rig tracks detected and auto-fixed. "
                    "Proceeding with animation generation."));
    }

    // 5. 处理每一帧并收集关键帧数据
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;
    int32 ProcessedFrames = 0;
    int32 FailedFrames = 0;
    int32 KeyframesAdded = 0;

    for (int32 FrameIndex = 0; FrameIndex < JsonArray.Num(); ++FrameIndex) {
        TSharedPtr<FJsonObject> FrameObject = JsonArray[FrameIndex]->AsObject();

        // 使用 StringFlow 特定的方法处理帧（负责提取 frame 和 hand_infos）
        StringFlowAnimationHelper::ProcessStringFlowAnimationFrame(
            FrameObject, ControlKeyframeData, FrameIndex, FailedFrames,
            KeyframesAdded);

        ProcessedFrames++;
    }

    // 8. 配置批量插入设置
    FBatchInsertKeyframesSettings Settings;

    // 9. 批量插入关键帧（使用通用方法）
    UInstrumentAnimationUtility::BatchInsertControlRigKeys(
        LevelSequence, ControlRigInstance, ControlKeyframeData, Settings);

    // 10. 标记为已修改
    LevelSequence->MarkPackageDirty();

    UE_LOG(LogTemp, Warning,
           TEXT("========== MakeStringAnimation Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed: %d frames"),
           ProcessedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Failed frames: %d"), FailedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Total keyframes added to Sequencer: %d"),
           KeyframesAdded);
    UE_LOG(LogTemp, Warning,
           TEXT("========== MakeStringAnimation Completed =========="));

#endif
}

void UStringFlowAnimationProcessor::MakePerformerAnimation(
    AStringFlowUnreal* StringFlowActor, const FString& AnimationFilePath,
    ULevelSequence* LevelSequence) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("MakePerformerAnimation: StringFlowActor is null"));
        return;
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("MakePerformerAnimation: LevelSequence is null"));
        return;
    }

    if (!StringFlowActor->SkeletalMeshActor) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "SkeletalMeshActor is not assigned in MakePerformerAnimation"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== MakePerformerAnimation Started: %s =========="),
           *AnimationFilePath);

#if WITH_EDITOR
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
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON array from file: %s"),
               *AnimationFilePath);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Loaded %d animation frames"),
           JsonArray.Num());

    // 3. 获取演奏者模型的 Control Rig Instance - 使用缓存机制
    UControlRig* ControlRigInstance =
        StringFlowActor->GetCachedControlRig(TEXT("Performer"));
    UControlRigBlueprint* ControlRigBlueprint =
        StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    // 不再提供后备查询，如果缓存未命中则直接失败
    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowAnimationProcessor: Failed to get ControlRig "
                    "for Performer - cache miss"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRigInstance is null in MakePerformerAnimation"));
        return;
    }

    // 4. 验证并修复重复的轨道
    bool bHasDuplicateTracks =
        UInstrumentAnimationUtility::ValidateNoExistingTracks(
            LevelSequence, ControlRigInstance, true);
    if (bHasDuplicateTracks) {
        UE_LOG(LogTemp, Warning,
               TEXT("Duplicate Control Rig tracks detected and auto-fixed. "
                    "Proceeding with animation generation."));
    }

    // 5. 根据文件路径确定要清理的控制器集合
    TSet<FString> ControlNamesToClean;

    // 判断是左手还是右手动画
    bool bIsLeftHand =
        AnimationFilePath.Contains(TEXT("left"), ESearchCase::IgnoreCase);
    bool bIsRightHand =
        AnimationFilePath.Contains(TEXT("right"), ESearchCase::IgnoreCase);

    if (bIsLeftHand) {
        // 只收集左手控制器 - 使用带过滤的方法
        ControlNamesToClean =
            StringFlowAnimationHelper::GetValidStringFlowControllerNames(
                StringFlowAnimationHelper::EControllerFilterType::LeftHand);
        UE_LOG(LogTemp, Warning,
               TEXT("Detected LEFT HAND animation, will only clear %d left "
                    "hand controllers"),
               ControlNamesToClean.Num());
    } else if (bIsRightHand) {
        // 只收集右手控制器 - 使用带过滤的方法
        ControlNamesToClean =
            StringFlowAnimationHelper::GetValidStringFlowControllerNames(
                StringFlowAnimationHelper::EControllerFilterType::RightHand);
        UE_LOG(LogTemp, Warning,
               TEXT("Detected RIGHT HAND animation, will only clear %d right "
                    "hand controllers"),
               ControlNamesToClean.Num());
    } else {
        // 如果无法判断，收集所有控制器（保持原有行为）
        ControlNamesToClean =
            StringFlowAnimationHelper::GetValidStringFlowControllerNames(
                StringFlowAnimationHelper::EControllerFilterType::All);
        UE_LOG(LogTemp, Warning,
               TEXT("Could not determine hand type from path, clearing all %d "
                    "controllers"),
               ControlNamesToClean.Num());
    }

    UE_LOG(LogTemp, Warning, TEXT("Starting to process %d animation frames"),
           JsonArray.Num());

    // 7. 处理每一帧并收集关键帧数据
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;
    int32 ProcessedFrames = 0;
    int32 FailedFrames = 0;
    int32 KeyframesAdded = 0;

    for (int32 FrameIndex = 0; FrameIndex < JsonArray.Num(); ++FrameIndex) {
        TSharedPtr<FJsonObject> FrameObject = JsonArray[FrameIndex]->AsObject();

        // 使用 StringFlow 特定的方法处理帧（负责提取 frame 和 hand_infos）
        StringFlowAnimationHelper::ProcessStringFlowAnimationFrame(
            FrameObject, ControlKeyframeData, FrameIndex, FailedFrames,
            KeyframesAdded);

        ProcessedFrames++;
    }

    // 8. 配置批量插入设置
    FBatchInsertKeyframesSettings Settings;

    // 9. 批量插入关键帧（使用通用方法）
    UInstrumentAnimationUtility::BatchInsertControlRigKeys(
        LevelSequence, ControlRigInstance, ControlKeyframeData, Settings);

    // 10. 标记为已修改
    LevelSequence->MarkPackageDirty();

    UE_LOG(LogTemp, Warning,
           TEXT("========== MakePerformerAnimation Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed: %d frames"),
           ProcessedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Failed frames: %d"), FailedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Total keyframes added to Sequencer: %d"),
           KeyframesAdded);
    UE_LOG(LogTemp, Warning,
           TEXT("========== MakePerformerAnimation Completed =========="));

#endif
}

bool UStringFlowAnimationProcessor::ParseStringFlowConfigFile(
    AStringFlowUnreal* StringFlowActor, FString& OutLeftHandAnimationPath,
    FString& OutRightHandAnimationPath, FString& OutStringVibrationPath,
    FString& OutActivityCurvePath) {
    OutLeftHandAnimationPath.Empty();
    OutRightHandAnimationPath.Empty();
    OutStringVibrationPath.Empty();
    OutActivityCurvePath.Empty();

    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowActor is null in ParseStringFlowConfigFile"));
        return false;
    }

    if (StringFlowActor->AnimationFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("AnimationFilePath is empty in ParseStringFlowConfigFile"));
        return false;
    }

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent,
                                       *StringFlowActor->AnimationFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load JSON file: %s"),
               *StringFlowActor->AnimationFilePath);
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) ||
        !JsonObject.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON file: %s"),
               *StringFlowActor->AnimationFilePath);
        return false;
    }

    // 解析左手动画路径
    if (JsonObject->HasField(TEXT("left_hand_animation_file"))) {
        OutLeftHandAnimationPath =
            JsonObject->GetStringField(TEXT("left_hand_animation_file"));
    }

    // 解析右手动画路径
    if (JsonObject->HasField(TEXT("right_hand_animation_file"))) {
        OutRightHandAnimationPath =
            JsonObject->GetStringField(TEXT("right_hand_animation_file"));
    }

    // 解析弦振动动画路径
    if (JsonObject->HasField(TEXT("string_animation_file"))) {
        OutStringVibrationPath =
            JsonObject->GetStringField(TEXT("string_animation_file"));
    }

    // 解析 activity curve 路径
    if (JsonObject->HasField(TEXT("activity_curve_path"))) {
        OutActivityCurvePath =
            JsonObject->GetStringField(TEXT("activity_curve_path"));
    }

    return true;
}

void UStringFlowAnimationProcessor::GenerateInstrumentMaterialAnimation(
    AStringFlowUnreal* StringFlowActor,
    const FString& InstrumentAnimationDataPath) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateInstrumentMaterialAnimation: StringFlowActor is "
                    "null"));
        return;
    }

    if (InstrumentAnimationDataPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("InstrumentAnimationDataPath is empty in "
                    "GenerateInstrumentMaterialAnimation"));
        return;
    }

    // 调用StringFlowMusicInstrumentProcessor的实际实现
    UStringFlowMusicInstrumentProcessor::GenerateInstrumentMaterialAnimation(
        StringFlowActor, InstrumentAnimationDataPath);
}

#undef LOCTEXT_NAMESPACE
