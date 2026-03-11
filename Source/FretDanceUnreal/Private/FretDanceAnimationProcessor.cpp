#include "FretDanceAnimationProcessor.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "FretDanceMusicInstrumentProcessor.h"
#include "FretDanceUnreal.h"
#include "InstrumentAnimationUtility.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "LevelSequence.h"

#define LOCTEXT_NAMESPACE "FretDanceAnimationProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FretDance-specific static helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace FretDanceAnimationHelper {
/**
 * 获取有效的 FretDance 控制器名称集合
 */
static const TSet<FString>& GetValidFretDanceControllerNames() {
    // 使用静态初始化一次，避免重复构建
    static const TSet<FString> ValidControllers = []() {
        TSet<FString> ValidSet;

        // 左手控制器 (9 个)
        ValidSet.Append({TEXT("H_L"), TEXT("HP_L"), TEXT("H_rotation_L"),
                         TEXT("T_L"), TEXT("TP_L"), TEXT("I_L"), TEXT("M_L"),
                         TEXT("R_L"), TEXT("P_L")});

        // 右手控制器 (2-7 个，取决于乐器类型)
        // 基础右手控制器 (所有类型都有)
        ValidSet.Append(
            {TEXT("H_R"), TEXT("HP_R"), TEXT("H_rotation_R"), TEXT("T_R")});

        // 指弹/Bass 特有的右手手指
        ValidSet.Append(
            {TEXT("TP_R"), TEXT("I_R"), TEXT("M_R"), TEXT("R_R"), TEXT("P_R")});

        return ValidSet;
    }();

    return ValidControllers;
}

/**
 * 收集 FretDance 中的所有控制器名称
 */
static void CollectFretDanceControllerNames(AFretDanceUnreal* FretDanceActor,
                                            TSet<FString>& OutControllerNames) {
    if (!FretDanceActor) {
        return;
    }

    // 收集左手控制器
    for (const auto& Pair : FretDanceActor->LeftHandControllers) {
        OutControllerNames.Add(Pair.Value);
    }

    // 收集右手控制器
    for (const auto& Pair : FretDanceActor->RightHandControllers) {
        OutControllerNames.Add(Pair.Value);
    }

    // 收集左手手指控制器
    for (const auto& Pair : FretDanceActor->LeftFingerControllers) {
        OutControllerNames.Add(Pair.Value);
    }

    // 收集右手手指控制器
    for (const auto& Pair : FretDanceActor->RightFingerControllers) {
        OutControllerNames.Add(Pair.Value);
    }

    // 收集手掌旋转控制器
    for (const auto& Pair : FretDanceActor->HandRotationControllers) {
        OutControllerNames.Add(Pair.Value);
    }
}

/**
 * 处理单个动画帧 - FretDance 特定的 JSON 结构解析
 * JSON 结构：{ "frame": N, "fingerInfos": {...} }
 */
static void ProcessFretDanceAnimationFrame(
    TSharedPtr<FJsonObject> FrameObject,
    TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
    int32 FrameIndex, int32& OutFailedFrames, int32& OutKeyframesAdded) {
    if (!FrameObject.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("Frame %d is not a valid JSON object"),
               FrameIndex);
        OutFailedFrames++;
        return;
    }

    // 获取帧编号（FretDance 特定字段）
    double FrameNumberDouble = FrameIndex;
    if (FrameObject->HasField(TEXT("frame"))) {
        FrameNumberDouble = FrameObject->GetNumberField(TEXT("frame"));
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Frame %d does not have 'frame' field"),
               FrameIndex);
    }

    // 转换为整数帧（Sequencer 使用整数帧）
    int32 FrameNumber = FMath::RoundToInt(FrameNumberDouble);

    // 获取 fingerInfos 对象（FretDance 特定字段）
    TSharedPtr<FJsonObject> FingerInfos = nullptr;
    if (FrameObject->HasField(TEXT("fingerInfos"))) {
        FingerInfos = FrameObject->GetObjectField(TEXT("fingerInfos"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Frame %d does not have 'fingerInfos' field"), FrameIndex);
        OutFailedFrames++;
        return;
    }

    if (!FingerInfos.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("Frame %d fingerInfos is not valid"),
               FrameIndex);
        OutFailedFrames++;
        return;
    }

    // 调用通用方法处理控件容器
    UInstrumentAnimationUtility::ProcessControlsContainer(
        FingerInfos, FrameNumber, ControlKeyframeData,
        GetValidFretDanceControllerNames(), OutKeyframesAdded);
}

}  // namespace FretDanceAnimationHelper

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public methods implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UFretDanceAnimationProcessor::GeneratePerformerAnimation(
    AFretDanceUnreal* FretDanceActor) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GeneratePerformerAnimation: FretDanceActor is null"));
        return;
    }

    // 获取 LevelSequence
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error, TEXT("请确保已打开 Level Sequence"));
        return;
    }

    FString LeftHandAnimationPath;
    FString RightHandAnimationPath;
    FString StringRecorderPath;

    // 解析配置文件
    if (!ParseFretDanceConfigFile(FretDanceActor, LeftHandAnimationPath,
                                  RightHandAnimationPath, StringRecorderPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse FretDance config file in "
                    "GeneratePerformerAnimation"));
        return;
    }

    // 生成左手动画
    if (!LeftHandAnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating left hand animation from: %s"),
               *LeftHandAnimationPath);
        MakePerformerAnimation(FretDanceActor, LeftHandAnimationPath,
                               LevelSequence);
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Left hand animation path is empty"));
    }

    // 生成右手动画
    if (!RightHandAnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating right hand animation from: %s"),
               *RightHandAnimationPath);
        MakePerformerAnimation(FretDanceActor, RightHandAnimationPath,
                               LevelSequence);
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Right hand animation path is empty"));
    }

    FretDanceActor->TriggerControlRigReregistration(
        TEXT("Generate Performer Animation"));
}

void UFretDanceAnimationProcessor::GenerateInstrumentAnimation(
    AFretDanceUnreal* FretDanceActor) {
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

    FString LeftHandAnimationPath;
    FString RightHandAnimationPath;
    FString StringRecorderPath;

    // 解析配置文件获取弦记录器路径
    if (!ParseFretDanceConfigFile(FretDanceActor, LeftHandAnimationPath,
                                  RightHandAnimationPath, StringRecorderPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse FretDance config file in "
                    "GenerateInstrumentAnimation"));
        return;
    }

    if (StringRecorderPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("String recorder path is empty, skipping instrument "
                    "animation"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Started =========="));
    UE_LOG(LogTemp, Warning, TEXT("Generating instrument animation from: %s"),
           *StringRecorderPath);

    // 调用 FretDanceMusicInstrumentProcessor 生成弦振动动画
    UFretDanceMusicInstrumentProcessor::GenerateInstrumentAnimation(
        FretDanceActor, StringRecorderPath);

    FretDanceActor->TriggerControlRigReregistration(
        TEXT("Generate Instrument Animation"));

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Completed =========="));
}

void UFretDanceAnimationProcessor::GenerateAllAnimation(
    AFretDanceUnreal* FretDanceActor) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GenerateAllAnimation: FretDanceActor is null"));
        return;
    }

    FString LeftHandAnimationPath;
    FString RightHandAnimationPath;
    FString StringRecorderPath;

    // 解析配置文件
    if (!ParseFretDanceConfigFile(FretDanceActor, LeftHandAnimationPath,
                                  RightHandAnimationPath, StringRecorderPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse FretDance config file in "
                    "GenerateAllAnimation"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateAllAnimation Started =========="));

    // 生成演奏动画
    GeneratePerformerAnimation(FretDanceActor);

    // 生成弦动画
    if (!StringRecorderPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating instrument animation from: %s"),
               *StringRecorderPath);
        GenerateInstrumentAnimation(FretDanceActor);

    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("String recorder path is empty, skipping instrument "
                    "animation"));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateAllAnimation Completed =========="));
}

bool UFretDanceAnimationProcessor::ParseFretDanceConfigFile(
    AFretDanceUnreal* FretDanceActor, FString& OutLeftHandAnimationPath,
    FString& OutRightHandAnimationPath, FString& OutStringRecorderPath) {
    OutLeftHandAnimationPath.Empty();
    OutRightHandAnimationPath.Empty();
    OutStringRecorderPath.Empty();

    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FretDanceActor is null in ParseFretDanceConfigFile"));
        return false;
    }

    if (FretDanceActor->AnimationFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("AnimationFilePath is empty in ParseFretDanceConfigFile"));
        return false;
    }

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent,
                                       *FretDanceActor->AnimationFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load JSON file: %s"),
               *FretDanceActor->AnimationFilePath);
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) ||
        !JsonObject.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON file: %s"),
               *FretDanceActor->AnimationFilePath);
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

    // 解析弦记录器路径
    if (JsonObject->HasField(TEXT("guitar_string_recorder_file"))) {
        OutStringRecorderPath =
            JsonObject->GetStringField(TEXT("guitar_string_recorder_file"));
    }

    return true;
}

void UFretDanceAnimationProcessor::MakePerformerAnimation(
    AFretDanceUnreal* FretDanceActor, const FString& AnimationFilePath,
    ULevelSequence* LevelSequence) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("MakePerformerAnimation: FretDanceActor is null"));
        return;
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("MakePerformerAnimation: LevelSequence is null"));
        return;
    }

    if (!FretDanceActor->SkeletalMeshActor) {
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

    // 2. 解析 JSON 数组
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
        FretDanceActor->GetCachedControlRig(TEXT("Performer"));
    UControlRigBlueprint* ControlRigBlueprint =
        FretDanceActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    // 不再提供后备查询，如果缓存未命中则直接失败
    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("FretDanceAnimationProcessor: Failed to get ControlRig for "
                    "Performer - cache miss"));
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
        // 只收集左手控制器
        ControlNamesToClean.Append(
            {TEXT("H_L"), TEXT("HP_L"), TEXT("H_rotation_L"), TEXT("T_L"),
             TEXT("TP_L"), TEXT("I_L"), TEXT("M_L"), TEXT("R_L"), TEXT("P_L")});
        UE_LOG(LogTemp, Warning,
               TEXT("Detected LEFT HAND animation, will only clear %d left "
                    "hand controllers"),
               ControlNamesToClean.Num());
    } else if (bIsRightHand) {
        // 只收集右手控制器
        ControlNamesToClean.Append(
            {TEXT("H_R"), TEXT("HP_R"), TEXT("H_rotation_R"), TEXT("T_R"),
             TEXT("TP_R"), TEXT("I_R"), TEXT("M_R"), TEXT("R_R"), TEXT("P_R")});
        UE_LOG(LogTemp, Warning,
               TEXT("Detected RIGHT HAND animation, will only clear %d right "
                    "hand controllers"),
               ControlNamesToClean.Num());
    } else {
        // 如果无法判断，收集所有控制器
        ControlNamesToClean =
            FretDanceAnimationHelper::GetValidFretDanceControllerNames();
        UE_LOG(LogTemp, Warning,
               TEXT("Could not determine hand type from path, clearing all %d "
                    "controllers"),
               ControlNamesToClean.Num());
    }

    // 6. 清空关键帧（使用通用方法）
    UE_LOG(LogTemp, Warning,
           TEXT("Clearing existing Control Rig keyframes before adding new "
                "keyframes"));
    UInstrumentAnimationUtility::ClearControlRigKeyframes(
        LevelSequence, ControlRigInstance, ControlNamesToClean);

    UE_LOG(LogTemp, Warning, TEXT("Starting to process %d animation frames"),
           JsonArray.Num());

    // 7. 处理每一帧并收集关键帧数据
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;
    int32 ProcessedFrames = 0;
    int32 FailedFrames = 0;
    int32 KeyframesAdded = 0;

    for (int32 FrameIndex = 0; FrameIndex < JsonArray.Num(); ++FrameIndex) {
        TSharedPtr<FJsonObject> FrameObject = JsonArray[FrameIndex]->AsObject();

        // 使用 FretDance 特定的方法处理帧（负责提取 frame 和 fingerInfos）
        FretDanceAnimationHelper::ProcessFretDanceAnimationFrame(
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

#undef LOCTEXT_NAMESPACE
