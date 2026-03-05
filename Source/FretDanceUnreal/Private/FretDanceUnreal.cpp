#include "FretDanceUnreal.h"

#include "Animation/SkeletalMeshActor.h"
#include "Components/SceneComponent.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "FretDanceTransformSyncProcessor.h"
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
        bool bIncludeLocation = true,
        bool bIncludeRotation = true) {
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
            RecorderObj->SetArrayField(TEXT("rotation_quaternion"), RotationArray);
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
    CurrentLeftHandState = EFretDanceLeftHandState::NORMAL;
    CurrentRightHandState = EFretDanceRightHandState::LOW;
    bEnableRealtimeSync = false;
    
    // 初始化所有控制器和记录器
    InitializeControllersAndRecorders();
    
    InitializeRecorderTransforms();
}

// Called when the game starts or when spawned
void AFretDanceUnreal::BeginPlay() {
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("FretDanceUnreal: BeginPlay called"));
    
}

void AFretDanceUnreal::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    // 实时同步逻辑将在后续阶段实现
    if (bEnableRealtimeSync) {
        UFretDanceTransformSyncProcessor::SyncAllInstrumentTransforms(this);
    }
}

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

    // 初始化手掌旋转控制器
    HandRotationControllers.Empty();
    HandRotationControllers.Add("left_hand_rotation_controller",
                                "H_rotation_L");
    HandRotationControllers.Add("right_hand_rotation_controller",
                                "H_rotation_R");

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

    // 初始化右手记录器
    RightHandPositionRecorders.Empty();
    RightHandRotationRecorders.Empty();

    // 右手位置记录器
    TArray<FString> RightHandStates = {"0", "end", "3"};
    TArray<TPair<FString, FString>> FingerMappings = {
        {"p", "p"},
        {"tp", "tp"},
        {"i", "i"},
        {"m", "m"},
        {"a", "a"},
        {"ch", "ch"}
    };

    for (const FString& State : RightHandStates) {
        // 手掌位置记录器
        RightHandPositionRecorders.Add(State + "_h", FFretDanceStringArray());
        RightHandPositionRecorders[State + "_h"].Add(
            FString::Printf(TEXT("Normal_P%s_H_R"), *State));

        RightHandPositionRecorders.Add(State + "_hp", FFretDanceStringArray());
        RightHandPositionRecorders[State + "_hp"].Add(
            FString::Printf(TEXT("Normal_P%s_HP_R"), *State));

        // 手指位置记录器（所有乐器类型都记录完整的手指）
        for (const auto& FingerPair : FingerMappings) {
            const FString& Key = FingerPair.Key;
            const FString& Prefix = FingerPair.Value;
            
            RightHandPositionRecorders.Add(State + "_" + Key, FFretDanceStringArray());
            RightHandPositionRecorders[State + "_" + Key].Add(
                FString::Printf(TEXT("%s%s"), *Prefix, *State));
        }
    }

    // 右手旋转记录器
    RightHandRotationRecorders.Add("high", "Normal_P3_H_rotation_R");
    RightHandRotationRecorders.Add("end", "Normal_Pend_H_rotation_R");
    RightHandRotationRecorders.Add("low", "Normal_P0_H_rotation_R");

    // 初始化指板位置记录器
    GuitarFretPositions.Empty();
    GuitarFretPositions.Add("P0", "P0");
    GuitarFretPositions.Add("P1", "P1");
    GuitarFretPositions.Add("P2", "P2");
    GuitarFretPositions.Add("P3", "P3");
    GuitarFretPositions.Add("P4", "P4");

    // 初始化辅助线（乐器类型差异体现在辅助线上）
    GuideLines.Empty();
    GuideLines.Add("string_move_direction", "string_move_direction");

    if (InstrumentType == EFretDanceInstrumentType::ELECTRIC_GUITAR) {
        GuideLines.Add("left_thumb_move_direction", "T_line");
    } else {
        GuideLines.Add("left_hand_high_normal", "right_hand_normal_p3");
        GuideLines.Add("left_hand_low_normal", "right_hand_normal_p0");
        GuideLines.Add("thumb_direction_high", "right_thumb_direct_p3");
        GuideLines.Add("thumb_direction_low", "right_thumb_direct_p0");
        GuideLines.Add("finger_direction_high", "right_finger_direct_p3");
        GuideLines.Add("finger_direction_low", "right_finger_direct_p0");
    }

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
            StateStr = "0";
            break;
        case EFretDanceRightHandState::END:
            StateStr = "end";
            break;
        case EFretDanceRightHandState::HIGH:
            StateStr = "3";
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
        TEXT("NORMAL_LEFT_HAND_POSITIONS"),
        TEXT("OUTER_LEFT_HAND_POSITIONS"),
        TEXT("INNER_LEFT_HAND_POSITIONS"),
        TEXT("BARRE_LEFT_HAND_POSITIONS"),
        TEXT("LEFT_FINGER_POSITIONS"),
        TEXT("ROTATIONS"),
        TEXT("RIGHT_HAND_POSITIONS"),
        TEXT("RIGHT_HAND_LINES")
    };

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

            // 导出左手控制器
            for (const auto& ControllerPair : LeftHandControllers) {
                const FString& ControllerName = ControllerPair.Value;
                FString RecorderKey =
                    GetLeftHandRecorderName(Position, State, ControllerName);

                const FFretDanceRecorderTransform* Transform =
                    RecorderTransforms.Find(RecorderKey);
                if (Transform) {
                    PositionObj->SetObjectField(
                        *ControllerName, FFretDanceHelpers::CreateRecorderJsonObject(*Transform));
                    TotalExported++;
                }
            }

            // 导出左手手指控制器
            for (const auto& FingerPair : LeftFingerControllers) {
                const FString& ControllerName = FingerPair.Value;
                FString RecorderKey =
                    GetLeftHandRecorderName(Position, State, ControllerName);

                const FFretDanceRecorderTransform* Transform =
                    RecorderTransforms.Find(RecorderKey);
                if (Transform) {
                    PositionObj->SetObjectField(
                        *ControllerName, FFretDanceHelpers::CreateRecorderJsonObject(*Transform));
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

    // === 导出旋转（按照 Python 版本的结构） ===
    TSharedPtr<FJsonObject> RotationsObj = CategoryObjects[TEXT("ROTATIONS")];

    // 导出左手旋转
    TSharedPtr<FJsonObject> LeftRotationObj = MakeShareable(new FJsonObject);
    for (const auto& StatePair : LeftHandStates) {
        const FString& StateName = StatePair.Key;
        EFretDanceLeftHandState State = StatePair.Value;

        TSharedPtr<FJsonObject> StateRotationObj =
            MakeShareable(new FJsonObject);

        for (int32 PosIdx = 0; PosIdx <= 4; PosIdx++) {
            EFretDanceBasePosition Position =
                static_cast<EFretDanceBasePosition>(PosIdx);

            if (!IsValidLeftHandCombination(Position, State)) {
                continue;
            }

            FString PositionStr = FString::Printf(TEXT("P%d"), PosIdx);

            // 获取 H_rotation_L 的 recorder
            FString RecorderKey =
                GetLeftHandRecorderName(Position, State, TEXT("H_rotation_L"));
            const FFretDanceRecorderTransform* Transform =
                RecorderTransforms.Find(RecorderKey);
            if (Transform) {
                TArray<TSharedPtr<FJsonValue>> RotationArray;
                RotationArray.Add(
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.W)));
                RotationArray.Add(
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.X)));
                RotationArray.Add(
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.Y)));
                RotationArray.Add(
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.Z)));
                StateRotationObj->SetArrayField(*PositionStr, RotationArray);
                TotalExported++;
            }
        }

        if (StateRotationObj->Values.Num() > 0) {
            LeftRotationObj->SetObjectField(*StateName, StateRotationObj);
        }
    }

    if (LeftRotationObj->Values.Num() > 0) {
        RotationsObj->SetObjectField(TEXT("H_rotation_L"), LeftRotationObj);
    }

    // 导出右手旋转（H_rotation_R）
    TSharedPtr<FJsonObject> RightRotationObj = MakeShareable(new FJsonObject);
    TSharedPtr<FJsonObject> NormalRightRotationObj =
        MakeShareable(new FJsonObject);

    for (const auto& RotationPair : RightHandRotationRecorders) {
        const FString& StateKey = RotationPair.Key;  // "low", "end", "high"
        const FString& RecorderName =
            RotationPair.Value;  // "Normal_P0_H_rotation_R" etc

        const FFretDanceRecorderTransform* Transform =
            RecorderTransforms.Find(RecorderName);
        if (Transform) {
            // 将 "low"/"end"/"high" 转换为 "P0"/"Pend"/"P3"
            FString PositionStr;
            if (StateKey == TEXT("low")) {
                PositionStr = TEXT("P0");
            } else if (StateKey == TEXT("end")) {
                PositionStr = TEXT("Pend");
            } else if (StateKey == TEXT("high")) {
                PositionStr = TEXT("P3");
            }

            if (!PositionStr.IsEmpty()) {
                TArray<TSharedPtr<FJsonValue>> RotationArray;
                RotationArray.Add(
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.W)));
                RotationArray.Add(
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.X)));
                RotationArray.Add(
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.Y)));
                RotationArray.Add(
                    MakeShareable(new FJsonValueNumber(Transform->Rotation.Z)));
                NormalRightRotationObj->SetArrayField(*PositionStr,
                                                      RotationArray);
                TotalExported++;
            }
        }
    }

    if (NormalRightRotationObj->Values.Num() > 0) {
        RightRotationObj->SetObjectField(TEXT("Normal"),
                                         NormalRightRotationObj);
        RotationsObj->SetObjectField(TEXT("H_rotation_R"), RightRotationObj);
    }

    // === 导出右手位置 ===
    TSharedPtr<FJsonObject> RightHandPositionsObj =
        CategoryObjects[TEXT("RIGHT_HAND_POSITIONS")];
    for (const auto& RecorderPair : RightHandPositionRecorders) {
        const FString& RecorderKey = RecorderPair.Key;
        const FFretDanceStringArray& RecorderArray = RecorderPair.Value;

        for (int32 i = 0; i < RecorderArray.Num(); ++i) {
            const FString& RecorderName = RecorderArray[i];
            const FFretDanceRecorderTransform* Transform =
                RecorderTransforms.Find(RecorderName);
            if (Transform) {
                RightHandPositionsObj->SetArrayField(
                    *RecorderName,
                    FFretDanceHelpers::CreateRecorderJsonObject(*Transform)
                        ->GetArrayField(TEXT("location")));
                TotalExported++;
            }
        }
    }

    // === 导出辅助线 ===
    TSharedPtr<FJsonObject> RightHandLinesObj =
        CategoryObjects[TEXT("RIGHT_HAND_LINES")];
    for (const auto& GuidePair : GuideLines) {
        const FString& GuideLineName = GuidePair.Value;
        const FFretDanceRecorderTransform* Transform =
            RecorderTransforms.Find(GuideLineName);
        if (Transform) {
            TSharedPtr<FJsonObject> LineObj = MakeShareable(new FJsonObject);

            // Vector (方向)
            FRotator Rotator = Transform->Rotation.Rotator();
            FVector Direction = Rotator.Vector();
            TArray<TSharedPtr<FJsonValue>> VectorArray;
            VectorArray.Add(MakeShareable(new FJsonValueNumber(Direction.X)));
            VectorArray.Add(MakeShareable(new FJsonValueNumber(Direction.Y)));
            VectorArray.Add(MakeShareable(new FJsonValueNumber(Direction.Z)));
            LineObj->SetArrayField(TEXT("vector"), VectorArray);

            // Location
            TArray<TSharedPtr<FJsonValue>> LocationArray;
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform->Location.X)));
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform->Location.Y)));
            LocationArray.Add(
                MakeShareable(new FJsonValueNumber(Transform->Location.Z)));
            LineObj->SetArrayField(TEXT("location"), LocationArray);

            RightHandLinesObj->SetObjectField(*GuideLineName, LineObj);
            TotalExported++;
        }
    }

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

            // 导入左手控制器
            for (const auto& ControllerPair : LeftHandControllers) {
                const FString& ControllerName = ControllerPair.Value;
                if (!PositionObj->HasField(*ControllerName)) {
                    continue;
                }

                FString RecorderKey =
                    GetLeftHandRecorderName(Position, State, ControllerName);
                TSharedPtr<FJsonObject> RecorderObj =
                    PositionObj->GetObjectField(*ControllerName);

                FFretDanceRecorderTransform Transform =
                    FFretDanceHelpers::ReadRecorderTransform(RecorderObj);
                RecorderTransforms.Add(RecorderKey, Transform);
                ImportedCount++;
            }

            // 导入左手手指控制器
            for (const auto& FingerPair : LeftFingerControllers) {
                const FString& ControllerName = FingerPair.Value;
                if (!PositionObj->HasField(*ControllerName)) {
                    continue;
                }

                FString RecorderKey =
                    GetLeftHandRecorderName(Position, State, ControllerName);
                TSharedPtr<FJsonObject> RecorderObj =
                    PositionObj->GetObjectField(*ControllerName);

                FFretDanceRecorderTransform Transform =
                    FFretDanceHelpers::ReadRecorderTransform(RecorderObj);
                RecorderTransforms.Add(RecorderKey, Transform);
                ImportedCount++;
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
            }
        }
    }

    // === 导入旋转 ===
    if (JsonObject->HasField(TEXT("ROTATIONS"))) {
        TSharedPtr<FJsonObject> RotationsObj =
            JsonObject->GetObjectField(TEXT("ROTATIONS"));

        // 导入左手旋转（H_rotation_L）
        if (RotationsObj->HasField(TEXT("H_rotation_L"))) {
            TSharedPtr<FJsonObject> LeftRotationObj =
                RotationsObj->GetObjectField(TEXT("H_rotation_L"));

            for (const auto& StatePair : LeftHandStates) {
                const FString& StateName = StatePair.Key;
                EFretDanceLeftHandState State = StatePair.Value;

                if (!LeftRotationObj->HasField(*StateName)) {
                    continue;
                }

                TSharedPtr<FJsonObject> StateRotationObj =
                    LeftRotationObj->GetObjectField(*StateName);

                for (int32 PosIdx = 0; PosIdx <= 4; PosIdx++) {
                    EFretDanceBasePosition Position =
                        static_cast<EFretDanceBasePosition>(PosIdx);

                    if (!IsValidLeftHandCombination(Position, State)) {
                        continue;
                    }

                    FString PositionStr = FString::Printf(TEXT("P%d"), PosIdx);
                    if (!StateRotationObj->HasField(*PositionStr)) {
                        continue;
                    }

                    TArray<TSharedPtr<FJsonValue>> RotationArray =
                        StateRotationObj->GetArrayField(*PositionStr);

                    if (RotationArray.Num() == 4) {
                        // 关键：将 H_rotation_L 的旋转值应用到 H_L 上
                        FString RealRecorderName = GetLeftHandRecorderName(
                            Position, State, TEXT("H_L"));

                        FFretDanceRecorderTransform* ExistingTransform =
                            RecorderTransforms.Find(RealRecorderName);

                        FFretDanceRecorderTransform Transform;
                        if (ExistingTransform) {
                            Transform = *ExistingTransform;
                        } else {
                            Transform.Location = FVector::ZeroVector;
                        }

                        FFretDanceHelpers::ReadRotationFromArray(
                            RotationArray, Transform.Rotation);

                        RecorderTransforms.Add(RealRecorderName, Transform);
                        ImportedCount++;
                    }
                }
            }
        }

        // 导入右手旋转（H_rotation_R）
        if (RotationsObj->HasField(TEXT("H_rotation_R"))) {
            TSharedPtr<FJsonObject> RightRotationObj =
                RotationsObj->GetObjectField(TEXT("H_rotation_R"));

            if (RightRotationObj->HasField(TEXT("Normal"))) {
                TSharedPtr<FJsonObject> NormalRotationObj =
                    RightRotationObj->GetObjectField(TEXT("Normal"));

                // P0 -> low
                if (NormalRotationObj->HasField(TEXT("P0"))) {
                    TArray<TSharedPtr<FJsonValue>> RotationArray =
                        NormalRotationObj->GetArrayField(TEXT("P0"));
                    if (RotationArray.Num() == 4) {
                        const FString* RecorderNamePtr = RightHandRotationRecorders.Find(TEXT("low"));
                        if (RecorderNamePtr) {
                            FFretDanceRecorderTransform Transform;
                            Transform.Location = FVector::ZeroVector;
                            FFretDanceHelpers::ReadRotationFromArray(
                                RotationArray, Transform.Rotation);
                            RecorderTransforms.Add(*RecorderNamePtr, Transform);
                            ImportedCount++;
                        }
                    }
                }

                // Pend -> end
                if (NormalRotationObj->HasField(TEXT("Pend"))) {
                    TArray<TSharedPtr<FJsonValue>> RotationArray =
                        NormalRotationObj->GetArrayField(TEXT("Pend"));
                    if (RotationArray.Num() == 4) {
                        const FString* RecorderNamePtr = RightHandRotationRecorders.Find(TEXT("end"));
                        if (RecorderNamePtr) {
                            FFretDanceRecorderTransform Transform;
                            Transform.Location = FVector::ZeroVector;
                            FFretDanceHelpers::ReadRotationFromArray(
                                RotationArray, Transform.Rotation);
                            RecorderTransforms.Add(*RecorderNamePtr, Transform);
                            ImportedCount++;
                        }
                    }
                }

                // P3 -> high
                if (NormalRotationObj->HasField(TEXT("P3"))) {
                    TArray<TSharedPtr<FJsonValue>> RotationArray =
                        NormalRotationObj->GetArrayField(TEXT("P3"));
                    if (RotationArray.Num() == 4) {
                        const FString* RecorderNamePtr = RightHandRotationRecorders.Find(TEXT("high"));
                        if (RecorderNamePtr) {
                            FFretDanceRecorderTransform Transform;
                            Transform.Location = FVector::ZeroVector;
                            FFretDanceHelpers::ReadRotationFromArray(
                                RotationArray, Transform.Rotation);
                            RecorderTransforms.Add(*RecorderNamePtr, Transform);
                            ImportedCount++;
                        }
                    }
                }
            }
        }
    }

    // === 导入右手位置 ===
    if (JsonObject->HasField(TEXT("RIGHT_HAND_POSITIONS"))) {
        TSharedPtr<FJsonObject> RightHandPositionsObj =
            JsonObject->GetObjectField(TEXT("RIGHT_HAND_POSITIONS"));

        for (auto& RecorderPair : RightHandPositionRecorders) {
            const FString& RecorderKey = RecorderPair.Key;
            FFretDanceStringArray& RecorderArray = RecorderPair.Value;

            for (int32 i = 0; i < RecorderArray.Num(); ++i) {
                const FString& RecorderName = RecorderArray[i];

                if (!RightHandPositionsObj->HasField(*RecorderName)) {
                    continue;
                }

                TArray<TSharedPtr<FJsonValue>> LocationArray =
                    RightHandPositionsObj->GetArrayField(*RecorderName);

                if (LocationArray.Num() == 3) {
                    FFretDanceRecorderTransform Transform;
                    Transform.Rotation = FQuat::Identity;
                    FFretDanceHelpers::ReadLocationFromArray(
                        LocationArray, Transform.Location);
                    RecorderTransforms.Add(RecorderName, Transform);
                    ImportedCount++;
                }
            }
        }
    }

    // === 导入辅助线 ===
    if (JsonObject->HasField(TEXT("RIGHT_HAND_LINES"))) {
        TSharedPtr<FJsonObject> RightHandLinesObj =
            JsonObject->GetObjectField(TEXT("RIGHT_HAND_LINES"));

        for (const auto& GuidePair : GuideLines) {
            const FString& GuideLineName = GuidePair.Value;

            if (!RightHandLinesObj->HasField(*GuideLineName)) {
                continue;
            }

            TSharedPtr<FJsonObject> LineObj =
                RightHandLinesObj->GetObjectField(*GuideLineName);

            FFretDanceRecorderTransform Transform;
            Transform.Location = FVector::ZeroVector;
            Transform.Rotation = FQuat::Identity;

            // 读取 Location
            if (LineObj->HasField(TEXT("location"))) {
                TArray<TSharedPtr<FJsonValue>> LocationArray =
                    LineObj->GetArrayField(TEXT("location"));
                FFretDanceHelpers::ReadLocationFromArray(LocationArray,
                                                         Transform.Location);
            }

            // 读取 Vector 并转换为旋转
            if (LineObj->HasField(TEXT("vector"))) {
                TArray<TSharedPtr<FJsonValue>> VectorArray =
                    LineObj->GetArrayField(TEXT("vector"));
                if (VectorArray.Num() == 3) {
                    FVector Direction;
                    FFretDanceHelpers::ReadLocationFromArray(VectorArray,
                                                             Direction);
                    Transform.Rotation = FQuat::FindBetweenVectors(
                        FVector::ForwardVector, Direction.GetSafeNormal());
                }
            }

            RecorderTransforms.Add(GuideLineName, Transform);
            ImportedCount++;
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("ImportRecorderInfo: Successfully imported %d recorders from %s"),
        ImportedCount, *FilePath);

    return true;
}

void AFretDanceUnreal::SetInstrumentType(EFretDanceInstrumentType NewType) {
    if (InstrumentType == NewType) {
        UE_LOG(LogTemp, Verbose,
               TEXT("SetInstrumentType: Instrument type unchanged (%d)"),
               (int32)NewType);
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("SetInstrumentType: Changing from %d to %d"),
           (int32)InstrumentType, (int32)NewType);

    // 更新乐器类型
    InstrumentType = NewType;

    // 重新初始化右手手指控制器
    RightFingerControllers = GetRightFingerControllersForInstrumentType();
    UE_LOG(LogTemp, Warning,
           TEXT("Updated RightFingerControllers: %d controllers"),
           RightFingerControllers.Num());

    // 重新初始化辅助线（根据乐器类型）
    GuideLines.Empty();
    GuideLines.Add("string_move_direction", "string_move_direction");

    if (InstrumentType == EFretDanceInstrumentType::ELECTRIC_GUITAR) {
        GuideLines.Add("left_thumb_move_direction", "T_line");
        UE_LOG(LogTemp, Warning, TEXT("Added electric guitar guideline: T_line"));
    } else {
        // Finger Style Guitar or Bass
        GuideLines.Add("left_hand_high_normal", "right_hand_normal_p3");
        GuideLines.Add("left_hand_low_normal", "right_hand_normal_p0");
        GuideLines.Add("thumb_direction_high", "right_thumb_direct_p3");
        GuideLines.Add("thumb_direction_low", "right_thumb_direct_p0");
        GuideLines.Add("finger_direction_high", "right_finger_direct_p3");
        GuideLines.Add("finger_direction_low", "right_finger_direct_p0");
        UE_LOG(LogTemp, Warning,
               TEXT("Added finger style/bass guidelines: 6 guidelines added"));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ SetInstrumentType completed. Total guidelines: %d"),
           GuideLines.Num());
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

    // 使用通用接口查询ControlRig
    UControlRig* ControlRig =
        CacheSubsystem->GetControlRig(Actor, LevelSequence);

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
        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "GetCachedControlRigBlueprint: Performer not implemented yet"));
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

void AFretDanceUnreal::TriggerControlRigReregistration(const FString& ErrorMessage) {
    UE_LOG(LogTemp, Warning, TEXT("TriggerControlRigReregistration: %s, triggering ControlRig re-registration for relevant components"), *ErrorMessage);

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
               TEXT("RegisterAllControlRigs: ControlRig Cache Subsystem is not available"));
        return;
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("RegisterAllControlRigs: No Level Sequence is currently open"));
        return;
    }

    // FretDance 需要同时注册演奏者和吉他两个 ControlRig
    // 因为手部控制器在演奏者身上，而弦振动动画在吉他身上
    int32 RegisteredCount = 0;

    // 1. 注册演奏者（Performer）的 ControlRig - 用于手部控制器
    if (SkeletalMeshActor) {
        CacheSubsystem->TriggerRegistrationIfNeeded(
            SkeletalMeshActor, LevelSequence);
        RegisteredCount++;
        UE_LOG(LogTemp, Warning,
               TEXT("RegisterAllControlRigs: Registered Performer ControlRig for %s"),
               *SkeletalMeshActor->GetName());
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("RegisterAllControlRigs: SkeletalMeshActor (Performer) is null"));
    }

    // 2. 注册吉他（Guitar）的 ControlRig - 用于弦振动动画
    if (Guitar) {
        CacheSubsystem->TriggerRegistrationIfNeeded(
            Guitar, LevelSequence);
        RegisteredCount++;
        UE_LOG(LogTemp, Warning,
               TEXT("RegisterAllControlRigs: Registered Guitar ControlRig for %s"),
               *Guitar->GetName());
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("RegisterAllControlRigs: Guitar is null"));
    }

    UE_LOG(LogTemp, Warning,
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
            RecorderTransforms.Add(Pair.Value[i], DefaultTransform);
            KeyCount++;
        }
    }

    // 初始化右手旋转记录器
    for (const auto& Pair : RightHandRotationRecorders) {
        for (int32 i = 0; i < RightHandRotationRecorders.Num(); ++i) {
            RecorderTransforms.Add(Pair.Value, DefaultTransform);
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

    // 初始化辅助线记录器
    for (const auto& GuidePair : GuideLines) {
        RecorderTransforms.Add(GuidePair.Value, DefaultTransform);
        KeyCount++;
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

    // 为左手旋转控制器构造 recorder 名称
    for (const auto& RotationPair : HandRotationControllers) {
        const FString& ControllerName = RotationPair.Value;
        if (ControllerName.EndsWith(TEXT("_L"))) {  // 只处理左手
            FString RecorderName =
                GetLeftHandRecorderName(Position, State, ControllerName);
            if (!RecorderName.IsEmpty()) {
                Mapping.Add(ControllerName, RecorderName);
            }
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
            StateStr = "0";
            break;
        case EFretDanceRightHandState::END:
            StateStr = "end";
            break;
        case EFretDanceRightHandState::HIGH:
            StateStr = "3";
            break;
        default:
            return Mapping;  // 返回空映射
    }

    // 西班牙手指缩写到英文控制器的映射
    TMap<FString, FString> FingerMapping;
    FingerMapping.Add("p", "T_R");    // 拇指
    FingerMapping.Add("tp", "TP_R");  // 拇指 ik pivot
    FingerMapping.Add("i", "I_R");    // 食指
    FingerMapping.Add("m", "M_R");    // 中指
    FingerMapping.Add("a", "R_R");    // 无名指
    FingerMapping.Add("ch", "P_R");   // 小指

    // 根据乐器类型确定要处理的手指
    TArray<FString> CurrentFingers;
    if (InstrumentType == EFretDanceInstrumentType::ELECTRIC_GUITAR) {
        CurrentFingers.Add("p");  // 电吉他只有大拇指
    } else {
        CurrentFingers.Append({"p", "i", "m", "a", "ch", "tp"});
    }

    // 为每个手指控制器构造 recorder 名称
    for (const auto& FingerPair : FingerMapping) {
        if (CurrentFingers.Contains(FingerPair.Key)) {
            FString RecorderName =
                GetRightHandRecorderName(State, FingerPair.Key);
            Mapping.Add(FingerPair.Value, RecorderName);
        }
    }

    // 为手掌控制器构造 recorder 名称
    Mapping.Add("H_R", StateStr + "_h");
    Mapping.Add("HP_R", StateStr + "_hp");

    // 为右手旋转控制器构造 recorder 名称（只有 H_rotation_R）
    TMap<FString, FString> RotationMapping;
    RotationMapping.Add("0", "low");
    RotationMapping.Add("end", "end");
    RotationMapping.Add("3", "high");

    if (RotationMapping.Contains(StateStr)) {
        FString RotationRecorderKey = RotationMapping[StateStr];
        if (RightHandRotationRecorders.Contains(RotationRecorderKey)) {
            Mapping.Add("H_rotation_R",
                        RightHandRotationRecorders[RotationRecorderKey]);
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