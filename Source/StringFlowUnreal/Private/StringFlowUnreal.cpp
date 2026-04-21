// Fill out your copyright notice in the Description page of Project Settings.

#include "StringFlowUnreal.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"
#include "Editor/EditorEngine.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ISequencer.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequenceActor.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Sets default values
AStringFlowUnreal::AStringFlowUnreal() {
    // Set this actor to call Tick() every frame. You can turn this off to
    // improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    OneHandFingerNumber = 4;
    StringNumber = 4;
    bIsInitialized = false;

    InitializeControllersAndRecorders();
    bIsInitialized = true;
}

// Called when the game starts or when spawned
void AStringFlowUnreal::BeginPlay() { Super::BeginPlay(); }

void AStringFlowUnreal::BeginDestroy() { Super::BeginDestroy(); }

void AStringFlowUnreal::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);
}

FString AStringFlowUnreal::GetFingerControllerName(
    int32 FingerNumber, EStringFlowHandType HandType) const {
    FString HandStr =
        (HandType == EStringFlowHandType::LEFT) ? TEXT("_L") : TEXT("_R");
    FString BaseName = FString::Printf(TEXT("%d%s"), FingerNumber, *HandStr);
    return BaseName;
}

FString AStringFlowUnreal::GetLeftFingerRecorderName(
    int32 StringIndex, int32 FretIndex, int32 FingerNumber,
    const FString& PositionType) const {
    FString BaseName = FString::Printf(TEXT("p_s%d_f%d_%d_L_%s"), StringIndex,
                                       FretIndex, FingerNumber, *PositionType);
    return BaseName;
}

FString AStringFlowUnreal::GetRightFingerRecorderName(
    int32 StringIndex, int32 FingerNumber, const FString& PositionType) const {
    return FString::Printf(TEXT("p_s%d_%d_R_%s"), StringIndex, FingerNumber,
                           *PositionType);
}

FString AStringFlowUnreal::GetHandControllerName(
    const FString& HandControllerType, EStringFlowHandType HandType) const {
    FString HandStr =
        (HandType == EStringFlowHandType::LEFT) ? TEXT("_L") : TEXT("_R");
    FString BaseName;

    if (HandControllerType == TEXT("hand_controller")) {
        BaseName = FString(TEXT("H")) + HandStr;
    } else if (HandControllerType == TEXT("hand_pivot_controller")) {
        BaseName = FString(TEXT("HP")) + HandStr;
    } else if (HandControllerType == TEXT("hand_rotation_controller")) {
        BaseName = FString(TEXT("H_rotation")) + HandStr;
    } else if (HandControllerType == TEXT("thumb_controller")) {
        BaseName = FString(TEXT("T")) + HandStr;
    } else {
        BaseName = TEXT("") + HandStr;
    }

    return BaseName;
}

FString AStringFlowUnreal::GetLeftHandRecorderName(
    int32 StringIndex, int32 FretIndex, const FString& HandControllerType,
    const FString& PositionType) const {
    FString HandControllerBaseName;

    if (HandControllerType == TEXT("hand_controller")) {
        HandControllerBaseName = TEXT("H");
    } else if (HandControllerType == TEXT("hand_pivot_controller")) {
        HandControllerBaseName = TEXT("HP");
    } else if (HandControllerType == TEXT("hand_rotation_controller")) {
        HandControllerBaseName = TEXT("H_rotation");
    } else if (HandControllerType == TEXT("thumb_controller")) {
        HandControllerBaseName = TEXT("T");
    }

    FString BaseName =
        FString::Printf(TEXT("%s_L_s%d_f%d_%s"), *HandControllerBaseName,
                        StringIndex, FretIndex, *PositionType);
    return BaseName;
}

FString AStringFlowUnreal::GetRightHandRecorderName(
    int32 StringIndex, const FString& HandControllerType,
    const FString& PositionType) const {
    FString HandControllerBaseName;

    if (HandControllerType == TEXT("hand_controller")) {
        HandControllerBaseName = TEXT("H");
    } else if (HandControllerType == TEXT("hand_pivot_controller")) {
        HandControllerBaseName = TEXT("HP");
    } else if (HandControllerType == TEXT("hand_rotation_controller")) {
        HandControllerBaseName = TEXT("H_rotation");
    } else if (HandControllerType == TEXT("thumb_controller")) {
        HandControllerBaseName = TEXT("T");
    }

    return FString::Printf(TEXT("%s_R_%s_s%d"), *HandControllerBaseName,
                           *PositionType, StringIndex);
}

void AStringFlowUnreal::InitializeControllersAndRecorders() {
    // ========== 初始化手指控制器 ==========
    LeftFingerControllers.Empty();
    RightFingerControllers.Empty();

    for (int32 FingerNumber = 1; FingerNumber <= OneHandFingerNumber;
         FingerNumber++) {
        LeftFingerControllers.Add(
            FString::FromInt(FingerNumber),
            GetFingerControllerName(FingerNumber, EStringFlowHandType::LEFT));
        RightFingerControllers.Add(
            FString::FromInt(FingerNumber),
            GetFingerControllerName(FingerNumber, EStringFlowHandType::RIGHT));
    }

    // ========== 初始化左手掌部控制器 ==========
    LeftHandControllers.Empty();
    LeftHandControllers.Add(TEXT("hand_controller"),
                            GetHandControllerName(TEXT("hand_controller"),
                                                  EStringFlowHandType::LEFT));
    LeftHandControllers.Add(TEXT("hand_pivot_controller"),
                            GetHandControllerName(TEXT("hand_pivot_controller"),
                                                  EStringFlowHandType::LEFT));

    LeftHandControllers.Add(TEXT("thumb_controller"),
                            GetHandControllerName(TEXT("thumb_controller"),
                                                  EStringFlowHandType::LEFT));
    // 注意：TP_L 不加入 LeftHandControllers，但会在 SetupControllers 中单独创建

    // ========== 初始化右手掌部控制器 ==========
    RightHandControllers.Empty();
    RightHandControllers.Add(TEXT("hand_controller"),
                             GetHandControllerName(TEXT("hand_controller"),
                                                   EStringFlowHandType::RIGHT));
    RightHandControllers.Add(
        TEXT("hand_pivot_controller"),
        GetHandControllerName(TEXT("hand_pivot_controller"),
                              EStringFlowHandType::RIGHT));

    RightHandControllers.Add(TEXT("thumb_controller"),
                             GetHandControllerName(TEXT("thumb_controller"),
                                                   EStringFlowHandType::RIGHT));
    // 注意：TP_R 不加入 RightHandControllers，但会在 SetupControllers 中单独创建

    // ========== 初始化辅助线 ==========
    GuideLines.Empty();
    GuideLines.Add(TEXT("string_vibration_direction"),
                   TEXT("string_vibration_direction"));
    GuideLines.Add(TEXT("violin_normal_line"), TEXT("violin_normal_line"));

    // ========== 初始化左手手指记录器 ==========
    // 结构: p_s{StringIndex}_f{FretIndex}_{FingerNumber}_L_{PositionType}
    LeftFingerRecorders.Empty();
    FStringFlowStringArray left_finger_recorders_array;

    for (int32 StringIndex = 0; StringIndex < StringNumber; StringIndex++) {
        for (int32 FretIndex : {1, 9, 12}) {
            for (int32 FingerNumber = 1; FingerNumber <= OneHandFingerNumber;
                 FingerNumber++) {
                for (EStringFlowLeftHandPositionType PositionType :
                     {EStringFlowLeftHandPositionType::NORMAL,
                      EStringFlowLeftHandPositionType::INNER,
                      EStringFlowLeftHandPositionType::OUTER}) {
                    FString PositionStr =
                        GetLeftHandPositionTypeString(PositionType);
                    FString RecorderName = GetLeftFingerRecorderName(
                        StringIndex, FretIndex, FingerNumber, PositionStr);
                    left_finger_recorders_array.Add(RecorderName);
                }
            }
        }
    }
    LeftFingerRecorders.Add(TEXT("left_finger_recorders"),
                            left_finger_recorders_array);

    // ========== 初始化左手控制器位置记录器 ==========
    // 结构: {ControllerName}_s{StringIndex}_f{FretIndex}_{PositionType}_L
    LeftHandPositionRecorders.Empty();
    FStringFlowStringArray left_hand_position_recorders_array;

    for (int32 StringIndex = 0; StringIndex < StringNumber; StringIndex++) {
        for (int32 FretIndex : {1, 9, 12}) {
            for (EStringFlowLeftHandPositionType PositionType :
                 {EStringFlowLeftHandPositionType::NORMAL,
                  EStringFlowLeftHandPositionType::INNER,
                  EStringFlowLeftHandPositionType::OUTER}) {
                FString PositionStr =
                    GetLeftHandPositionTypeString(PositionType);

                // 主控制器 (H, HP, T)
                for (const auto& ControllerPair : LeftHandControllers) {
                    if (!ControllerPair.Key.Contains(TEXT("rotation"))) {
                        FString RecorderName = GetLeftHandRecorderName(
                            StringIndex, FretIndex, ControllerPair.Key,
                            PositionStr);
                        left_hand_position_recorders_array.Add(RecorderName);
                    }
                }
            }
        }
    }
    LeftHandPositionRecorders.Add(TEXT("left_hand_position_recorders"),
                                  left_hand_position_recorders_array);

    // ========== 初始化左手拇指控制器记录器 ==========
    // 注：拇指也有位置和旋转记录器，但这里统一放在 LeftThumbRecorders
    LeftThumbRecorders.Empty();
    FStringFlowStringArray left_thumb_position_recorders_array;

    for (int32 StringIndex = 0; StringIndex < StringNumber; StringIndex++) {
        for (int32 FretIndex : {1, 9, 12}) {
            for (EStringFlowLeftHandPositionType PositionType :
                 {EStringFlowLeftHandPositionType::NORMAL,
                  EStringFlowLeftHandPositionType::INNER,
                  EStringFlowLeftHandPositionType::OUTER}) {
                FString PositionStr =
                    GetLeftHandPositionTypeString(PositionType);

                // 拇指位置记录器 (T) - 直接指定thumb控制器
                for (const auto& ControllerPair : LeftHandControllers) {
                    if (ControllerPair.Key == TEXT("thumb_controller")) {
                        FString RecorderName = GetLeftHandRecorderName(
                            StringIndex, FretIndex, ControllerPair.Key,
                            PositionStr);
                        left_thumb_position_recorders_array.Add(RecorderName);
                    }
                }
            }
        }
    }
    LeftThumbRecorders.Add(TEXT("left_thumb_position_recorders"),
                           left_thumb_position_recorders_array);

    // ========== 初始化右手手指记录器 ==========
    // 结构: p_s{StringIndex}_{FingerNumber}_R_{PositionType}
    RightFingerRecorders.Empty();
    FStringFlowStringArray right_finger_recorders_array;

    for (int32 StringIndex = 0; StringIndex < StringNumber; StringIndex++) {
        for (EStringFlowRightHandPositionType PositionType :
             {EStringFlowRightHandPositionType::NEAR,
              EStringFlowRightHandPositionType::FAR,
              EStringFlowRightHandPositionType::PIZZICATO}) {
            for (int32 FingerNumber = 1; FingerNumber <= OneHandFingerNumber;
                 FingerNumber++) {
                FString PositionStr =
                    GetRightHandPositionTypeString(PositionType);
                FString RecorderName = GetRightFingerRecorderName(
                    StringIndex, FingerNumber, PositionStr);
                right_finger_recorders_array.Add(RecorderName);
            }
        }
    }
    RightFingerRecorders.Add(TEXT("right_finger_recorders"),
                             right_finger_recorders_array);

    // ========== 初始化右手控制器位置记录器 ==========
    // 结构: {ControllerName}_R_s{StringIndex}_{PositionType}
    RightHandPositionRecorders.Empty();
    FStringFlowStringArray right_hand_position_recorders_array;

    for (int32 StringIndex = 0; StringIndex < StringNumber; StringIndex++) {
        for (EStringFlowRightHandPositionType PositionType :
             {EStringFlowRightHandPositionType::NEAR,
              EStringFlowRightHandPositionType::FAR,
              EStringFlowRightHandPositionType::PIZZICATO}) {
            FString PositionStr = GetRightHandPositionTypeString(PositionType);

            // 主控制器 (H, HP, T)
            for (const auto& ControllerPair : RightHandControllers) {
                if (!ControllerPair.Key.Contains(TEXT("rotation"))) {
                    FString RecorderName = GetRightHandRecorderName(
                        StringIndex, ControllerPair.Key, PositionStr);
                    right_hand_position_recorders_array.Add(RecorderName);
                }
            }
        }
    }
    RightHandPositionRecorders.Add(TEXT("right_hand_position_recorders"),
                                   right_hand_position_recorders_array);

    // ========== 初始化右手拇指控制器记录器 ==========
    RightThumbRecorders.Empty();
    FStringFlowStringArray right_thumb_recorders_array;

    for (int32 StringIndex = 0; StringIndex < StringNumber; StringIndex++) {
        for (EStringFlowRightHandPositionType PositionType :
             {EStringFlowRightHandPositionType::NEAR,
              EStringFlowRightHandPositionType::FAR,
              EStringFlowRightHandPositionType::PIZZICATO}) {
            FString PositionStr = GetRightHandPositionTypeString(PositionType);

            // 拇指控制器 (T) - 直接指定thumb控制器
            for (const auto& ControllerPair : RightHandControllers) {
                if (ControllerPair.Key == TEXT("thumb_controller")) {
                    FString RecorderName = GetRightHandRecorderName(
                        StringIndex, ControllerPair.Key, PositionStr);
                    right_thumb_recorders_array.Add(RecorderName);
                }
            }
        }
    }
    RightThumbRecorders.Add(TEXT("right_thumb_position_recorders"),
                            right_thumb_recorders_array);

    // ========== 初始化其他记录器 ==========
    // 包括: position_s*_f* (弦端点位置)、触弦点、弓位置
    // mid_s*, f9_s* 不需要存储数据，它们由蓝图动态生成
    OtherRecorders.Empty();
    FStringFlowStringArray other_recorders_array;

    for (int32 StringIndex = 0; StringIndex < StringNumber; StringIndex++) {
        // 弦端点位置记录器 (position_s{i}_f0 和 position_s{i}_f12)
        for (int32 FretEnd : {0, 12}) {
            FString PositionRecorderName =
                FString::Printf(TEXT("position_s%d_f%d"), StringIndex, FretEnd);
            other_recorders_array.Add(PositionRecorderName);
        }

        // 触弦点和弓位置记录器 (为每种右手位置类型)
        for (EStringFlowRightHandPositionType PositionType :
             {EStringFlowRightHandPositionType::NEAR,
              EStringFlowRightHandPositionType::FAR,
              EStringFlowRightHandPositionType::PIZZICATO}) {
            FString PositionStr = GetRightHandPositionTypeString(PositionType);

            // 触弦点记录器 (stp_{StringIndex}_{PositionType})
            FString STPRecorderName =
                FString::Printf(TEXT("stp_%d_%s"), StringIndex, *PositionStr);
            other_recorders_array.Add(STPRecorderName);

            // 弓位置记录器 (bow_position_s{StringIndex}_{PositionType})
            FString BowRecorderName = FString::Printf(
                TEXT("bow_position_s%d_%s"), StringIndex, *PositionStr);
            other_recorders_array.Add(BowRecorderName);

            // Right_Hand_Tar 记录器 (right_hand_tar_{PositionType}_s{StringIndex})
            FString RHTRecorderName = FString::Printf(
                TEXT("right_hand_tar_%s_s%d"), *PositionStr, StringIndex);
            other_recorders_array.Add(RHTRecorderName);
        }
    }
    OtherRecorders.Add(TEXT("other_recorders"), other_recorders_array);

    // ========== 初始化辅助线记录器到RecorderTransforms ==========
    FStringFlowRecorderTransform DefaultTransform;
    DefaultTransform.Location = FVector::ZeroVector;
    DefaultTransform.Rotation = FQuat::Identity;

    for (const auto& GuidePair : GuideLines) {
        RecorderTransforms.Add(GuidePair.Value, DefaultTransform);
    }

    // ========== 初始化所有recorder到RecorderTransforms ==========
    // 初始化左手手指记录器
    const FStringFlowStringArray* LeftFingerRecordersArray =
        LeftFingerRecorders.Find(TEXT("left_finger_recorders"));
    if (LeftFingerRecordersArray) {
        for (int32 i = 0; i < LeftFingerRecordersArray->Num(); ++i) {
            RecorderTransforms.Add(LeftFingerRecordersArray->Get(i),
                                   DefaultTransform);
        }
    }

    // 初始化左手控制器位置记录器
    const FStringFlowStringArray* LeftHandPositionRecordersArray =
        LeftHandPositionRecorders.Find(TEXT("left_hand_position_recorders"));
    if (LeftHandPositionRecordersArray) {
        for (int32 i = 0; i < LeftHandPositionRecordersArray->Num(); ++i) {
            RecorderTransforms.Add(LeftHandPositionRecordersArray->Get(i),
                                   DefaultTransform);
        }
    }

    // 初始化左手拇指记录器
    const FStringFlowStringArray* LeftThumbRecordersArray =
        LeftThumbRecorders.Find(TEXT("left_thumb_position_recorders"));
    if (LeftThumbRecordersArray) {
        for (int32 i = 0; i < LeftThumbRecordersArray->Num(); ++i) {
            RecorderTransforms.Add(LeftThumbRecordersArray->Get(i),
                                   DefaultTransform);
        }
    }

    // 初始化右手手指记录器
    const FStringFlowStringArray* RightFingerRecordersArray =
        RightFingerRecorders.Find(TEXT("right_finger_recorders"));
    if (RightFingerRecordersArray) {
        for (int32 i = 0; i < RightFingerRecordersArray->Num(); ++i) {
            RecorderTransforms.Add(RightFingerRecordersArray->Get(i),
                                   DefaultTransform);
        }
    }

    // 初始化右手控制器位置记录器
    const FStringFlowStringArray* RightHandPositionRecordersArray =
        RightHandPositionRecorders.Find(TEXT("right_hand_position_recorders"));
    if (RightHandPositionRecordersArray) {
        for (int32 i = 0; i < RightHandPositionRecordersArray->Num(); ++i) {
            RecorderTransforms.Add(RightHandPositionRecordersArray->Get(i),
                                   DefaultTransform);
        }
    }

    // 初始化右手拇指记录器
    const FStringFlowStringArray* RightThumbRecordersArray =
        RightThumbRecorders.Find(TEXT("right_thumb_position_recorders"));
    if (RightThumbRecordersArray) {
        for (int32 i = 0; i < RightThumbRecordersArray->Num(); ++i) {
            RecorderTransforms.Add(RightThumbRecordersArray->Get(i),
                                   DefaultTransform);
        }
    }

    // 初始化其他记录器
    const FStringFlowStringArray* OtherRecordersArray =
        OtherRecorders.Find(TEXT("other_recorders"));
    if (OtherRecordersArray) {
        for (int32 i = 0; i < OtherRecordersArray->Num(); ++i) {
            RecorderTransforms.Add(OtherRecordersArray->Get(i),
                                   DefaultTransform);
        }
    }
}

FString AStringFlowUnreal::GetLeftHandPositionTypeString(
    EStringFlowLeftHandPositionType PositionType) const {
    switch (PositionType) {
        case EStringFlowLeftHandPositionType::NORMAL:
            return TEXT("Normal");
        case EStringFlowLeftHandPositionType::INNER:
            return TEXT("Inner");
        case EStringFlowLeftHandPositionType::OUTER:
            return TEXT("Outer");
        default:
            return TEXT("");
    }
}

FString AStringFlowUnreal::GetRightHandPositionTypeString(
    EStringFlowRightHandPositionType PositionType) const {
    switch (PositionType) {
        case EStringFlowRightHandPositionType::NEAR:
            return TEXT("near");
        case EStringFlowRightHandPositionType::FAR:
            return TEXT("far");
        case EStringFlowRightHandPositionType::PIZZICATO:
            return TEXT("pizzicato");
        default:
            return TEXT("");
    }
}

FString AStringFlowUnreal::GetCurrentInstrumentName() const {
    return CurrentInstrumentConfig.GetInstrumentName();
}

int32 AStringFlowUnreal::GetCurrentStringNote(int32 StringIndex) const {
    return CurrentInstrumentConfig.GetStringNote(StringIndex);
}

void AStringFlowUnreal::SetInstrumentToViolin() {
    CurrentInstrumentConfig = FStringFlowInstrumentConfig::GetViolinConfig();
}

void AStringFlowUnreal::SetInstrumentToViola() {
    CurrentInstrumentConfig = FStringFlowInstrumentConfig::GetViolaConfig();
}

void AStringFlowUnreal::SetInstrumentToCello() {
    CurrentInstrumentConfig = FStringFlowInstrumentConfig::GetCelloConfig();
}

void AStringFlowUnreal::SetCustomInstrumentConfig(
    const TArray<int32>& InStringNotes) {
    if (InStringNotes.Num() == 4) {
        CurrentInstrumentConfig =
            FStringFlowInstrumentConfig::GetCustomConfig(InStringNotes);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("SetCustomInstrumentConfig: InStringNotes must have 4 "
                    "elements, got %d"),
               InStringNotes.Num());
    }
}

void AStringFlowUnreal::ExportRecorderInfo(const FString& FilePath) {
    if (FilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("ExportRecorderInfo: FilePath is empty"));
        return;
    }

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    // 保存配置参数
    TSharedPtr<FJsonObject> ConfigObject = MakeShareable(new FJsonObject);
    ConfigObject->SetNumberField(TEXT("one_hand_finger_number"),
                                 OneHandFingerNumber);
    ConfigObject->SetNumberField(TEXT("string_number"), StringNumber);
    ConfigObject->SetBoolField(TEXT("is_unreal"), true);

    // 保存乐器配置
    switch (CurrentInstrumentConfig.InstrumentType) {
        case EStringFlowInstrumentType::VIOLIN:
            ConfigObject->SetStringField(TEXT("instrument"), TEXT("violin"));
            break;
        case EStringFlowInstrumentType::VIOLA:
            ConfigObject->SetStringField(TEXT("instrument"), TEXT("viola"));
            break;
        case EStringFlowInstrumentType::CELLO:
            ConfigObject->SetStringField(TEXT("instrument"), TEXT("cello"));
            break;
        case EStringFlowInstrumentType::CUSTOM:
            ConfigObject->SetStringField(TEXT("instrument"), TEXT("custom"));
            // 对于自定义乐器，导出弦音高
            if (CurrentInstrumentConfig.StringNotes.Num() == 4) {
                TArray<TSharedPtr<FJsonValue>> StringNotesArray;
                for (int32 i = 0; i < 4; ++i) {
                    StringNotesArray.Add(MakeShareable(new FJsonValueNumber(
                        CurrentInstrumentConfig.StringNotes[i])));
                }
                ConfigObject->SetArrayField(TEXT("string_notes"),
                                            StringNotesArray);
            }
            break;
        default:
            ConfigObject->SetStringField(TEXT("instrument"), TEXT("violin"));
            break;
    }

    JsonObject->SetObjectField(TEXT("config"), ConfigObject);

    // 创建分类对象
    TMap<FString, TSharedPtr<FJsonObject>> CategoryObjects;
    CategoryObjects.Add(TEXT("left_finger_recorders"),
                        MakeShareable(new FJsonObject));
    CategoryObjects.Add(TEXT("left_hand_position_recorders"),
                        MakeShareable(new FJsonObject));
    CategoryObjects.Add(TEXT("left_hand_rotation_recorders"),
                        MakeShareable(new FJsonObject));
    CategoryObjects.Add(TEXT("left_thumb_position_recorders"),
                        MakeShareable(new FJsonObject));
    CategoryObjects.Add(TEXT("right_finger_recorders"),
                        MakeShareable(new FJsonObject));
    CategoryObjects.Add(TEXT("right_hand_position_recorders"),
                        MakeShareable(new FJsonObject));
    CategoryObjects.Add(TEXT("right_hand_rotation_recorders"),
                        MakeShareable(new FJsonObject));
    CategoryObjects.Add(TEXT("right_thumb_position_recorders"),
                        MakeShareable(new FJsonObject));
    CategoryObjects.Add(TEXT("other_recorders"),
                        MakeShareable(new FJsonObject));
    CategoryObjects.Add(TEXT("guide_lines_rotations"),
                        MakeShareable(new FJsonObject));

    // 辅助Lambda：创建recorder JSON对象
    auto CreateRecorderObject =
        [](const FStringFlowRecorderTransform* Transform,
           bool bIncludeLocation = true,
           bool bIsRotationOnly = false) -> TSharedPtr<FJsonObject> {
        TSharedPtr<FJsonObject> RecorderObj = MakeShareable(new FJsonObject);

        if (bIncludeLocation && !bIsRotationOnly) {
            TArray<TSharedPtr<FJsonValue>> LocationArray;
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform->Location.X)));
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform->Location.Y)));
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform->Location.Z)));
            RecorderObj->SetArrayField(TEXT("location"), LocationArray);
        }

        RecorderObj->SetStringField(TEXT("rotation_mode"), TEXT("QUATERNION"));

        TArray<TSharedPtr<FJsonValue>> RotationArray;
        RotationArray.Add(
            MakeShareable(new FJsonValueNumber(Transform->Rotation.W)));
        RotationArray.Add(
            MakeShareable(new FJsonValueNumber(Transform->Rotation.X)));
        RotationArray.Add(
            MakeShareable(new FJsonValueNumber(Transform->Rotation.Y)));
        RotationArray.Add(
            MakeShareable(new FJsonValueNumber(Transform->Rotation.Z)));
        RecorderObj->SetArrayField(TEXT("rotation_quaternion"), RotationArray);

        return RecorderObj;
    };

    // 辅助Lambda：导出指定分类的记录器
    auto ExportRecordersCategory = [this, &CreateRecorderObject,
                                    &CategoryObjects](
                                       const FString& CategoryName) {
        TSharedPtr<FJsonObject> CategoryObj = CategoryObjects[CategoryName];

        // 获取对应分类的recorder列表
        const FStringFlowStringArray* RecorderArray = nullptr;

        if (CategoryName == TEXT("left_finger_recorders")) {
            RecorderArray =
                LeftFingerRecorders.Find(TEXT("left_finger_recorders"));
        } else if (CategoryName == TEXT("left_hand_position_recorders") ||
                   CategoryName == TEXT("left_hand_rotation_recorders")) {
            RecorderArray = LeftHandPositionRecorders.Find(
                TEXT("left_hand_position_recorders"));
        } else if (CategoryName == TEXT("left_thumb_position_recorders")) {
            RecorderArray =
                LeftThumbRecorders.Find(TEXT("left_thumb_position_recorders"));
        } else if (CategoryName == TEXT("right_finger_recorders")) {
            RecorderArray =
                RightFingerRecorders.Find(TEXT("right_finger_recorders"));
        } else if (CategoryName == TEXT("right_hand_position_recorders") ||
                   CategoryName == TEXT("right_hand_rotation_recorders")) {
            RecorderArray = RightHandPositionRecorders.Find(
                TEXT("right_hand_position_recorders"));
        } else if (CategoryName == TEXT("right_thumb_position_recorders")) {
            RecorderArray = RightThumbRecorders.Find(
                TEXT("right_thumb_position_recorders"));
        } else if (CategoryName == TEXT("other_recorders")) {
            RecorderArray = OtherRecorders.Find(TEXT("other_recorders"));
        }

        if (!RecorderArray) {
            UE_LOG(LogTemp, Warning,
                   TEXT("ExportRecorderInfo: Category '%s' not found"),
                   *CategoryName);
            return;
        }

        UE_LOG(LogTemp, Warning,
               TEXT("ExportRecorderInfo: Exporting %d recorders from '%s'"),
               RecorderArray->Num(), *CategoryName);

        // 遍历该分类下的所有recorder名称
        for (int32 i = 0; i < RecorderArray->Num(); ++i) {
            FString RecorderName = RecorderArray->Get(i);

            // 特殊处理：判断是否为旋转分类
            bool bIsRotationCategory = CategoryName.Contains(TEXT("rotation"));
            FString RealRecorderName =
                bIsRotationCategory
                    ? RecorderName.Replace(TEXT("H_"), TEXT("H_rotation_"))
                    : RecorderName;

            // 从RecorderTransforms中查找该recorder的变换数据
            const FStringFlowRecorderTransform* Transform =
                RecorderTransforms.Find(RecorderName);
            if (Transform) {
                // 如果是旋转recorder，只导出旋转部分
                TSharedPtr<FJsonObject> RecorderObj = CreateRecorderObject(
                    Transform, !bIsRotationCategory, bIsRotationCategory);
                CategoryObj->SetObjectField(*RealRecorderName, RecorderObj);
            }
        }
    };

    // 按照Python版本的顺序导出
    ExportRecordersCategory(TEXT("left_finger_recorders"));
    ExportRecordersCategory(TEXT("left_hand_position_recorders"));
    ExportRecordersCategory(TEXT("left_hand_rotation_recorders"));
    ExportRecordersCategory(TEXT("left_thumb_position_recorders"));
    ExportRecordersCategory(TEXT("right_hand_position_recorders"));
    ExportRecordersCategory(TEXT("right_hand_rotation_recorders"));
    ExportRecordersCategory(TEXT("right_thumb_position_recorders"));
    ExportRecordersCategory(TEXT("right_finger_recorders"));
    ExportRecordersCategory(TEXT("other_recorders"));

    // 导出辅助线
    TSharedPtr<FJsonObject> GuideLinesObj =
        CategoryObjects[TEXT("guide_lines_rotations")];
    for (const auto& GuidePair : GuideLines) {
        const FString& GuideLineName = GuidePair.Value;
        const FStringFlowRecorderTransform* Transform =
            RecorderTransforms.Find(GuideLineName);
        if (Transform) {
            TSharedPtr<FJsonObject> RecorderObj =
                CreateRecorderObject(Transform, true);
            GuideLinesObj->SetObjectField(*GuideLineName, RecorderObj);
        }
    }

    // 将所有分类添加到主JSON对象
    for (const auto& CategoryPair : CategoryObjects) {
        JsonObject->SetObjectField(*CategoryPair.Key, CategoryPair.Value);
    }

    // 序列化为字符串并写入文件
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    if (FFileHelper::SaveStringToFile(OutputString, *FilePath)) {
        UE_LOG(LogTemp, Warning,
               TEXT("ExportRecorderInfo: Successfully exported to %s"),
               *FilePath);
    } else {
        UE_LOG(LogTemp, Error, TEXT("ExportRecorderInfo: Failed to save to %s"),
               *FilePath);
    }
}

bool AStringFlowUnreal::ImportRecorderInfo(const FString& FilePath) {
    if (FilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("ImportRecorderInfo: FilePath is empty"));
        return false;
    }

    // 读取文件
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("ImportRecorderInfo: Failed to load file %s"), *FilePath);
        return false;
    }

    // 解析JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) ||
        !JsonObject.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("ImportRecorderInfo: Failed to parse JSON from %s"),
               *FilePath);
        return false;
    }

    // 导入配置参数
    if (JsonObject->HasField(TEXT("config"))) {
        TSharedPtr<FJsonObject> ConfigObject =
            JsonObject->GetObjectField(TEXT("config"));
        if (ConfigObject->HasField(TEXT("one_hand_finger_number"))) {
            OneHandFingerNumber =
                ConfigObject->GetIntegerField(TEXT("one_hand_finger_number"));
        }
        if (ConfigObject->HasField(TEXT("string_number"))) {
            StringNumber = ConfigObject->GetIntegerField(TEXT("string_number"));
        }

        // 导入乐器配置
        if (ConfigObject->HasField(TEXT("instrument"))) {
            FString InstrumentType =
                ConfigObject->GetStringField(TEXT("instrument"));

            if (InstrumentType == TEXT("violin")) {
                SetInstrumentToViolin();
            } else if (InstrumentType == TEXT("viola")) {
                SetInstrumentToViola();
            } else if (InstrumentType == TEXT("cello")) {
                SetInstrumentToCello();
            } else if (InstrumentType == TEXT("custom")) {
                // 对于自定义乐器，读取string_notes
                if (ConfigObject->HasField(TEXT("string_notes"))) {
                    TArray<TSharedPtr<FJsonValue>> StringNotesArray =
                        ConfigObject->GetArrayField(TEXT("string_notes"));
                    if (StringNotesArray.Num() == 4) {
                        TArray<int32> StringNotes;
                        for (int32 i = 0; i < 4; ++i) {
                            StringNotes.Add(
                                (int32)StringNotesArray[i]->AsNumber());
                        }
                        SetCustomInstrumentConfig(StringNotes);
                    }
                }
            }
        } else {
            // 如果没有instrument字段，默认使用小提琴
            SetInstrumentToViolin();
        }
    }

    // 清空现有的RecorderTransforms并导入新数据
    RecorderTransforms.Empty();

    int32 ImportedCount = 0;

    // 新格式：分类层级结构
    UE_LOG(LogTemp, Warning,
           TEXT("Importing categorized format with hierarchy"));

    // 辅助 Lambda：用于处理各种recorder分类的导入
    auto ImportRecordersCategory = [this, JsonObject, &ImportedCount](
                                       const FString& CategoryName) {
        if (!JsonObject->HasField(*CategoryName)) {
            return;
        }

        TSharedPtr<FJsonObject> CategoryObj =
            JsonObject->GetObjectField(*CategoryName);

        for (const auto& RecorderPair : CategoryObj->Values) {
            const FString& RecorderName = RecorderPair.Key;
            TSharedPtr<FJsonObject> RecorderObj =
                RecorderPair.Value->AsObject();

            if (!RecorderObj.IsValid()) {
                continue;
            }

            // 特殊处理：判断是否为旋转recorder（如H_rotation_L）
            bool bIsRotationRecorder = RecorderName.Contains(TEXT("rotation"));
            FString RealRecorderName =
                bIsRotationRecorder
                    ? RecorderName.Replace(TEXT("_rotation"), TEXT(""))
                    : RecorderName;

            // 获取或创建真实recorder的Transform
            FStringFlowRecorderTransform* TargetTransform =
                RecorderTransforms.Find(RealRecorderName);
            if (!TargetTransform) {
                FStringFlowRecorderTransform NewTransform;
                NewTransform.Location = FVector::ZeroVector;
                NewTransform.Rotation = FQuat::Identity;
                TargetTransform =
                    &RecorderTransforms.Add(RealRecorderName, NewTransform);
            }

            // 读取位置（仅非旋转recorder）
            if (!bIsRotationRecorder &&
                RecorderObj->HasField(TEXT("location"))) {
                TArray<TSharedPtr<FJsonValue>> LocationArray =
                    RecorderObj->GetArrayField(TEXT("location"));
                if (LocationArray.Num() == 3) {
                    TargetTransform->Location.X = LocationArray[0]->AsNumber();
                    TargetTransform->Location.Y = LocationArray[1]->AsNumber();
                    TargetTransform->Location.Z = LocationArray[2]->AsNumber();
                }
            }

            // 读取旋转（四元数）
            if (RecorderObj->HasField(TEXT("rotation_quaternion"))) {
                TArray<TSharedPtr<FJsonValue>> RotationArray =
                    RecorderObj->GetArrayField(TEXT("rotation_quaternion"));
                if (RotationArray.Num() == 4) {
                    TargetTransform->Rotation.W = RotationArray[0]->AsNumber();
                    TargetTransform->Rotation.X = RotationArray[1]->AsNumber();
                    TargetTransform->Rotation.Y = RotationArray[2]->AsNumber();
                    TargetTransform->Rotation.Z = RotationArray[3]->AsNumber();
                }
            }

            ImportedCount++;
        }
    };

    // 导入各类记录器
    ImportRecordersCategory(TEXT("left_finger_recorders"));
    ImportRecordersCategory(TEXT("left_hand_position_recorders"));
    ImportRecordersCategory(TEXT("left_hand_rotation_recorders"));
    ImportRecordersCategory(TEXT("left_thumb_position_recorders"));
    ImportRecordersCategory(TEXT("right_finger_recorders"));
    ImportRecordersCategory(TEXT("right_hand_position_recorders"));
    ImportRecordersCategory(TEXT("right_hand_rotation_recorders"));
    ImportRecordersCategory(TEXT("right_thumb_position_recorders"));
    ImportRecordersCategory(TEXT("other_recorders"));
    ImportRecordersCategory(TEXT("guide_lines_rotations"));

    UE_LOG(
        LogTemp, Warning,
        TEXT("ImportRecorderInfo: Successfully imported %d recorders from %s"),
        ImportedCount, *FilePath);
    return true;
}

// ========== 缓存管理方法实现 ==========

ASkeletalMeshActor* AStringFlowUnreal::GetSkeletalMeshActorByName(
    FName ComponentName) const {
    if (ComponentName == TEXT("StringInstrument")) {
        return StringInstrument;
    } else if (ComponentName == TEXT("Bow")) {
        return Bow;
    } else if (ComponentName == TEXT("Performer")) {
        return SkeletalMeshActor;
    }
    return nullptr;
}

UControlRig* AStringFlowUnreal::GetCachedControlRig(FName ComponentName) {
    // 详细诊断GEngine和Subsystem状态
    if (!GEngine) {
        UE_LOG(LogTemp, Error, TEXT("GetCachedControlRig: GEngine is NULL"));
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ControlRig for component %s - GEngine "
                    "unavailable"),
               *ComponentName.ToString());
        return nullptr;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();

    if (!CacheSubsystem) {
        UE_LOG(
            LogTemp, Error,
            TEXT("GetCachedControlRig: CacheSubsystem not found in GEngine"));
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ControlRig for component %s - "
                    "CacheSubsystem unavailable"),
               *ComponentName.ToString());
        return nullptr;
    }

    ASkeletalMeshActor* Actor = GetSkeletalMeshActorByName(ComponentName);
    if (!Actor) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRig: Actor not found for component %s"),
               *ComponentName.ToString());
        return nullptr;
    }

    // 根据 ComponentName 确定 RootControlName
    FString RootControlName;
    if (ComponentName == TEXT("StringInstrument")) {
        RootControlName = TEXT("violin_root");
    } else if (ComponentName == TEXT("Bow")) {
        RootControlName = TEXT("bow_root");
    } else if (ComponentName == TEXT("Performer")) {
        RootControlName = TEXT("controller_root");
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRig: No LevelSequence found"));
        return nullptr;
    }

    // 使用通用接口查询ControlRig
    UControlRig* ControlRig =
        CacheSubsystem->GetControlRig(Actor, LevelSequence, RootControlName);

    return ControlRig;
}

UControlRigBlueprint* AStringFlowUnreal::GetCachedControlRigBlueprint(
    FName ComponentName) {
    // 详细诊断GEngine和Subsystem状态
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("GetCachedControlRigBlueprint: GEngine is NULL"));
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ControlRigBlueprint for component %s - "
                    "GEngine unavailable"),
               *ComponentName.ToString());
        return nullptr;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();

    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("GetCachedControlRigBlueprint: CacheSubsystem not found in "
                    "GEngine"));
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ControlRigBlueprint for component %s - "
                    "CacheSubsystem unavailable"),
               *ComponentName.ToString());
        return nullptr;
    }

    ASkeletalMeshActor* Actor = GetSkeletalMeshActorByName(ComponentName);
    if (!Actor) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRigBlueprint: Actor not found for "
                    "component %s"),
               *ComponentName.ToString());
        return nullptr;
    }

    // 根据 ComponentName 确定 RootControlName
    FString RootControlName;
    if (ComponentName == TEXT("StringInstrument")) {
        RootControlName = TEXT("violin_root");
    } else if (ComponentName == TEXT("Bow")) {
        RootControlName = TEXT("bow_root");
    } else if (ComponentName == TEXT("Performer")) {
        RootControlName = TEXT("controller_root");
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRigBlueprint: No LevelSequence found"));
        return nullptr;
    }

    // 使用通用接口查询ControlRig Blueprint
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(Actor, LevelSequence, RootControlName);

    return ControlRigBlueprint;
}

void AStringFlowUnreal::TriggerControlRigReregistration(
    const FString& ErrorMessage) {
    UE_LOG(LogTemp, Warning,
           TEXT("TriggerControlRigReregistration: %s, triggering ControlRig "
                "re-registration for all components"),
           *ErrorMessage);

    // 触发所有组件的ControlRig重新注册
    if (GEngine) {
        UControlRigCacheSubsystem* CacheSubsystem =
            GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
        if (CacheSubsystem) {
            ULevelSequence* CurrentSequence =
                UInstrumentAnimationUtility::GetCurrentLevelSequence();
            if (CurrentSequence) {
                // 为演奏者组件重新注册
                if (SkeletalMeshActor) {
                    CacheSubsystem->TriggerRegistrationIfNeeded(
                        SkeletalMeshActor, CurrentSequence);
                    UE_LOG(LogTemp, Log,
                           TEXT("Re-registering ControlRig for Performer "
                                "component"));
                }

                // 为乐器组件重新注册
                if (StringInstrument) {
                    CacheSubsystem->TriggerRegistrationIfNeeded(
                        StringInstrument, CurrentSequence);
                    UE_LOG(LogTemp, Log,
                           TEXT("Re-registering ControlRig for "
                                "StringInstrument component"));
                }

                // 为琴弓组件重新注册
                if (Bow) {
                    CacheSubsystem->TriggerRegistrationIfNeeded(
                        Bow, CurrentSequence);
                    UE_LOG(LogTemp, Log,
                           TEXT("Re-registering ControlRig for Bow component"));
                }
            }
        }
    }
}
