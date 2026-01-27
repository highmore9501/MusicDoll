// Fill out your copyright notice in the Description page of Project Settings.

#include "KeyRippleUnreal.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Sets default values
AKeyRippleUnreal::AKeyRippleUnreal() {
    // Set this actor to call Tick() every frame.  You can turn this off to
    // improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    OneHandFingerNumber = 5;
    LeftestPosition = 0;
    LeftPosition = 0;
    MiddleLeftPosition = 0;
    MiddleRightPosition = 0;
    RightPosition = 0;
    RightestPosition = 0;
    MinKey = 0;
    MaxKey = 127;
    HandRange = 10;
    LeftHandKeyType = EKeyType::WHITE;
    LeftHandPositionType = EPositionType::MIDDLE;
    RightHandKeyType = EKeyType::WHITE;
    RightHandPositionType = EPositionType::MIDDLE;

    InitializeControllersAndRecorders();
}

// Called when the game starts or when spawned
void AKeyRippleUnreal::BeginPlay() { Super::BeginPlay(); }

// Called every frame
void AKeyRippleUnreal::Tick(float DeltaTime) { Super::Tick(DeltaTime); }

FString AKeyRippleUnreal::GetControllerName(int32 FingerNumber,
                                            EHandType HandType) const {
    FString HandStr = (HandType == EHandType::LEFT) ? TEXT("_L") : TEXT("_R");
    FString BaseName = FString::Printf(TEXT("%d%s"), FingerNumber, *HandStr);

    return BaseName;
}

FString AKeyRippleUnreal::GetRecorderName(EPositionType PositionType,
                                          EKeyType KeyType, int32 FingerNumber,
                                          EHandType HandType) const {
    FString PositionStr;
    switch (PositionType) {
        case EPositionType::HIGH:
            PositionStr = TEXT("high");
            break;
        case EPositionType::LOW:
            PositionStr = TEXT("low");
            break;
        case EPositionType::MIDDLE:
            PositionStr = TEXT("middle");
            break;
    }

    FString KeyStr =
        (KeyType == EKeyType::WHITE) ? TEXT("white") : TEXT("black");

    FString HandStr = (HandType == EHandType::LEFT) ? TEXT("_L") : TEXT("_R");
    FString BaseName = FString::Printf(TEXT("%s_%s_%d%s"), *PositionStr,
                                       *KeyStr, FingerNumber, *HandStr);

    return BaseName;
}

FString AKeyRippleUnreal::GetHandControllerName(
    const FString &HandControllerType, EHandType HandType) const {
    FString HandStr = (HandType == EHandType::LEFT) ? TEXT("_L") : TEXT("_R");
    FString BaseName;
    if (HandControllerType == TEXT("left_hand_controller") ||
        HandControllerType == TEXT("right_hand_controller")) {
        BaseName = FString(TEXT("H")) + HandStr;
    } else if (HandControllerType == TEXT("left_hand_pivot_controller") ||
               HandControllerType == TEXT("right_hand_pivot_controller")) {
        BaseName = FString(TEXT("HP")) + HandStr;
    } else if (HandControllerType == TEXT("left_hand_rotation_controller") ||
               HandControllerType == TEXT("right_hand_rotation_controller")) {
        BaseName = FString(TEXT("H_rotation")) + HandStr;
    } else {
        BaseName = TEXT("") + HandStr;
    }

    return BaseName;
}

FString AKeyRippleUnreal::GetHandRecorderName(EPositionType PositionType,
                                              EKeyType KeyType,
                                              const FString &HandControllerType,
                                              EHandType HandType) const {
    FString PositionStr;
    switch (PositionType) {
        case EPositionType::HIGH:
            PositionStr = TEXT("high");
            break;
        case EPositionType::LOW:
            PositionStr = TEXT("low");
            break;
        case EPositionType::MIDDLE:
            PositionStr = TEXT("middle");
            break;
    }

    FString KeyStr =
        (KeyType == EKeyType::WHITE) ? TEXT("white") : TEXT("black");

    FString HandControllerBaseName;
    if (HandControllerType == TEXT("left_hand_controller") ||
        HandControllerType == TEXT("right_hand_controller")) {
        HandControllerBaseName = TEXT("H");
    } else if (HandControllerType == TEXT("left_hand_pivot_controller") ||
               HandControllerType == TEXT("right_hand_pivot_controller")) {
        HandControllerBaseName = TEXT("HP");
    } else if (HandControllerType == TEXT("left_hand_rotation_controller") ||
               HandControllerType == TEXT("right_hand_rotation_controller")) {
        HandControllerBaseName = TEXT("H_rotation");
    }

    FString HandStr = (HandType == EHandType::LEFT) ? TEXT("_L") : TEXT("_R");
    FString BaseName =
        FString::Printf(TEXT("%s_%s_%s%s"), *PositionStr, *KeyStr,
                        *HandControllerBaseName, *HandStr);

    return BaseName;
}

void AKeyRippleUnreal::InitializeControllersAndRecorders() {
    FingerControllers.Empty();
    RecorderTransforms.Empty();  // 清空recorder transforms数据表
    FingerRecorders.Empty();
    FStringArray left_finger_recorders;
    FStringArray right_finger_recorders;

    for (int32 finger_number = 0; finger_number < 2 * OneHandFingerNumber;
         finger_number++) {
        bool is_left_hand = finger_number < OneHandFingerNumber;
        FString controller_name = GetControllerName(
            finger_number, is_left_hand ? EHandType::LEFT : EHandType::RIGHT);
        FString finger_number_str = FString::FromInt(finger_number);
        FingerControllers.Add(finger_number_str, controller_name);

        for (EKeyType key_type : {EKeyType::WHITE, EKeyType::BLACK}) {
            for (EPositionType position_type :
                 {EPositionType::HIGH, EPositionType::LOW,
                  EPositionType::MIDDLE}) {
                FString recorder_name =
                    is_left_hand
                        ? GetRecorderName(position_type, key_type,
                                          finger_number, EHandType::LEFT)
                        : GetRecorderName(position_type, key_type,
                                          finger_number, EHandType::RIGHT);
                // 根据is_left_hand判断添加到对应的数组
                if (is_left_hand) {
                    left_finger_recorders.Add(recorder_name);
                } else {
                    right_finger_recorders.Add(recorder_name);
                }
            }
        }
    }

    FingerRecorders.Add(TEXT("left_finger_recorders"), left_finger_recorders);
    FingerRecorders.Add(TEXT("right_finger_recorders"), right_finger_recorders);

    HandControllers.Empty();
    HandControllers.Add(
        TEXT("left_hand_controller"),
        GetHandControllerName(TEXT("left_hand_controller"), EHandType::LEFT));
    HandControllers.Add(
        TEXT("left_hand_pivot_controller"),
        GetHandControllerName(TEXT("left_hand_pivot_controller"),
                              EHandType::LEFT));
    HandControllers.Add(
        TEXT("left_hand_rotation_controller"),
        GetHandControllerName(TEXT("left_hand_rotation_controller"),
                              EHandType::LEFT));
    HandControllers.Add(
        TEXT("right_hand_controller"),
        GetHandControllerName(TEXT("right_hand_controller"), EHandType::RIGHT));
    HandControllers.Add(
        TEXT("right_hand_pivot_controller"),
        GetHandControllerName(TEXT("right_hand_pivot_controller"),
                              EHandType::RIGHT));
    HandControllers.Add(
        TEXT("right_hand_rotation_controller"),
        GetHandControllerName(TEXT("right_hand_rotation_controller"),
                              EHandType::RIGHT));

    HandRecorders.Empty();
    TArray<FString> left_hand_recorders;
    TArray<FString> right_hand_recorders;

    for (const auto &pair : HandControllers) {
        FString controller_name = pair.Value;

        for (EKeyType key_type : {EKeyType::WHITE, EKeyType::BLACK}) {
            for (EPositionType position_type :
                 {EPositionType::HIGH, EPositionType::LOW,
                  EPositionType::MIDDLE}) {
                FString recorder_name = GetHandRecorderName(
                    position_type, key_type, pair.Key,
                    (controller_name.EndsWith(TEXT("_L"))) ? EHandType::LEFT
                                                           : EHandType::RIGHT);

                // 移除重复的_rotation后缀添加，因为GetHandRecorderName已经包含了rotation信息

                if (controller_name.EndsWith(TEXT("_L"))) {
                    left_hand_recorders.Add(recorder_name);
                } else {
                    right_hand_recorders.Add(recorder_name);
                }
            }
        }
    }

    FStringArray left_hand_recorders_wrapper;
    left_hand_recorders_wrapper.Strings = left_hand_recorders;
    FStringArray right_hand_recorders_wrapper;
    right_hand_recorders_wrapper.Strings = right_hand_recorders;

    HandRecorders.Add(TEXT("left_hand_recorders"), left_hand_recorders_wrapper);
    HandRecorders.Add(TEXT("right_hand_recorders"),
                      right_hand_recorders_wrapper);

    KeyBoardPositions.Empty();
    KeyBoardPositions.Add(TEXT("black_key_position"), TEXT("black_key"));
    KeyBoardPositions.Add(TEXT("highest_white_key_position"),
                          TEXT("highest_white_key"));
    KeyBoardPositions.Add(TEXT("lowest_white_key_position"),
                          TEXT("lowest_white_key"));
    KeyBoardPositions.Add(TEXT("normal_hand_expand_position"),
                          TEXT("normal_hand_expand_position"));
    KeyBoardPositions.Add(TEXT("wide_expand_hand_position"),
                          TEXT("wide_expand_hand_position"));

    Guidelines.Empty();
    Guidelines.Add(TEXT("press_key_direction"), TEXT("press_key_direction"));

    TargetPoints.Empty();
    TargetPoints.Add(TEXT("body_target"), TEXT("Tar_Body"));
    TargetPoints.Add(TEXT("chest_target"), TEXT("Tar_Chest"));
    TargetPoints.Add(TEXT("butt_target"), TEXT("Tar_Butt"));

    ShoulderControllers.Empty();
    ShoulderControllers.Add(TEXT("left_shoulder_controller"), TEXT("S_L"));
    ShoulderControllers.Add(TEXT("right_shoulder_controller"), TEXT("S_R"));

    ShoulderRecorders.Empty();
    TArray<FString> left_shoulder_recorders;
    TArray<FString> right_shoulder_recorders;

    for (const auto &pair : ShoulderControllers) {
        FString controller_name = pair.Value;
        for (EKeyType key_type : {EKeyType::WHITE, EKeyType::BLACK}) {
            for (EPositionType position_type :
                 {EPositionType::HIGH, EPositionType::LOW,
                  EPositionType::MIDDLE}) {
                FString recorder_name = FString::Printf(
                    TEXT("%s_%s_%s"), *GetPositionTypeString(position_type),
                    *GetKeyTypeString(key_type), *controller_name);
                if (controller_name.EndsWith(TEXT("_L"))) {
                    left_shoulder_recorders.Add(recorder_name);
                } else {
                    right_shoulder_recorders.Add(recorder_name);
                }
            }
        }
    }

    FStringArray left_shoulder_recorders_wrapper;
    left_shoulder_recorders_wrapper.Strings = left_shoulder_recorders;
    FStringArray right_shoulder_recorders_wrapper;
    right_shoulder_recorders_wrapper.Strings = right_shoulder_recorders;

    ShoulderRecorders.Add(TEXT("left_shoulder_recorders"),
                          left_shoulder_recorders_wrapper);
    ShoulderRecorders.Add(TEXT("right_shoulder_recorders"),
                          right_shoulder_recorders_wrapper);

    TargetPointsRecorders.Empty();

    TArray<FString> tar_body_recorders;
    TArray<FString> tar_chest_recorders;
    TArray<FString> tar_butt_recorders;

    for (const auto &pair : TargetPoints) {
        FString controller_name = pair.Value;
        for (EKeyType key_type : {EKeyType::WHITE, EKeyType::BLACK}) {
            for (EPositionType position_type :
                 {EPositionType::HIGH, EPositionType::LOW,
                  EPositionType::MIDDLE}) {
                FString recorder_name = FString::Printf(
                    TEXT("%s_%s_%s"), *GetPositionTypeString(position_type),
                    *GetKeyTypeString(key_type), *controller_name);
                if (controller_name.Contains(TEXT("body"))) {
                    tar_body_recorders.Add(recorder_name);
                } else if (controller_name.Contains(TEXT("chest"))) {
                    tar_chest_recorders.Add(recorder_name);
                } else if (controller_name.Contains(TEXT("butt"))) {
                    tar_butt_recorders.Add(recorder_name);
                }
            }
        }
    }

    FStringArray tar_body_recorders_wrapper;
    tar_body_recorders_wrapper.Strings = tar_body_recorders;
    FStringArray tar_chest_recorders_wrapper;
    tar_chest_recorders_wrapper.Strings = tar_chest_recorders;
    FStringArray tar_butt_recorders_wrapper;
    tar_butt_recorders_wrapper.Strings = tar_butt_recorders;

    TargetPointsRecorders.Add(TEXT("tar_body_recorders"),
                              tar_body_recorders_wrapper);
    TargetPointsRecorders.Add(TEXT("tar_chest_recorders"),
                              tar_chest_recorders_wrapper);
    TargetPointsRecorders.Add(TEXT("tar_butt_recorders"),
                              tar_butt_recorders_wrapper);

    // 初始化 PolePoints - 为每个手指控制器创建对应的 pole target
    PolePoints.Empty();
    for (const auto &FingerPair : FingerControllers) {
        FString FingerControllerName = FingerPair.Value;  // 例如 "0_L", "5_R"

        // 提取手指编号 - 从控制器名称中去除 "_L" 或 "_R" 后缀
        FString FingerNumber = FingerControllerName;
        if (FingerControllerName.EndsWith(TEXT("_L"))) {
            FingerNumber = FingerControllerName.LeftChop(2);  // 去除 "_L"
        } else if (FingerControllerName.EndsWith(TEXT("_R"))) {
            FingerNumber = FingerControllerName.LeftChop(2);  // 去除 "_R"
        }

        // 创建 pole target 名称，格式为 "pole_X" 其中 X 是手指编号
        FString PoleTargetName =
            FString::Printf(TEXT("pole_%s"), *FingerNumber);

        // 添加映射：finger_key -> pole_target_name
        PolePoints.Add(FingerPair.Key, PoleTargetName);
    }
}

FString AKeyRippleUnreal::GetPositionTypeString(
    EPositionType PositionType) const {
    switch (PositionType) {
        case EPositionType::HIGH:
            return TEXT("high");
        case EPositionType::LOW:
            return TEXT("low");
        case EPositionType::MIDDLE:
            return TEXT("middle");
        default:
            return TEXT("");
    }
}

FString AKeyRippleUnreal::GetKeyTypeString(EKeyType KeyType) const {
    switch (KeyType) {
        case EKeyType::WHITE:
            return TEXT("white");
        case EKeyType::BLACK:
            return TEXT("black");
        default:
            return TEXT("");
    }
}
