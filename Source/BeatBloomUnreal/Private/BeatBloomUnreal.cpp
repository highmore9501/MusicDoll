#include "BeatBloomUnreal.h"

#include "Animation/SkeletalMeshActor.h"
#include "BeatBloomTransformSyncProcessor.h"
#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

ABeatBloomUnreal::ABeatBloomUnreal() {
    PrimaryActorTick.bCanEverTick = true;

    bEnableRealtimeSync = false;
    CurrentLeftHandState = EBeatBloomState::BEAT;
    CurrentRightHandState = EBeatBloomState::BEAT;
    CurrentLeftFootState = EBeatBloomState::BEAT;
    CurrentRightFootState = EBeatBloomState::BEAT;
    CurrentTargetState = EBeatBloomState::BEAT;

    CachedDrumKitRelativeTransform = FTransform::Identity;
    bIsInitialized = false;

    InitializeControllersAndRecorders();
}

void ABeatBloomUnreal::BeginPlay() {
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("BeatBloomUnreal: BeginPlay called"));
}

void ABeatBloomUnreal::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (bEnableRealtimeSync) {
        UBeatBloomTransformSyncProcessor::SyncAllInstrumentTransforms(this);
    }
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

    // 目标控制器（BeatBloom 独有）
    TargetControllers.Empty();
    TargetControllers.Add(TEXT("body_target"), TEXT("Tar_Body"));
    TargetControllers.Add(TEXT("chest_target"), TEXT("Tar_Chest"));
    TargetControllers.Add(TEXT("head_target"), TEXT("Tar_Head"));
    
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
    // 参考设计文档 08_BeatBloom_DataFormats.md 第三节
    
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    
    // 遍历所有记录器，构建 JSON 对象
    for (const auto& RecorderPair : RecorderTransforms) {
        const FString& RecorderName = RecorderPair.Key;
        const FBeatBloomRecorderTransform& Data = RecorderPair.Value;
        
        TSharedPtr<FJsonObject> RecorderObject = MakeShareable(new FJsonObject);
        
        // 设置 location 数组
        TArray<TSharedPtr<FJsonValue>> LocationArray;
        LocationArray.Add(MakeShareable(new FJsonValueNumber(Data.Location.X)));
        LocationArray.Add(MakeShareable(new FJsonValueNumber(Data.Location.Y)));
        LocationArray.Add(MakeShareable(new FJsonValueNumber(Data.Location.Z)));
        RecorderObject->SetArrayField(TEXT("location"), LocationArray);
        
        // 设置 rotation_quaternion 数组 [w, x, y, z]
        TArray<TSharedPtr<FJsonValue>> RotationArray;
        RotationArray.Add(MakeShareable(new FJsonValueNumber(Data.Rotation.W)));
        RotationArray.Add(MakeShareable(new FJsonValueNumber(Data.Rotation.X)));
        RotationArray.Add(MakeShareable(new FJsonValueNumber(Data.Rotation.Y)));
        RotationArray.Add(MakeShareable(new FJsonValueNumber(Data.Rotation.Z)));
        RecorderObject->SetArrayField(TEXT("rotation_quaternion"), RotationArray);
        
        // 设置 rotation_mode
        RecorderObject->SetStringField(TEXT("rotation_mode"), TEXT("QUATERNION"));
        
        RootObject->SetObjectField(RecorderName, RecorderObject);
    }
    
    // 序列化为 JSON 字符串
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    
    if (FFileHelper::SaveStringToFile(JsonString, *FilePath)) {
        UE_LOG(LogTemp, Log, TEXT("Successfully exported recorder info to %s (%d recorders)"), *FilePath, RecorderTransforms.Num());
    } else {
        UE_LOG(LogTemp, Error, TEXT("Failed to export recorder info to %s"), *FilePath);
    }
}

bool ABeatBloomUnreal::ImportRecorderInfo(const FString& FilePath) {
    // 从 .drummer JSON 文件导入 RecorderTransforms
    
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
    
    // 遍历 JSON 对象的所有字段
    const TMap<FString, TSharedPtr<FJsonValue>>& Fields = JsonObject->Values;
    int32 ImportedCount = 0;
    
    for (const auto& Field : Fields) {
        const FString& RecorderName = Field.Key;
        TSharedPtr<FJsonObject> RecorderObj = Field.Value->AsObject();
        
        FBeatBloomRecorderTransform Data;
        
        // 解析 location 数组
        const TArray<TSharedPtr<FJsonValue>>* LocationArrayPtr;
        if (RecorderObj->TryGetArrayField(TEXT("location"), LocationArrayPtr) && LocationArrayPtr->Num() == 3) {
            float X = (*LocationArrayPtr)[0]->AsNumber();
            float Y = (*LocationArrayPtr)[1]->AsNumber();
            float Z = (*LocationArrayPtr)[2]->AsNumber();
            Data.Location = FVector(X, Y, Z);
        }
        
        // 解析 rotation_quaternion 数组 [w, x, y, z]
        const TArray<TSharedPtr<FJsonValue>>* RotationArrayPtr;
        if (RecorderObj->TryGetArrayField(TEXT("rotation_quaternion"), RotationArrayPtr) && RotationArrayPtr->Num() == 4) {
            float W = (*RotationArrayPtr)[0]->AsNumber();
            float X = (*RotationArrayPtr)[1]->AsNumber();
            float Y = (*RotationArrayPtr)[2]->AsNumber();
            float Z = (*RotationArrayPtr)[3]->AsNumber();
            // 注意：Unreal 的 FQuat 构造函数是 (X, Y, Z, W)
            Data.Rotation = FQuat(X, Y, Z, W);
        }
        
        RecorderTransforms.Add(RecorderName, Data);
        ImportedCount++;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Successfully imported %d recorders from %s"), ImportedCount, *FilePath);
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
    
    // 目标控制器映射
    if (!CurrentTargetDrumKit.IsEmpty()) {
        FString Prefix = CurrentTargetDrumKit + TEXT("_") + GetStateString(CurrentTargetState) + TEXT("_");
        Mapping.Add(TEXT("Tar_Body"), TEXT("Tar_Body_") + Prefix + TEXT("z"));
        Mapping.Add(TEXT("Tar_Chest"), TEXT("Tar_Chest_") + Prefix + TEXT("z"));
        Mapping.Add(TEXT("Tar_Head"), TEXT("Tar_Head_") + Prefix + TEXT("z"));
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
    
    // 获取当前 LevelSequence
    ULevelSequence* LevelSequence = UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRig: No LevelSequence found"));
        return nullptr;
    }
    
    // 使用通用接口查询 ControlRig
    UControlRig* ControlRig = CacheSubsystem->GetControlRig(Actor, LevelSequence);
    
    // 如果 ControlRig 为空，尝试触发注册后再查询
    if (!ControlRig) {
        UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRig: ControlRig is null, triggering registration for %s"), *Actor->GetName());
        
        // 触发注册
        CacheSubsystem->TriggerRegistrationIfNeeded(Actor, LevelSequence);
        
        // 再次查询
        ControlRig = CacheSubsystem->GetControlRig(Actor, LevelSequence);
        
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
    
    // 获取当前 LevelSequence
    ULevelSequence* LevelSequence = UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning, TEXT("GetCachedControlRigBlueprint: No LevelSequence found"));
        return nullptr;
    }
    
    // 使用通用接口查询 ControlRigBlueprint
    UControlRigBlueprint* ControlRigBlueprint = CacheSubsystem->GetControlRigBlueprint(Actor, LevelSequence);
    
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
}

void ABeatBloomUnreal::AddTargetRecorders() {
    // 为目标控制器添加记录器（只给手部可驱动的组件）
    TSet<FString> HandDrivenComponents;
    
    // 收集所有手部可驱动的鼓组件
    for (const FBeatBloomDrumComponent& Component : DrumKitConfig.Components) {
        for (const FBeatBloomDrivableLimb& DrivableLimb : Component.DrivableLimbs) {
            if (DrivableLimb.Limb == TEXT("left_hand") || DrivableLimb.Limb == TEXT("right_hand")) {
                HandDrivenComponents.Add(Component.Name);
                break;
            }
        }
    }
    
    // 收集所有手部可驱动的特殊动作
    for (const FBeatBloomSpecialAction& Action : DrumKitConfig.SpecialActions) {
        if (Action.Limbs.Contains(TEXT("left_hand")) || Action.Limbs.Contains(TEXT("right_hand"))) {
            HandDrivenComponents.Add(Action.Name);
        }
    }
    
    // 为目标记录器生成三种状态
    TArray<EBeatBloomState> States = {
        EBeatBloomState::BEAT,
        EBeatBloomState::READY,
        EBeatBloomState::REST
    };
    
    for (const FString& ComponentName : HandDrivenComponents) {
        for (EBeatBloomState State : States) {
            FString StateStr = GetStateString(State);
            FString Prefix = ComponentName + TEXT("_") + StateStr + TEXT("_");
            
            // Tar_Body, Tar_Chest, Tar_Head 的 Z 轴记录器
            FString BodyRecorder = TEXT("Tar_Body_") + Prefix + TEXT("z");
            FString ChestRecorder = TEXT("Tar_Chest_") + Prefix + TEXT("z");
            FString HeadRecorder = TEXT("Tar_Head_") + Prefix + TEXT("z");
            
            AddRecorder(BodyRecorder);
            AddRecorder(ChestRecorder);
            AddRecorder(HeadRecorder);
            
            TargetRecorders.Add(BodyRecorder, BodyRecorder);
            TargetRecorders.Add(ChestRecorder, ChestRecorder);
            TargetRecorders.Add(HeadRecorder, HeadRecorder);
        }
    }
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
