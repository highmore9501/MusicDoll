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
    // 初始化手指控制器
    for (int i = 0; i < OneHandFingerNumber; ++i) {
        FingerControllers.Add(GetControllerName(i, EHandType::LEFT));
        FingerControllers.Add(GetControllerName(i, EHandType::RIGHT));
    }

    // 初始化手指记录器
    for (int pos = static_cast<int>(EPositionType::HIGH);
         pos <= static_cast<int>(EPositionType::LOW); ++pos) {
        for (int key = static_cast<int>(EKeyType::WHITE);
             key <= static_cast<int>(EKeyType::BLACK); ++key) {
            for (int finger = 0; finger < OneHandFingerNumber; ++finger) {
                for (int hand = static_cast<int>(EHandType::LEFT);
                     hand <= static_cast<int>(EHandType::RIGHT); ++hand) {
                    FString RecorderName =
                        GetRecorderName(static_cast<EPositionType>(pos),
                                        static_cast<EKeyType>(key), finger,
                                        static_cast<EHandType>(hand));

                    FingerRecorders.Add(RecorderName);
                }
            }
        }
    }

    // 初始化手部控制器
    TArray<FString> HandControllerTypes = {
        TEXT("left_hand_controller"),
        TEXT("right_hand_controller"),
        TEXT("left_hand_pivot_controller"),
        TEXT("right_hand_pivot_controller"),
        TEXT("left_hand_rotation_controller"),
        TEXT("right_hand_rotation_controller")};

    for (const FString &HandControllerType : HandControllerTypes) {
        HandControllers.Add(HandControllerType + TEXT("_L"));
        HandControllers.Add(HandControllerType + TEXT("_R"));
    }

    // 初始化手部记录器
    for (int pos = static_cast<int>(EPositionType::HIGH);
         pos <= static_cast<int>(EPositionType::LOW); ++pos) {
        for (int key = static_cast<int>(EKeyType::WHITE);
             key <= static_cast<int>(EKeyType::BLACK); ++key) {
            for (const FString &HandControllerType : HandControllerTypes) {
                for (int hand = static_cast<int>(EHandType::LEFT);
                     hand <= static_cast<int>(EHandType::RIGHT); ++hand) {
                    FString RecorderName = GetHandRecorderName(
                        static_cast<EPositionType>(pos),
                        static_cast<EKeyType>(key), HandControllerType,
                        static_cast<EHandType>(hand));

                    HandRecorders.Add(RecorderName);
                }
            }
        }
    }

    // 初始化键盘位置
    for (int key = MinKey; key <= MaxKey; ++key) {
        FString KeyName = FString::Printf(TEXT("key_%d"), key);
        KeyBoardPositions.Add(KeyName);
    }

    // 初始化指南针
    Guidelines.Add(TEXT("PianoCenter"));

    // 初始化目标点
    for (int pos = static_cast<int>(EPositionType::HIGH);
         pos <= static_cast<int>(EPositionType::LOW); ++pos) {
        for (int key = static_cast<int>(EKeyType::WHITE);
             key <= static_cast<int>(EKeyType::BLACK); ++key) {
            for (int finger = 0; finger < OneHandFingerNumber; ++finger) {
                for (int hand = static_cast<int>(EHandType::LEFT);
                     hand <= static_cast<int>(EHandType::RIGHT); ++hand) {
                    FString TargetPointName = FString::Printf(
                        TEXT("target_%s_%s_%d_%s"),
                        *GetPositionTypeString(static_cast<EPositionType>(pos)),
                        *GetKeyTypeString(static_cast<EKeyType>(key)), finger,
                        (static_cast<EHandType>(hand) == EHandType::LEFT
                             ? TEXT("L")
                             : TEXT("R")));
                    TargetPoints.Add(TargetPointName);
                    TargetPointsRecorders.Add(TargetPointName);
                }
            }
        }
    }

    // 初始化肩膀控制器
    ShoulderControllers.Add(TEXT("left_shoulder_L"));
    ShoulderControllers.Add(TEXT("right_shoulder_R"));

    // 初始化肩膀记录器
    for (int pos = static_cast<int>(EPositionType::HIGH);
         pos <= static_cast<int>(EPositionType::LOW); ++pos) {
        for (int key = static_cast<int>(EKeyType::WHITE);
             key <= static_cast<int>(EKeyType::BLACK); ++key) {
            for (int hand = static_cast<int>(EHandType::LEFT);
                 hand <= static_cast<int>(EHandType::RIGHT); ++hand) {
                FString RecorderName = FString::Printf(
                    TEXT("shoulder_%s_%s_%s"),
                    *GetPositionTypeString(static_cast<EPositionType>(pos)),
                    *GetKeyTypeString(static_cast<EKeyType>(key)),
                    (static_cast<EHandType>(hand) == EHandType::LEFT
                         ? TEXT("L")
                         : TEXT("R")));

                ShoulderRecorders.Add(RecorderName);
            }
        }
    }

    // 初始化极点
    PolePoints.Add(TEXT("left_elbow_L"));
    PolePoints.Add(TEXT("right_elbow_R"));
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
