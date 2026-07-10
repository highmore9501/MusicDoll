#include "FretDanceUnreal.h"

#include "Animation/SkeletalMeshActor.h"
#include "Components/SceneComponent.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

struct FFretDanceHelpers {
    // 辅助方法：从 JSON 数组读取 FVector（位置）
    static bool ReadLocationFromArray(
        const TArray<TSharedPtr<FJsonValue>>& LocationArray,
        FVector& OutLocation) {
        if (LocationArray.Num() != 3) {
            return false;
        }
        OutLocation.X = LocationArray[0]->AsNumber();
        OutLocation.Y = LocationArray[1]->AsNumber();
        OutLocation.Z = LocationArray[2]->AsNumber();
        return true;
    }

    // 辅助方法：从 JSON 数组读取 FQuat（旋转）
    static bool ReadRotationFromArray(
        const TArray<TSharedPtr<FJsonValue>>& RotationArray,
        FQuat& OutRotation) {
        if (RotationArray.Num() != 4) {
            return false;
        }
        OutRotation.W = RotationArray[0]->AsNumber();
        OutRotation.X = RotationArray[1]->AsNumber();
        OutRotation.Y = RotationArray[2]->AsNumber();
        OutRotation.Z = RotationArray[3]->AsNumber();
        return true;
    }

    // 辅助方法：从 JSON 对象读取完整的 Transform（位置 + 旋转）
    static FFretDanceRecorderTransform ReadRecorderTransform(
        TSharedPtr<FJsonObject> RecorderObj) {
        FFretDanceRecorderTransform Transform;
        Transform.Location = FVector::ZeroVector;
        Transform.Rotation = FQuat::Identity;

        if (RecorderObj->HasField(TEXT("location"))) {
            TArray<TSharedPtr<FJsonValue>> LocationArray =
                RecorderObj->GetArrayField(TEXT("location"));
            ReadLocationFromArray(LocationArray, Transform.Location);
        }

        if (RecorderObj->HasField(TEXT("rotation_quaternion"))) {
            TArray<TSharedPtr<FJsonValue>> RotationArray =
                RecorderObj->GetArrayField(TEXT("rotation_quaternion"));
            ReadRotationFromArray(RotationArray, Transform.Rotation);
        }

        return Transform;
    }

    // 辅助方法：创建包含位置和旋转的 JSON 对象
    static TSharedPtr<FJsonObject> CreateRecorderJsonObject(
        const FFretDanceRecorderTransform& Transform,
        bool bIncludeLocation = true, bool bIncludeRotation = true) {
        TSharedPtr<FJsonObject> RecorderObj = MakeShareable(new FJsonObject);

        if (bIncludeLocation) {
            // 位置
            TArray<TSharedPtr<FJsonValue>> LocationArray;
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform.Location.X)));
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform.Location.Y)));
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform.Location.Z)));
            RecorderObj->SetArrayField(TEXT("location"), LocationArray);
        }

        if (bIncludeRotation) {
            // 旋转
            TArray<TSharedPtr<FJsonValue>> RotationArray;
            RotationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform.Rotation.W)));
            RotationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform.Rotation.X)));
            RotationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform.Rotation.Y)));
            RotationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform.Rotation.Z)));
            RecorderObj->SetArrayField(TEXT("rotation_quaternion"),
                                       RotationArray);
        }

        return RecorderObj;
    }
};

// Sets default values
AFretDanceUnreal::AFretDanceUnreal() {
    PrimaryActorTick.bCanEverTick = true;

    // 默认配置
    InstrumentType = EFretDanceInstrumentType::FINGER_STYLE_GUITAR;
    StringNumber = 6;
    CurrentBasePosition = EFretDanceBasePosition::P0;

    // 初始化所有控制器和记录器
    InitializeControllersAndRecorders();

    InitializeRecorderTransforms();
}

// Called when the game starts or when spawned
void AFretDanceUnreal::BeginPlay() {
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("FretDanceUnreal: BeginPlay called"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AFretDanceUnreal::Tick(float DeltaTime) { Super::Tick(DeltaTime); }

void AFretDanceUnreal::InitializeControllersAndRecorders() {
    UE_LOG(LogTemp, Warning,
           TEXT("FretDanceUnreal: Initializing controllers and recorders"));

    // 初始化无效组合表
    InvalidCombinations.Empty();

    FFretDanceInvalidLeftHandCombinations P0Invalid;
    P0Invalid.AddInvalidState(EFretDanceLeftHandState::INNER);
    InvalidCombinations.Add(EFretDanceBasePosition::P0, P0Invalid);

    FFretDanceInvalidLeftHandCombinations P2Invalid;
    P2Invalid.AddInvalidState(EFretDanceLeftHandState::INNER);
    InvalidCombinations.Add(EFretDanceBasePosition::P2, P2Invalid);

    FFretDanceInvalidLeftHandCombinations P1Invalid;
    P1Invalid.AddInvalidState(EFretDanceLeftHandState::OUTER);
    InvalidCombinations.Add(EFretDanceBasePosition::P1, P1Invalid);

    FFretDanceInvalidLeftHandCombinations P3Invalid;
    P3Invalid.AddInvalidState(EFretDanceLeftHandState::OUTER);
    InvalidCombinations.Add(EFretDanceBasePosition::P3, P3Invalid);

    // P4 只支持 NORMAL 状态，其他状态都无效
    FFretDanceInvalidLeftHandCombinations P4Invalid;
    P4Invalid.AddInvalidState(EFretDanceLeftHandState::OUTER);
    P4Invalid.AddInvalidState(EFretDanceLeftHandState::INNER);
    P4Invalid.AddInvalidState(EFretDanceLeftHandState::BARRE);
    InvalidCombinations.Add(EFretDanceBasePosition::P4, P4Invalid);

    // 初始化左手控制器映射
    LeftHandControllers.Empty();
    LeftHandControllers.Add("left_hand_controller", "H_L");
    LeftHandControllers.Add("left_hand_ik_pivot_controller", "HP_L");
    LeftHandControllers.Add("left_thumb_controller", "T_L");
    LeftHandControllers.Add("left_thumb_ik_pivot_controller", "TP_L");

    // 初始化右手控制器映射
    RightHandControllers.Empty();
    RightHandControllers.Add("right_hand_controller", "H_R");
    RightHandControllers.Add("right_hand_ik_pivot_controller", "HP_R");

    // 初始化左手手指控制器
    LeftFingerControllers.Empty();
    LeftFingerControllers.Add("left_index_controller", "I_L");
    LeftFingerControllers.Add("left_middle_controller", "M_L");
    LeftFingerControllers.Add("left_ring_controller", "R_L");
    LeftFingerControllers.Add("left_little_controller", "P_L");

    // 初始化右手手指控制器（根据乐器类型）
    RightFingerControllers = GetRightFingerControllersForInstrumentType();

    // 初始化左手记录器
    LeftHandPositionRecorders.Empty();
    for (int32 PosIdx = 0; PosIdx <= 4; PosIdx++) {
        EFretDanceBasePosition Position =
            static_cast<EFretDanceBasePosition>(PosIdx);
        for (int32 StateIdx = 0; StateIdx <= 3; StateIdx++) {
            EFretDanceLeftHandState State =
                static_cast<EFretDanceLeftHandState>(StateIdx);

            // 跳过无效组合
            if (!IsValidLeftHandCombination(Position, State)) {
                continue;
            }

            // 为每个有效的组合创建记录器
            for (const auto& ControllerPair : LeftHandControllers) {
                FString RecorderKey = GetLeftHandRecorderName(
                    Position, State, ControllerPair.Value);
                if (!RecorderKey.IsEmpty()) {
                    FFretDanceStringArray RecorderArray;
                    RecorderArray.Add(RecorderKey);
                    LeftHandPositionRecorders.Add(RecorderKey, RecorderArray);
                }
            }

            for (const auto& FingerPair : LeftFingerControllers) {
                FString RecorderKey =
                    GetLeftHandRecorderName(Position, State, FingerPair.Value);
                if (!RecorderKey.IsEmpty()) {
                    FFretDanceStringArray RecorderArray;
                    RecorderArray.Add(RecorderKey);
                    LeftHandPositionRecorders.Add(RecorderKey, RecorderArray);
                }
            }
        }
    }

    // 初始化右手记录器（通过统一入口生成）
    RightHandPositionRecorders.Empty();
    {
        TArray<TPair<FString, FString>> RightHandEntries;
        GenerateRightHandRecorderEntries(RightHandEntries);
        for (const auto& Entry : RightHandEntries) {
            FFretDanceStringArray Array;
            Array.Add(Entry.Value);
            RightHandPositionRecorders.Add(Entry.Key, Array);
        }
    }

    // 初始化指板位置记录器
    GuitarFretPositions.Empty();
    GuitarFretPositions.Add("P0", "P0");
    GuitarFretPositions.Add("P1", "P1");
    GuitarFretPositions.Add("P2", "P2");
    GuitarFretPositions.Add("P3", "P3");
    GuitarFretPositions.Add("P4", "P4");

    // 初始化RecorderTransforms
    RecorderTransforms.Empty();

    UE_LOG(LogTemp, Warning,
           TEXT("FretDanceUnreal: Controllers and recorders initialization "
                "completed"));
    UE_LOG(LogTemp, Warning, TEXT("Left Hand Recorders: %d"),
           LeftHandPositionRecorders.Num());
    UE_LOG(LogTemp, Warning, TEXT("Right Hand Recorders: %d"),
           RightHandPositionRecorders.Num());
}

TMap<FString, FString>
AFretDanceUnreal::GetRightFingerControllersForInstrumentType() const {
    TMap<FString, FString> Result;

    // 所有乐器类型都包含完整的右手手指控制器
    Result.Add("right_thumb_controller", "T_R");
    Result.Add("right_thumb_ik_pivot_controller", "TP_R");
    Result.Add("right_index_controller", "I_R");
    Result.Add("right_middle_controller", "M_R");
    Result.Add("right_ring_controller", "R_R");
    Result.Add("right_little_controller", "P_R");

    return Result;
}

FString AFretDanceUnreal::GetLeftHandRecorderName(
    EFretDanceBasePosition Position, EFretDanceLeftHandState State,
    const FString& ControllerName) const {
    // 检查无效组合
    if (!IsValidLeftHandCombination(Position, State)) {
        return FString();
    }

    // 构造记录器名称
    FString PositionStr;
    switch (Position) {
        case EFretDanceBasePosition::P0:
            PositionStr = "P0";
            break;
        case EFretDanceBasePosition::P1:
            PositionStr = "P1";
            break;
        case EFretDanceBasePosition::P2:
            PositionStr = "P2";
            break;
        case EFretDanceBasePosition::P3:
            PositionStr = "P3";
            break;
        case EFretDanceBasePosition::P4:
            PositionStr = "P4";
            break;
        default:
            return FString();
    }

    FString StateStr;
    switch (State) {
        case EFretDanceLeftHandState::NORMAL:
            StateStr = "Normal";
            break;
        case EFretDanceLeftHandState::OUTER:
            StateStr = "Outer";
            break;
        case EFretDanceLeftHandState::INNER:
            StateStr = "Inner";
            break;
        case EFretDanceLeftHandState::BARRE:
            StateStr = "Barre";
            break;
        default:
            return FString();
    }

    return FString::Printf(TEXT("%s_%s_%s"), *StateStr, *PositionStr,
                           *ControllerName);
}

FString AFretDanceUnreal::GetRightHandRecorderName(
    EFretDanceRightHandState State, const FString& FingerKey) const {
    FString StateStr;
    switch (State) {
        case EFretDanceRightHandState::LOW:
            StateStr = "low";
            break;
        case EFretDanceRightHandState::END:
            StateStr = "end";
            break;
        case EFretDanceRightHandState::HIGH:
            StateStr = "high";
            break;
        case EFretDanceRightHandState::RELEASE:
            StateStr = "release";
            break;
        case EFretDanceRightHandState::UP:
            StateStr = "up";
            break;
        case EFretDanceRightHandState::DOWN:
            StateStr = "down";
            break;
        default:
            return FString();
    }

    return FString::Printf(TEXT("%s%s"), *FingerKey, *StateStr);
}

bool AFretDanceUnreal::IsValidLeftHandCombination(
    EFretDanceBasePosition Position, EFretDanceLeftHandState State) const {
    if (InvalidCombinations.Contains(Position)) {
        const FFretDanceInvalidLeftHandCombinations& InvalidCombo =
            InvalidCombinations[Position];
        return !InvalidCombo.Contains(State);
    }
    return true;
}

void AFretDanceUnreal::ExportRecorderInfo(const FString& FilePath) {
    if (FilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("ExportRecorderInfo: FilePath is empty"));
        return;
    }

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    // 创建分类对象（按照 Python 版本的 JSON 结构）
    TMap<FString, TSharedPtr<FJsonObject>> CategoryObjects;
    TArray<FString> CategoryNames = {
        TEXT("NORMAL_LEFT_HAND_POSITIONS"), TEXT("OUTER_LEFT_HAND_POSITIONS"),
        TEXT("INNER_LEFT_HAND_POSITIONS"),  TEXT("BARRE_LEFT_HAND_POSITIONS"),
        TEXT("LEFT_FINGER_POSITIONS"),      TEXT("RIGHT_HAND_POSITIONS"),
        TEXT("RIGHT_HAND_LINES"),           TEXT("OTHER_SETTING")};

    for (const FString& CategoryName : CategoryNames) {
        CategoryObjects.Add(CategoryName, MakeShareable(new FJsonObject));
    }

    int32 TotalExported = 0;

    // === 导出左手位置（按状态分类） ===
    TArray<TPair<FString, EFretDanceLeftHandState>> LeftHandStates;
    LeftHandStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("NORMAL"), EFretDanceLeftHandState::NORMAL));
    LeftHandStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("OUTER"), EFretDanceLeftHandState::OUTER));
    LeftHandStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("INNER"), EFretDanceLeftHandState::INNER));
    LeftHandStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("BARRE"), EFretDanceLeftHandState::BARRE));

    for (const auto& StatePair : LeftHandStates) {
        const FString& StateName = StatePair.Key;
        EFretDanceLeftHandState State = StatePair.Value;

        TSharedPtr<FJsonObject> StateCategoryObj = nullptr;
        if (StateName == TEXT("NORMAL")) {
            StateCategoryObj =
                CategoryObjects[TEXT("NORMAL_LEFT_HAND_POSITIONS")];
        } else if (StateName == TEXT("OUTER")) {
            StateCategoryObj =
                CategoryObjects[TEXT("OUTER_LEFT_HAND_POSITIONS")];
        } else if (StateName == TEXT("INNER")) {
            StateCategoryObj =
                CategoryObjects[TEXT("INNER_LEFT_HAND_POSITIONS")];
        } else if (StateName == TEXT("BARRE")) {
            StateCategoryObj =
                CategoryObjects[TEXT("BARRE_LEFT_HAND_POSITIONS")];
        }

        if (!StateCategoryObj) continue;

        // 遍历所有位置
        for (int32 PosIdx = 0; PosIdx <= 4; PosIdx++) {
            EFretDanceBasePosition Position =
                static_cast<EFretDanceBasePosition>(PosIdx);

            // 跳过无效组合
            if (!IsValidLeftHandCombination(Position, State)) {
                continue;
            }

            FString PositionStr = FString::Printf(TEXT("P%d"), PosIdx);
            TSharedPtr<FJsonObject> PositionObj =
                MakeShareable(new FJsonObject);

            // 导出左手控制器 - 输出复合对象 {position: [x,y,z], rotation:
            // [w,x,y,z]}
            for (const auto& ControllerPair : LeftHandControllers) {
                const FString& ControllerName = ControllerPair.Value;
                FString RecorderKey =
                    GetLeftHandRecorderName(Position, State, ControllerName);

                if (const FFretDanceRecorderTransform* Transform =
                        RecorderTransforms.Find(RecorderKey)) {
                    TSharedPtr<FJsonObject> ControlObj =
                        MakeShareable(new FJsonObject);

                    TArray<TSharedPtr<FJsonValue>> LocationArray;
                    LocationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Location.X)));
                    LocationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Location.Y)));
                    LocationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Location.Z)));
                    ControlObj->SetArrayField(TEXT("position"), LocationArray);

                    TArray<TSharedPtr<FJsonValue>> RotationArray;
                    RotationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Rotation.W)));
                    RotationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Rotation.X)));
                    RotationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Rotation.Y)));
                    RotationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Rotation.Z)));
                    ControlObj->SetArrayField(TEXT("rotation"), RotationArray);

                    PositionObj->SetObjectField(*ControllerName, ControlObj);
                    TotalExported++;
                }
            }

            // 导出左手手指控制器 - 输出复合对象 {position: [x,y,z], rotation:
            // [w,x,y,z]}
            for (const auto& FingerPair : LeftFingerControllers) {
                const FString& ControllerName = FingerPair.Value;
                FString RecorderKey =
                    GetLeftHandRecorderName(Position, State, ControllerName);

                if (const FFretDanceRecorderTransform* Transform =
                        RecorderTransforms.Find(RecorderKey)) {
                    TSharedPtr<FJsonObject> ControlObj =
                        MakeShareable(new FJsonObject);

                    TArray<TSharedPtr<FJsonValue>> LocationArray;
                    LocationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Location.X)));
                    LocationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Location.Y)));
                    LocationArray.Add(MakeShareable(
                        new FJsonValueNumber(Transform->Location.Z)));
                    ControlObj->SetArrayField(TEXT("position"), LocationArray);
                    ControlObj->SetArrayField(
                        TEXT("rotation"),
                        {MakeShareable(new FJsonValueNumber(1.0)),
                         MakeShareable(new FJsonValueNumber(0.0)),
                         MakeShareable(new FJsonValueNumber(0.0)),
                         MakeShareable(new FJsonValueNumber(0.0))});

                    PositionObj->SetObjectField(*ControllerName, ControlObj);
                    TotalExported++;
                }
            }

            if (PositionObj->Values.Num() > 0) {
                StateCategoryObj->SetObjectField(*PositionStr, PositionObj);
            }
        }
    }

    // === 导出指板位置 ===
    TSharedPtr<FJsonObject> LeftFingerPositionsObj =
        CategoryObjects[TEXT("LEFT_FINGER_POSITIONS")];
    for (const auto& FretPair : GuitarFretPositions) {
        const FString& PositionKey = FretPair.Key;
        const FString& RecorderName = FretPair.Value;

        const FFretDanceRecorderTransform* Transform =
            RecorderTransforms.Find(RecorderName);
        if (Transform) {
            TArray<TSharedPtr<FJsonValue>> LocationArray;
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform->Location.X)));
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform->Location.Y)));
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform->Location.Z)));
            LeftFingerPositionsObj->SetArrayField(*PositionKey, LocationArray);
            TotalExported++;
        }
    }

    // === 导出右手位置 - 输出复合对象 {position: [x,y,z], rotation: [w,x,y,z]}
    // ===
    TSharedPtr<FJsonObject> RightHandPositionsObj =
        CategoryObjects[TEXT("RIGHT_HAND_POSITIONS")];

    // 直接枚举所有6个状态和所有手指，不依赖 RightHandPositionRecorders
    // 这样即使热重载后记录器未重建，也能输出正确的新格式键名
    TArray<FString> AllRightHandStates = {TEXT("low"),  TEXT("end"),
                                          TEXT("high"), TEXT("release"),
                                          TEXT("up"),   TEXT("down")};
    TArray<FString> RightHandFingerKeys = {TEXT("p"), TEXT("tp"), TEXT("i"),
                                           TEXT("m"), TEXT("a"),  TEXT("ch")};

    for (const FString& State : AllRightHandStates) {
        bool bIsVibrato = (State == TEXT("release") || State == TEXT("up") ||
                           State == TEXT("down"));
        // 颤音三态只在启用摇杆时导出
        if (bIsVibrato && !bUseVibratoBar) continue;

        // ---- 手掌 H_R ----
        FString HRName;
        if (bIsVibrato) {
            FString Capitalized = State.Left(1).ToUpper() + State.RightChop(1);
            HRName = FString::Printf(TEXT("Vibrato_%s_H_R"), *Capitalized);
        } else {
            HRName = FString::Printf(TEXT("Normal_P%s_H_R"), *State);
        }
        if (const FFretDanceRecorderTransform* Transform =
                RecorderTransforms.Find(HRName)) {
            TSharedPtr<FJsonObject> ControlObj = MakeShareable(new FJsonObject);
            TArray<TSharedPtr<FJsonValue>> LocArr = {
                MakeShareable(new FJsonValueNumber(Transform->Location.X)),
                MakeShareable(new FJsonValueNumber(Transform->Location.Y)),
                MakeShareable(new FJsonValueNumber(Transform->Location.Z)),
            };
            ControlObj->SetArrayField(TEXT("position"), LocArr);
            TArray<TSharedPtr<FJsonValue>> RotArr = {
                MakeShareable(new FJsonValueNumber(Transform->Rotation.W)),
                MakeShareable(new FJsonValueNumber(Transform->Rotation.X)),
                MakeShareable(new FJsonValueNumber(Transform->Rotation.Y)),
                MakeShareable(new FJsonValueNumber(Transform->Rotation.Z)),
            };
            ControlObj->SetArrayField(TEXT("rotation"), RotArr);
            RightHandPositionsObj->SetObjectField(*HRName, ControlObj);
            TotalExported++;
        }

        // ---- 手掌 HP_R ----
        FString HPRName;
        if (bIsVibrato) {
            FString Capitalized = State.Left(1).ToUpper() + State.RightChop(1);
            HPRName = FString::Printf(TEXT("Vibrato_%s_HP_R"), *Capitalized);
        } else {
            HPRName = FString::Printf(TEXT("Normal_P%s_HP_R"), *State);
        }
        if (const FFretDanceRecorderTransform* Transform =
                RecorderTransforms.Find(HPRName)) {
            TSharedPtr<FJsonObject> ControlObj = MakeShareable(new FJsonObject);
            TArray<TSharedPtr<FJsonValue>> LocArr = {
                MakeShareable(new FJsonValueNumber(Transform->Location.X)),
                MakeShareable(new FJsonValueNumber(Transform->Location.Y)),
                MakeShareable(new FJsonValueNumber(Transform->Location.Z)),
            };
            ControlObj->SetArrayField(TEXT("position"), LocArr);
            TArray<TSharedPtr<FJsonValue>> RotArr = {
                MakeShareable(new FJsonValueNumber(Transform->Rotation.W)),
                MakeShareable(new FJsonValueNumber(Transform->Rotation.X)),
                MakeShareable(new FJsonValueNumber(Transform->Rotation.Y)),
                MakeShareable(new FJsonValueNumber(Transform->Rotation.Z)),
            };
            ControlObj->SetArrayField(TEXT("rotation"), RotArr);
            RightHandPositionsObj->SetObjectField(*HPRName, ControlObj);
            TotalExported++;
        }

        // ---- 手指控制器 ----
        for (const FString& Finger : RightHandFingerKeys) {
            FString FingerName = Finger + State;
            if (const FFretDanceRecorderTransform* Transform =
                    RecorderTransforms.Find(FingerName)) {
                TSharedPtr<FJsonObject> ControlObj =
                    MakeShareable(new FJsonObject);
                TArray<TSharedPtr<FJsonValue>> LocArr = {
                    MakeShareable(new FJsonValueNumber(Transform->Location.X)),
                    MakeShareable(new FJsonValueNumber(Transform->Location.Y)),
                    MakeShareable(new FJsonValueNumber(Transform->Location.Z)),
                };
                ControlObj->SetArrayField(TEXT("position"), LocArr);
                TArray<TSharedPtr<FJsonValue>> RotArr = {
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.W)),
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.X)),
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.Y)),
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.Z)),
                };
                ControlObj->SetArrayField(TEXT("rotation"), RotArr);
                RightHandPositionsObj->SetObjectField(*FingerName, ControlObj);
                TotalExported++;
            }
        }
    }

    // 添加unreal标志信息
    TSharedPtr<FJsonObject> OtherSettingObj =
        CategoryObjects[TEXT("OTHER_SETTING")];
    OtherSettingObj->SetBoolField(TEXT("is_unreal"), true);
    OtherSettingObj->SetBoolField(TEXT("use_vibrato_bar"), bUseVibratoBar);

    // 将所有分类添加到主 JSON 对象
    for (const auto& CategoryPair : CategoryObjects) {
        if (CategoryPair.Value->Values.Num() > 0) {
            JsonObject->SetObjectField(*CategoryPair.Key, CategoryPair.Value);
        }
    }

    // 序列化为字符串并写入文件
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    if (FFileHelper::SaveStringToFile(OutputString, *FilePath)) {
        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "ExportRecorderInfo: Successfully exported %d recorders to %s"),
            TotalExported, *FilePath);
    } else {
        UE_LOG(LogTemp, Error, TEXT("ExportRecorderInfo: Failed to save to %s"),
               *FilePath);
    }
}

bool AFretDanceUnreal::ImportRecorderInfo(const FString& FilePath) {
    if (FilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("ImportRecorderInfo: FilePath is empty"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== ImportRecorderInfo Started =========="));
    UE_LOG(LogTemp, Warning, TEXT("Importing from file: %s"), *FilePath);

    // 读取文件
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("ImportRecorderInfo: Failed to load file %s"), *FilePath);
        return false;
    }

    // 解析 JSON
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

    int32 ImportedCount = 0;

    // === 导入左手位置（按状态分类） ===
    TArray<TPair<FString, EFretDanceLeftHandState>> LeftHandStates;
    LeftHandStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("NORMAL"), EFretDanceLeftHandState::NORMAL));
    LeftHandStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("OUTER"), EFretDanceLeftHandState::OUTER));
    LeftHandStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("INNER"), EFretDanceLeftHandState::INNER));
    LeftHandStates.Add(TPair<FString, EFretDanceLeftHandState>(
        TEXT("BARRE"), EFretDanceLeftHandState::BARRE));

    for (const auto& StatePair : LeftHandStates) {
        const FString& StateName = StatePair.Key;
        EFretDanceLeftHandState State = StatePair.Value;

        FString CategoryName = StateName + TEXT("_LEFT_HAND_POSITIONS");
        if (!JsonObject->HasField(*CategoryName)) {
            continue;
        }

        TSharedPtr<FJsonObject> StateCategoryObj =
            JsonObject->GetObjectField(*CategoryName);

        // 遍历所有位置
        for (int32 PosIdx = 0; PosIdx <= 4; PosIdx++) {
            EFretDanceBasePosition Position =
                static_cast<EFretDanceBasePosition>(PosIdx);

            if (!IsValidLeftHandCombination(Position, State)) {
                continue;
            }

            FString PositionStr = FString::Printf(TEXT("P%d"), PosIdx);
            if (!StateCategoryObj->HasField(*PositionStr)) {
                continue;
            }

            TSharedPtr<FJsonObject> PositionObj =
                StateCategoryObj->GetObjectField(*PositionStr);

            // 导入左手控制器 - 从复合对象读取位置和旋转
            for (const auto& ControllerPair : LeftHandControllers) {
                const FString& ControllerName = ControllerPair.Value;
                if (!PositionObj->HasField(*ControllerName)) {
                    continue;
                }

                FString RecorderKey =
                    GetLeftHandRecorderName(Position, State, ControllerName);

                TSharedPtr<FJsonObject> ControlObj =
                    PositionObj->GetObjectField(*ControllerName);

                if (ControlObj.IsValid()) {
                    FFretDanceRecorderTransform Transform;
                    Transform.Rotation = FQuat::Identity;

                    // 读取位置
                    if (ControlObj->HasField(TEXT("position"))) {
                        TArray<TSharedPtr<FJsonValue>> LocArray =
                            ControlObj->GetArrayField(TEXT("position"));
                        FFretDanceHelpers::ReadLocationFromArray(
                            LocArray, Transform.Location);
                    }

                    // 读取旋转
                    if (ControlObj->HasField(TEXT("rotation"))) {
                        TArray<TSharedPtr<FJsonValue>> RotArray =
                            ControlObj->GetArrayField(TEXT("rotation"));
                        FFretDanceHelpers::ReadRotationFromArray(
                            RotArray, Transform.Rotation);
                    }

                    RecorderTransforms.Add(RecorderKey, Transform);
                    ImportedCount++;

                    UE_LOG(LogTemp, Log,
                           TEXT("  [L-Hand] Read '%s' from %s.%s → "
                                "RecorderTransforms['%s'] | Loc: (%.3f, "
                                "%.3f, %.3f) Rot: (%.3f, %.3f, %.3f, %.3f)"),
                           *ControllerName, *CategoryName, *PositionStr,
                           *RecorderKey, Transform.Location.X,
                           Transform.Location.Y, Transform.Location.Z,
                           Transform.Rotation.W, Transform.Rotation.X,
                           Transform.Rotation.Y, Transform.Rotation.Z);
                }
            }

            // 导入左手手指控制器 - 从复合对象读取位置
            for (const auto& FingerPair : LeftFingerControllers) {
                const FString& ControllerName = FingerPair.Value;
                if (!PositionObj->HasField(*ControllerName)) {
                    continue;
                }

                FString RecorderKey =
                    GetLeftHandRecorderName(Position, State, ControllerName);

                TSharedPtr<FJsonObject> ControlObj =
                    PositionObj->GetObjectField(*ControllerName);

                if (ControlObj.IsValid()) {
                    FFretDanceRecorderTransform Transform;
                    Transform.Rotation = FQuat::Identity;

                    if (ControlObj->HasField(TEXT("position"))) {
                        TArray<TSharedPtr<FJsonValue>> LocArray =
                            ControlObj->GetArrayField(TEXT("position"));
                        FFretDanceHelpers::ReadLocationFromArray(
                            LocArray, Transform.Location);
                    }

                    RecorderTransforms.Add(RecorderKey, Transform);
                    ImportedCount++;

                    UE_LOG(LogTemp, Log,
                           TEXT("  [L-Finger] Read '%s' from %s.%s → "
                                "RecorderTransforms['%s'] | Loc: (%.3f, "
                                "%.3f, %.3f)"),
                           *ControllerName, *CategoryName, *PositionStr,
                           *RecorderKey, Transform.Location.X,
                           Transform.Location.Y, Transform.Location.Z);
                }
            }
        }
    }

    // === 提前读取 use_vibrato_bar，确保右手记录器包含颤音摇杆状态 ===
    // 必须在 RIGHT_HAND_POSITIONS 导入之前完成，否则 vibrato 数据会被跳过
    if (JsonObject->HasField(TEXT("OTHER_SETTING"))) {
        TSharedPtr<FJsonObject> OtherSettingEarly =
            JsonObject->GetObjectField(TEXT("OTHER_SETTING"));
        if (OtherSettingEarly->HasField(TEXT("use_vibrato_bar"))) {
            bool bImportedVibrato =
                OtherSettingEarly->GetBoolField(TEXT("use_vibrato_bar"));
            if (bImportedVibrato && !bUseVibratoBar) {
                bUseVibratoBar = true;
                UE_LOG(LogTemp, Warning,
                       TEXT("  [Import] Early set use_vibrato_bar = true"));
            }
        }
    }

    // === 导入右手位置 ===
    if (JsonObject->HasField(TEXT("RIGHT_HAND_POSITIONS"))) {
        TSharedPtr<FJsonObject> RightHandPositionsObj =
            JsonObject->GetObjectField(TEXT("RIGHT_HAND_POSITIONS"));

        // 直接遍历 JSON 中的所有字段名，不依赖 RightHandPositionRecorders
        // 这样无论 JSON 中的键名是旧格式还是新格式，都能正确导入
        for (const auto& JsonEntry : RightHandPositionsObj->Values) {
            const FString& JsonKeyName = JsonEntry.Key;
            TSharedPtr<FJsonObject> ControlObj =
                RightHandPositionsObj->GetObjectField(*JsonKeyName);

            if (ControlObj.IsValid()) {
                FFretDanceRecorderTransform Transform;
                Transform.Rotation = FQuat::Identity;

                if (ControlObj->HasField(TEXT("position"))) {
                    TArray<TSharedPtr<FJsonValue>> LocArray =
                        ControlObj->GetArrayField(TEXT("position"));
                    FFretDanceHelpers::ReadLocationFromArray(
                        LocArray, Transform.Location);
                }

                if (ControlObj->HasField(TEXT("rotation"))) {
                    TArray<TSharedPtr<FJsonValue>> RotArray =
                        ControlObj->GetArrayField(TEXT("rotation"));
                    FFretDanceHelpers::ReadRotationFromArray(
                        RotArray, Transform.Rotation);
                }

                // 直接使用 JSON 中的键名存储，与内部键名统一
                RecorderTransforms.Add(JsonKeyName, Transform);
                ImportedCount++;

                UE_LOG(LogTemp, Log,
                       TEXT("  [R-Hand] Read '%s' from RIGHT_HAND_POSITIONS → "
                            "Write to RecorderTransforms['%s'] | Loc: (%.3f, "
                            "%.3f, %.3f) Rot: (%.3f, %.3f, %.3f, %.3f)"),
                       *JsonKeyName, *JsonKeyName, Transform.Location.X,
                       Transform.Location.Y, Transform.Location.Z,
                       Transform.Rotation.W, Transform.Rotation.X,
                       Transform.Rotation.Y, Transform.Rotation.Z);
            }
        }
    }

    // === 导入指板位置 ===
    if (JsonObject->HasField(TEXT("LEFT_FINGER_POSITIONS"))) {
        TSharedPtr<FJsonObject> LeftFingerPositionsObj =
            JsonObject->GetObjectField(TEXT("LEFT_FINGER_POSITIONS"));

        for (const auto& FretPair : GuitarFretPositions) {
            const FString& PositionKey = FretPair.Key;
            const FString& RecorderName = FretPair.Value;

            if (!LeftFingerPositionsObj->HasField(*PositionKey)) {
                continue;
            }

            TArray<TSharedPtr<FJsonValue>> LocationArray =
                LeftFingerPositionsObj->GetArrayField(*PositionKey);

            if (LocationArray.Num() == 3) {
                FFretDanceRecorderTransform Transform;
                Transform.Rotation = FQuat::Identity;
                FFretDanceHelpers::ReadLocationFromArray(LocationArray,
                                                         Transform.Location);
                RecorderTransforms.Add(RecorderName, Transform);
                ImportedCount++;

                UE_LOG(
                    LogTemp, Log,
                    TEXT("  [Fretboard] Read '%s' from LEFT_FINGER_POSITIONS → "
                         "Write to RecorderTransforms['%s'] | Loc: (%.3f, "
                         "%.3f, %.3f)"),
                    *PositionKey, *RecorderName, Transform.Location.X,
                    Transform.Location.Y, Transform.Location.Z);
            }
        }
    }

    // === 导入 OTHER_SETTING ===
    if (JsonObject->HasField(TEXT("OTHER_SETTING"))) {
        TSharedPtr<FJsonObject> OtherSettingObj =
            JsonObject->GetObjectField(TEXT("OTHER_SETTING"));

        if (OtherSettingObj->HasField(TEXT("use_vibrato_bar"))) {
            bUseVibratoBar =
                OtherSettingObj->GetBoolField(TEXT("use_vibrato_bar"));
            UE_LOG(LogTemp, Warning,
                   TEXT("  [OtherSetting] Read 'use_vibrato_bar' = %s"),
                   bUseVibratoBar ? TEXT("true") : TEXT("false"));
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("ImportRecorderInfo: Successfully imported %d recorders from %s"),
        ImportedCount, *FilePath);

    UE_LOG(LogTemp, Warning, TEXT("Total recorders in RecorderTransforms: %d"),
           RecorderTransforms.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== ImportRecorderInfo Completed =========="));

    return true;
}

void AFretDanceUnreal::SetInstrumentType(EFretDanceInstrumentType NewType) {
    if (InstrumentType == NewType) {
        UE_LOG(LogTemp, Verbose,
               TEXT("SetInstrumentType: Instrument type unchanged (%d)"),
               (int32)NewType);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SetInstrumentType: Changing from %d to %d"),
           (int32)InstrumentType, (int32)NewType);

    // 更新乐器类型
    InstrumentType = NewType;

    // 如果切换到非电吉他，自动关闭颤音摇杆
    if (NewType != EFretDanceInstrumentType::ELECTRIC_GUITAR) {
        bUseVibratoBar = false;
    }

    // 重新初始化右手手指控制器
    RightFingerControllers = GetRightFingerControllersForInstrumentType();
    UE_LOG(LogTemp, Warning,
           TEXT("Updated RightFingerControllers: %d controllers"),
           RightFingerControllers.Num());
}

UControlRig* AFretDanceUnreal::GetCachedControlRig(FName ComponentName) {
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

    // 根据 ComponentName 获取对应的 SkeletalMeshActor
    ASkeletalMeshActor* Actor = nullptr;
    if (ComponentName == TEXT("Guitar")) {
        Actor = Guitar;
    } else if (ComponentName == TEXT("Performer")) {
        // Performer 使用父类的 SkeletalMeshActor 属性
        Actor = SkeletalMeshActor;
    }

    if (!Actor) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRig: Actor not found for component %s"),
               *ComponentName.ToString());
        return nullptr;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRig: No LevelSequence found"));
        return nullptr;
    }

    // 根据 ComponentName 确定 RootControlName
    FString RootControlName;
    if (ComponentName == TEXT("Guitar")) {
        RootControlName = TEXT("guitar_root");
    } else if (ComponentName == TEXT("Performer")) {
        RootControlName = TEXT("controller_root");
    }

    // 使用通用接口查询 ControlRig
    UControlRig* ControlRig =
        CacheSubsystem->GetControlRig(Actor, LevelSequence, RootControlName);

    // 如果 ControlRig 为空，尝试触发注册后再查询
    if (!ControlRig) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRig: ControlRig is null, triggering "
                    "registration for %s with root control '%s'"),
               *Actor->GetName(), *RootControlName);

        // 触发注册
        CacheSubsystem->TriggerRegistrationIfNeeded(Actor, LevelSequence,
                                                    RootControlName);

        // 再次查询
        ControlRig = CacheSubsystem->GetControlRig(Actor, LevelSequence,
                                                   RootControlName);

        if (!ControlRig) {
            UE_LOG(LogTemp, Error,
                   TEXT("GetCachedControlRig: Still failed to get ControlRig "
                        "after registration for %s"),
                   *Actor->GetName());
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("GetCachedControlRig: Successfully got ControlRig "
                        "after registration for %s"),
                   *Actor->GetName());
        }
    }

    return ControlRig;
}

UControlRigBlueprint* AFretDanceUnreal::GetCachedControlRigBlueprint(
    FName ComponentName) {
    // 详细诊断GEngine和Subsystem状态
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("GetCachedControlRigBlueprint: GEngine is NULL"));
        UE_LOG(
            LogTemp, Error,
            TEXT("Failed to get ControlRigBlueprint for component %s - GEngine "
                 "unavailable"),
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

    // 根据ComponentName获取对应的SkeletalMeshActor
    ASkeletalMeshActor* Actor = nullptr;
    if (ComponentName == TEXT("Guitar")) {
        Actor = Guitar;
    } else if (ComponentName == TEXT("Performer")) {
        // FretDance中暂未定义Performer，如果有需要可以添加
        Actor = SkeletalMeshActor;
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRigBlueprint: ComponentName %s is not "
                    "correct"),
               *ComponentName.ToString());
        return nullptr;
    }

    if (!Actor) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRigBlueprint: Actor not found for "
                    "component %s"),
               *ComponentName.ToString());
        return nullptr;
    }

    // 获取当前LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetCachedControlRigBlueprint: No LevelSequence found"));
        return nullptr;
    }

    // 使用通用接口查询ControlRigBlueprint
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(Actor, LevelSequence);

    return ControlRigBlueprint;
}

void AFretDanceUnreal::TriggerControlRigReregistration(
    const FString& ErrorMessage) {
    UE_LOG(LogTemp, Warning,
           TEXT("TriggerControlRigReregistration: %s, triggering ControlRig "
                "re-registration for relevant components"),
           *ErrorMessage);

    // 调用统一的注册方法
    RegisterAllControlRigs();
}

void AFretDanceUnreal::RegisterAllControlRigs() {
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("RegisterAllControlRigs: GEngine is not available"));
        return;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("RegisterAllControlRigs: ControlRig Cache Subsystem is not "
                    "available"));
        return;
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "RegisterAllControlRigs: No Level Sequence is currently open"));
        return;
    }

    // FretDance 需要同时注册演奏者和吉他两个 ControlRig
    // 因为手部控制器在演奏者身上，而弦振动动画在吉他身上
    int32 RegisteredCount = 0;

    // 1. 注册演奏者（Performer）的 ControlRig - 用于手部控制器
    if (SkeletalMeshActor) {
        CacheSubsystem->TriggerRegistrationIfNeeded(SkeletalMeshActor,
                                                    LevelSequence);
        RegisteredCount++;
        UE_LOG(LogTemp, Warning,
               TEXT("RegisterAllControlRigs: Registered Performer ControlRig "
                    "for %s"),
               *SkeletalMeshActor->GetName());
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("RegisterAllControlRigs: SkeletalMeshActor (Performer) is "
                    "null"));
    }

    // 2. 注册吉他（Guitar）的 ControlRig - 用于弦振动动画
    if (Guitar) {
        CacheSubsystem->TriggerRegistrationIfNeeded(Guitar, LevelSequence);
        RegisteredCount++;
        UE_LOG(
            LogTemp, Warning,
            TEXT("RegisterAllControlRigs: Registered Guitar ControlRig for %s"),
            *Guitar->GetName());
    } else {
        UE_LOG(LogTemp, Error, TEXT("RegisterAllControlRigs: Guitar is null"));
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("RegisterAllControlRigs: Successfully registered %d ControlRigs"),
        RegisteredCount);
}

void AFretDanceUnreal::InitializeRecorderTransforms() {
    if (!Guitar) {
        UE_LOG(LogTemp, Warning,
               TEXT("InitializeRecorderTransforms: Guitar is not assigned"));
        return;
    }

    // 如果已经初始化过，跳过重复初始化
    if (IsInitialized()) {
        UE_LOG(
            LogTemp, Verbose,
            TEXT(
                "InitializeRecorderTransforms: Already initialized, skipping"));
        return;
    }

    RecorderTransforms.Empty();

    UE_LOG(LogTemp, Warning,
           TEXT("Initializing all recorder keys in RecorderTransforms map from "
                "existing lists..."));

    int32 KeyCount = 0;
    FFretDanceRecorderTransform DefaultTransform;
    DefaultTransform.Location = FVector::ZeroVector;
    DefaultTransform.Rotation = FQuat::Identity;

    // 初始化左手手指记录器
    const FFretDanceStringArray* LeftFingerArray =
        LeftFingerRecorders.Find(TEXT("left_finger_recorders"));
    if (LeftFingerArray) {
        for (int32 i = 0; i < LeftFingerArray->Num(); ++i) {
            RecorderTransforms.Add((*LeftFingerArray)[i], DefaultTransform);
            KeyCount++;
        }
    }

    // 初始化左手位置记录器
    for (const auto& Pair : LeftHandPositionRecorders) {
        for (int32 i = 0; i < Pair.Value.Num(); ++i) {
            RecorderTransforms.Add(Pair.Value[i], DefaultTransform);
            KeyCount++;
        }
    }

    // 初始化右手手指记录器
    const FFretDanceStringArray* RightFingerArray =
        RightFingerRecorders.Find(TEXT("right_finger_recorders"));
    if (RightFingerArray) {
        for (int32 i = 0; i < RightFingerArray->Num(); ++i) {
            RecorderTransforms.Add((*RightFingerArray)[i], DefaultTransform);
            KeyCount++;
        }
    }

    // 初始化右手位置记录器
    for (const auto& Pair : RightHandPositionRecorders) {
        for (int32 i = 0; i < Pair.Value.Num(); ++i) {
            // JSON 键名与内部键名已统一，直接使用
            FString RecorderKeyName = Pair.Value[i];
            RecorderTransforms.Add(RecorderKeyName, DefaultTransform);
            KeyCount++;
        }
    }

    // 初始化其他记录器
    const FFretDanceStringArray* OtherArray =
        OtherRecorders.Find(TEXT("other_recorders"));
    if (OtherArray) {
        for (int32 i = 0; i < OtherArray->Num(); ++i) {
            RecorderTransforms.Add((*OtherArray)[i], DefaultTransform);
            KeyCount++;
        }
    }

    // 初始化吉他品格位置记录器
    for (const auto& FretPair : GuitarFretPositions) {
        RecorderTransforms.Add(FretPair.Value, DefaultTransform);
        KeyCount++;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Initialized %d recorder keys in RecorderTransforms map from "
                "existing lists"),
           KeyCount);
}

// ============================================================================
// 右手记录器条目生成（单点定义拼接逻辑）
// ============================================================================
void AFretDanceUnreal::GenerateRightHandRecorderEntries(
    TArray<TPair<FString, FString>>& OutEntries) const {
    OutEntries.Empty();

    TArray<FString> RightHandStates = {TEXT("low"), TEXT("end"), TEXT("high")};
    TArray<TPair<FString, FString>> FingerMappings = {
        {TEXT("p"), TEXT("p")}, {TEXT("tp"), TEXT("tp")},
        {TEXT("i"), TEXT("i")}, {TEXT("m"), TEXT("m")},
        {TEXT("a"), TEXT("a")}, {TEXT("ch"), TEXT("ch")}};

    for (const FString& State : RightHandStates) {
        OutEntries.Add({State + TEXT("_h"),
                        FString::Printf(TEXT("Normal_P%s_H_R"), *State)});
        OutEntries.Add({State + TEXT("_hp"),
                        FString::Printf(TEXT("Normal_P%s_HP_R"), *State)});
        for (const auto& FingerPair : FingerMappings) {
            OutEntries.Add(
                {State + TEXT("_") + FingerPair.Key,
                 FString::Printf(TEXT("%s%s"), *FingerPair.Value, *State)});
        }
    }

    if (bUseVibratoBar) {
        TArray<FString> VibratoStates = {TEXT("release"), TEXT("up"),
                                         TEXT("down")};
        for (const FString& State : VibratoStates) {
            FString Capitalized = State.Left(1).ToUpper() + State.RightChop(1);
            OutEntries.Add(
                {State + TEXT("_h"),
                 FString::Printf(TEXT("Vibrato_%s_H_R"), *Capitalized)});
            OutEntries.Add(
                {State + TEXT("_hp"),
                 FString::Printf(TEXT("Vibrato_%s_HP_R"), *Capitalized)});
            for (const auto& FingerPair : FingerMappings) {
                OutEntries.Add(
                    {State + TEXT("_") + FingerPair.Key,
                     FString::Printf(TEXT("%s%s"), *FingerPair.Value, *State)});
            }
        }
    }
}

void AFretDanceUnreal::UpdateRecorderKeys() {
    UE_LOG(LogTemp, Warning,
           TEXT("========== UpdateRecorderKeys Started =========="));

    int32 AddedCount = 0;
    int32 RemovedCount = 0;

    // ── 左手：根据当前逻辑生成期望的键名集合 ──
    TSet<FString> ExpectedLeftKeys;
    for (int32 PosIdx = 0; PosIdx <= 4; PosIdx++) {
        EFretDanceBasePosition Position =
            static_cast<EFretDanceBasePosition>(PosIdx);
        for (int32 StateIdx = 0; StateIdx <= 3; StateIdx++) {
            EFretDanceLeftHandState State =
                static_cast<EFretDanceLeftHandState>(StateIdx);
            if (!IsValidLeftHandCombination(Position, State)) {
                continue;
            }
            for (const auto& ControllerPair : LeftHandControllers) {
                FString Key = GetLeftHandRecorderName(Position, State,
                                                      ControllerPair.Value);
                if (!Key.IsEmpty()) {
                    ExpectedLeftKeys.Add(Key);
                }
            }
            for (const auto& FingerPair : LeftFingerControllers) {
                FString Key =
                    GetLeftHandRecorderName(Position, State, FingerPair.Value);
                if (!Key.IsEmpty()) {
                    ExpectedLeftKeys.Add(Key);
                }
            }
        }
    }

    // 对左手做 diff：删除多余，增加缺少
    TArray<FString> LeftKeysToRemove;
    for (const auto& Pair : LeftHandPositionRecorders) {
        if (!ExpectedLeftKeys.Contains(Pair.Key)) {
            LeftKeysToRemove.Add(Pair.Key);
        }
    }
    for (const FString& Key : LeftKeysToRemove) {
        LeftHandPositionRecorders.Remove(Key);
        RemovedCount++;
        UE_LOG(LogTemp, Log, TEXT("  [L] REMOVED: %s"), *Key);
    }
    for (const FString& Key : ExpectedLeftKeys) {
        if (!LeftHandPositionRecorders.Contains(Key)) {
            FFretDanceStringArray NewArray;
            NewArray.Add(Key);
            LeftHandPositionRecorders.Add(Key, NewArray);
            AddedCount++;
            UE_LOG(LogTemp, Log, TEXT("  [L] ADDED: %s"), *Key);
        }
    }

    // ── 右手：通过统一入口生成期望的键值对 ──
    TArray<TPair<FString, FString>> ExpectedRightEntries;
    GenerateRightHandRecorderEntries(ExpectedRightEntries);

    // 对右手做 diff
    TSet<FString> ExpectedRightKeys;
    for (const auto& Entry : ExpectedRightEntries) {
        ExpectedRightKeys.Add(Entry.Key);
    }

    // 删除不在期望中的条目
    TArray<FString> RightKeysToRemove;
    for (const auto& Pair : RightHandPositionRecorders) {
        if (!ExpectedRightKeys.Contains(Pair.Key)) {
            RightKeysToRemove.Add(Pair.Key);
        }
    }
    for (const FString& Key : RightKeysToRemove) {
        RightHandPositionRecorders.Remove(Key);
        RemovedCount++;
        UE_LOG(LogTemp, Log, TEXT("  [R] REMOVED: %s"), *Key);
    }

    // 增加缺少的条目，或更新值不匹配的条目
    for (const auto& Entry : ExpectedRightEntries) {
        if (!RightHandPositionRecorders.Contains(Entry.Key)) {
            FFretDanceStringArray NewArray;
            NewArray.Add(Entry.Value);
            RightHandPositionRecorders.Add(Entry.Key, NewArray);
            AddedCount++;
            UE_LOG(LogTemp, Log, TEXT("  [R] ADDED: %s → %s"), *Entry.Key,
                   *Entry.Value);
        } else {
            const FFretDanceStringArray& ExistingArray =
                RightHandPositionRecorders[Entry.Key];
            if (ExistingArray.Num() > 0 && ExistingArray[0] != Entry.Value) {
                RightHandPositionRecorders[Entry.Key].Strings[0] = Entry.Value;
                AddedCount++;
                UE_LOG(LogTemp, Log, TEXT("  [R] UPDATED: %s → %s (was %s)"),
                       *Entry.Key, *Entry.Value, *ExistingArray[0]);
            }
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("UpdateRecorderKeys: Added=%d, Removed=%d, LeftRecorders=%d, "
                "RightRecorders=%d"),
           AddedCount, RemovedCount, LeftHandPositionRecorders.Num(),
           RightHandPositionRecorders.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== UpdateRecorderKeys Completed =========="));
}

TMap<FString, FString> AFretDanceUnreal::GetLeftHandControllerToRecorderMapping(
    EFretDanceBasePosition Position, EFretDanceLeftHandState State) const {
    TMap<FString, FString> Mapping;

    // 检查无效组合
    if (!IsValidLeftHandCombination(Position, State)) {
        return Mapping;  // 返回空映射
    }

    // 为每个左手控制器构造 recorder 名称
    for (const auto& ControllerPair : LeftHandControllers) {
        const FString& ControllerName = ControllerPair.Value;
        FString RecorderName =
            GetLeftHandRecorderName(Position, State, ControllerName);
        if (!RecorderName.IsEmpty()) {
            Mapping.Add(ControllerName, RecorderName);
        }
    }

    // 为每个左手手指控制器构造 recorder 名称
    for (const auto& FingerPair : LeftFingerControllers) {
        const FString& ControllerName = FingerPair.Value;
        FString RecorderName =
            GetLeftHandRecorderName(Position, State, ControllerName);
        if (!RecorderName.IsEmpty()) {
            Mapping.Add(ControllerName, RecorderName);
        }
    }

    return Mapping;
}

TMap<FString, FString> AFretDanceUnreal::GetLeftHandRecorderToControllerMapping(
    EFretDanceBasePosition Position, EFretDanceLeftHandState State) const {
    TMap<FString, FString> ReverseMapping;

    // 获取正向映射
    TMap<FString, FString> ForwardMapping =
        GetLeftHandControllerToRecorderMapping(Position, State);

    // 反转映射
    for (const auto& Pair : ForwardMapping) {
        ReverseMapping.Add(Pair.Value, Pair.Key);
    }

    return ReverseMapping;
}

TMap<FString, FString>
AFretDanceUnreal::GetRightHandControllerToRecorderMapping(
    EFretDanceRightHandState State) const {
    TMap<FString, FString> Mapping;

    // 获取右手状态字符串
    FString StateStr;
    switch (State) {
        case EFretDanceRightHandState::LOW:
            StateStr = "low";
            break;
        case EFretDanceRightHandState::END:
            StateStr = "end";
            break;
        case EFretDanceRightHandState::HIGH:
            StateStr = "high";
            break;
        case EFretDanceRightHandState::RELEASE:
            StateStr = "release";
            break;
        case EFretDanceRightHandState::UP:
            StateStr = "up";
            break;
        case EFretDanceRightHandState::DOWN:
            StateStr = "down";
            break;
        default:
            return Mapping;  // 返回空映射
    }

    // 从 RightHandPositionRecorders 中查找，不再自行拼接
    TMap<FString, FString> ControllerToKeySuffix;
    ControllerToKeySuffix.Add("H_R", "_h");
    ControllerToKeySuffix.Add("HP_R", "_hp");
    ControllerToKeySuffix.Add("T_R", "_p");
    ControllerToKeySuffix.Add("TP_R", "_tp");
    ControllerToKeySuffix.Add("I_R", "_i");
    ControllerToKeySuffix.Add("M_R", "_m");
    ControllerToKeySuffix.Add("R_R", "_a");
    ControllerToKeySuffix.Add("P_R", "_ch");

    for (const auto& Pair : ControllerToKeySuffix) {
        FString MapKey = StateStr + Pair.Value;
        if (const FFretDanceStringArray* RecorderArray =
                RightHandPositionRecorders.Find(MapKey)) {
            if (RecorderArray->Num() > 0) {
                Mapping.Add(Pair.Key, (*RecorderArray)[0]);
            }
        }
    }

    return Mapping;
}

TMap<FString, FString>
AFretDanceUnreal::GetRightHandRecorderToControllerMapping(
    EFretDanceRightHandState State) const {
    TMap<FString, FString> ReverseMapping;

    // 获取正向映射
    TMap<FString, FString> ForwardMapping =
        GetRightHandControllerToRecorderMapping(State);

    // 反转映射
    for (const auto& Pair : ForwardMapping) {
        ReverseMapping.Add(Pair.Value, Pair.Key);
    }

    return ReverseMapping;
}