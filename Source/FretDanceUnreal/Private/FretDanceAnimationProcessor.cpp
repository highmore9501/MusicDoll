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
        ValidSet.Append({TEXT("H_L"), TEXT("HP_L"), TEXT("T_L"), TEXT("TP_L"),
                         TEXT("I_L"), TEXT("M_L"), TEXT("R_L"), TEXT("P_L")});

        // 右手控制器 (2-7 个，取决于乐器类型)
        // 基础右手控制器 (所有类型都有)
        ValidSet.Append({TEXT("H_R"), TEXT("HP_R"), TEXT("T_R")});

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

    // FretDance 控件数据格式：{"controller_name": {"position": [x,y,z],
    // "rotation": [w,x,y,z]}}
    for (const auto& Pair : FingerInfos->Values) {
        FString RawControlName = Pair.Key;

        FString ControlName =
            UInstrumentAnimationUtility::ValidateControllerName(
                RawControlName, GetValidFretDanceControllerNames(),
                TEXT("FretDance"));

        if (ControlName.IsEmpty()) {
            continue;
        }

        TSharedPtr<FJsonValue> ControlDataValue = Pair.Value;
        if (!ControlDataValue.IsValid()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Frame %d control %s has invalid data"), FrameNumber,
                   *ControlName);
            continue;
        }

        TSharedPtr<FJsonObject> ControlObj = ControlDataValue->AsObject();
        if (!ControlObj.IsValid()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Frame %d control %s is not a valid object"),
                   FrameNumber, *ControlName);
            continue;
        }

        FAnimationKeyframe Keyframe;
        Keyframe.FrameNumber = FrameNumber;

        // 读取位置数据
        if (ControlObj->HasField(TEXT("position"))) {
            TArray<TSharedPtr<FJsonValue>> PosArray =
                ControlObj->GetArrayField(TEXT("position"));
            if (PosArray.Num() == 3) {
                FVector Location;
                Location.X = PosArray[0]->AsNumber();
                Location.Y = PosArray[1]->AsNumber();
                Location.Z = PosArray[2]->AsNumber();
                Keyframe.Translation = Location;
                Keyframe.bHasLocation = true;
            }
        }

        // 读取旋转数据
        if (ControlObj->HasField(TEXT("rotation"))) {
            TArray<TSharedPtr<FJsonValue>> RotArray =
                ControlObj->GetArrayField(TEXT("rotation"));
            if (RotArray.Num() == 4) {
                FQuat Rotation;
                Rotation.W = RotArray[0]->AsNumber();
                Rotation.X = RotArray[1]->AsNumber();
                Rotation.Y = RotArray[2]->AsNumber();
                Rotation.Z = RotArray[3]->AsNumber();
                Rotation.Normalize();
                Keyframe.Rotation = Rotation;
                Keyframe.bHasRotation = true;
            }
        }

        // 至少有一项数据时才添加关键帧
        if (Keyframe.bHasLocation || Keyframe.bHasRotation) {
            ControlKeyframeData.FindOrAdd(ControlName).Add(Keyframe);
            OutKeyframesAdded++;
        }
    }
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
    FString ControllerRootAnimationPath;
    FString ActivityCurvePath;
    FString VibratoShapeKeyPath;

    // 解析配置文件
    if (!ParseFretDanceConfigFile(FretDanceActor, LeftHandAnimationPath,
                                  RightHandAnimationPath, StringRecorderPath,
                                  ControllerRootAnimationPath,
                                  ActivityCurvePath, VibratoShapeKeyPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse FretDance config file in "
                    "GeneratePerformerAnimation"));
        return;
    }

    // 生成 controller_root 动画
    if (!ControllerRootAnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating controller root animation from: %s"),
               *ControllerRootAnimationPath);
        MakeControllerRootAnimation(FretDanceActor, ControllerRootAnimationPath,
                                    LevelSequence);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Controller root animation path is empty"));
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

    // 写入 active curve
    if (!ActivityCurvePath.IsEmpty() && FretDanceActor->SkeletalMeshActor) {
        UE_LOG(LogTemp, Warning, TEXT("Writing active curve from: %s"),
               *ActivityCurvePath);
        UInstrumentAnimationUtility::WriteActiveCurveFromFile(
            FretDanceActor->SkeletalMeshActor, ActivityCurvePath,
            LevelSequence);
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
    FString ControllerRootAnimationPath;
    FString ActivityCurvePath;
    FString VibratoShapeKeyPath;

    // 解析配置文件获取弦记录器路径
    if (!ParseFretDanceConfigFile(FretDanceActor, LeftHandAnimationPath,
                                  RightHandAnimationPath, StringRecorderPath,
                                  ControllerRootAnimationPath,
                                  ActivityCurvePath, VibratoShapeKeyPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse FretDance config file in "
                    "GenerateInstrumentAnimation"));
        return;
    }

    if (StringRecorderPath.IsEmpty() && VibratoShapeKeyPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Both string recorder and vibrato paths are empty, "
                    "skipping instrument animation"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Started =========="
                "\nString: %s\nVibrato: %s"),
           *StringRecorderPath, *VibratoShapeKeyPath);

    // 调用 FretDanceMusicInstrumentProcessor 生成弦振动 + 摇把动画
    UFretDanceMusicInstrumentProcessor::GenerateInstrumentAnimation(
        FretDanceActor, StringRecorderPath, VibratoShapeKeyPath);

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
    FString ControllerRootAnimationPath;
    FString ActivityCurvePath;
    FString VibratoShapeKeyPath;

    // 解析配置文件
    if (!ParseFretDanceConfigFile(FretDanceActor, LeftHandAnimationPath,
                                  RightHandAnimationPath, StringRecorderPath,
                                  ControllerRootAnimationPath,
                                  ActivityCurvePath, VibratoShapeKeyPath)) {
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
    FString& OutRightHandAnimationPath, FString& OutStringRecorderPath,
    FString& OutControllerRootAnimationPath, FString& OutActivityCurvePath,
    FString& OutVibratoShapeKeyPath) {
    OutLeftHandAnimationPath.Empty();
    OutRightHandAnimationPath.Empty();
    OutStringRecorderPath.Empty();
    OutControllerRootAnimationPath.Empty();
    OutActivityCurvePath.Empty();
    OutVibratoShapeKeyPath.Empty();

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

    // 解析 controller_root 动画路径
    if (JsonObject->HasField(TEXT("controller_root_animation_file"))) {
        OutControllerRootAnimationPath =
            JsonObject->GetStringField(TEXT("controller_root_animation_file"));
    }

    // 解析 activity curve 路径
    if (JsonObject->HasField(TEXT("activity_curve_path"))) {
        OutActivityCurvePath =
            JsonObject->GetStringField(TEXT("activity_curve_path"));
    }

    // 解析摇把 shape key 路径
    if (JsonObject->HasField(TEXT("vibrato_shape_key_file"))) {
        OutVibratoShapeKeyPath =
            JsonObject->GetStringField(TEXT("vibrato_shape_key_file"));
    }

    return true;
}

void UFretDanceAnimationProcessor::MakeControllerRootAnimation(
    AFretDanceUnreal* FretDanceActor, const FString& AnimationFilePath,
    ULevelSequence* LevelSequence) {
    if (!FretDanceActor) {
        UE_LOG(LogTemp, Error,
               TEXT("MakeControllerRootAnimation: FretDanceActor is null"));
        return;
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("MakeControllerRootAnimation: LevelSequence is null"));
        return;
    }

    if (!FretDanceActor->SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("MakeControllerRootAnimation: SkeletalMeshActor is null"));
        return;
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("========== MakeControllerRootAnimation Started: %s =========="),
        *AnimationFilePath);

#if WITH_EDITOR
    // 1. 读取动画文件
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *AnimationFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load controller root file: %s"),
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

    UE_LOG(LogTemp, Warning, TEXT("Loaded %d controller root frames"),
           JsonArray.Num());

    // 3. 获取 Control Rig
    UControlRig* ControlRigInstance =
        FretDanceActor->GetCachedControlRig(TEXT("Performer"));

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("MakeControllerRootAnimation: Failed to get ControlRig for "
                    "Performer - cache miss"));
        return;
    }

    // 4. 处理每一帧，合并位置和旋转到 controller_root_offset
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;
    int32 ProcessedFrames = 0;
    int32 FailedFrames = 0;

    for (int32 FrameIndex = 0; FrameIndex < JsonArray.Num(); ++FrameIndex) {
        TSharedPtr<FJsonObject> FrameObject = JsonArray[FrameIndex]->AsObject();
        if (!FrameObject.IsValid()) {
            FailedFrames++;
            continue;
        }

        double FrameNumberDouble = FrameIndex;
        if (FrameObject->HasField(TEXT("frame"))) {
            FrameNumberDouble = FrameObject->GetNumberField(TEXT("frame"));
        }
        int32 FrameNumber = FMath::RoundToInt(FrameNumberDouble);

        TSharedPtr<FJsonObject> FingerInfos;
        if (FrameObject->HasField(TEXT("fingerInfos"))) {
            FingerInfos = FrameObject->GetObjectField(TEXT("fingerInfos"));
        }
        if (!FingerInfos.IsValid()) {
            FailedFrames++;
            continue;
        }

        FAnimationKeyframe Keyframe;
        Keyframe.FrameNumber = FrameNumber;

        // 提取 controller_root 对象（新的数据结构：{ position: [...], rotation:
        // [...] }）
        if (FingerInfos->HasField(TEXT("controller_root"))) {
            TSharedPtr<FJsonObject> ControllerRootObj =
                FingerInfos->GetObjectField(TEXT("controller_root"));

            if (ControllerRootObj.IsValid()) {
                // 提取位置
                if (ControllerRootObj->HasField(TEXT("position"))) {
                    TArray<TSharedPtr<FJsonValue>> PosArr =
                        ControllerRootObj->GetArrayField(TEXT("position"));
                    if (PosArr.Num() == 3) {
                        Keyframe.Translation = FVector(PosArr[0]->AsNumber(),
                                                       PosArr[1]->AsNumber(),
                                                       PosArr[2]->AsNumber());
                        Keyframe.bHasLocation = true;
                    }
                }

                // 提取旋转
                if (ControllerRootObj->HasField(TEXT("rotation"))) {
                    TArray<TSharedPtr<FJsonValue>> RotArr =
                        ControllerRootObj->GetArrayField(TEXT("rotation"));
                    if (RotArr.Num() == 4) {
                        FQuat Rotation;
                        Rotation.W = RotArr[0]->AsNumber();
                        Rotation.X = RotArr[1]->AsNumber();
                        Rotation.Y = RotArr[2]->AsNumber();
                        Rotation.Z = RotArr[3]->AsNumber();
                        Rotation.Normalize();
                        Keyframe.Rotation = Rotation;
                        Keyframe.bHasRotation = true;
                    }
                }
            }
        }

        if (Keyframe.bHasLocation || Keyframe.bHasRotation) {
            // 改为写入 controller_root_offset
            ControlKeyframeData.FindOrAdd(TEXT("controller_root_offset"))
                .Add(Keyframe);
        }

        ProcessedFrames++;
    }

    // 6. 批量插入关键帧
    FBatchInsertKeyframesSettings Settings;
    UInstrumentAnimationUtility::BatchInsertControlRigKeys(
        LevelSequence, ControlRigInstance, ControlKeyframeData, Settings);

    LevelSequence->MarkPackageDirty();

    UE_LOG(LogTemp, Warning,
           TEXT("========== MakeControllerRootAnimation Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed: %d frames"),
           ProcessedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Failed frames: %d"), FailedFrames);
    UE_LOG(LogTemp, Warning,
           TEXT("========== MakeControllerRootAnimation Completed =========="));

#endif
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

    // 判断是左手还是右手动画（仅用于日志）
    bool bIsLeftHand =
        AnimationFilePath.Contains(TEXT("left"), ESearchCase::IgnoreCase);
    bool bIsRightHand =
        AnimationFilePath.Contains(TEXT("right"), ESearchCase::IgnoreCase);

    if (bIsLeftHand) {
        UE_LOG(LogTemp, Warning, TEXT("Detected LEFT HAND animation"));
    } else if (bIsRightHand) {
        UE_LOG(LogTemp, Warning, TEXT("Detected RIGHT HAND animation"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Could not determine hand type from path"));
    }

    UE_LOG(LogTemp, Warning, TEXT("Starting to process %d animation frames"),
           JsonArray.Num());

    // 6. 处理每一帧并收集关键帧数据
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
