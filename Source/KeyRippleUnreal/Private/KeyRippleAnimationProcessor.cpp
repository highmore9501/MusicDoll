#include "KeyRippleAnimationProcessor.h"

#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "InstrumentAnimationUtility.h"
#include "KeyRippleControlRigProcessor.h"
#include "KeyRipplePianoProcessor.h"

#define LOCTEXT_NAMESPACE "KeyRippleAnimationProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// KeyRipple-specific static helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace KeyRippleAnimationHelper {
/**
 * 获取有效的 KeyRipple 控制器名称集合
 */
static const TSet<FString>& GetValidKeyRippleControllerNames() {
    static const TSet<FString> ValidControllerNames = {
        TEXT("H_L"), TEXT("H_R"), TEXT("HP_L"),        TEXT("HP_R"),
        TEXT("0_L"), TEXT("1_L"), TEXT("2_L"),         TEXT("3_L"),
        TEXT("4_L"), TEXT("5_R"), TEXT("6_R"),         TEXT("7_R"),
        TEXT("8_R"), TEXT("9_R"), TEXT("Head_Control")};
    return ValidControllerNames;
}

/**
 * 收集 KeyRipple 中的所有控制器名称
 */
static void CollectKeyRippleControllerNames(AKeyRippleUnreal* KeyRippleActor,
                                            TSet<FString>& OutControllerNames) {
    if (!KeyRippleActor) {
        return;
    }

    // 收集手指控制器
    for (const auto& Pair : KeyRippleActor->FingerControllers) {
        OutControllerNames.Add(Pair.Value);
    }

    // 收集手部控制器
    for (const auto& Pair : KeyRippleActor->HandControllers) {
        OutControllerNames.Add(Pair.Value);
    }

    // 收集目标点（Head_Control, Look_At, Mid_Hand）
    for (const auto& Pair : KeyRippleActor->TargetPoints) {
        OutControllerNames.Add(Pair.Value);
    }
}
}  // namespace KeyRippleAnimationHelper

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// KeyRippleAnimationProcessor implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UKeyRippleAnimationProcessor::GeneratePerformerAnimation(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GeneratePerformerAnimation: KeyRippleActor is null"));
        return;
    }

    FString AnimationPath;
    FString KeyAnimationPath;
    FString ActivityCurvePath;

    // 解析KeyRipple文件
    if (!ParseKeyRippleFile(KeyRippleActor, AnimationPath, KeyAnimationPath,
                            ActivityCurvePath)) {
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

    // 调用直接处理动画文件的方法
    GeneratePerformerAnimationDirect(KeyRippleActor, AnimationPath);

    // 写入 active curve
    if (!ActivityCurvePath.IsEmpty() && KeyRippleActor->SkeletalMeshActor) {
        ULevelSequence* LevelSequence = nullptr;
        TSharedPtr<ISequencer> Sequencer = nullptr;
        if (UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
                LevelSequence, Sequencer)) {
            UE_LOG(LogTemp, Warning, TEXT("Writing active curve from: %s"),
                   *ActivityCurvePath);
            UInstrumentAnimationUtility::WriteActiveCurveFromFile(
                KeyRippleActor->SkeletalMeshActor, ActivityCurvePath,
                LevelSequence);
        }
    }
}

void UKeyRippleAnimationProcessor::GeneratePerformerAnimationDirect(
    AKeyRippleUnreal* KeyRippleActor, const FString& AnimationFilePath) {
    if (!KeyRippleActor) {
        UE_LOG(
            LogTemp, Error,
            TEXT("GeneratePerformerAnimationDirect: KeyRippleActor is null"));
        return;
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("Generating performer animation with Control Rig integration: %s"),
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

    // 3. 通过Subsystem获取 Control Rig Instance
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("InitAnimationControlRig: GEngine is not available"));
        return;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("InitAnimationControlRig: ControlRig Cache Subsystem is "
                    "not available"));
        return;
    }

    // 4. 获取 Sequencer 和 Level Sequence
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get active Level Sequence and Sequencer"));
        return;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(KeyRippleActor->SkeletalMeshActor,
                                      LevelSequence, TEXT("controller_root"));
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            KeyRippleActor->SkeletalMeshActor, LevelSequence,
            TEXT("controller_root"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    // 5. 验证并修复重复的轨道
    bool bHasDuplicateTracks =
        UInstrumentAnimationUtility::ValidateNoExistingTracks(
            LevelSequence, ControlRigInstance, true);
    if (bHasDuplicateTracks) {
        UE_LOG(LogTemp, Warning,
               TEXT("Duplicate Control Rig tracks detected and auto-fixed. "
                    "Proceeding with animation generation."));
    }

    UE_LOG(LogTemp, Warning, TEXT("Starting to process %d animation frames"),
           JsonArray.Num());

    // 8. 处理每一帧并收集关键帧数据
    TMap<FString, TArray<FAnimationKeyframe>> ControlKeyframeData;
    int32 ProcessedFrames = 0;
    int32 FailedFrames = 0;
    int32 KeyframesAdded = 0;

    for (int32 FrameIndex = 0; FrameIndex < JsonArray.Num(); ++FrameIndex) {
        TSharedPtr<FJsonObject> FrameObject = JsonArray[FrameIndex]->AsObject();
        if (!FrameObject.IsValid()) {
            FailedFrames++;
            continue;
        }

        if (!FrameObject->HasField(TEXT("frame"))) {
            FailedFrames++;
            continue;
        }
        int32 FrameNumber = FrameObject->GetIntegerField(TEXT("frame"));

        if (!FrameObject->HasField(TEXT("hand_infos"))) {
            FailedFrames++;
            continue;
        }
        TSharedPtr<FJsonObject> ControlsContainer =
            FrameObject->GetObjectField(TEXT("hand_infos"));

        // KeyRipple 控件数据格式：每个控制器值为直接数组
        //   - 3 元素 = 位置 [x, y, z]
        //   - 4 元素 = 四元数旋转 [w, x, y, z]
        for (const auto& Pair : ControlsContainer->Values) {
            FString RawControlName = Pair.Key;

            FString ControlName =
                UInstrumentAnimationUtility::ValidateControllerName(
                    RawControlName,
                    KeyRippleAnimationHelper::
                        GetValidKeyRippleControllerNames(),
                    TEXT("KeyRipple"));

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
                KeyframesAdded++;
            } else if (DataArray.Num() == 4) {
                // 四元数旋转 [w, x, y, z] → 写入去掉 _rotation 的控制器
                FQuat Rotation;
                Rotation.W = DataArray[0]->AsNumber();
                Rotation.X = DataArray[1]->AsNumber();
                Rotation.Y = DataArray[2]->AsNumber();
                Rotation.Z = DataArray[3]->AsNumber();
                Rotation.Normalize();
                Keyframe.Rotation = Rotation;
                Keyframe.bHasRotation = true;
                ControlKeyframeData.FindOrAdd(ControlName).Add(Keyframe);
                KeyframesAdded++;
            } else if (DataArray.Num() == 7) {
                // 合并格式 [x, y, z, w, i, j, k] → 位置+旋转合并写入 H_* 控制器
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
                KeyframesAdded++;
            } else {
                continue;
            }
        }

        ProcessedFrames++;
    }

    // 9. 配置批量插入设置
    FBatchInsertKeyframesSettings Settings;

    // 配置特殊控制器处理（Tar_ 控制器只插入 X 轴）
    Settings.SpecialControllerRules.Add(TEXT("Tar_"), ESpecialAxisMode::X);

    // 10. 批量插入关键帧（使用通用方法）
    UInstrumentAnimationUtility::BatchInsertControlRigKeys(
        LevelSequence, ControlRigInstance, ControlKeyframeData, Settings);

    UE_LOG(
        LogTemp, Warning,
        TEXT("========== GeneratePerformerAnimationDirect Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed: %d frames"),
           ProcessedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Failed frames: %d"), FailedFrames);
    UE_LOG(LogTemp, Warning, TEXT("Total keyframes added to Sequencer: %d"),
           KeyframesAdded);
    UE_LOG(LogTemp, Warning,
           TEXT("========== GeneratePerformerAnimationDirect Completed "
                "=========="));
}

bool UKeyRippleAnimationProcessor::ParseKeyRippleFile(
    AKeyRippleUnreal* KeyRippleActor, FString& OutAnimationPath,
    FString& OutKeyAnimationPath, FString& OutActivityCurvePath) {
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

    // 提取 activity curve 路径
    if (JsonObject->HasField(TEXT("activity_curve_path"))) {
        OutActivityCurvePath =
            JsonObject->GetStringField(TEXT("activity_curve_path"));
    } else {
        OutActivityCurvePath = FString();
    }

    UE_LOG(LogTemp, Warning, TEXT("ParseKeyRippleFile succeeded"));
    UE_LOG(LogTemp, Warning, TEXT("  Animation Path: %s"), *OutAnimationPath);
    UE_LOG(LogTemp, Warning, TEXT("  Key Animation Path: %s"),
           *OutKeyAnimationPath);
    UE_LOG(LogTemp, Warning, TEXT("  Activity Curve Path: %s"),
           *OutActivityCurvePath);

    return true;
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
    UKeyRipplePianoProcessor::GenerateInstrumentAnimation(
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
    FString ActivityCurvePath;

    // 解析KeyRipple文件
    if (!ParseKeyRippleFile(KeyRippleActor, AnimationPath, KeyAnimationPath,
                            ActivityCurvePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse KeyRipple file in GenerateAllAnimation"));
        return;
    }

    // 生成演奏动画
    if (!AnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Generating performer animation from: %s"), *AnimationPath);
        GeneratePerformerAnimationDirect(KeyRippleActor, AnimationPath);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Animation path is empty, skipping performer animation "
                    "generation"));
    }

    // 写入 active curve
    if (!ActivityCurvePath.IsEmpty() && KeyRippleActor->SkeletalMeshActor) {
        ULevelSequence* LevelSequence = nullptr;
        TSharedPtr<ISequencer> Sequencer = nullptr;
        if (UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
                LevelSequence, Sequencer)) {
            UE_LOG(LogTemp, Warning, TEXT("Writing active curve from: %s"),
                   *ActivityCurvePath);
            UInstrumentAnimationUtility::WriteActiveCurveFromFile(
                KeyRippleActor->SkeletalMeshActor, ActivityCurvePath,
                LevelSequence);
        }
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

    // 收集需要清理的所有控制器名称
    TSet<FString> ControlNamesToClean;
    KeyRippleAnimationHelper::CollectKeyRippleControllerNames(
        KeyRippleActor, ControlNamesToClean);

    UE_LOG(LogTemp, Warning,
           TEXT("Identified %d control names to clean from animation tracks"),
           ControlNamesToClean.Num());

    // 调用通用方法清理关键帧
    UInstrumentAnimationUtility::ClearControlRigKeyframes(
        LevelSequence, ControlRigInstance, ControlNamesToClean);

    // 标记LevelSequence为已修改
    LevelSequence->MarkPackageDirty();

    UE_LOG(LogTemp, Warning,
           TEXT("Control Rig keyframes cleared for specified controls"));
}

#undef LOCTEXT_NAMESPACE