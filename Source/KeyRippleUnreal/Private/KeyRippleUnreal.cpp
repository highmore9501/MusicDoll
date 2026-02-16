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

    UE_LOG(LogTemp, Warning,
           TEXT("GetControllerName: FingerNumber=%d, HandType=%s, Result=%s"),
           FingerNumber,
           (HandType == EHandType::LEFT) ? TEXT("LEFT") : TEXT("RIGHT"),
           *BaseName);

    return BaseName;
}

FString AKeyRippleUnreal::GetRecorderName(EPositionType PositionType,
                                          EKeyType KeyType, int32 FingerNumber,
                                          EHandType HandType) const {
    FString PositionStr = GetPositionTypeString(PositionType);
    FString KeyStr = GetKeyTypeString(KeyType);
    FString HandStr = (HandType == EHandType::LEFT) ? TEXT("_L") : TEXT("_R");
    FString BaseName = FString::Printf(TEXT("%s_%s_%d%s"), *PositionStr,
                                       *KeyStr, FingerNumber, *HandStr);

    return BaseName;
}

FString AKeyRippleUnreal::GetHandControllerName(
    const FString& HandControllerType, EHandType HandType) const {
    FString HandStr = (HandType == EHandType::LEFT) ? TEXT("_L") : TEXT("_R");
    FString BaseName;
    if (HandControllerType == TEXT("left_hand_controller") ||
        HandControllerType == TEXT("right_hand_controller")) {
        BaseName = FString(TEXT("H")) + HandStr;
    } else if (HandControllerType == TEXT("left_hand_pivot_controller") ||
               HandControllerType == TEXT("right_hand_pivot_controller")) {
        BaseName = FString(TEXT("HP")) + HandStr;
    } else {
        BaseName = TEXT("") + HandStr;
    }

    return BaseName;
}

FString AKeyRippleUnreal::GetHandRecorderName(EPositionType PositionType,
                                              EKeyType KeyType,
                                              const FString& HandControllerType,
                                              EHandType HandType) const {
    FString PositionStr = GetPositionTypeString(PositionType);
    FString KeyStr = GetKeyTypeString(KeyType);

    FString HandControllerBaseName;
    if (HandControllerType == TEXT("left_hand_controller") ||
        HandControllerType == TEXT("right_hand_controller")) {
        HandControllerBaseName = TEXT("H");
    } else if (HandControllerType == TEXT("left_hand_pivot_controller") ||
               HandControllerType == TEXT("right_hand_pivot_controller")) {
        HandControllerBaseName = TEXT("HP");
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

        UE_LOG(LogTemp, Warning,
               TEXT("InitializeControllers: finger_number=%d, is_left_hand=%s, "
                    "controller_name=%s"),
               finger_number, is_left_hand ? TEXT("true") : TEXT("false"),
               *controller_name);

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
        TEXT("right_hand_controller"),
        GetHandControllerName(TEXT("right_hand_controller"), EHandType::RIGHT));
    HandControllers.Add(
        TEXT("right_hand_pivot_controller"),
        GetHandControllerName(TEXT("right_hand_pivot_controller"),
                              EHandType::RIGHT));

    HandRecorders.Empty();
    TArray<FString> left_hand_recorders;
    TArray<FString> right_hand_recorders;

    for (const auto& pair : HandControllers) {
        FString controller_name = pair.Value;

        for (EKeyType key_type : {EKeyType::WHITE, EKeyType::BLACK}) {
            for (EPositionType position_type :
                 {EPositionType::HIGH, EPositionType::LOW,
                  EPositionType::MIDDLE}) {
                FString recorder_name = GetHandRecorderName(
                    position_type, key_type, pair.Key,
                    (controller_name.EndsWith(TEXT("_L"))) ? EHandType::LEFT
                                                           : EHandType::RIGHT);

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

    for (const auto& pair : ShoulderControllers) {
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

    for (const auto& pair : TargetPoints) {
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
    for (const auto& FingerPair : FingerControllers) {
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

// ========================================
// JSON serialization helpers
// ========================================

static TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector) {
    TArray<TSharedPtr<FJsonValue>> Array;
    Array.Add(MakeShareable(new FJsonValueNumber(Vector.X)));
    Array.Add(MakeShareable(new FJsonValueNumber(Vector.Y)));
    Array.Add(MakeShareable(new FJsonValueNumber(Vector.Z)));
    return Array;
}

static TArray<TSharedPtr<FJsonValue>> QuatToJsonArray(const FQuat& Quat) {
    TArray<TSharedPtr<FJsonValue>> Array;
    Array.Add(MakeShareable(new FJsonValueNumber(Quat.W)));
    Array.Add(MakeShareable(new FJsonValueNumber(Quat.X)));
    Array.Add(MakeShareable(new FJsonValueNumber(Quat.Y)));
    Array.Add(MakeShareable(new FJsonValueNumber(Quat.Z)));
    return Array;
}

static bool JsonArrayToVector(const TArray<TSharedPtr<FJsonValue>>& Array,
                              FVector& OutVector) {
    if (Array.Num() != 3) return false;
    OutVector.X = Array[0]->AsNumber();
    OutVector.Y = Array[1]->AsNumber();
    OutVector.Z = Array[2]->AsNumber();
    return true;
}

static bool JsonArrayToQuat(const TArray<TSharedPtr<FJsonValue>>& Array,
                            FQuat& OutQuat) {
    if (Array.Num() != 4) return false;
    OutQuat.W = Array[0]->AsNumber();
    OutQuat.X = Array[1]->AsNumber();
    OutQuat.Y = Array[2]->AsNumber();
    OutQuat.Z = Array[3]->AsNumber();
    return true;
}

// ========================================
// Export/Import helper functions
// ========================================

static void ProcessTransformDataForStringArray(
    AKeyRippleUnreal* KeyRippleActor, TSharedPtr<FJsonObject> JsonObject,
    const TMap<FString, FStringArray>& Recorders, const FString& CategoryName) {
    TSharedPtr<FJsonObject> CategoryObject = MakeShareable(new FJsonObject);

    for (const auto& RecorderListPair : Recorders) {
        FString ListName = RecorderListPair.Key;
        const FStringArray& RecorderList = RecorderListPair.Value;

        TSharedPtr<FJsonObject> ListObject = MakeShareable(new FJsonObject);

        for (const FString& RecorderName : RecorderList.Strings) {
            const FRecorderTransform* FoundTransform =
                KeyRippleActor->RecorderTransforms.Find(RecorderName);
            if (FoundTransform) {
                TSharedPtr<FJsonObject> RecorderObject =
                    MakeShareable(new FJsonObject);

                RecorderObject->SetArrayField(
                    TEXT("rotation_quaternion"),
                    QuatToJsonArray(FoundTransform->Rotation));
                RecorderObject->SetStringField(TEXT("rotation_mode"),
                                               TEXT("QUATERNION"));
                RecorderObject->SetArrayField(
                    TEXT("location"),
                    VectorToJsonArray(FoundTransform->Location));

                ListObject->SetObjectField(*RecorderName, RecorderObject);

                bool isHandRecorder = RecorderName.Contains(TEXT("H_L")) ||
                                      RecorderName.Contains(TEXT("H_R"));

                if (isHandRecorder) {
                    FString RotationControllerName =
                        RecorderName.Replace(TEXT("_H_"), TEXT("_H_rotation_"));

                    UE_LOG(LogTemp, Log,
                           TEXT("isHandRecorder: %s, changed to %s"),
                           *RecorderName, *RotationControllerName);

                    ListObject->SetObjectField(*RotationControllerName,
                                               RecorderObject);
                }
            }

            CategoryObject->SetObjectField(*ListName, ListObject);
        }
    }

    JsonObject->SetObjectField(*CategoryName, CategoryObject);
}

static void ProcessTransformData(AKeyRippleUnreal* KeyRippleActor,
                                 TSharedPtr<FJsonObject> JsonObject,
                                 const TMap<FString, FString>& SimpleData,
                                 const FString& CategoryName) {
    TSharedPtr<FJsonObject> CategoryObject = MakeShareable(new FJsonObject);

    for (const auto& DataPair : SimpleData) {
        FString Key = DataPair.Key;
        FString RecorderName = DataPair.Value;
        bool isGuildLine = RecorderName.Contains("direction");

        TSharedPtr<FJsonObject> DataObject = MakeShareable(new FJsonObject);
        DataObject->SetStringField(TEXT("name"), RecorderName);

        const FRecorderTransform* FoundTransform =
            KeyRippleActor->RecorderTransforms.Find(RecorderName);
        if (FoundTransform) {
            DataObject->SetArrayField(
                TEXT("location"), VectorToJsonArray(FoundTransform->Location));

            if (isGuildLine) {
                DataObject->SetArrayField(
                    TEXT("rotation_quaternion"),
                    QuatToJsonArray(FoundTransform->Rotation));
                DataObject->SetStringField(TEXT("rotation_mode"),
                                           TEXT("QUATERNION"));
            }
        }

        CategoryObject->SetObjectField(*Key, DataObject);
    }

    JsonObject->SetObjectField(*CategoryName, CategoryObject);
}

static void ProcessImportTransformDataForStringArray(
    AKeyRippleUnreal* KeyRippleActor, TSharedPtr<FJsonObject> JsonObject,
    const FString& CategoryName, int32& ImportedCount, int32& FailedCount) {
    if (!JsonObject->HasField(*CategoryName)) {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Importing %s..."), *CategoryName);
    TSharedPtr<FJsonObject> CategoryObject =
        JsonObject->GetObjectField(*CategoryName);

    for (const auto& RecorderListPair : CategoryObject->Values) {
        TSharedPtr<FJsonObject> RecorderListObject =
            RecorderListPair.Value->AsObject();

        for (const auto& RecorderPair : RecorderListObject->Values) {
            FString RecorderName = RecorderPair.Key;
            TSharedPtr<FJsonObject> RecorderObject =
                RecorderPair.Value->AsObject();

            bool isRotationController = RecorderName.Contains("rotation");
            FString RealRecorderName =
                isRotationController
                    ? RecorderName.Replace(TEXT("_rotation"), TEXT(""))
                    : RecorderName;

            FRecorderTransform* TargetTransform =
                KeyRippleActor->RecorderTransforms.Find(RealRecorderName);
            if (!TargetTransform) {
                FRecorderTransform NewTransform;
                NewTransform.Location = FVector::ZeroVector;
                NewTransform.Rotation = FQuat::Identity;
                TargetTransform = &KeyRippleActor->RecorderTransforms.Add(
                    RealRecorderName, NewTransform);
            }

            if (RecorderObject->HasField(TEXT("rotation_quaternion"))) {
                TArray<TSharedPtr<FJsonValue>> RotationArray =
                    RecorderObject->GetArrayField(TEXT("rotation_quaternion"));
                if (JsonArrayToQuat(RotationArray, TargetTransform->Rotation)) {
                    UE_LOG(LogTemp, Warning,
                           TEXT("Updated ROTATION for '%s': "
                                "(%.2f,%.2f,%.2f,%.2f)"),
                           *RealRecorderName, TargetTransform->Rotation.W,
                           TargetTransform->Rotation.X,
                           TargetTransform->Rotation.Y,
                           TargetTransform->Rotation.Z);
                }
            }
            if (RecorderObject->HasField(TEXT("location"))) {
                TArray<TSharedPtr<FJsonValue>> LocationArray =
                    RecorderObject->GetArrayField(TEXT("location"));
                if (JsonArrayToVector(LocationArray,
                                      TargetTransform->Location)) {
                    UE_LOG(LogTemp, Warning,
                           TEXT("  📂 Updated LOCATION for '%s': "
                                "(%.2f,%.2f,%.2f)"),
                           *RealRecorderName, TargetTransform->Location.X,
                           TargetTransform->Location.Y,
                           TargetTransform->Location.Z);
                }
            }

            ImportedCount++;
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("  ✓ %s imported"), *CategoryName);
}

static void ProcessImportTransformData(AKeyRippleUnreal* KeyRippleActor,
                                       TSharedPtr<FJsonObject> JsonObject,
                                       const FString& CategoryName,
                                       int32& ImportedCount,
                                       int32& FailedCount) {
    if (!JsonObject->HasField(*CategoryName)) {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Importing %s..."), *CategoryName);
    TSharedPtr<FJsonObject> CategoryObject =
        JsonObject->GetObjectField(*CategoryName);

    for (const auto& DataPair : CategoryObject->Values) {
        TSharedPtr<FJsonObject> ItemObject = DataPair.Value->AsObject();

        FString ObjName;
        if (ItemObject.IsValid() && ItemObject->HasField(TEXT("name"))) {
            ObjName = ItemObject->GetStringField(TEXT("name"));
        } else {
            ObjName = DataPair.Key;
        }

        FRecorderTransform RecorderTransform;

        if (ItemObject.IsValid() && ItemObject->HasField(TEXT("location"))) {
            TArray<TSharedPtr<FJsonValue>> LocationArray =
                ItemObject->GetArrayField(TEXT("location"));
            JsonArrayToVector(LocationArray, RecorderTransform.Location);
        }

        if (ItemObject.IsValid() &&
            ItemObject->HasField(TEXT("rotation_quaternion"))) {
            TArray<TSharedPtr<FJsonValue>> RotationArray =
                ItemObject->GetArrayField(TEXT("rotation_quaternion"));
            JsonArrayToQuat(RotationArray, RecorderTransform.Rotation);
        }

        KeyRippleActor->RecorderTransforms.Add(ObjName, RecorderTransform);
        ImportedCount++;
    }
    UE_LOG(LogTemp, Warning, TEXT("  ✓ %s imported"), *CategoryName);
}

static void ProcessImportConfigParameters(AKeyRippleUnreal* KeyRippleActor,
                                          TSharedPtr<FJsonObject> JsonObject,
                                          int32& ImportedCount,
                                          int32& FailedCount) {
    if (!JsonObject->HasField(TEXT("config"))) {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Importing config parameters..."));
    TSharedPtr<FJsonObject> ConfigObject =
        JsonObject->GetObjectField(TEXT("config"));

    KeyRippleActor->OneHandFingerNumber =
        ConfigObject->GetIntegerField(TEXT("one_hand_finger_number"));
    KeyRippleActor->LeftestPosition =
        ConfigObject->GetIntegerField(TEXT("leftest_position"));
    KeyRippleActor->LeftPosition =
        ConfigObject->GetIntegerField(TEXT("left_position"));
    KeyRippleActor->MiddleLeftPosition =
        ConfigObject->GetIntegerField(TEXT("middle_left_position"));
    KeyRippleActor->MiddleRightPosition =
        ConfigObject->GetIntegerField(TEXT("middle_right_position"));
    KeyRippleActor->RightPosition =
        ConfigObject->GetIntegerField(TEXT("right_position"));
    KeyRippleActor->RightestPosition =
        ConfigObject->GetIntegerField(TEXT("rightest_position"));
    KeyRippleActor->MinKey = ConfigObject->GetIntegerField(TEXT("min_key"));
    KeyRippleActor->MaxKey = ConfigObject->GetIntegerField(TEXT("max_key"));
    KeyRippleActor->HandRange =
        ConfigObject->GetIntegerField(TEXT("hand_range"));

    if (ConfigObject->HasField(TEXT("right_hand_original_direction"))) {
        TArray<TSharedPtr<FJsonValue>> RightHandDirArray =
            ConfigObject->GetArrayField(TEXT("right_hand_original_direction"));
        JsonArrayToVector(RightHandDirArray,
                          KeyRippleActor->RightHandOriginalDirection);
    }
    if (ConfigObject->HasField(TEXT("left_hand_original_direction"))) {
        TArray<TSharedPtr<FJsonValue>> LeftHandDirArray =
            ConfigObject->GetArrayField(TEXT("left_hand_original_direction"));
        JsonArrayToVector(LeftHandDirArray,
                          KeyRippleActor->LeftHandOriginalDirection);
    }

    ImportedCount++;
    UE_LOG(LogTemp, Warning, TEXT("  ✓ Config parameters imported"));
}

void AKeyRippleUnreal::ExportRecorderInfo() {
    if (!this) {
        UE_LOG(LogTemp, Error,
               TEXT("ExportRecorderInfo: KeyRippleActor is null"));
        return;
    }

    if (IOFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("IOFilePath is empty, cannot export recorder info"));
        return;
    }

    FString OutputFilePath = FString::Printf(TEXT("%s"), *IOFilePath);
    UE_LOG(LogTemp, Warning, TEXT("Exporting to file: %s"), *OutputFilePath);

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    UE_LOG(LogTemp, Warning, TEXT("Exporting config parameters..."));
    TSharedPtr<FJsonObject> ConfigObject = MakeShareable(new FJsonObject);
    ConfigObject->SetNumberField(TEXT("one_hand_finger_number"),
                                 OneHandFingerNumber);
    ConfigObject->SetNumberField(TEXT("leftest_position"), LeftestPosition);
    ConfigObject->SetNumberField(TEXT("left_position"), LeftPosition);
    ConfigObject->SetNumberField(TEXT("middle_left_position"),
                                 MiddleLeftPosition);
    ConfigObject->SetNumberField(TEXT("middle_right_position"),
                                 MiddleRightPosition);
    ConfigObject->SetNumberField(TEXT("right_position"), RightPosition);
    ConfigObject->SetNumberField(TEXT("rightest_position"), RightestPosition);
    ConfigObject->SetNumberField(TEXT("min_key"), MinKey);
    ConfigObject->SetNumberField(TEXT("max_key"), MaxKey);
    ConfigObject->SetNumberField(TEXT("hand_range"), HandRange);
    ConfigObject->SetArrayField(TEXT("right_hand_original_direction"),
                                VectorToJsonArray(RightHandOriginalDirection));
    ConfigObject->SetArrayField(TEXT("left_hand_original_direction"),
                                VectorToJsonArray(LeftHandOriginalDirection));
    JsonObject->SetObjectField(TEXT("config"), ConfigObject);

    ProcessTransformDataForStringArray(this, JsonObject, FingerRecorders,
                                       TEXT("finger_recorders"));
    ProcessTransformDataForStringArray(this, JsonObject, HandRecorders,
                                       TEXT("hand_recorders"));
    ProcessTransformDataForStringArray(this, JsonObject, ShoulderRecorders,
                                       TEXT("shoulder_recorders"));
    ProcessTransformDataForStringArray(this, JsonObject, TargetPointsRecorders,
                                       TEXT("target_points_recorders"));
    ProcessTransformData(this, JsonObject, KeyBoardPositions,
                         TEXT("key_board_positions"));
    ProcessTransformData(this, JsonObject, Guidelines, TEXT("guidelines"));

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    if (FFileHelper::SaveStringToFile(OutputString, *OutputFilePath)) {
        UE_LOG(LogTemp, Warning,
               TEXT("Recorder info successfully exported to %s"),
               *OutputFilePath);
    } else {
        UE_LOG(LogTemp, Error, TEXT("Failed to save recorder info to %s"),
               *OutputFilePath);
    }
}

bool AKeyRippleUnreal::ImportRecorderInfo() {
    if (!this) {
        UE_LOG(LogTemp, Error,
               TEXT("ImportRecorderInfo: KeyRippleActor is null"));
        return false;
    }

    if (IOFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("IOFilePath is empty, cannot import recorder info"));
        return false;
    }

    FString InputFilePath = IOFilePath;
    UE_LOG(LogTemp, Warning, TEXT("Importing from file: %s"), *InputFilePath);

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *InputFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *InputFilePath);
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) ||
        !JsonObject.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON from file: %s"),
               *InputFilePath);
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== ImportRecorderInfo Started =========="));

    RecorderTransforms.Empty();

    int32 ImportedCount = 0;
    int32 FailedCount = 0;

    ProcessImportConfigParameters(this, JsonObject, ImportedCount, FailedCount);

    ProcessImportTransformDataForStringArray(
        this, JsonObject, TEXT("finger_recorders"), ImportedCount, FailedCount);

    ProcessImportTransformDataForStringArray(
        this, JsonObject, TEXT("hand_recorders"), ImportedCount, FailedCount);

    ProcessImportTransformDataForStringArray(this, JsonObject,
                                             TEXT("shoulder_recorders"),
                                             ImportedCount, FailedCount);

    ProcessImportTransformDataForStringArray(this, JsonObject,
                                             TEXT("target_points_recorders"),
                                             ImportedCount, FailedCount);

    ProcessImportTransformData(this, JsonObject, TEXT("key_board_positions"),
                               ImportedCount, FailedCount);

    ProcessImportTransformData(this, JsonObject, TEXT("guidelines"),
                               ImportedCount, FailedCount);

    UE_LOG(LogTemp, Warning,
           TEXT("========== ImportRecorderInfo Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully imported: %d transforms"),
           ImportedCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed to import: %d transforms"),
           FailedCount);
    UE_LOG(LogTemp, Warning, TEXT("Total RecorderTransforms entries: %d"),
           RecorderTransforms.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== ImportRecorderInfo Completed =========="));

    if (this) {
        MarkPackageDirty();
        UE_LOG(LogTemp, Warning,
               TEXT("Marked KeyRippleActor package as dirty for saving"));
    }

    return ImportedCount > 0;
}
