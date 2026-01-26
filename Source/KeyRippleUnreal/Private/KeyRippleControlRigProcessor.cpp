#include "KeyRippleControlRigProcessor.h"

#include "Dom/JsonObject.h"  // 包含FJsonObject
#include "Dom/JsonValue.h"   // 包含FJsonValue
#include "ISequencer.h"
#include "KeyRipplePianoProcessor.h"
#include "LevelEditor.h"
#include "LevelEditorSequencerIntegration.h"
#include "Rigs/RigHierarchyController.h"
#include "Serialization/JsonReader.h"      // 包含TJsonReader
#include "Serialization/JsonSerializer.h"  // 包含FJsonSerializer
#include "Serialization/JsonWriter.h"      // 包含TJsonWriter

#define LOCTEXT_NAMESPACE "KeyRippleControlRigProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations - Static helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Control existence and validation helpers
static bool StrictControlExistenceCheck(URigHierarchy* RigHierarchy,
                                        const FString& ControllerName);
static bool GetControlRigInstanceAndBlueprint(
    AKeyRippleUnreal* KeyRippleActor, UControlRig*& OutControlRigInstance,
    UControlRigBlueprint*& OutControlRigBlueprint);

// JSON data processing helpers for export/import
static void ProcessTransformDataForStringArray(
    AKeyRippleUnreal* KeyRippleActor, TSharedPtr<FJsonObject> JsonObject,
    const TMap<FString, FStringArray>& Recorders, const FString& CategoryName);
static void ProcessTransformData(AKeyRippleUnreal* KeyRippleActor,
                                 TSharedPtr<FJsonObject> JsonObject,
                                 const TMap<FString, FString>& SimpleData,
                                 const FString& CategoryName);

// Import helpers
static void ProcessImportTransformDataForStringArray(
    AKeyRippleUnreal* KeyRippleActor, TSharedPtr<FJsonObject> JsonObject,
    const FString& CategoryName, int32& ImportedCount, int32& FailedCount);
static void ProcessImportTransformData(AKeyRippleUnreal* KeyRippleActor,
                                       TSharedPtr<FJsonObject> JsonObject,
                                       const FString& CategoryName,
                                       int32& ImportedCount,
                                       int32& FailedCount);
static void ProcessImportConfigParameters(AKeyRippleUnreal* KeyRippleActor,
                                          TSharedPtr<FJsonObject> JsonObject,
                                          int32& ImportedCount,
                                          int32& FailedCount);

// Utility and validation helpers
static bool ValidateKeyRippleActor(AKeyRippleUnreal* KeyRippleActor,
                                   const FString& FunctionName);
static void LogStandardStart(const FString& OperationName);
static void LogStandardEnd(const FString& OperationName, int32 SuccessCount,
                           int32 FailCount, int32 TotalCount);
static void ProcessHandControllerPairing(
    TMap<FString, TArray<FControlKeyframe>>& ControlKeyframeData);

// Controller and recorder management helpers
static TSet<FString> GetAllControllerNames(AKeyRippleUnreal* KeyRippleActor);
static TArray<FString> GenerateStateDependentRecorders(
    AKeyRippleUnreal* KeyRippleActor, const FString& ControllerName);
static void InitializeControllerRecorderItem(AKeyRippleUnreal* KeyRippleActor,
                                             const FString& RecorderName);
static void AddControllerRecordersToTransforms(
    AKeyRippleUnreal* KeyRippleActor, const TMap<FString, FString>& Controllers,
    bool bIsStateDependent);
static void InitializeRecorderTransforms(AKeyRippleUnreal* KeyRippleActor);

// Transform save/load helpers
static void SaveControllerTransform(AKeyRippleUnreal* KeyRippleActor,
                                    URigHierarchy* RigHierarchy,
                                    const FString& ControlName,
                                    const FString& RecorderName,
                                    int32& SavedCount, int32& FailedCount);
static void LoadControllerTransform(AKeyRippleUnreal* KeyRippleActor,
                                    URigHierarchy* RigHierarchy,
                                    const FString& ControlName,
                                    const FString& ExpectedRecorderName,
                                    int32& LoadedCount, int32& FailedCount);
static void SaveControllers(AKeyRippleUnreal* KeyRippleActor,
                            URigHierarchy* RigHierarchy,
                            const TMap<FString, FString>& Controllers,
                            int32& SavedCount, int32& FailedCount,
                            bool bIsFingerControl, bool isStateDependent);
static void LoadControllers(AKeyRippleUnreal* KeyRippleActor,
                            URigHierarchy* RigHierarchy,
                            const TMap<FString, FString>& Controllers,
                            int32& LoadedCount, int32& FailedCount,
                            bool bIsFingerControl, bool isStateDependent);

// Control setup and cleanup helpers
static void CleanupDuplicateControls(
    AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
    const TSet<FString>& ExpectedControllerNames);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Static helper function implementations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 辅助方法：更严格的Control存在性检查
static bool StrictControlExistenceCheck(URigHierarchy* RigHierarchy,
                                        const FString& ControllerName) {
    if (!RigHierarchy) {
        return false;
    }

    FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);

    // 基本存在性检查
    if (!RigHierarchy->Contains(ElementKey)) {
        return false;
    }

    // 获取Control元素并验证其完整性
    FRigControlElement* ControlElement =
        RigHierarchy->Find<FRigControlElement>(ElementKey);
    if (!ControlElement) {
        UE_LOG(LogTemp, Warning,
               TEXT("Control '%s' exists in hierarchy but element is null - "
                    "considering as non-existent"),
               *ControllerName);
        return false;
    }

    // 检查Control的基本设置是否有效
    if (ControlElement->Settings.ControlType == ERigControlType::Bool &&
        ControllerName !=
            TEXT("controller_root")) {  // controller_root可能是Bool类型
        UE_LOG(LogTemp, Warning,
               TEXT("Control '%s' has unexpected Bool type - may be corrupted"),
               *ControllerName);
        return false;
    }

    return true;
}

// 辅助方法：获取Control Rig实例和蓝图
static bool GetControlRigInstanceAndBlueprint(
    AKeyRippleUnreal* KeyRippleActor, UControlRig*& OutControlRigInstance,
    UControlRigBlueprint*& OutControlRigBlueprint) {
    // 通过SkeletalMeshActor获取Control Rig Instance和Blueprint
    return UKeyRippleControlRigProcessor::GetControlRigFromSkeletalMeshActor(
        KeyRippleActor->SkeletalMeshActor, OutControlRigInstance,
        OutControlRigBlueprint);
}

// 辅助方法：通用的JSON数据处理模板 - 用于导出记录器信息
static void ProcessTransformDataForStringArray(
    AKeyRippleUnreal* KeyRippleActor, TSharedPtr<FJsonObject> JsonObject,
    const TMap<FString, FStringArray>& Recorders, const FString& CategoryName) {
    // 创建分类对象
    TSharedPtr<FJsonObject> CategoryObject = MakeShareable(new FJsonObject);

    for (const auto& RecorderListPair : Recorders) {
        FString ListName = RecorderListPair.Key;
        const FStringArray& RecorderList = RecorderListPair.Value;

        // 为每个记录器列表创建对象
        TSharedPtr<FJsonObject> ListObject = MakeShareable(new FJsonObject);

        for (const FString& RecorderName : RecorderList.Strings) {
            bool isRotationController = RecorderName.Contains("rotation");
            FString RealRecorderName =
                isRotationController
                    ? RecorderName.Replace(TEXT("_rotation"), TEXT(""))
                    : RecorderName;
            // 从RecorderTransforms中查找对应的变换数据
            const FRecorderTransform* FoundTransform =
                KeyRippleActor->RecorderTransforms.Find(RealRecorderName);
            if (FoundTransform) {
                // 创建记录器对象
                TSharedPtr<FJsonObject> RecorderObject =
                    MakeShareable(new FJsonObject);

                if (isRotationController) {
                    // 添加旋转信息
                    TArray<TSharedPtr<FJsonValue>> RotationArray;
                    RotationArray.Add(MakeShareable(
                        new FJsonValueNumber(FoundTransform->Rotation.W)));
                    RotationArray.Add(MakeShareable(
                        new FJsonValueNumber(FoundTransform->Rotation.X)));
                    RotationArray.Add(MakeShareable(
                        new FJsonValueNumber(FoundTransform->Rotation.Y)));
                    RotationArray.Add(MakeShareable(
                        new FJsonValueNumber(FoundTransform->Rotation.Z)));
                    RecorderObject->SetArrayField(TEXT("rotation_quaternion"),
                                                  RotationArray);

                    // 添加旋转模式字段
                    RecorderObject->SetStringField(TEXT("rotation_mode"),
                                                   TEXT("QUATERNION"));
                } else {
                    // 添加位置信息
                    TArray<TSharedPtr<FJsonValue>> LocationArray;
                    LocationArray.Add(MakeShareable(
                        new FJsonValueNumber(FoundTransform->Location.X)));
                    LocationArray.Add(MakeShareable(
                        new FJsonValueNumber(FoundTransform->Location.Y)));
                    LocationArray.Add(MakeShareable(
                        new FJsonValueNumber(FoundTransform->Location.Z)));
                    RecorderObject->SetArrayField(TEXT("location"),
                                                  LocationArray);
                }

                // 将记录器对象添加到列表对象中
                ListObject->SetObjectField(*RecorderName, RecorderObject);
            } else {
                UE_LOG(LogTemp, Warning,
                       TEXT("  ! Recorder transform not found: %s"),
                       *RecorderName);
            }
        }

        // 将列表对象添加到分类对象中
        CategoryObject->SetObjectField(*ListName, ListObject);
    }

    // 将分类对象添加到主JSON对象中
    JsonObject->SetObjectField(*CategoryName, CategoryObject);
}

// 辅助方法：通用的JSON数据处理模板 - 用于导出简单的TMap<FString,
// FString>类型数据
static void ProcessTransformData(AKeyRippleUnreal* KeyRippleActor,
                                 TSharedPtr<FJsonObject> JsonObject,
                                 const TMap<FString, FString>& SimpleData,
                                 const FString& CategoryName) {
    // 创建分类对象
    TSharedPtr<FJsonObject> CategoryObject = MakeShareable(new FJsonObject);

    for (const auto& DataPair : SimpleData) {
        FString Key = DataPair.Key;
        FString RecorderName = DataPair.Value;
        bool isGuildLine = RecorderName.Contains("direction");

        // 创建数据项对象
        TSharedPtr<FJsonObject> DataObject = MakeShareable(new FJsonObject);
        DataObject->SetStringField(TEXT("name"), RecorderName);

        // 从RecorderTransforms中查找对应的变换数据
        const FRecorderTransform* FoundTransform =
            KeyRippleActor->RecorderTransforms.Find(RecorderName);
        if (FoundTransform) {
            // 添加位置信息
            TArray<TSharedPtr<FJsonValue>> LocationArray;
            LocationArray.Add(MakeShareable(
                new FJsonValueNumber(FoundTransform->Location.X)));
            LocationArray.Add(MakeShareable(
                new FJsonValueNumber(FoundTransform->Location.Y)));
            LocationArray.Add(MakeShareable(
                new FJsonValueNumber(FoundTransform->Location.Z)));
            DataObject->SetArrayField(TEXT("location"), LocationArray);

            if (isGuildLine) {
                // 添加旋转信息
                TArray<TSharedPtr<FJsonValue>> RotationArray;
                RotationArray.Add(MakeShareable(
                    new FJsonValueNumber(FoundTransform->Rotation.W)));
                RotationArray.Add(MakeShareable(
                    new FJsonValueNumber(FoundTransform->Rotation.X)));
                RotationArray.Add(MakeShareable(
                    new FJsonValueNumber(FoundTransform->Rotation.Y)));
                RotationArray.Add(MakeShareable(
                    new FJsonValueNumber(FoundTransform->Rotation.Z)));
                DataObject->SetArrayField(TEXT("rotation_quaternion"),
                                          RotationArray);

                // 添加旋转模式字段
                DataObject->SetStringField(TEXT("rotation_mode"),
                                           TEXT("QUATERNION"));
            }
        }

        // 将数据对象添加到分类对象中
        CategoryObject->SetObjectField(*Key, DataObject);
    }

    // 将分类对象添加到主JSON对象中
    JsonObject->SetObjectField(*CategoryName, CategoryObject);
}

// 辅助方法：安全的KeyRippleActor检查
static bool ValidateKeyRippleActor(AKeyRippleUnreal* KeyRippleActor,
                                   const FString& FunctionName) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error, TEXT("%s: KeyRippleActor is null"),
               *FunctionName);
        return false;
    }
    return true;
}

// 辅助方法：标准日志开始消息
static void LogStandardStart(const FString& OperationName) {
    UE_LOG(LogTemp, Warning, TEXT("========== %s Started =========="),
           *OperationName);
}

// 辅助方法：标准日志结束消息
static void LogStandardEnd(const FString& OperationName, int32 SuccessCount,
                           int32 FailCount, int32 TotalCount) {
    UE_LOG(LogTemp, Warning, TEXT("========== %s Summary =========="),
           *OperationName);
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed: %d items"),
           SuccessCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed to process: %d items"), FailCount);
    UE_LOG(LogTemp, Warning, TEXT("Total items: %d"), TotalCount);
    UE_LOG(LogTemp, Warning, TEXT("========== %s Completed =========="),
           *OperationName);
}

// 辅助方法：处理手掌控制器配对逻辑
static void ProcessHandControllerPairing(
    TMap<FString, TArray<FControlKeyframe>>& ControlKeyframeData) {
    // 处理 H_L 和 H_rotation_L
    TArray<FControlKeyframe>* H_L_Data = ControlKeyframeData.Find(TEXT("H_L"));
    TArray<FControlKeyframe>* H_rotation_L_Data =
        ControlKeyframeData.Find(TEXT("H_rotation_L"));

    if (H_L_Data && H_rotation_L_Data) {
        int32 MinSize = FMath::Min(H_L_Data->Num(), H_rotation_L_Data->Num());

        // 用 H_rotation_L 的旋转数据替换 H_L 的旋转数据
        for (int32 i = 0; i < MinSize; i++) {
            (*H_L_Data)[i].Rotation = (*H_rotation_L_Data)[i].Rotation;
        }

        // 移除 H_rotation_L
        ControlKeyframeData.Remove(TEXT("H_rotation_L"));
    }

    // 处理 H_R 和 H_rotation_R
    TArray<FControlKeyframe>* H_R_Data = ControlKeyframeData.Find(TEXT("H_R"));
    TArray<FControlKeyframe>* H_rotation_R_Data =
        ControlKeyframeData.Find(TEXT("H_rotation_R"));

    if (H_R_Data && H_rotation_R_Data) {
        int32 MinSize = FMath::Min(H_R_Data->Num(), H_rotation_R_Data->Num());

        // 用 H_rotation_R 的旋转数据替换 H_R 的旋转数据
        for (int32 i = 0; i < MinSize; i++) {
            (*H_R_Data)[i].Rotation = (*H_rotation_R_Data)[i].Rotation;
        }

        // 移除 H_rotation_R
        ControlKeyframeData.Remove(TEXT("H_rotation_R"));
    }
}

// 工具方法：根据Control名字和当前手部状态获取对应的Recorder名字
FString UKeyRippleControlRigProcessor::GetRecorderNameForControl(
    AKeyRippleUnreal* KeyRippleActor, const FString& ControlName,
    bool bIsFingerControl) {
    // 判断是左手还是右手
    bool bIsLeftHand = ControlName.EndsWith(TEXT("_L"));

    // 确定位置类型
    EPositionType PositionType = bIsLeftHand
                                     ? KeyRippleActor->LeftHandPositionType
                                     : KeyRippleActor->RightHandPositionType;

    FString PositionTypeStr =
        (PositionType == EPositionType::HIGH)  ? TEXT("high")
        : (PositionType == EPositionType::LOW) ? TEXT("low")
                                               : TEXT("middle");

    // 确定键类型
    EKeyType KeyType = bIsLeftHand ? KeyRippleActor->LeftHandKeyType
                                   : KeyRippleActor->RightHandKeyType;

    FString KeyTypeStr =
        (KeyType == EKeyType::WHITE) ? TEXT("white") : TEXT("black");

    // 构建Recorder名字
    FString RecorderName = FString::Printf(TEXT("%s_%s_%s"), *PositionTypeStr,
                                           *KeyTypeStr, *ControlName);

    // 🔧 DEBUG: 添加调试日志，显示状态到记录器名称的映射过程
    UE_LOG(LogTemp, Warning,
           TEXT("GetRecorderNameForControl: %s -> %s | Hand: %s | Position: "
                "%s | KeyType: %s"),
           *ControlName, *RecorderName,
           bIsLeftHand ? TEXT("LEFT") : TEXT("RIGHT"), *PositionTypeStr,
           *KeyTypeStr);

    return RecorderName;
}

// 辅助方法：通用的JSON数据处理模板 - 用于导入记录器信息 (TMap<FString,
// FStringArray> 类型)
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

            // 检查是否为旋转控制器
            bool isRotationController = RecorderName.Contains("rotation");
            FString RealRecorderName =
                isRotationController
                    ? RecorderName.Replace(TEXT("_rotation"), TEXT(""))
                    : RecorderName;

            // 🔧 FIX: 获取或创建目标的 RecorderTransform
            FRecorderTransform* TargetTransform =
                KeyRippleActor->RecorderTransforms.Find(RealRecorderName);
            if (!TargetTransform) {
                // 如果不存在，创建新的
                FRecorderTransform NewTransform;
                NewTransform.Location = FVector::ZeroVector;
                NewTransform.Rotation = FQuat::Identity;
                TargetTransform = &KeyRippleActor->RecorderTransforms.Add(
                    RealRecorderName, NewTransform);
            }

            if (isRotationController) {
                // 🔧 只更新旋转数据，保持位移数据不变
                if (RecorderObject->HasField(TEXT("rotation_quaternion"))) {
                    TArray<TSharedPtr<FJsonValue>> RotationArray =
                        RecorderObject->GetArrayField(
                            TEXT("rotation_quaternion"));
                    if (RotationArray.Num() == 4) {
                        TargetTransform->Rotation.W =
                            RotationArray[0]->AsNumber();
                        TargetTransform->Rotation.X =
                            RotationArray[1]->AsNumber();
                        TargetTransform->Rotation.Y =
                            RotationArray[2]->AsNumber();
                        TargetTransform->Rotation.Z =
                            RotationArray[3]->AsNumber();

                        UE_LOG(LogTemp, Warning,
                               TEXT("Updated ROTATION for '%s': "
                                    "(%.2f,%.2f,%.2f,%.2f)"),
                               *RealRecorderName, TargetTransform->Rotation.W,
                               TargetTransform->Rotation.X,
                               TargetTransform->Rotation.Y,
                               TargetTransform->Rotation.Z);
                    }
                }
            } else {
                // 🔧 只更新位移数据，保持旋转数据不变
                if (RecorderObject->HasField(TEXT("location"))) {
                    TArray<TSharedPtr<FJsonValue>> LocationArray =
                        RecorderObject->GetArrayField(TEXT("location"));
                    if (LocationArray.Num() == 3) {
                        TargetTransform->Location.X =
                            LocationArray[0]->AsNumber();
                        TargetTransform->Location.Y =
                            LocationArray[1]->AsNumber();
                        TargetTransform->Location.Z =
                            LocationArray[2]->AsNumber();

                        UE_LOG(LogTemp, Warning,
                               TEXT("  📂 Updated LOCATION for '%s': "
                                    "(%.2f,%.2f,%.2f)"),
                               *RealRecorderName, TargetTransform->Location.X,
                               TargetTransform->Location.Y,
                               TargetTransform->Location.Z);
                    }
                }
            }

            ImportedCount++;
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("  ✓ %s imported"), *CategoryName);
}

// 辅助方法：通用的JSON数据处理模板 - 用于导入简单的TMap<FString,
// FString>类型数据
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

        // 优先从name字段获取对象名称，如果不存在则使用键名
        FString ObjName;
        if (ItemObject.IsValid() && ItemObject->HasField(TEXT("name"))) {
            ObjName = ItemObject->GetStringField(TEXT("name"));
        } else {
            // 如果没有name字段，则使用键名作为对象名称
            ObjName = DataPair.Key;
        }

        FRecorderTransform RecorderTransform;

        // 读取位置
        if (ItemObject.IsValid() && ItemObject->HasField(TEXT("location"))) {
            TArray<TSharedPtr<FJsonValue>> LocationArray =
                ItemObject->GetArrayField(TEXT("location"));
            if (LocationArray.Num() == 3) {
                RecorderTransform.Location.X = LocationArray[0]->AsNumber();
                RecorderTransform.Location.Y = LocationArray[1]->AsNumber();
                RecorderTransform.Location.Z = LocationArray[2]->AsNumber();
            }
        }

        // 读取旋转
        if (ItemObject.IsValid() &&
            ItemObject->HasField(TEXT("rotation_quaternion"))) {
            TArray<TSharedPtr<FJsonValue>> RotationArray =
                ItemObject->GetArrayField(TEXT("rotation_quaternion"));
            if (RotationArray.Num() == 4) {
                RecorderTransform.Rotation.W = RotationArray[0]->AsNumber();
                RecorderTransform.Rotation.X = RotationArray[1]->AsNumber();
                RecorderTransform.Rotation.Y = RotationArray[2]->AsNumber();
                RecorderTransform.Rotation.Z = RotationArray[3]->AsNumber();
            }
        }

        KeyRippleActor->RecorderTransforms.Add(ObjName, RecorderTransform);
        ImportedCount++;
    }
    UE_LOG(LogTemp, Warning, TEXT("  ✓ %s imported"), *CategoryName);
}

// 辅助方法：导入配置参数
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

    ImportedCount++;  // 计为一个整体导入项
    UE_LOG(LogTemp, Warning, TEXT("  ✓ Config parameters imported"));
}

// 辅助方法：获取所有控制器名称的集合
static TSet<FString> GetAllControllerNames(AKeyRippleUnreal* KeyRippleActor) {
    TSet<FString> AllControllerNames;

    // 添加手指控制器
    for (const auto& Pair : KeyRippleActor->FingerControllers) {
        AllControllerNames.Add(Pair.Value);
    }

    // 添加手部控制器
    for (const auto& Pair : KeyRippleActor->HandControllers) {
        AllControllerNames.Add(Pair.Value);
    }

    // 添加键板位置
    for (const auto& Pair : KeyRippleActor->KeyBoardPositions) {
        AllControllerNames.Add(Pair.Value);
    }

    // 添加引导线
    for (const auto& Pair : KeyRippleActor->Guidelines) {
        AllControllerNames.Add(Pair.Value);
    }

    // 添加目标点
    for (const auto& Pair : KeyRippleActor->TargetPoints) {
        AllControllerNames.Add(Pair.Value);
    }

    // 添加肩膀控制器
    for (const auto& Pair : KeyRippleActor->ShoulderControllers) {
        AllControllerNames.Add(Pair.Value);
    }

    // 添加 Pole Points
    for (const auto& Pair : KeyRippleActor->PolePoints) {
        AllControllerNames.Add(Pair.Value);
    }

    return AllControllerNames;
}

// 辅助方法：为与状态相关的控制器生成所有可能的记录器名称
static TArray<FString> GenerateStateDependentRecorders(
    AKeyRippleUnreal* KeyRippleActor, const FString& ControllerName) {
    TArray<FString> Result;

    // 为每种可能的状态组合生成记录器名称
    for (EPositionType PositionType :
         {EPositionType::HIGH, EPositionType::LOW, EPositionType::MIDDLE}) {
        for (EKeyType KeyType : {EKeyType::WHITE, EKeyType::BLACK}) {
            FString PositionStr =
                (PositionType == EPositionType::HIGH)  ? TEXT("high")
                : (PositionType == EPositionType::LOW) ? TEXT("low")
                                                       : TEXT("middle");

            FString KeyStr =
                (KeyType == EKeyType::WHITE) ? TEXT("white") : TEXT("black");

            FString RecorderName = FString::Printf(
                TEXT("%s_%s_%s"), *PositionStr, *KeyStr, *ControllerName);
            Result.Add(RecorderName);
        }
    }

    return Result;
}

// 辅助方法：初始化RecorderTransforms

static void InitializeControllerRecorderItem(AKeyRippleUnreal* KeyRippleActor,
                                             const FString& RecorderName) {
    FRecorderTransform DefaultTransform;
    DefaultTransform.Location = FVector::ZeroVector;
    DefaultTransform.Rotation = FQuat::Identity;

    KeyRippleActor->RecorderTransforms.Add(RecorderName, DefaultTransform);
}

// 辅助方法：为指定的控制器集合添加记录器变换数据
static void AddControllerRecordersToTransforms(
    AKeyRippleUnreal* KeyRippleActor, const TMap<FString, FString>& Controllers,
    bool bIsStateDependent) {
    for (const auto& ControllerPair : Controllers) {
        FString ControllerName = ControllerPair.Value;

        if (bIsStateDependent) {
            // 与状态相关的控制器：为每个控制器生成所有状态组合的记录器
            TArray<FString> StateRecorders =
                GenerateStateDependentRecorders(KeyRippleActor, ControllerName);

            for (const FString& RecorderName : StateRecorders) {
                FRecorderTransform DefaultTransform;
                InitializeControllerRecorderItem(KeyRippleActor, RecorderName);
            }
        } else {
            InitializeControllerRecorderItem(KeyRippleActor, ControllerName);
        }
    }
}

// 辅助方法：初始化RecoderTransform数据 - 按两大类处理
static void InitializeRecorderTransforms(AKeyRippleUnreal* KeyRippleActor) {
    // 清空现有的RecorderTransforms数据
    KeyRippleActor->RecorderTransforms.Empty();

    // 第一类：与状态相关的控制器
    // 包括：FingerControllers, HandControllers, ShoulderControllers,
    // TargetPoints

    // 1. FingerControllers - 为每个手指控制器生成所有状态组合的记录器
    AddControllerRecordersToTransforms(KeyRippleActor,
                                       KeyRippleActor->FingerControllers, true);

    // 2. HandControllers - 为每个手部控制器生成所有状态组合的记录器
    AddControllerRecordersToTransforms(KeyRippleActor,
                                       KeyRippleActor->HandControllers, true);

    // 3. ShoulderControllers - 为每个肩膊控制器生成所有状态组合的记录器
    AddControllerRecordersToTransforms(
        KeyRippleActor, KeyRippleActor->ShoulderControllers, true);

    // 4. TargetPoints - 为每个目标点生成所有状态组合的记录器
    AddControllerRecordersToTransforms(KeyRippleActor,
                                       KeyRippleActor->TargetPoints, true);

    // 第二类：与状态无关的控制器
    // 包括：KeyBoardPositions, Guidelines

    // 5. KeyBoardPositions - 记录器名称与控制器名称相同
    AddControllerRecordersToTransforms(
        KeyRippleActor, KeyRippleActor->KeyBoardPositions, false);

    // 6. Guidelines - 记录器名称与控制器名称相同
    AddControllerRecordersToTransforms(KeyRippleActor,
                                       KeyRippleActor->Guidelines, false);
}

// 辅助方法：通用的控制器Transform保存函数
static void SaveControllerTransform(AKeyRippleUnreal* KeyRippleActor,
                                    URigHierarchy* RigHierarchy,
                                    const FString& ControlName,
                                    const FString& RecorderName,
                                    int32& SavedCount, int32& FailedCount) {
    // 🔧 DEBUG: 添加保存操作的详细日志
    UE_LOG(LogTemp, Warning,
           TEXT("SaveControllerTransform: Control='%s' -> Recorder='%s'"),
           *ControlName, *RecorderName);

    FRigElementKey ControlKey(*ControlName, ERigElementType::Control);
    if (RigHierarchy->Contains(ControlKey)) {
        FRigControlElement* ControlElement =
            RigHierarchy->Find<FRigControlElement>(ControlKey);
        if (ControlElement) {
            FRigControlValue CurrentValue = RigHierarchy->GetControlValue(
                ControlElement, ERigControlValueType::Current);
            FTransform CurrentTransform = CurrentValue.GetAsTransform(
                ControlElement->Settings.ControlType,
                ControlElement->Settings.PrimaryAxis);

            FRecorderTransform RecorderTransform;
            RecorderTransform.FromTransform(CurrentTransform);

            KeyRippleActor->RecorderTransforms.Add(RecorderName,
                                                   RecorderTransform);

            // 🔧 DEBUG: 记录成功保存的详细信息
            UE_LOG(LogTemp, Warning,
                   TEXT("  SAVED: '%s' at Pos(%.2f,%.2f,%.2f) "
                        "Rot(%.2f,%.2f,%.2f,%.2f)"),
                   *RecorderName, CurrentTransform.GetLocation().X,
                   CurrentTransform.GetLocation().Y,
                   CurrentTransform.GetLocation().Z,
                   CurrentTransform.GetRotation().W,
                   CurrentTransform.GetRotation().X,
                   CurrentTransform.GetRotation().Y,
                   CurrentTransform.GetRotation().Z);

            SavedCount++;
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("  ✗ Failed to get ControlElement for: %s"),
                   *ControlName);
            FailedCount++;
        }
    } else {
        UE_LOG(LogTemp, Warning, TEXT("  ✗ Control not found: %s"),
               *ControlName);
        FailedCount++;
    }
}

// 辅助方法：通用的控制器Transform加载函数
static void LoadControllerTransform(AKeyRippleUnreal* KeyRippleActor,
                                    URigHierarchy* RigHierarchy,
                                    const FString& ControlName,
                                    const FString& ExpectedRecorderName,
                                    int32& LoadedCount, int32& FailedCount) {
    // 🔧 DEBUG: 添加加载操作的详细日志
    UE_LOG(LogTemp, Warning,
           TEXT("LoadControllerTransform: Control='%s' <- Expected "
                "Recorder='%s'"),
           *ControlName, *ExpectedRecorderName);

    const FRecorderTransform* FoundTransform =
        KeyRippleActor->RecorderTransforms.Find(ExpectedRecorderName);

    if (!FoundTransform) {
        UE_LOG(LogTemp, Warning,
               TEXT("MISSING: Expected recorder not in data table: %s"),
               *ExpectedRecorderName);

        FailedCount++;
        return;
    }

    // 🔧 DEBUG: 记录找到的Transform数据
    FTransform LoadTransform = FoundTransform->ToTransform();
    UE_LOG(LogTemp, Warning,
           TEXT("FOUND: '%s' with Pos(%.2f,%.2f,%.2f) "
                "Rot(%.2f,%.2f,%.2f,%.2f)"),
           *ExpectedRecorderName, LoadTransform.GetLocation().X,
           LoadTransform.GetLocation().Y, LoadTransform.GetLocation().Z,
           LoadTransform.GetRotation().W, LoadTransform.GetRotation().X,
           LoadTransform.GetRotation().Y, LoadTransform.GetRotation().Z);

    FRigElementKey ControlKey(*ControlName, ERigElementType::Control);
    if (RigHierarchy->Contains(ControlKey)) {
        FRigControlElement* ControlElement =
            RigHierarchy->Find<FRigControlElement>(ControlKey);
        if (ControlElement) {
            FTransform NewTransform = FoundTransform->ToTransform();

            FRigControlValue NewValue;
            NewValue.SetFromTransform(NewTransform,
                                      ControlElement->Settings.ControlType,
                                      ControlElement->Settings.PrimaryAxis);

            RigHierarchy->SetControlValue(ControlElement, NewValue,
                                          ERigControlValueType::Current);

            // 🔧 DEBUG: 记录成功加载的详细信息
            UE_LOG(LogTemp, Warning,
                   TEXT("LOADED: Applied transform to control '%s'"),
                   *ControlName);

            LoadedCount++;
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("Failed to get ControlElement for: %s"), *ControlName);
            FailedCount++;
        }
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Control not found: %s"), *ControlName);
        FailedCount++;
    }
}

// 辅助方法：保存与状态相关的控制器的Transform
static void SaveControllers(AKeyRippleUnreal* KeyRippleActor,
                            URigHierarchy* RigHierarchy,
                            const TMap<FString, FString>& Controllers,
                            int32& SavedCount, int32& FailedCount,
                            bool bIsFingerControl = true,
                            bool isStateDependent = true) {
    for (const auto& ControllerPair : Controllers) {
        FString ControlName = ControllerPair.Value;
        FString RecorderName =
            isStateDependent
                ? UKeyRippleControlRigProcessor::GetRecorderNameForControl(
                      KeyRippleActor, ControlName, bIsFingerControl)
                : ControlName;

        SaveControllerTransform(KeyRippleActor, RigHierarchy, ControlName,
                                RecorderName, SavedCount, FailedCount);
    }
}

// 辅助方法：加载与状态相关的控制器的Transform
static void LoadControllers(AKeyRippleUnreal* KeyRippleActor,
                            URigHierarchy* RigHierarchy,
                            const TMap<FString, FString>& Controllers,
                            int32& LoadedCount, int32& FailedCount,
                            bool bIsFingerControl = true,
                            bool isStateDependent = true) {
    for (const auto& ControllerPair : Controllers) {
        FString ControlName = ControllerPair.Value;

        FString ExpectedRecorderName =
            isStateDependent
                ? UKeyRippleControlRigProcessor::GetRecorderNameForControl(
                      KeyRippleActor, ControlName, bIsFingerControl)
                : ControlName;

        LoadControllerTransform(KeyRippleActor, RigHierarchy, ControlName,
                                ExpectedRecorderName, LoadedCount, FailedCount);
    }
}

bool UKeyRippleControlRigProcessor::GetControlRigFromSkeletalMeshActor(
    ASkeletalMeshActor* SkeletalMeshActor, UControlRig*& OutControlRigInstance,
    UControlRigBlueprint*& OutControlRigBlueprint) {
    OutControlRigInstance = nullptr;
    OutControlRigBlueprint = nullptr;

    if (!SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("SkeletalMeshActor is null in "
                    "GetControlRigFromSkeletalMeshActor"));
        return false;
    }

    // 从当前打开的Level Sequence中获取绑定的Control Rig
    ULevelSequence* LevelSequence = nullptr;

    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
        TArray<TWeakPtr<ISequencer>> WeakSequencers =
            FLevelEditorSequencerIntegration::Get().GetSequencers();

        TArray<TSharedPtr<ISequencer>> OpenSequencers;
        for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
            if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin()) {
                OpenSequencers.Add(Sequencer);
            }
        }

        for (const TSharedPtr<ISequencer>& Sequencer : OpenSequencers) {
            UMovieSceneSequence* RootSequence =
                Sequencer->GetRootMovieSceneSequence();

            if (!RootSequence) {
                continue;
            }

            LevelSequence = Cast<ULevelSequence>(RootSequence);
            if (LevelSequence) {
                break;
            }
        }
    } else {
        UE_LOG(LogTemp, Warning, TEXT("LevelEditor module is not loaded"));
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("LevelSequence is null - cannot get control rigs"));
    }

    TArray<FControlRigSequencerBindingProxy> RigBindings =
        UControlRigSequencerEditorLibrary::GetControlRigs(LevelSequence);

    if (RigBindings.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("No Control Rig bindings found in the sequence"));
        return false;
    }

    for (int32 RigIndex = 0; RigIndex < RigBindings.Num(); ++RigIndex) {
        const FControlRigSequencerBindingProxy& Proxy = RigBindings[RigIndex];

        UControlRig* CurrentControlRigInstance = Proxy.ControlRig;

        if (!CurrentControlRigInstance) {
            continue;
        }

        // 检查该绑定是否关联到SkeletalMeshActor
        if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
            TArray<TWeakPtr<ISequencer>> WeakSequencers =
                FLevelEditorSequencerIntegration::Get().GetSequencers();

            for (int32 SeqIndex = 0; SeqIndex < WeakSequencers.Num();
                 ++SeqIndex) {
                const TWeakPtr<ISequencer>& WeakSequencer =
                    WeakSequencers[SeqIndex];

                if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin()) {
                    // 从Proxy获取绑定ID
                    FGuid BindingID = Proxy.Proxy.BindingID;

                    if (!BindingID.IsValid()) {
                        continue;
                    }

                    TArray<UObject*> BoundObjects;

                    TArrayView<TWeakObjectPtr<UObject>> WeakBoundObjects =
                        Sequencer->FindBoundObjects(
                            BindingID, Sequencer->GetFocusedTemplateID());

                    for (int32 ObjIndex = 0; ObjIndex < WeakBoundObjects.Num();
                         ++ObjIndex) {
                        const TWeakObjectPtr<UObject>& WeakObj =
                            WeakBoundObjects[ObjIndex];

                        if (WeakObj.IsValid()) {
                            UObject* BoundObj = WeakObj.Get();
                            BoundObjects.Add(BoundObj);
                        }
                    }

                    if (BoundObjects.Contains(SkeletalMeshActor)) {
                        OutControlRigInstance = CurrentControlRigInstance;

                        // 获取 Blueprint 资产
                        UObject* GeneratedBy =
                            OutControlRigInstance->GetClass()->ClassGeneratedBy;

                        OutControlRigBlueprint =
                            Cast<UControlRigBlueprint>(GeneratedBy);

                        if (OutControlRigBlueprint) {
                            return true;
                        } else {
                            UE_LOG(LogTemp, Warning,
                                   TEXT("Failed to cast GeneratedBy to "
                                        "UControlRigBlueprint, type: %s"),
                                   GeneratedBy
                                       ? *GeneratedBy->GetClass()->GetName()
                                       : TEXT("null"));
                            return false;
                        }
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Failed to find Control Rig bound to SkeletalMeshActor: %s"),
           *SkeletalMeshActor->GetName());
    return false;
}

// 工具方法：从Recorder名字中提取Control名字
FString UKeyRippleControlRigProcessor::GetControlNameFromRecorder(
    const FString& RecorderName) {
    // RecorderName 格式: position_type_key_type_control_name (例如:
    // middle_white_0_L) 我们需要提取最后部分（control_name）

    // 查找第一个下划线的位置
    int32 FirstUnderscore = RecorderName.Find(TEXT("_"));
    if (FirstUnderscore == INDEX_NONE) {
        // 如果没有下划线，说明整个名字就是control name
        return RecorderName;
    }

    // 查找第二个下划线的位置
    int32 SecondUnderscore =
        RecorderName.Find(TEXT("_"), ESearchCase::CaseSensitive,
                          ESearchDir::FromStart, FirstUnderscore + 1);
    if (SecondUnderscore == INDEX_NONE) {
        // 如果只有1个下划线，也说明整个名字都是control name
        return RecorderName;
    }

    // 有至少两个下划线，第三个部分（control_name）是从第二个下划线之后的内容
    return RecorderName.Mid(SecondUnderscore + 1);
}

void UKeyRippleControlRigProcessor::CheckObjectsStatus(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("CheckObjectsStatus"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigInstanceAndBlueprint(KeyRippleActor, ControlRigInstance,
                                           ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    // 这里可以扩展与ControlRig相关的检查逻辑
    TSet<FString> ExpectedObjects;

    for (const auto& Pair : KeyRippleActor->FingerControllers) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : KeyRippleActor->HandControllers) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : KeyRippleActor->KeyBoardPositions) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : KeyRippleActor->Guidelines) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : KeyRippleActor->TargetPoints) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : KeyRippleActor->ShoulderControllers) {
        ExpectedObjects.Add(Pair.Value);
    }

    // 检查这些对象在Control Rig层级结构中的存在状态
    TArray<FString> ExistingObjects;
    TArray<FString> MissingObjects;

    if (ControlRigBlueprint) {
        // 将ControlRig转换为UControlRigBlueprint类型
        // ControlRigBlueprint 已经通过上面的逻辑获取
        if (ControlRigBlueprint) {
            URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
            if (RigHierarchy) {
                for (const FString& ObjectName : ExpectedObjects) {
                    // 首先尝试Control类型
                    bool bFound = false;
                    FRigElementKey ElementKey(*ObjectName,
                                              ERigElementType::Control);

                    if (RigHierarchy->Contains(ElementKey)) {
                        ExistingObjects.Add(ObjectName);
                        bFound = true;
                    } else {
                        // 如果不是Control，尝试Bone类型
                        ElementKey.Type = ERigElementType::Bone;
                        if (RigHierarchy->Contains(ElementKey)) {
                            ExistingObjects.Add(ObjectName);
                            bFound = true;
                        }
                    }

                    if (!bFound) {
                        MissingObjects.Add(ObjectName);
                    }
                }
            }
        }

        UE_LOG(LogTemp, Warning,
               TEXT("KeyRipple 对象状态报告 (Control Rig 版本)"));
        UE_LOG(LogTemp, Warning, TEXT("========================"));
        UE_LOG(LogTemp, Warning, TEXT("预期对象总数: %d"),
               ExpectedObjects.Num());
        UE_LOG(LogTemp, Warning, TEXT("存在的对象数量: %d"),
               ExistingObjects.Num());
        UE_LOG(LogTemp, Warning, TEXT("缺失的对象数量: %d"),
               MissingObjects.Num());

        if (ExistingObjects.Num() > 0) {
            UE_LOG(LogTemp, Warning, TEXT("存在的对象:"));
            for (const FString& ObjName : ExistingObjects) {
                UE_LOG(LogTemp, Warning, TEXT("  - %s"), *ObjName);
            }
        }

        if (MissingObjects.Num() > 0) {
            UE_LOG(LogTemp, Warning, TEXT("缺失的对象:"));
            for (const FString& ObjName : MissingObjects) {
                UE_LOG(LogTemp, Warning, TEXT("  - %s"), *ObjName);
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("========================"));
    }
}

void UKeyRippleControlRigProcessor::SetupAllObjects(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("SetupAllObjects"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigInstanceAndBlueprint(KeyRippleActor, ControlRigInstance,
                                           ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    // 设置所有对象
    SetupControllers(KeyRippleActor);
    SetupRecorders(KeyRippleActor);  // 初始化recorder transforms数据表

    UE_LOG(LogTemp, Warning, TEXT("All KeyRipple objects have been set up"));
}

void UKeyRippleControlRigProcessor::SaveState(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("SaveState"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigInstanceAndBlueprint(KeyRippleActor, ControlRigInstance,
                                           ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(
            LogTemp, Error,
            TEXT("Failed to get Control Rig Instance from SkeletalMeshActor"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigInstance"));
        return;
    }

    LogStandardStart(TEXT("SaveState"));

    // 输出KeyRippleUnreal的当前状态
    UE_LOG(LogTemp, Warning,
           TEXT("========== KeyRippleUnreal Current Status =========="));
    UE_LOG(LogTemp, Warning, TEXT("Left Hand:"));
    UE_LOG(LogTemp, Warning, TEXT("  Key Type: %s"),
           KeyRippleActor->LeftHandKeyType == EKeyType::WHITE ? TEXT("WHITE")
                                                              : TEXT("BLACK"));
    UE_LOG(LogTemp, Warning, TEXT("  Position Type: %s"),
           KeyRippleActor->LeftHandPositionType == EPositionType::HIGH
               ? TEXT("HIGH")
               : (KeyRippleActor->LeftHandPositionType == EPositionType::LOW
                      ? TEXT("LOW")
                      : TEXT("MIDDLE")));

    UE_LOG(LogTemp, Warning, TEXT("Right Hand:"));
    UE_LOG(LogTemp, Warning, TEXT("  Key Type: %s"),
           KeyRippleActor->RightHandKeyType == EKeyType::WHITE ? TEXT("WHITE")
                                                               : TEXT("BLACK"));
    UE_LOG(LogTemp, Warning, TEXT("  Position Type: %s"),
           KeyRippleActor->RightHandPositionType == EPositionType::HIGH
               ? TEXT("HIGH")
               : (KeyRippleActor->RightHandPositionType == EPositionType::LOW
                      ? TEXT("LOW")
                      : TEXT("MIDDLE")));
    UE_LOG(LogTemp, Warning, TEXT("========== End Status =========="));

    int32 SavedCount = 0;
    int32 FailedCount = 0;

    // 第一类：与状态相关的控制器
    UE_LOG(LogTemp, Warning, TEXT("Processing state-dependent controllers..."));

    ControlRigInstance->Evaluate_AnyThread();

    // 1. FingerControllers - 与状态相关，使用true表示是手指控制器
    SaveControllers(KeyRippleActor, RigHierarchy,
                    KeyRippleActor->FingerControllers, SavedCount, FailedCount,
                    true, true);

    // 2. HandControllers - 与状态相关，使用false表示不是手指控制器
    SaveControllers(KeyRippleActor, RigHierarchy,
                    KeyRippleActor->HandControllers, SavedCount, FailedCount,
                    false, true);

    // 3. ShoulderControllers - 与状态相关，使用false表示不是手指控制器
    SaveControllers(KeyRippleActor, RigHierarchy,
                    KeyRippleActor->ShoulderControllers, SavedCount,
                    FailedCount, false, true);

    // 4. TargetPoints - 与状态相关，使用false表示不是手指控制器
    SaveControllers(KeyRippleActor, RigHierarchy, KeyRippleActor->TargetPoints,
                    SavedCount, FailedCount, false, true);

    // 第二类：与状态无关的控制器
    UE_LOG(LogTemp, Warning,
           TEXT("Processing state-independent controllers..."));

    // 5. KeyBoardPositions - 与状态无关
    SaveControllers(KeyRippleActor, RigHierarchy,
                    KeyRippleActor->KeyBoardPositions, SavedCount, FailedCount,
                    false, false);

    // 6. Guidelines - 与状态无关
    SaveControllers(KeyRippleActor, RigHierarchy, KeyRippleActor->Guidelines,
                    SavedCount, FailedCount, false, false);

    LogStandardEnd(TEXT("SaveState"), SavedCount, FailedCount,
                   KeyRippleActor->RecorderTransforms.Num());

    // 标记包为dirty，以便在保存时包含修改后的数据
    if (KeyRippleActor) {
        KeyRippleActor->MarkPackageDirty();
    }
}

void UKeyRippleControlRigProcessor::LoadState(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("LoadState"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigInstanceAndBlueprint(KeyRippleActor, ControlRigInstance,
                                           ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigInstance"));
        return;
    }

    LogStandardStart(TEXT("LoadState"));

    // 输出KeyRippleUnreal的当前状态
    UE_LOG(LogTemp, Warning,
           TEXT("========== KeyRippleUnreal Current Status =========="));
    UE_LOG(LogTemp, Warning, TEXT("Left Hand:"));
    UE_LOG(LogTemp, Warning, TEXT("  Key Type: %s"),
           KeyRippleActor->LeftHandKeyType == EKeyType::WHITE ? TEXT("WHITE")
                                                              : TEXT("BLACK"));
    UE_LOG(LogTemp, Warning, TEXT("  Position Type: %s"),
           KeyRippleActor->LeftHandPositionType == EPositionType::HIGH
               ? TEXT("HIGH")
               : (KeyRippleActor->LeftHandPositionType == EPositionType::LOW
                      ? TEXT("LOW")
                      : TEXT("MIDDLE")));

    UE_LOG(LogTemp, Warning, TEXT("Right Hand:"));
    UE_LOG(LogTemp, Warning, TEXT("  Key Type: %s"),
           KeyRippleActor->RightHandKeyType == EKeyType::WHITE ? TEXT("WHITE")
                                                               : TEXT("BLACK"));
    UE_LOG(LogTemp, Warning, TEXT("  Position Type: %s"),
           KeyRippleActor->RightHandPositionType == EPositionType::HIGH
               ? TEXT("HIGH")
               : (KeyRippleActor->RightHandPositionType == EPositionType::LOW
                      ? TEXT("LOW")
                      : TEXT("MIDDLE")));
    UE_LOG(LogTemp, Warning, TEXT("========== End Status =========="));

    int32 LoadedCount = 0;
    int32 FailedCount = 0;

    // 第一类：与状态相关的控制器
    UE_LOG(LogTemp, Warning, TEXT("Loading state-dependent controllers..."));

    // 1. FingerControllers - 与状态相关，使用true表示是手指控制器
    LoadControllers(KeyRippleActor, RigHierarchy,
                    KeyRippleActor->FingerControllers, LoadedCount, FailedCount,
                    true, true);

    // 2. HandControllers - 与状态相关，使用false表示不是手指控制器
    LoadControllers(KeyRippleActor, RigHierarchy,
                    KeyRippleActor->HandControllers, LoadedCount, FailedCount,
                    false, true);

    // 3. ShoulderControllers - 与状态相关，使用false表示不是手指控制器
    LoadControllers(KeyRippleActor, RigHierarchy,
                    KeyRippleActor->ShoulderControllers, LoadedCount,
                    FailedCount, false, true);

    // 4. TargetPoints - 与状态相关，使用false表示不是手指控制器
    LoadControllers(KeyRippleActor, RigHierarchy, KeyRippleActor->TargetPoints,
                    LoadedCount, FailedCount, false, true);

    // 第二类：与状态无关的控制器
    UE_LOG(LogTemp, Warning, TEXT("Loading state-independent controllers..."));

    // 5. KeyBoardPositions - 与状态无关
    LoadControllers(KeyRippleActor, RigHierarchy,
                    KeyRippleActor->KeyBoardPositions, LoadedCount, FailedCount,
                    false, false);

    // 6. Guidelines - 与状态无关
    LoadControllers(KeyRippleActor, RigHierarchy, KeyRippleActor->Guidelines,
                    LoadedCount, FailedCount, false, false);

    LogStandardEnd(TEXT("LoadState"), LoadedCount, FailedCount,
                   KeyRippleActor->RecorderTransforms.Num());
}

void UKeyRippleControlRigProcessor::ExportRecorderInfo(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("ExportRecorderInfo"))) {
        return;
    }

    // 验证 IO 文件路径
    if (KeyRippleActor->IOFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("IOFilePath is empty, cannot export recorder info"));
        return;
    }

    // 构建输出文件路径
    FString OutputFilePath =
        FString::Printf(TEXT("%s"), *KeyRippleActor->IOFilePath);
    UE_LOG(LogTemp, Warning, TEXT("Exporting to file: %s"), *OutputFilePath);

    // 创建 JSON 对象
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    // 保存配置参数
    UE_LOG(LogTemp, Warning, TEXT("Exporting config parameters..."));
    TSharedPtr<FJsonObject> ConfigObject = MakeShareable(new FJsonObject);
    ConfigObject->SetNumberField(TEXT("one_hand_finger_number"),
                                 KeyRippleActor->OneHandFingerNumber);
    ConfigObject->SetNumberField(TEXT("leftest_position"),
                                 KeyRippleActor->LeftestPosition);
    ConfigObject->SetNumberField(TEXT("left_position"),
                                 KeyRippleActor->LeftPosition);
    ConfigObject->SetNumberField(TEXT("middle_left_position"),
                                 KeyRippleActor->MiddleLeftPosition);
    ConfigObject->SetNumberField(TEXT("middle_right_position"),
                                 KeyRippleActor->MiddleRightPosition);
    ConfigObject->SetNumberField(TEXT("right_position"),
                                 KeyRippleActor->RightPosition);
    ConfigObject->SetNumberField(TEXT("rightest_position"),
                                 KeyRippleActor->RightestPosition);
    ConfigObject->SetNumberField(TEXT("min_key"), KeyRippleActor->MinKey);
    ConfigObject->SetNumberField(TEXT("max_key"), KeyRippleActor->MaxKey);
    ConfigObject->SetNumberField(TEXT("hand_range"), KeyRippleActor->HandRange);
    JsonObject->SetObjectField(TEXT("config"), ConfigObject);

    // 导出各种记录器信息
    ProcessTransformDataForStringArray(KeyRippleActor, JsonObject,
                                       KeyRippleActor->FingerRecorders,
                                       TEXT("finger_recorders"));
    ProcessTransformDataForStringArray(KeyRippleActor, JsonObject,
                                       KeyRippleActor->HandRecorders,
                                       TEXT("hand_recorders"));
    ProcessTransformDataForStringArray(KeyRippleActor, JsonObject,
                                       KeyRippleActor->ShoulderRecorders,
                                       TEXT("shoulder_recorders"));
    // 添加 TargetPointsRecorders 的导出支持
    ProcessTransformDataForStringArray(KeyRippleActor, JsonObject,
                                       KeyRippleActor->TargetPointsRecorders,
                                       TEXT("target_points_recorders"));
    ProcessTransformData(KeyRippleActor, JsonObject,
                         KeyRippleActor->KeyBoardPositions,
                         TEXT("key_board_positions"));
    ProcessTransformData(KeyRippleActor, JsonObject, KeyRippleActor->Guidelines,
                         TEXT("guidelines"));

    // 将 JSON 对象序列化为字符串
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    // 将字符串写入文件
    if (FFileHelper::SaveStringToFile(OutputString, *OutputFilePath)) {
        UE_LOG(LogTemp, Warning,
               TEXT("Recorder info successfully exported to %s"),
               *OutputFilePath);
    } else {
        UE_LOG(LogTemp, Error, TEXT("Failed to save recorder info to %s"),
               *OutputFilePath);
    }
}

bool UKeyRippleControlRigProcessor::ImportRecorderInfo(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("ImportRecorderInfo"))) {
        return false;
    }

    // 验证 IO 文件路径
    if (KeyRippleActor->IOFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("IOFilePath is empty, cannot import recorder info"));
        return false;
    }

    // 读取JSON文件
    FString InputFilePath = KeyRippleActor->IOFilePath;
    UE_LOG(LogTemp, Warning, TEXT("Importing from file: %s"), *InputFilePath);

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *InputFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *InputFilePath);
        return false;
    }

    // 解析JSON
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

    // 清空现有的RecorderTransforms数据
    KeyRippleActor->RecorderTransforms.Empty();

    int32 ImportedCount = 0;
    int32 FailedCount = 0;

    // 使用辅助方法导入各种类型的数据

    // 1. 导入配置参数
    ProcessImportConfigParameters(KeyRippleActor, JsonObject, ImportedCount,
                                  FailedCount);

    // 2. 导入手指记录器信息
    ProcessImportTransformDataForStringArray(KeyRippleActor, JsonObject,
                                             TEXT("finger_recorders"),
                                             ImportedCount, FailedCount);

    // 3. 导入手掌记录器信息
    ProcessImportTransformDataForStringArray(KeyRippleActor, JsonObject,
                                             TEXT("hand_recorders"),
                                             ImportedCount, FailedCount);

    // 4. 导入肩膀记录器信息
    ProcessImportTransformDataForStringArray(KeyRippleActor, JsonObject,
                                             TEXT("shoulder_recorders"),
                                             ImportedCount, FailedCount);

    // 添加 TargetPointsRecorders 的导入支持
    ProcessImportTransformDataForStringArray(KeyRippleActor, JsonObject,
                                             TEXT("target_points_recorders"),
                                             ImportedCount, FailedCount);

    // 5. 导入键盘基准点信息
    ProcessImportTransformData(KeyRippleActor, JsonObject,
                               TEXT("key_board_positions"), ImportedCount,
                               FailedCount);

    // 6. 导入辅助线信息
    ProcessImportTransformData(KeyRippleActor, JsonObject, TEXT("guidelines"),
                               ImportedCount, FailedCount);

    UE_LOG(LogTemp, Warning,
           TEXT("========== ImportRecorderInfo Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully imported: %d transforms"),
           ImportedCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed to import: %d transforms"),
           FailedCount);
    UE_LOG(LogTemp, Warning, TEXT("Total RecorderTransforms entries: %d"),
           KeyRippleActor->RecorderTransforms.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== ImportRecorderInfo Completed =========="));

    // 标记包为dirty，以便在保存时包含修改后的数据
    if (KeyRippleActor) {
        KeyRippleActor->MarkPackageDirty();
        UE_LOG(LogTemp, Warning,
               TEXT("Marked KeyRippleActor package as dirty for saving"));
    }

    return ImportedCount > 0;
}

AActor* UKeyRippleControlRigProcessor::CreateController(
    AKeyRippleUnreal* KeyRippleActor, const FString& ControllerName,
    bool bIsRotation, const FString& ParentControllerName) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("CreateController"))) {
        return nullptr;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigInstanceAndBlueprint(KeyRippleActor, ControlRigInstance,
                                           ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return nullptr;
    }

    // 获取层级结构和控制器
    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigBlueprint"));
        return nullptr;
    }

    URigHierarchyController* HierarchyController =
        RigHierarchy->GetController();
    if (!HierarchyController) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get hierarchy controller"));
        return nullptr;
    }

    // 🔧 FIX: 使用更严格的存在性检查防止重复创建
    if (StrictControlExistenceCheck(RigHierarchy, ControllerName)) {
        UE_LOG(LogTemp, Warning,
               TEXT("✅ Controller %s already exists (verified)"),
               *ControllerName);
        return nullptr;  // 如果已存在，返回nullptr，因为不需要创建
    }

    // 🔧 FIX: 额外的预防措施 - 再次检查可能的重复
    FRigElementKey ExistingElementKey(*ControllerName,
                                      ERigElementType::Control);
    if (RigHierarchy->Contains(ExistingElementKey)) {
        // 如果通过基础检查发现存在，但严格检查认为不存在，说明可能有损坏的Control
        UE_LOG(LogTemp, Warning,
               TEXT("⚠️ Found potentially corrupted control '%s' - attempting "
                    "cleanup before creation"),
               *ControllerName);

        // 尝试删除损坏的Control
        bool bRemoved =
            HierarchyController->RemoveElement(ExistingElementKey, true, false);
        if (bRemoved) {
            UE_LOG(LogTemp, Warning,
                   TEXT("🧹 Successfully removed corrupted control '%s'"),
                   *ControllerName);
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("❌ Failed to remove corrupted control '%s' - aborting "
                        "creation"),
                   *ControllerName);
            return nullptr;
        }
    }

    // 确定父控制器键
    FRigElementKey ParentKey;
    if (!ParentControllerName.IsEmpty()) {
        // 🔧 FIX: 使用严格检查验证父控制器
        if (StrictControlExistenceCheck(RigHierarchy, ParentControllerName)) {
            FRigElementKey PotentialParentKey(*ParentControllerName,
                                              ERigElementType::Control);
            ParentKey = PotentialParentKey;
            UE_LOG(LogTemp, Warning,
                   TEXT("✅ Using verified parent controller '%s' for '%s'"),
                   *ParentControllerName, *ControllerName);
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("⚠️ Parent controller '%s' does not exist or is "
                        "corrupted, creating child "
                        "controller '%s' without parent"),
                   *ParentControllerName, *ControllerName);
        }
    }

    // 确定控制器类型和形状
    FRigControlSettings ControlSettings;
    ControlSettings.ControlType = ERigControlType::Transform;
    ControlSettings.DisplayName = FName(*ControllerName);

    // 根据控制器名称确定形状
    if (ControllerName.Contains(TEXT("hand"), ESearchCase::IgnoreCase) &&
        !ControllerName.Contains(TEXT("rotation"), ESearchCase::IgnoreCase)) {
        // HandControllers且不包含rotation的使用立方体形状
        ControlSettings.ShapeName = FName(TEXT("Cube"));
    } else if (ControllerName.Contains(TEXT("rotation"),
                                       ESearchCase::IgnoreCase)) {
        // 包含rotation的使用圆形形状
        ControlSettings.ShapeName = FName(TEXT("Circle"));
    } else if (ControllerName.StartsWith(TEXT("pole_"))) {
        // Pole控制器使用钻石形状
        ControlSettings.ShapeName = FName(TEXT("Diamond"));
    } else {
        // 其他控制器使用小球形
        ControlSettings.ShapeName = FName(TEXT("Sphere"));
    }

    FTransform InitialTransform = FTransform::Identity;
    FRigControlValue InitialValue;
    InitialValue.SetFromTransform(InitialTransform, ERigControlType::Transform,
                                  ERigControlAxis::X);

    // 🔧 FIX: 在创建前添加最后一次验证
    FRigElementKey PreCreateCheck(*ControllerName, ERigElementType::Control);
    if (RigHierarchy->Contains(PreCreateCheck)) {
        UE_LOG(LogTemp, Warning,
               TEXT("🚫 Aborting creation: Controller '%s' appeared during "
                    "setup process"),
               *ControllerName);
        return nullptr;
    }

    // 添加控制
    FRigElementKey NewControlKey = HierarchyController->AddControl(
        FName(*ControllerName), ParentKey, ControlSettings, InitialValue,
        FTransform::Identity,  // Offset transform
        FTransform::Identity,  // Shape transform
        true,                  // bSetupUndo
        false                  // bPrintPythonCommand
    );

    if (NewControlKey.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("✅ Successfully created controller: %s"),
               *ControllerName);

        // 🔧 FIX: 创建后验证Control确实存在且正确
        if (!StrictControlExistenceCheck(RigHierarchy, ControllerName)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("⚠️ Created controller '%s' but verification failed - "
                        "may need manual check"),
                   *ControllerName);
        }

        return nullptr;  // 在Control Rig中创建的控制器不需要返回AActor指针
    } else {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create controller: %s"),
               *ControllerName);
        return nullptr;
    }
}

void UKeyRippleControlRigProcessor::SetupTargetActorDriver(
    AKeyRippleUnreal* KeyRippleActor, AActor* TargetActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor,
                                TEXT("SetupTargetActorDriver"))) {
        return;
    }

    UClass* KeyRippleActorClass = KeyRippleActor->GetClass();
    bool bIsControlRigBlueprint =
        KeyRippleActorClass->IsChildOf(UControlRigBlueprint::StaticClass());
    if (!bIsControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleActor is not a UControlRigBlueprint type in "
                    "SetupTargetActorDriver, actual type: %s"),
               *KeyRippleActorClass->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Setting up target actor driver with Control Rig integration"));
}

void UKeyRippleControlRigProcessor::CleanupUnusedActors(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("CleanupUnusedActors"))) {
        return;
    }

    UClass* KeyRippleActorClass = KeyRippleActor->GetClass();
    bool bIsControlRigBlueprint =
        KeyRippleActorClass->IsChildOf(UControlRigBlueprint::StaticClass());
    if (!bIsControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleActor is not a UControlRigBlueprint type in "
                    "CleanupUnusedActors, actual type: %s"),
               *KeyRippleActorClass->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Cleaning up unused actors with Control Rig integration"));
}

// 辅助方法：清理可能损坏或重复的Controls
static void CleanupDuplicateControls(
    AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
    const TSet<FString>& ExpectedControllerNames) {
    if (!RigHierarchy) {
        return;
    }

    URigHierarchyController* HierarchyController =
        RigHierarchy->GetController();
    if (!HierarchyController) {
        UE_LOG(LogTemp, Warning,
               TEXT("Cannot get HierarchyController for cleanup"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Starting cleanup of duplicate/corrupted controls..."));

    // 获取所有现有的Control元素
    TArray<FRigElementKey> ExistingControlKeys =
        RigHierarchy->GetAllKeys(false);
    TArray<FRigElementKey> FilteredControlKeys;

    // 筛选出Control类型的元素
    for (const FRigElementKey& Key : ExistingControlKeys) {
        if (Key.Type == ERigElementType::Control) {
            FilteredControlKeys.Add(Key);
        }
    }

    int32 DuplicatesFound = 0;
    TMap<FString, TArray<FRigElementKey>> ControlGroups;  // 按名称分组

    // 将控件按名称分组，检测重复
    for (const FRigElementKey& ControlKey : FilteredControlKeys) {
        FString ControlName = ControlKey.Name.ToString();

        // 只处理我们期望的控制器
        if (ExpectedControllerNames.Contains(ControlName) ||
            ControlName == TEXT("controller_root")) {
            ControlGroups.FindOrAdd(ControlName).Add(ControlKey);
        } else {
            // 🔧 FIX: 记录非期望的控制器，但不处理它们
            UE_LOG(LogTemp, VeryVerbose,
                   TEXT("Skipping non-expected control '%s' during cleanup"),
                   *ControlName);
        }
    }

    // 对于每个控制器组，如果有多个实例，删除除第一个外的所有实例
    for (const auto& GroupPair : ControlGroups) {
        const FString& ControlName = GroupPair.Key;
        const TArray<FRigElementKey>& ControlInstances = GroupPair.Value;

        if (ControlInstances.Num() > 1) {
            UE_LOG(LogTemp, Warning,
                   TEXT("  🔍 Found %d instances of control '%s' - removing "
                        "duplicates"),
                   ControlInstances.Num(), *ControlName);

            // 保留第一个，删除其余的
            for (int32 i = 1; i < ControlInstances.Num(); i++) {
                bool bRemoved = HierarchyController->RemoveElement(
                    ControlInstances[i], true, false);
                if (bRemoved) {
                    UE_LOG(LogTemp, Warning,
                           TEXT("    ✅ Removed duplicate control '%s' "
                                "instance %d"),
                           *ControlName, i + 1);
                    DuplicatesFound++;
                } else {
                    UE_LOG(LogTemp, Warning,
                           TEXT("    ❌ Failed to remove duplicate control "
                                "'%s' instance %d"),
                           *ControlName, i + 1);
                }
            }
        }
    }

    if (DuplicatesFound > 0) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("Cleanup completed: Removed %d duplicate control instances"),
            DuplicatesFound);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Cleanup completed: No duplicates found"));
    }
}

void UKeyRippleControlRigProcessor::SetupControllers(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("SetupControllers"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigInstanceAndBlueprint(KeyRippleActor, ControlRigInstance,
                                           ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "SkeletalMeshActor"));
        return;
    }

    // 获取层级结构
    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigBlueprint"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Setting up controllers with Control Rig integration"));

    // 🔧 FIX: 步骤 0 - 在开始之前清理任何重复的Controls
    TSet<FString> AllControllerNames = GetAllControllerNames(KeyRippleActor);
    CleanupDuplicateControls(KeyRippleActor, RigHierarchy, AllControllerNames);

    // 首先确保 controller_root 存在
    FRigElementKey RootControllerKey(TEXT("controller_root"),
                                     ERigElementType::Control);
    bool bRootExists =
        StrictControlExistenceCheck(RigHierarchy, TEXT("controller_root"));

    if (!bRootExists) {
        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "Root controller controller_root does not exist, creating..."));
        CreateController(KeyRippleActor, TEXT("controller_root"), false,
                         TEXT(""));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("Root controller controller_root already exists"));
    }

    // 🔧 FIX: 将TSet转换为TArray并排序，确保pole控制器最后处理
    TArray<FString> SortedControllerNames = AllControllerNames.Array();
    SortedControllerNames.Sort([](const FString& A, const FString& B) {
        // pole控制器排在后面
        bool bAIsPole = A.StartsWith(TEXT("pole_"));
        bool bBIsPole = B.StartsWith(TEXT("pole_"));

        if (bAIsPole && !bBIsPole) return false;  // A是pole，B不是，A排后面
        if (!bAIsPole && bBIsPole) return true;   // A不是pole，B是，A排前面

        return A < B;  // 都是pole或都不是pole，按字典序排列
    });

    // 遍历所有其他控制器名称，检查是否存在，如果不存在则创建
    for (const FString& ControllerName : SortedControllerNames) {
        // 🔧 FIX: 使用更严格的存在性检查
        bool bExists =
            StrictControlExistenceCheck(RigHierarchy, ControllerName);

        if (!bExists) {
            // 如果不存在，则创建控制器
            UE_LOG(LogTemp, Warning,
                   TEXT("Controller %s does not exist, creating as child of "
                        "controller_root..."),
                   *ControllerName);

            // 确定父控制器并创建控制器
            FString ParentControllerName = TEXT("controller_root");

            // 检查是否为 pole 控制器
            if (ControllerName.StartsWith(TEXT("pole_"))) {
                // 这是一个 pole 控制器，需要找到对应的手指控制器作为父级
                FString PoleFingerNumber =
                    ControllerName.Mid(5);  // 去掉 "pole_" 前缀

                // 查找对应的手指控制器
                for (const auto& FingerPair :
                     KeyRippleActor->FingerControllers) {
                    if (FingerPair.Key == PoleFingerNumber) {
                        ParentControllerName =
                            FingerPair.Value;  // 例如 "0_L" 或 "5_R"
                        UE_LOG(LogTemp, Warning,
                               TEXT("Found finger controller %s as parent for "
                                    "pole %s"),
                               *ParentControllerName, *ControllerName);
                        break;
                    }
                }
            }

            CreateController(KeyRippleActor, ControllerName,
                             ControllerName.Contains(TEXT("rotation"),
                                                     ESearchCase::IgnoreCase),
                             ParentControllerName);
        } else {
            UE_LOG(LogTemp, Warning, TEXT("✅ Controller %s already exists"),
                   *ControllerName);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Finished setting up controllers"));
}

void UKeyRippleControlRigProcessor::SetupRecorders(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!ValidateKeyRippleActor(KeyRippleActor, TEXT("SetupRecorders"))) {
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Initializing recorder transforms data table"));

    // 使用新的重构方法初始化RecorderTransforms，按两大类处理
    InitializeRecorderTransforms(KeyRippleActor);

    UE_LOG(LogTemp, Warning,
           TEXT("Recorder transforms data table initialized with %d entries"),
           KeyRippleActor->RecorderTransforms.Num());
}

#undef LOCTEXT_NAMESPACE