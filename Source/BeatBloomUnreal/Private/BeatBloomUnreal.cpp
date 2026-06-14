#include "BeatBloomUnreal.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

ABeatBloomUnreal::ABeatBloomUnreal() {
    PrimaryActorTick.bCanEverTick = true;

    CurrentLeftHandState = EBeatBloomState::BEAT;
    CurrentRightHandState = EBeatBloomState::BEAT;
    CurrentLeftFootState = EBeatBloomState::BEAT;
    CurrentRightFootState = EBeatBloomState::BEAT;

    bIsInitialized = false;

    InitializeControllersAndRecorders();
}

void ABeatBloomUnreal::BeginPlay() {
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("BeatBloomUnreal: BeginPlay called"));
}

void ABeatBloomUnreal::Tick(float DeltaTime) {
}

void ABeatBloomUnreal::InitializeControllersAndRecorders() {
    // 初始化固定的控制器映射
    // 参考设计文档 01_BeatBloomUnreal_CoreActor.md 第三节
    
    // 手部控制器（与 FretDance 类似）
    HandControllers.Empty();
    HandControllers.Add(TEXT("left_hand_controller"), TEXT("H_L"));
    HandControllers.Add(TEXT("right_hand_controller"), TEXT("H_R"));
    HandControllers.Add(TEXT("left_hand_IK_pivot"), TEXT("HP_L"));
    HandControllers.Add(TEXT("right_hand_IK_pivot"), TEXT("HP_R"));
    HandControllers.Add(TEXT("left_hand_rotation"), TEXT("H_rotation_L"));
    HandControllers.Add(TEXT("right_hand_rotation"), TEXT("H_rotation_R"));

    // 脚部控制器（BeatBloom 独有）
    FootControllers.Empty();
    FootControllers.Add(TEXT("left_foot_controller"), TEXT("F_L"));
    FootControllers.Add(TEXT("right_foot_controller"), TEXT("F_R"));
    FootControllers.Add(TEXT("left_foot_rotation"), TEXT("F_rotation_L"));
    FootControllers.Add(TEXT("right_foot_rotation"), TEXT("F_rotation_R"));

    // 目标控制器（BeatBloom 独有）- 新的三控制器系统
    TargetControllers.Empty();
    TargetControllers.Add(TEXT("middle_hand"), TEXT("Middle_Hand"));
    TargetControllers.Add(TEXT("look_at"), TEXT("Look_At"));
    TargetControllers.Add(TEXT("head_control"), TEXT("Head_Control"));

    // 双线性映射辅助控制器
    BilinearHelpers.Empty();
    BilinearHelpers.Add(TEXT("middle_hand_a"), TEXT("Middle_Hand_A"));
    BilinearHelpers.Add(TEXT("middle_hand_b"), TEXT("Middle_Hand_B"));
    BilinearHelpers.Add(TEXT("middle_hand_c"), TEXT("Middle_Hand_C"));
    BilinearHelpers.Add(TEXT("middle_hand_d"), TEXT("Middle_Hand_D"));
    BilinearHelpers.Add(TEXT("head_control_a"), TEXT("Head_Control_A"));
    BilinearHelpers.Add(TEXT("head_control_b"), TEXT("Head_Control_B"));
    BilinearHelpers.Add(TEXT("head_control_c"), TEXT("Head_Control_C"));
    BilinearHelpers.Add(TEXT("head_control_d"), TEXT("Head_Control_D"));
    BilinearHelpers.Add(TEXT("left_hand_a"),  TEXT("Left_Hand_A"));
    BilinearHelpers.Add(TEXT("left_hand_b"),  TEXT("Left_Hand_B"));
    BilinearHelpers.Add(TEXT("left_hand_c"),  TEXT("Left_Hand_C"));
    BilinearHelpers.Add(TEXT("left_hand_d"),  TEXT("Left_Hand_D"));
    BilinearHelpers.Add(TEXT("right_hand_a"), TEXT("Right_Hand_A"));
    BilinearHelpers.Add(TEXT("right_hand_b"), TEXT("Right_Hand_B"));
    BilinearHelpers.Add(TEXT("right_hand_c"), TEXT("Right_Hand_C"));
    BilinearHelpers.Add(TEXT("right_hand_d"), TEXT("Right_Hand_D"));
    
    UE_LOG(LogTemp, Log, TEXT("BeatBloomUnreal: Controllers initialized"));
}

bool ABeatBloomUnreal::LoadDrumKitConfig(const FString& FilePath) {
    // 加载 .drumkit JSON 文件，解析到 DrumKitConfig
    // 成功后调用 InitializeRecordersFromConfig()
    
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *FilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load drum kit config from %s"), *FilePath);
        return false;
    }
    
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse drum kit config JSON"));
        return false;
    }
    
    // 清空旧数据，防止多次加载时叠加
    DrumKitConfig = FBeatBloomDrumKitConfig();

    // 解析基本字段
    DrumKitConfig.Name = JsonObject->GetStringField(TEXT("name"));
    
    // 解析 limbs 数组
    const TArray<TSharedPtr<FJsonValue>>* LimbsArray;
    if (JsonObject->TryGetArrayField(TEXT("limbs"), LimbsArray)) {
        for (const TSharedPtr<FJsonValue>& LimbValue : *LimbsArray) {
            DrumKitConfig.Limbs.Add(LimbValue->AsString());
        }
    }
    
    // 解析 components 数组
    const TArray<TSharedPtr<FJsonValue>>* ComponentsArray;
    if (JsonObject->TryGetArrayField(TEXT("components"), ComponentsArray)) {
        for (const TSharedPtr<FJsonValue>& ComponentValue : *ComponentsArray) {
            TSharedPtr<FJsonObject> ComponentObj = ComponentValue->AsObject();
            FBeatBloomDrumComponent Component;
            
            Component.Name = ComponentObj->GetStringField(TEXT("name"));
            Component.Description = ComponentObj->GetStringField(TEXT("description"));
            Component.bDoubleComponent = ComponentObj->GetBoolField(TEXT("double_component"));
            
            // 解析 drivable_limbs
            const TArray<TSharedPtr<FJsonValue>>* DrivableLimbsArrayPtr;
            if (ComponentObj->TryGetArrayField(TEXT("drivable_limbs"), DrivableLimbsArrayPtr)) {
                for (const TSharedPtr<FJsonValue>& LimbValue : *DrivableLimbsArrayPtr) {
                    TSharedPtr<FJsonObject> LimbObj = LimbValue->AsObject();
                    FBeatBloomDrivableLimb DrivableLimb;
                    DrivableLimb.Limb = LimbObj->GetStringField(TEXT("limb"));
                    DrivableLimb.Coefficient = LimbObj->GetNumberField(TEXT("coefficient"));
                    Component.DrivableLimbs.Add(DrivableLimb);
                }
            }
            
            // 解析 midi_triggers
            const TArray<TSharedPtr<FJsonValue>>* MidiTriggersArrayPtr;
            if (ComponentObj->TryGetArrayField(TEXT("midi_triggers"), MidiTriggersArrayPtr)) {
                for (const TSharedPtr<FJsonValue>& TriggerValue : *MidiTriggersArrayPtr) {
                    TSharedPtr<FJsonObject> TriggerObj = TriggerValue->AsObject();
                    FBeatBloomMidiTrigger Trigger;
                    Trigger.Note = TriggerObj->GetNumberField(TEXT("note"));
                    Trigger.Sound = TriggerObj->GetStringField(TEXT("sound"));
                    Component.MidiTriggers.Add(Trigger);
                }
            }
            
            DrumKitConfig.Components.Add(Component);
        }
    }
    
    // 解析 special_actions 数组
    const TArray<TSharedPtr<FJsonValue>>* SpecialActionsArray;
    if (JsonObject->TryGetArrayField(TEXT("special_actions"), SpecialActionsArray)) {
        for (const TSharedPtr<FJsonValue>& ActionValue : *SpecialActionsArray) {
            TSharedPtr<FJsonObject> ActionObj = ActionValue->AsObject();
            FBeatBloomSpecialAction Action;
            
            Action.Name = ActionObj->GetStringField(TEXT("name"));
            Action.Description = ActionObj->GetStringField(TEXT("description"));
            
            // 解析 limbs 数组
            const TArray<TSharedPtr<FJsonValue>>* ActionLimbsArrayPtr;
            if (ActionObj->TryGetArrayField(TEXT("limbs"), ActionLimbsArrayPtr)) {
                for (const TSharedPtr<FJsonValue>& LimbValue : *ActionLimbsArrayPtr) {
                    Action.Limbs.Add(LimbValue->AsString());
                }
            }
            
            // 解析 midi_triggers
            const TArray<TSharedPtr<FJsonValue>>* ActionMidiTriggersArrayPtr;
            if (ActionObj->TryGetArrayField(TEXT("midi_triggers"), ActionMidiTriggersArrayPtr)) {
                for (const TSharedPtr<FJsonValue>& TriggerValue : *ActionMidiTriggersArrayPtr) {
                    TSharedPtr<FJsonObject> TriggerObj = TriggerValue->AsObject();
                    FBeatBloomMidiTrigger Trigger;
                    Trigger.Note = TriggerObj->GetNumberField(TEXT("note"));
                    Trigger.Sound = TriggerObj->GetStringField(TEXT("sound"));
                    Action.MidiTriggers.Add(Trigger);
                }
            }
            
            DrumKitConfig.SpecialActions.Add(Action);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Successfully loaded drum kit config: %s (%d components, %d special actions)"),
           *DrumKitConfig.Name, DrumKitConfig.Components.Num(), DrumKitConfig.SpecialActions.Num());
    
    InitializeRecordersFromConfig();
    return true;
}

void ABeatBloomUnreal::InitializeRecordersFromConfig() {
    // 根据 DrumKitConfig 动态生成所有记录器映射
    // 参考 Blender 版 beat_bloom_config.py 的 initialize_recorders_from_config
    
    HandRecorders.Empty();
    FootRecorders.Empty();
    TargetRecorders.Empty();
    HeadControlRecorders.Empty();
    RecorderTransforms.Empty();
    
    // 遍历所有鼓组件生成记录器
    for (const FBeatBloomDrumComponent& Component : DrumKitConfig.Components) {
        GenerateRecordersForComponent(Component.Name, Component.DrivableLimbs);
    }
    
    // 遍历所有特殊动作生成记录器
    for (const FBeatBloomSpecialAction& Action : DrumKitConfig.SpecialActions) {
        // 将 limb 字符串转换为 DrivableLimb 格式
        TArray<FBeatBloomDrivableLimb> ActionLimbs;
        for (const FString& LimbStr : Action.Limbs) {
            FBeatBloomDrivableLimb Limb;
            Limb.Limb = LimbStr;
            Limb.Coefficient = 1.0f;
            ActionLimbs.Add(Limb);
        }
        GenerateRecordersForComponent(Action.Name, ActionLimbs);
    }
    
    // 添加休息状态记录器（只给手部）
    AddRestRecorders();
    
    // 添加目标记录器（只给手部可驱动的组件和特殊动作）
    AddTargetRecorders();
    
    // 初始化所有记录器的默认值
    for (const auto& RecorderPair : RecorderTransforms) {
        RecorderTransforms.FindOrAdd(RecorderPair.Key);
    }
    
    bIsInitialized = true;
    
    UE_LOG(LogTemp, Log, TEXT("Initialized %d recorders from config"), RecorderTransforms.Num());
}

bool ABeatBloomUnreal::ParseBeatBloomFile(
    const FString& FilePath,
    FString& OutPerformerAnimationPath,
    FString& OutDrumKitAnimationPath) {
    
    // 读取文件内容
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load .beatbloom file: %s"), *FilePath);
        return false;
    }
    
    // 解析 JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse .beatbloom JSON: %s"), *FilePath);
        return false;
    }
    
    // 提取 animation_path（演奏者动画）
    if (!JsonObject->HasField(TEXT("animation_path"))) {
        UE_LOG(LogTemp, Error, TEXT("Missing 'animation_path' field in .beatbloom file: %s"), *FilePath);
        return false;
    }
    OutPerformerAnimationPath = JsonObject->GetStringField(TEXT("animation_path"));
    
    // 提取 shape_key_animation_path（鼓组动画）
    if (!JsonObject->HasField(TEXT("shape_key_animation_path"))) {
        UE_LOG(LogTemp, Error, TEXT("Missing 'shape_key_animation_path' field in .beatbloom file: %s"), *FilePath);
        return false;
    }
    OutDrumKitAnimationPath = JsonObject->GetStringField(TEXT("shape_key_animation_path"));
    
    UE_LOG(LogTemp, Warning, TEXT("Parsed .beatbloom file: %s"), *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("  Performer Animation: %s"), *OutPerformerAnimationPath);
    UE_LOG(LogTemp, Warning, TEXT("  DrumKit Animation: %s"), *OutDrumKitAnimationPath);
    
    return true;
}

void ABeatBloomUnreal::ExportRecorderInfo(const FString& FilePath) {
    // 将 RecorderTransforms 导出为 .drummer JSON 文件
    // 新格式：包含 RECORDER_INFO 和 MAPPING_HELPERS 两个顶层字段
    
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    
    // ========== 1. 构建 RECORDER_INFO ==========
    TSharedPtr<FJsonObject> RecorderInfoObject = MakeShareable(new FJsonObject);
    
    for (const auto& RecorderPair : RecorderTransforms) {
        const FString& RecorderName = RecorderPair.Key;
        const FBeatBloomRecorderTransform& Data = RecorderPair.Value;
        
        // 跳过双线性辅助记录器(它们属于 MAPPING_HELPERS)
        if (RecorderName.StartsWith(TEXT("Middle_Hand_A")) ||
            RecorderName.StartsWith(TEXT("Middle_Hand_B")) ||
            RecorderName.StartsWith(TEXT("Middle_Hand_C")) ||
            RecorderName.StartsWith(TEXT("Middle_Hand_D")) ||
            RecorderName.StartsWith(TEXT("Head_Control_A")) ||
            RecorderName.StartsWith(TEXT("Head_Control_B")) ||
            RecorderName.StartsWith(TEXT("Head_Control_C")) ||
            RecorderName.StartsWith(TEXT("Head_Control_D"))) {
            continue;
        }
        
        TSharedPtr<FJsonObject> RecorderObject = MakeShareable(new FJsonObject);
        
        // 设置 location 数组
        TArray<TSharedPtr<FJsonValue>> LocationArray;
        LocationArray.Add(MakeShareable(new FJsonValueNumber(Data.Location.X)));
        LocationArray.Add(MakeShareable(new FJsonValueNumber(Data.Location.Y)));
        LocationArray.Add(MakeShareable(new FJsonValueNumber(Data.Location.Z)));
        RecorderObject->SetArrayField(TEXT("location"), LocationArray);
        
        // Head_Control 记录器只保存 location，不保存旋转
        bool bIsHeadControlRecorder = HeadControlRecorders.Contains(RecorderName);
        if (!bIsHeadControlRecorder) {
            // 设置 rotation_quaternion 数组 [w, x, y, z]
            TArray<TSharedPtr<FJsonValue>> RotationArray;
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Data.Rotation.W)));
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Data.Rotation.X)));
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Data.Rotation.Y)));
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Data.Rotation.Z)));
            RecorderObject->SetArrayField(TEXT("rotation_quaternion"), RotationArray);
            
            // 设置 rotation_mode
            RecorderObject->SetStringField(TEXT("rotation_mode"), TEXT("QUATERNION"));
        }
        
        RecorderInfoObject->SetObjectField(RecorderName, RecorderObject);
    }
    
    RootObject->SetObjectField(TEXT("RECORDER_INFO"), RecorderInfoObject);
    
    // ========== 2. 构建 MAPPING_HELPERS ==========
    TSharedPtr<FJsonObject> MappingHelpersObject = MakeShareable(new FJsonObject);
    
    // 遍历 BilinearHelpers 映射，提取对应的记录器数据
    for (const auto& HelperPair : BilinearHelpers) {
        const FString& HelperKey = HelperPair.Key;      // 如 "middle_hand_a"
        const FString& ControllerName = HelperPair.Value; // 如 "Middle_Hand_A"
        
        // 查找对应的记录器名称
        const FBeatBloomRecorderTransform* Data = 
            RecorderTransforms.Find(ControllerName);
        
        if (!Data) {
            UE_LOG(LogTemp, Warning,
                   TEXT("BeatBloom: Bilinear helper %s not found in RecorderTransforms"),
                   *ControllerName);
            continue;
        }
        
        TSharedPtr<FJsonObject> HelperObject = MakeShareable(new FJsonObject);

        TArray<TSharedPtr<FJsonValue>> LocationArray;
        LocationArray.Add(MakeShareable(new FJsonValueNumber(Data->Location.X)));
        LocationArray.Add(MakeShareable(new FJsonValueNumber(Data->Location.Y)));
        LocationArray.Add(MakeShareable(new FJsonValueNumber(Data->Location.Z)));
        HelperObject->SetArrayField(TEXT("location"), LocationArray);

        // Left_Hand_* / Right_Hand_* 还需要保存旋转
        if (ControllerName.StartsWith(TEXT("Left_Hand_")) ||
            ControllerName.StartsWith(TEXT("Right_Hand_"))) {
            TArray<TSharedPtr<FJsonValue>> RotationArray;
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Data->Rotation.W)));
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Data->Rotation.X)));
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Data->Rotation.Y)));
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Data->Rotation.Z)));
            HelperObject->SetArrayField(TEXT("rotation_quaternion"), RotationArray);
        }

        MappingHelpersObject->SetObjectField(ControllerName, HelperObject);
    }
    
    RootObject->SetObjectField(TEXT("MAPPING_HELPERS"), MappingHelpersObject);
    
    // ========== 3. 序列化并保存 ==========
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    
    if (FFileHelper::SaveStringToFile(JsonString, *FilePath)) {
        UE_LOG(LogTemp, Log, 
               TEXT("Successfully exported recorder info to %s (%d recorders, %d helpers)"),
               *FilePath, RecorderInfoObject->Values.Num(), MappingHelpersObject->Values.Num());
    } else {
        UE_LOG(LogTemp, Error, TEXT("Failed to export recorder info to %s"), *FilePath);
    }
}

bool ABeatBloomUnreal::ImportRecorderInfo(const FString& FilePath) {
// 从 .drummer JSON 文件导入 RecorderTransforms
// 新格式：包含 RECORDER_INFO 和 MAPPING_HELPERS 两个顶层字段
    
// 必须先加载 DrumKitConfig，才能知道哪些键名是合法的
if (DrumKitConfig.Components.Num() == 0) {
    UE_LOG(LogTemp, Error, TEXT("ImportRecorderInfo: Please load .drumkit config first!"));
    return false;
}
    
FString JsonContent;
if (!FFileHelper::LoadFileToString(JsonContent, *FilePath)) {
    UE_LOG(LogTemp, Error, TEXT("Failed to load recorder info from %s"), *FilePath);
    return false;
}
    
TSharedPtr<FJsonObject> JsonObject;
TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
    
if (!FJsonSerializer::Deserialize(Reader, JsonObject)) {
    UE_LOG(LogTemp, Error, TEXT("Failed to parse recorder info JSON"));
    return false;
}
    
// 检查新格式是否包含必需字段
if (!JsonObject->HasField(TEXT("RECORDER_INFO")) ||
    !JsonObject->HasField(TEXT("MAPPING_HELPERS"))) {
    UE_LOG(LogTemp, Error, 
           TEXT("Invalid .drummer file format: missing RECORDER_INFO or MAPPING_HELPERS"));
    return false;
}
    
TSharedPtr<FJsonObject> RecorderInfoObj = 
    JsonObject->GetObjectField(TEXT("RECORDER_INFO"));
TSharedPtr<FJsonObject> MappingHelpersObj = 
    JsonObject->GetObjectField(TEXT("MAPPING_HELPERS"));
    
// ========== 关键步骤：先重新初始化记录器映射 ==========
// 这保证了：
// 1. RecorderTransforms 从干净状态开始（不会有旧的错误格式键残留）
// 2. HeadControlRecorders/HandRecorders 等映射都被正确填充（CleanupInvalidRecorderKeys 需要它们）
InitializeRecordersFromConfig();
    
int32 ImportedRecorderCount = 0;
int32 ImportedHelperCount = 0;
    
// ========== 1. 导入 RECORDER_INFO ==========
const TMap<FString, TSharedPtr<FJsonValue>>& RecorderFields = RecorderInfoObj->Values;
    
    for (const auto& Field : RecorderFields) {
        const FString& RecorderName = Field.Key;
        TSharedPtr<FJsonObject> RecorderObj = Field.Value->AsObject();
        
        FBeatBloomRecorderTransform Data;
        
        // 解析 location 数组
        const TArray<TSharedPtr<FJsonValue>>* LocationArrayPtr;
        if (RecorderObj->TryGetArrayField(TEXT("location"), LocationArrayPtr) && 
            LocationArrayPtr->Num() == 3) {
            float X = (*LocationArrayPtr)[0]->AsNumber();
            float Y = (*LocationArrayPtr)[1]->AsNumber();
            float Z = (*LocationArrayPtr)[2]->AsNumber();
            Data.Location = FVector(X, Y, Z);
        }
        
        // 解析 rotation_quaternion 数组 [w, x, y, z]
        // Head_Control 记录器可能没有旋转数据，使用 Identity
        const TArray<TSharedPtr<FJsonValue>>* RotationArrayPtr;
        if (RecorderObj->TryGetArrayField(TEXT("rotation_quaternion"), RotationArrayPtr) && 
            RotationArrayPtr->Num() == 4) {
            float W = (*RotationArrayPtr)[0]->AsNumber();
            float X = (*RotationArrayPtr)[1]->AsNumber();
            float Y = (*RotationArrayPtr)[2]->AsNumber();
            float Z = (*RotationArrayPtr)[3]->AsNumber();
            Data.Rotation = FQuat(X, Y, Z, W);
        } else {
            Data.Rotation = FQuat::Identity;
        }
        
        RecorderTransforms.Add(RecorderName, Data);
        ImportedRecorderCount++;
    }
    
    // ========== 2. 导入 MAPPING_HELPERS ==========
    const TMap<FString, TSharedPtr<FJsonValue>>& HelperFields = MappingHelpersObj->Values;
    
    for (const auto& Field : HelperFields) {
        const FString& HelperName = Field.Key;  // 如 "Middle_Hand_A"
        TSharedPtr<FJsonObject> HelperObj = Field.Value->AsObject();
        
        FBeatBloomRecorderTransform Data;
        Data.Rotation = FQuat::Identity;

        const TArray<TSharedPtr<FJsonValue>>* LocationArrayPtr;
        if (HelperObj->TryGetArrayField(TEXT("location"), LocationArrayPtr) &&
            LocationArrayPtr->Num() == 3) {
            Data.Location = FVector(
                (*LocationArrayPtr)[0]->AsNumber(),
                (*LocationArrayPtr)[1]->AsNumber(),
                (*LocationArrayPtr)[2]->AsNumber());
        }

        // Left_Hand_* / Right_Hand_* 还需要读取旋转
        const TArray<TSharedPtr<FJsonValue>>* RotationArrayPtr;
        if ((HelperName.StartsWith(TEXT("Left_Hand_")) ||
             HelperName.StartsWith(TEXT("Right_Hand_"))) &&
            HelperObj->TryGetArrayField(TEXT("rotation_quaternion"), RotationArrayPtr) &&
            RotationArrayPtr->Num() == 4) {
            Data.Rotation = FQuat(
                (*RotationArrayPtr)[1]->AsNumber(),
                (*RotationArrayPtr)[2]->AsNumber(),
                (*RotationArrayPtr)[3]->AsNumber(),
                (*RotationArrayPtr)[0]->AsNumber());
        }

        RecorderTransforms.Add(HelperName, Data);
        ImportedHelperCount++;
    }
    
    UE_LOG(LogTemp, Log, 
           TEXT("Successfully imported %d recorders and %d mapping helpers from %s"),
           ImportedRecorderCount, ImportedHelperCount, *FilePath);
    
    // 清理无效键名（之前版本可能生成的错误格式）
    CleanupInvalidRecorderKeys();
    
    return true;
}

TArray<FString> ABeatBloomUnreal::GetDrumKitOptionsForLimb(EBeatBloomLimb Limb) const {
    // 遍历 DrumKitConfig.Components 和 SpecialActions，
    // 筛选可被指定肢体驱动的鼓件名称
    
    TArray<FString> Options;
    FString LimbString = GetLimbString(Limb);
    
    // 检查普通鼓组件
    for (const FBeatBloomDrumComponent& Component : DrumKitConfig.Components) {
        for (const FBeatBloomDrivableLimb& DrivableLimb : Component.DrivableLimbs) {
            if (DrivableLimb.Limb == LimbString) {
                Options.Add(Component.Name);
                break;
            }
        }
    }
    
    // 检查特殊动作
    for (const FBeatBloomSpecialAction& Action : DrumKitConfig.SpecialActions) {
        if (Action.Limbs.Contains(LimbString)) {
            Options.Add(Action.Name);
        }
    }
    
    return Options;
}

TArray<FString> ABeatBloomUnreal::GetTargetDrumKitOptions() const {
    // 返回手部可驱动的鼓件列表（用于目标控制器选项）
    
    TSet<FString> OptionsSet;
    
    // 获取左手可驱动的鼓件
    TArray<FString> LeftHandOptions = GetDrumKitOptionsForLimb(EBeatBloomLimb::LEFT_HAND);
    for (const FString& Option : LeftHandOptions) {
        OptionsSet.Add(Option);
    }
    
    // 获取右手可驱动的鼓件
    TArray<FString> RightHandOptions = GetDrumKitOptionsForLimb(EBeatBloomLimb::RIGHT_HAND);
    for (const FString& Option : RightHandOptions) {
        OptionsSet.Add(Option);
    }
    
    // 转换为数组返回
    TArray<FString> Options;
    for (const FString& Option : OptionsSet) {
        Options.Add(Option);
    }
    
    return Options;
}

TMap<FString, FString> ABeatBloomUnreal::GetCurrentControllerToRecorderMapping() const {
    // 根据当前 UI 选择状态，构建控制器到记录器的完整映射
    
    TMap<FString, FString> Mapping;
    
    // 左手映射
    if (CurrentLeftHandDrumKit == TEXT("Rest")) {
        Mapping.Add(TEXT("H_L"), TEXT("H_Rest_L"));
        Mapping.Add(TEXT("HP_L"), TEXT("HP_Rest_L"));
        Mapping.Add(TEXT("H_rotation_L"), TEXT("H_rotation_Rest_L"));
    } else {
        FString Prefix = CurrentLeftHandDrumKit + TEXT("_") + GetStateString(CurrentLeftHandState) + TEXT("_");
        Mapping.Add(TEXT("H_L"), Prefix + TEXT("H_L"));
        Mapping.Add(TEXT("HP_L"), Prefix + TEXT("HP_L"));
        Mapping.Add(TEXT("H_rotation_L"), Prefix + TEXT("H_rotation_L"));
    }
    
    // 右手映射
    if (CurrentRightHandDrumKit == TEXT("Rest")) {
        Mapping.Add(TEXT("H_R"), TEXT("H_Rest_R"));
        Mapping.Add(TEXT("HP_R"), TEXT("HP_Rest_R"));
        Mapping.Add(TEXT("H_rotation_R"), TEXT("H_rotation_Rest_R"));
    } else {
        FString Prefix = CurrentRightHandDrumKit + TEXT("_") + GetStateString(CurrentRightHandState) + TEXT("_");
        Mapping.Add(TEXT("H_R"), Prefix + TEXT("H_R"));
        Mapping.Add(TEXT("HP_R"), Prefix + TEXT("HP_R"));
        Mapping.Add(TEXT("H_rotation_R"), Prefix + TEXT("H_rotation_R"));
    }
    
    // 左脚映射
    if (!CurrentLeftFootDrumKit.IsEmpty()) {
        FString Prefix = CurrentLeftFootDrumKit + TEXT("_") + GetStateString(CurrentLeftFootState) + TEXT("_");
        Mapping.Add(TEXT("F_L"), Prefix + TEXT("F_L"));
        Mapping.Add(TEXT("F_rotation_L"), Prefix + TEXT("F_rotation_L"));
    }
    
    // 右脚映射
    if (!CurrentRightFootDrumKit.IsEmpty()) {
        FString Prefix = CurrentRightFootDrumKit + TEXT("_") + GetStateString(CurrentRightFootState) + TEXT("_");
        Mapping.Add(TEXT("F_R"), Prefix + TEXT("F_R"));
        Mapping.Add(TEXT("F_rotation_R"), Prefix + TEXT("F_rotation_R"));
    }
    
    // Middle_Hand 是 Control Rig 自动计算的（H_L + H_R 中点），不需要记录器映射
    // Look_At 通过父子关系跟随 Middle_Hand，也不需要记录器映射
    
    // Head_Control 记录器映射（基于左手状态）
    // 格式: {ComponentName}_{State}_Head_Control
    if (CurrentLeftHandDrumKit == TEXT("Rest")) {
        Mapping.Add(TEXT("Head_Control"), TEXT("Head_Control_Rest"));
    } else if (!CurrentLeftHandDrumKit.IsEmpty()) {
        FString HCPrefix = CurrentLeftHandDrumKit + TEXT("_") + GetStateString(CurrentLeftHandState) + TEXT("_");
        Mapping.Add(TEXT("Head_Control"), HCPrefix + TEXT("Head_Control"));
    }
    
    return Mapping;
}

FString ABeatBloomUnreal::GetStateString(EBeatBloomState State) {
    switch (State) {
        case EBeatBloomState::BEAT:
            return TEXT("beat");
        case EBeatBloomState::READY:
            return TEXT("ready");
        case EBeatBloomState::REST:
            return TEXT("rest");
        default:
            return TEXT("beat");
    }
}

FString ABeatBloomUnreal::GetLimbString(EBeatBloomLimb Limb) {
    switch (Limb) {
        case EBeatBloomLimb::LEFT_HAND:
            return TEXT("left_hand");
        case EBeatBloomLimb::RIGHT_HAND:
            return TEXT("right_hand");
        case EBeatBloomLimb::LEFT_FOOT:
            return TEXT("left_foot");
        case EBeatBloomLimb::RIGHT_FOOT:
            return TEXT("right_foot");
        default:
            return TEXT("left_hand");
    }
}

UControlRig* ABeatBloomUnreal::GetCachedControlRig(FName ComponentName) {
    // 从 ControlRigCacheSubsystem 获取缓存的 ControlRig
    
    if (!GEngine) {
        UE_LOG(LogTemp, Error, TEXT("GetCachedControlRig: GEngine is NULL"));
        return nullptr;
    }
    
    UControlRigCacheSubsystem* CacheSubsystem = GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error, TEXT("GetCachedControlRig: CacheSubsystem not found in GEngine"));
        return nullptr;
    }
    
    // 根据 ComponentName 获取对应的 SkeletalMeshActor
    ASkeletalMeshActor* Actor = nullptr;
    if (ComponentName == TEXT("DrumKit")) {
        Actor = DrumKit;
    } else if (ComponentName == TEXT("Performer")) {
        // Performer 使用父类的 SkeletalMeshActor 属性
        Actor = SkeletalMeshActor;
    }
    
    if (!Actor) {
        UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRig: Actor not found for component %s"), *ComponentName.ToString());
        return nullptr;
    }

    // 根据 ComponentName 确定 RootControlName
    FString RootControlName;
    if (ComponentName == TEXT("DrumKit")) {
        RootControlName = TEXT("drumkit_root");
    } else if (ComponentName == TEXT("Performer")) {
        RootControlName = TEXT("controller_root");
    }
    
    // 获取当前 LevelSequence
    ULevelSequence* LevelSequence = UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRig: No LevelSequence found"));
        return nullptr;
    }
    
    // 使用通用接口查询 ControlRig
    UControlRig* ControlRig = CacheSubsystem->GetControlRig(Actor, LevelSequence, RootControlName);
    
    // 如果 ControlRig 为空，尝试触发注册后再查询
    if (!ControlRig) {
        UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRig: ControlRig is null, triggering registration for %s with root control '%s'"), *Actor->GetName(), *RootControlName);
        
        // 触发注册
        CacheSubsystem->TriggerRegistrationIfNeeded(Actor, LevelSequence, RootControlName);
        
        // 再次查询
        ControlRig = CacheSubsystem->GetControlRig(Actor, LevelSequence, RootControlName);
        
        if (!ControlRig) {
            UE_LOG(LogTemp, Error, TEXT("GetCachedControlRig: Still failed to get ControlRig after registration for %s"), *Actor->GetName());
        } else {
            UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRig: Successfully got ControlRig after registration for %s"), *Actor->GetName());
        }
    }
    
    return ControlRig;
}

UControlRigBlueprint* ABeatBloomUnreal::GetCachedControlRigBlueprint(FName ComponentName) {
    // 从 ControlRigCacheSubsystem 获取缓存的 ControlRig Blueprint
    
    if (!GEngine) {
        UE_LOG(LogTemp, Error, TEXT("GetCachedControlRigBlueprint: GEngine is NULL"));
        return nullptr;
    }
    
    UControlRigCacheSubsystem* CacheSubsystem = GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error, TEXT("GetCachedControlRigBlueprint: CacheSubsystem not found in GEngine"));
        return nullptr;
    }
    
    // 根据 ComponentName 获取对应的 SkeletalMeshActor
    ASkeletalMeshActor* Actor = nullptr;
    if (ComponentName == TEXT("DrumKit")) {
        Actor = DrumKit;
    } else if (ComponentName == TEXT("Performer")) {
        Actor = SkeletalMeshActor;
    } else {
        UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRigBlueprint: ComponentName %s is not recognized"), *ComponentName.ToString());
        return nullptr;
    }
    
    if (!Actor) {
        UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRigBlueprint: Actor not found for component %s"), *ComponentName.ToString());
        return nullptr;
    }

    // 根据 ComponentName 确定 RootControlName
    FString RootControlName;
    if (ComponentName == TEXT("DrumKit")) {
        RootControlName = TEXT("drumkit_root");
    } else if (ComponentName == TEXT("Performer")) {
        RootControlName = TEXT("controller_root");
    }
    
    // 获取当前 LevelSequence
    ULevelSequence* LevelSequence = UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRigBlueprint: No LevelSequence found"));
        return nullptr;
    }
    
    // 使用通用接口查询 ControlRigBlueprint
    UControlRigBlueprint* ControlRigBlueprint = CacheSubsystem->GetControlRigBlueprint(Actor, LevelSequence, RootControlName);
    
    return ControlRigBlueprint;
}

void ABeatBloomUnreal::RegisterAllControlRigs() {
    // 注册演奏者和鼓组的 ControlRig 到缓存子系统
    
    if (!GEngine) {
        UE_LOG(LogTemp, Error, TEXT("RegisterAllControlRigs: GEngine is not available"));
        return;
    }
    
    UControlRigCacheSubsystem* CacheSubsystem = GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error, TEXT("RegisterAllControlRigs: ControlRigCacheSubsystem is not available"));
        return;
    }
    
    ULevelSequence* LevelSequence = UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning, TEXT("RegisterAllControlRigs: No LevelSequence is currently open"));
        return;
    }
    
    int32 RegisteredCount = 0;
    
    // 1. 注册演奏者（Performer）的 ControlRig - 用于手部、脚部控制器
    if (SkeletalMeshActor) {
        CacheSubsystem->TriggerRegistrationIfNeeded(SkeletalMeshActor, LevelSequence);
        RegisteredCount++;
        UE_LOG(LogTemp, Verbose, TEXT("Registered Performer ControlRig"));
    }
    
    // 2. 注册鼓组（DrumKit）的 ControlRig - 用于鼓组动画
    if (DrumKit) {
        CacheSubsystem->TriggerRegistrationIfNeeded(DrumKit, LevelSequence);
        RegisteredCount++;
        UE_LOG(LogTemp, Verbose, TEXT("Registered DrumKit ControlRig"));
    }
    
    if (RegisteredCount > 0) {
        UE_LOG(LogTemp, Log, TEXT("Successfully registered %d ControlRigs"), RegisteredCount);
    } else {
        UE_LOG(LogTemp, Warning, TEXT("No ControlRigs were registered - check SkeletalMeshActor and DrumKit references"));
    }
}

void ABeatBloomUnreal::TriggerControlRigReregistration(const FString& ErrorMessage) {
    // 清除缓存并重新注册所有 ControlRig
    
    UE_LOG(LogTemp, Warning, TEXT("TriggerControlRigReregistration: %s"), *ErrorMessage);
    
    if (!GEngine) {
        UE_LOG(LogTemp, Error, TEXT("TriggerControlRigReregistration: GEngine is not available"));
        return;
    }
    
    UControlRigCacheSubsystem* CacheSubsystem = GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error, TEXT("TriggerControlRigReregistration: ControlRigCacheSubsystem is not available"));
        return;
    }
    
    // 清除所有缓存
    CacheSubsystem->ClearAllCaches();
    
    // 重新注册所有 ControlRig
    RegisterAllControlRigs();
    
    UE_LOG(LogTemp, Log, TEXT("ControlRig re-registration completed"));
}

int32 ABeatBloomUnreal::CleanupInvalidRecorderKeys() {
    // 收集所有合法的记录器键名
    TSet<FString> ValidKeys;
    
    // 1. 手部记录器（标准格式: {Component}_{State}_H_L/R, {Component}_{State}_HP_L/R, {Component}_{State}_H_rotation_L/R）
    for (const auto& Pair : HandRecorders) {
        ValidKeys.Add(Pair.Value);
    }
    
    // 2. 脚部记录器（标准格式: {Component}_{State}_F_L/R, {Component}_{State}_F_rotation_L/R）
    for (const auto& Pair : FootRecorders) {
        ValidKeys.Add(Pair.Value);
    }
    
    // 3. Head_Control 记录器（标准格式: {Component}_{State}_Head_Control）
    //    注意：Head_Control 在记录器名称的最后，不是开头
    for (const auto& Pair : HeadControlRecorders) {
        ValidKeys.Add(Pair.Value);
    }
    
    // 4. 目标记录器 (TargetRecorders) - 当前为空，保留以供将来扩展
    for (const auto& Pair : TargetRecorders) {
        ValidKeys.Add(Pair.Value);
    }
    
    // 5. 双线性辅助记录器（Middle_Hand_A/B/C/D, Head_Control_A/B/C/D）
    for (const auto& Pair : BilinearHelpers) {
        ValidKeys.Add(Pair.Value);
    }
    
    // 找出并删除无效键
    TArray<FString> KeysToRemove;
    for (const auto& Pair : RecorderTransforms) {
        const FString& Key = Pair.Key;
        
        // 如果键名不在合法集合中，标记删除
        if (!ValidKeys.Contains(Key)) {
            // 额外检查：删除所有以 "Head_Control_" 开头但不以 "_A/B/C/D" 结尾的错误格式键名
            // 正确格式应该是 "{Component}_{State}_Head_Control" 或 "Head_Control_A/B/C/D" 或 "Head_Control_Rest"
            if (Key.StartsWith(TEXT("Head_Control_")) && 
                !Key.EndsWith(TEXT("_A")) && !Key.EndsWith(TEXT("_B")) && 
                !Key.EndsWith(TEXT("_C")) && !Key.EndsWith(TEXT("_D")) &&
                Key != TEXT("Head_Control_Rest")) {
                UE_LOG(LogTemp, Warning, 
                       TEXT("BeatBloom: Found invalid Head_Control key format: %s (should be {Component}_{State}_Head_Control)"), 
                       *Key);
            }
            // 额外检查：删除所有以 "Middle_Hand_" 开头但不以 "_A/B/C/D" 结尾的错误格式键名
            else if (Key.StartsWith(TEXT("Middle_Hand_")) && 
                     !Key.EndsWith(TEXT("_A")) && !Key.EndsWith(TEXT("_B")) && 
                     !Key.EndsWith(TEXT("_C")) && !Key.EndsWith(TEXT("_D"))) {
                UE_LOG(LogTemp, Warning, 
                       TEXT("BeatBloom: Found invalid Middle_Hand key format: %s (Middle_Hand should not have per-component recorders)"), 
                       *Key);
            }
            
            KeysToRemove.Add(Key);
        }
    }
    
    for (const FString& Key : KeysToRemove) {
        RecorderTransforms.Remove(Key);
        UE_LOG(LogTemp, Warning, TEXT("BeatBloom: Removed invalid recorder key: %s"), *Key);
    }
    
    if (KeysToRemove.Num() > 0) {
        UE_LOG(LogTemp, Warning, TEXT("BeatBloom: CleanupInvalidRecorderKeys removed %d invalid keys"), KeysToRemove.Num());
    }
    
    return KeysToRemove.Num();
}

// ============ 辅助方法实现 ============

void ABeatBloomUnreal::GenerateRecordersForComponent(
    const FString& ComponentName,
    const TArray<FBeatBloomDrivableLimb>& DrivableLimbs) {
    
    // 遍历所有可驱动的肢体
    for (const FBeatBloomDrivableLimb& DrivableLimb : DrivableLimbs) {
        const FString& Limb = DrivableLimb.Limb;
        
        // 为每种状态生成记录器
        TArray<EBeatBloomState> States = {
            EBeatBloomState::BEAT,
            EBeatBloomState::READY,
            EBeatBloomState::REST
        };
        
        for (EBeatBloomState State : States) {
            FString StateStr = GetStateString(State);
            FString Prefix = ComponentName + TEXT("_") + StateStr + TEXT("_");
            
            if (Limb == TEXT("left_hand") || Limb == TEXT("right_hand")) {
                // 手部记录器：位置、旋转、轴点
                FString HandSuffix = (Limb == TEXT("left_hand")) ? TEXT("H_L") : TEXT("H_R");
                FString RotationSuffix = (Limb == TEXT("left_hand")) ? TEXT("H_rotation_L") : TEXT("H_rotation_R");
                FString PivotSuffix = (Limb == TEXT("left_hand")) ? TEXT("HP_L") : TEXT("HP_R");
                
                AddRecorder(Prefix + HandSuffix);
                AddRecorder(Prefix + RotationSuffix);
                AddRecorder(Prefix + PivotSuffix);
                
                // 添加到手部记录器映射
                HandRecorders.Add(Prefix + HandSuffix, Prefix + HandSuffix);
                HandRecorders.Add(Prefix + RotationSuffix, Prefix + RotationSuffix);
                HandRecorders.Add(Prefix + PivotSuffix, Prefix + PivotSuffix);
                
                // Head_Control 记录器（每个手部组件+状态记录一个）
                FString HCRecorderName = ComponentName + TEXT("_") + StateStr + TEXT("_Head_Control");
                AddRecorder(HCRecorderName);
                HeadControlRecorders.Add(HCRecorderName, HCRecorderName);
                
            } else if (Limb == TEXT("left_foot") || Limb == TEXT("right_foot")) {
                // 脚部记录器：位置、旋转
                FString FootSuffix = (Limb == TEXT("left_foot")) ? TEXT("F_L") : TEXT("F_R");
                FString RotationSuffix = (Limb == TEXT("left_foot")) ? TEXT("F_rotation_L") : TEXT("F_rotation_R");
                
                AddRecorder(Prefix + FootSuffix);
                AddRecorder(Prefix + RotationSuffix);
                
                // 添加到脚部记录器映射
                FootRecorders.Add(Prefix + FootSuffix, Prefix + FootSuffix);
                FootRecorders.Add(Prefix + RotationSuffix, Prefix + RotationSuffix);
            }
        }
    }
}

void ABeatBloomUnreal::AddRestRecorders() {
    // 添加左手休息状态记录器
    AddRecorder(TEXT("H_Rest_L"));
    AddRecorder(TEXT("H_rotation_Rest_L"));
    AddRecorder(TEXT("HP_Rest_L"));
    HandRecorders.Add(TEXT("H_Rest_L"), TEXT("H_Rest_L"));
    HandRecorders.Add(TEXT("H_rotation_Rest_L"), TEXT("H_rotation_Rest_L"));
    HandRecorders.Add(TEXT("HP_Rest_L"), TEXT("HP_Rest_L"));
    
    // 添加右手休息状态记录器
    AddRecorder(TEXT("H_Rest_R"));
    AddRecorder(TEXT("H_rotation_Rest_R"));
    AddRecorder(TEXT("HP_Rest_R"));
    HandRecorders.Add(TEXT("H_Rest_R"), TEXT("H_Rest_R"));
    HandRecorders.Add(TEXT("H_rotation_Rest_R"), TEXT("H_rotation_Rest_R"));
    HandRecorders.Add(TEXT("HP_Rest_R"), TEXT("HP_Rest_R"));
    
    // 添加全局休息状态的 Head_Control 记录器
    AddRecorder(TEXT("Head_Control_Rest"));
    HeadControlRecorders.Add(TEXT("Head_Control_Rest"), TEXT("Head_Control_Rest"));
}

void ABeatBloomUnreal::AddTargetRecorders() {
    // Head_Control 记录器已在 GenerateRecordersForComponent() 中创建
    // 格式: {ComponentName}_{State}_Head_Control (如 "Open Hi-Hat_beat_Head_Control")
    // Middle_Hand 是 Control Rig 自动计算的（H_L + H_R 中点），不需要 per-component 记录器
    // 双线性辅助记录器 (Middle_Hand_A/B/C/D, Head_Control_A/B/C/D) 在 BilinearHelpers 中管理
    
    // 此方法现在为空，保留以供将来扩展
}

void ABeatBloomUnreal::AddRecorder(const FString& RecorderName) {
    // 如果记录器已存在，跳过
    if (RecorderTransforms.Contains(RecorderName)) {
        return;
    }
    
    // 添加默认值
    FBeatBloomRecorderTransform DefaultTransform;
    DefaultTransform.Location = FVector::ZeroVector;
    DefaultTransform.Rotation = FQuat::Identity;
    RecorderTransforms.Add(RecorderName, DefaultTransform);
}
