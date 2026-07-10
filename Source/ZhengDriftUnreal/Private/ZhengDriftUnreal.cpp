#include "ZhengDriftUnreal.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "InstrumentAnimationUtility.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "ZhengDriftControlRigProcessor.h"

// ============================================================
// 辅助命名空间：JSON 读写工具
// ============================================================

struct FZhengDriftHelpers {
    static bool ReadLocationFromArray(const TArray<TSharedPtr<FJsonValue>>& Arr,
                                      FVector& Out) {
        if (Arr.Num() != 3) return false;
        Out.X = Arr[0]->AsNumber();
        Out.Y = Arr[1]->AsNumber();
        Out.Z = Arr[2]->AsNumber();
        return true;
    }

    // JSON 四元数格式 [x,y,z,w]（Rust/Blender）→ UE FQuat [w,x,y,z]
    static bool ReadRotationFromArray(const TArray<TSharedPtr<FJsonValue>>& Arr,
                                      FQuat& Out) {
        if (Arr.Num() != 4) return false;
        // 读取 WXYZ 顺序
        Out.W = Arr[0]->AsNumber();
        Out.X = Arr[1]->AsNumber();
        Out.Y = Arr[2]->AsNumber();
        Out.Z = Arr[3]->AsNumber();
        return true;
    }

    static TArray<TSharedPtr<FJsonValue>> VecToJsonArray(const FVector& V) {
        return {MakeShareable(new FJsonValueNumber(V.X)),
                MakeShareable(new FJsonValueNumber(V.Y)),
                MakeShareable(new FJsonValueNumber(V.Z))};
    }

    static TArray<TSharedPtr<FJsonValue>> QuatToJsonArray(const FQuat& Q) {
        // 输出格式 [w,x,y,z] 统一使用 WXYZ 顺序
        return {MakeShareable(new FJsonValueNumber(Q.W)),
                MakeShareable(new FJsonValueNumber(Q.X)),
                MakeShareable(new FJsonValueNumber(Q.Y)),
                MakeShareable(new FJsonValueNumber(Q.Z))};
    }
};

// ============================================================
// 构造函数
// ============================================================

AZhengDriftUnreal::AZhengDriftUnreal() {
    PrimaryActorTick.bCanEverTick = true;

    StringNumber = 21;

    CurrentLeftHandPosition = EZhengDriftHandPosition::MIDDLE;
    CurrentLeftHandAction = EZhengDriftLeftHandAction::NORMAL;
    CurrentRightHandPosition = EZhengDriftHandPosition::MIDDLE;
    CurrentRightHandAction = EZhengDriftRightHandAction::NORMAL;

    InitializeControllersAndRecorders();
}

void AZhengDriftUnreal::BeginPlay() { Super::BeginPlay(); }

// ============================================================
// Tick
// ============================================================

void AZhengDriftUnreal::Tick(float DeltaTime) { Super::Tick(DeltaTime); }

// ============================================================
// InitializeControllersAndRecorders
// ============================================================

void AZhengDriftUnreal::InitializeControllersAndRecorders() {
    // ========== 左手控制器（12 个） ==========
    LeftHandControllers.Empty();
    LeftHandControllers.Add(TEXT("left_hand_controller"), TEXT("H_L"));
    LeftHandControllers.Add(TEXT("left_hand_ik_pivot"), TEXT("HP_L"));
    LeftHandControllers.Add(TEXT("left_thumb_controller"), TEXT("T_L"));
    LeftHandControllers.Add(TEXT("left_thumb_ik_pivot"), TEXT("TP_L"));
    LeftHandControllers.Add(TEXT("left_index_controller"), TEXT("I_L"));
    LeftHandControllers.Add(TEXT("left_middle_controller"), TEXT("M_L"));
    LeftHandControllers.Add(TEXT("left_ring_controller"), TEXT("R_L"));
    LeftHandControllers.Add(TEXT("left_little_controller"), TEXT("P_L"));
    LeftHandControllers.Add(TEXT("left_index_pole"), TEXT("I_L_pole"));
    LeftHandControllers.Add(TEXT("left_middle_pole"), TEXT("M_L_pole"));
    LeftHandControllers.Add(TEXT("left_ring_pole"), TEXT("R_L_pole"));
    LeftHandControllers.Add(TEXT("left_little_pole"), TEXT("P_L_pole"));

    // ========== 右手控制器（12 个） ==========
    RightHandControllers.Empty();
    RightHandControllers.Add(TEXT("right_hand_controller"), TEXT("H_R"));
    RightHandControllers.Add(TEXT("right_hand_ik_pivot"), TEXT("HP_R"));
    RightHandControllers.Add(TEXT("right_thumb_controller"), TEXT("T_R"));
    RightHandControllers.Add(TEXT("right_thumb_ik_pivot"), TEXT("TP_R"));
    RightHandControllers.Add(TEXT("right_index_controller"), TEXT("I_R"));
    RightHandControllers.Add(TEXT("right_middle_controller"), TEXT("M_R"));
    RightHandControllers.Add(TEXT("right_ring_controller"), TEXT("R_R"));
    RightHandControllers.Add(TEXT("right_little_controller"), TEXT("P_R"));
    RightHandControllers.Add(TEXT("right_index_pole"), TEXT("I_R_pole"));
    RightHandControllers.Add(TEXT("right_middle_pole"), TEXT("M_R_pole"));
    RightHandControllers.Add(TEXT("right_ring_pole"), TEXT("R_R_pole"));
    RightHandControllers.Add(TEXT("right_little_pole"), TEXT("P_R_pole"));

    // ========== 脚部控制器（4 个） ==========
    FootControllers.Empty();
    FootControllers.Add(TEXT("left_foot_controller"), TEXT("F_L"));
    FootControllers.Add(TEXT("left_foot_pole"), TEXT("F_L_pole"));
    FootControllers.Add(TEXT("right_foot_controller"), TEXT("F_R"));
    FootControllers.Add(TEXT("right_foot_pole"), TEXT("F_R_pole"));

    // ========== Target 控制器（3 个特殊朝向控制器） ==========
    TargetControllers.Empty();
    TargetControllers.Add(TEXT("middle_hand"), TEXT("Middle_Hand"));
    TargetControllers.Add(TEXT("look_at"), TEXT("Look_At"));
    TargetControllers.Add(TEXT("head_control"), TEXT("Head_Control"));

    // ========== 双线性映射辅助控制器（8 个） ==========
    BilinearHelpers.Empty();
    BilinearHelpers.Add(TEXT("middle_hand_a"), TEXT("Middle_Hand_A"));
    BilinearHelpers.Add(TEXT("middle_hand_b"), TEXT("Middle_Hand_B"));
    BilinearHelpers.Add(TEXT("middle_hand_c"), TEXT("Middle_Hand_C"));
    BilinearHelpers.Add(TEXT("middle_hand_d"), TEXT("Middle_Hand_D"));
    BilinearHelpers.Add(TEXT("head_control_a"), TEXT("Head_Control_A"));
    BilinearHelpers.Add(TEXT("head_control_b"), TEXT("Head_Control_B"));
    BilinearHelpers.Add(TEXT("head_control_c"), TEXT("Head_Control_C"));
    BilinearHelpers.Add(TEXT("head_control_d"), TEXT("Head_Control_D"));

    // ========== 弦位置记录器（63 个：21弦 × 3点） ==========
    StringPositionRecorders.Empty();
    for (int32 i = 0; i <= 20; ++i) {
        StringPositionRecorders.Add(FString::Printf(TEXT("s%d_head"), i),
                                    FString::Printf(TEXT("s%dhead"), i));
        StringPositionRecorders.Add(FString::Printf(TEXT("s%d_end"), i),
                                    FString::Printf(TEXT("s%dend"), i));
        StringPositionRecorders.Add(FString::Printf(TEXT("s%d_mid"), i),
                                    FString::Printf(TEXT("s%dmid"), i));
    }

    // ========== 手部记录器生成 ==========
    // 规则（来自 Blender 插件 ZhengBaseState._get_recorder_key）：
    // - Pole 型控制器（*_pole）不生成记录器
    // - 记录器键格式：{Action}_{Position}_{finger}_{hand}
    // - 记录器名格式：{ControllerName}_{Action}_{Position}
    //
    // 手指简写映射：
    //   left_hand_controller    -> h
    //   left_hand_ik_pivot      -> hp
    //   left_thumb_controller   -> t
    //   left_thumb_ik_pivot     -> tp
    //   left_index_controller   -> i
    //   left_middle_controller  -> m
    //   left_ring_controller    -> r
    //   left_little_controller  -> p

    // 非 pole 控制器的 {内部键名后缀, 手指简写} 映射
    struct FFingerEntry {
        FString InternalSuffix;
        FString ShortName;
    };

    const TArray<FFingerEntry> FingerMap = {
        {TEXT("hand_controller"), TEXT("h")},
        {TEXT("hand_ik_pivot"), TEXT("hp")},
        {TEXT("thumb_controller"), TEXT("t")},
        {TEXT("thumb_ik_pivot"), TEXT("tp")},
        {TEXT("index_controller"), TEXT("i")},
        {TEXT("middle_controller"), TEXT("m")},
        {TEXT("ring_controller"), TEXT("r")},
        {TEXT("little_controller"), TEXT("p")},
    };

    const TArray<EZhengDriftHandPosition> Positions = {
        EZhengDriftHandPosition::FAR,
        EZhengDriftHandPosition::MIDDLE,
        EZhengDriftHandPosition::NEAR,
    };

    // --- 左手记录器 ---
    LeftHandRecorders.Empty();
    for (auto Pos : Positions) {
        FString PosStr = GetHandPositionString(Pos);

        for (int32 ActionIdx = 0; ActionIdx < 2; ++ActionIdx) {
            EZhengDriftLeftHandAction Action =
                static_cast<EZhengDriftLeftHandAction>(ActionIdx);
            FString ActStr = GetLeftHandActionString(Action);

            for (const FFingerEntry& F : FingerMap) {
                // 完整内部键名：left_{suffix}
                FString FullKey = TEXT("left_") + F.InternalSuffix;
                const FString* pCtrlName = LeftHandControllers.Find(FullKey);
                if (!pCtrlName) continue;

                // 记录器键：{Action}_{Position}_{short}_l
                FString RecKey = FString::Printf(TEXT("%s_%s_%s_l"), *ActStr,
                                                 *PosStr, *F.ShortName);

                // 记录器名：{ControllerName}_{Action}_{Position}
                FString RecValue = FString::Printf(
                    TEXT("%s_%s_%s"), **pCtrlName, *ActStr, *PosStr);

                LeftHandRecorders.Add(RecKey, RecValue);
            }
        }
    }

    // --- 右手记录器 ---
    RightHandRecorders.Empty();
    for (auto Pos : Positions) {
        FString PosStr = GetHandPositionString(Pos);

        for (int32 ActionIdx = 0; ActionIdx < 2; ++ActionIdx) {
            EZhengDriftRightHandAction Action =
                static_cast<EZhengDriftRightHandAction>(ActionIdx);
            FString ActStr = GetRightHandActionString(Action);

            for (const FFingerEntry& F : FingerMap) {
                FString FullKey = TEXT("right_") + F.InternalSuffix;
                const FString* pCtrlName = RightHandControllers.Find(FullKey);
                if (!pCtrlName) continue;

                FString RecKey = FString::Printf(TEXT("%s_%s_%s_r"), *ActStr,
                                                 *PosStr, *F.ShortName);
                FString RecValue = FString::Printf(
                    TEXT("%s_%s_%s"), **pCtrlName, *ActStr, *PosStr);

                RightHandRecorders.Add(RecKey, RecValue);
            }
        }
    }

    // --- 初始化 RecorderTransforms 默认条目 ---
    RecorderTransforms.Empty();
    FZhengDriftRecorderTransform DefaultTransform;

    auto AddDefaults = [&](const TMap<FString, FString>& Map) {
        for (const auto& Pair : Map) {
            RecorderTransforms.FindOrAdd(Pair.Value) = DefaultTransform;
        }
    };

    AddDefaults(StringPositionRecorders);
    AddDefaults(LeftHandRecorders);
    AddDefaults(RightHandRecorders);

    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDriftUnreal: InitializeControllersAndRecorders completed."
                " L=%d R=%d StringPos=%d RecorderTransforms=%d"),
           LeftHandRecorders.Num(), RightHandRecorders.Num(),
           StringPositionRecorders.Num(), RecorderTransforms.Num());
}

// ============================================================
// 静态辅助
// ============================================================

FString AZhengDriftUnreal::GetHandPositionString(
    EZhengDriftHandPosition Position) {
    switch (Position) {
        case EZhengDriftHandPosition::FAR:
            return TEXT("far");
        case EZhengDriftHandPosition::MIDDLE:
            return TEXT("middle");
        case EZhengDriftHandPosition::NEAR:
            return TEXT("near");
        default:
            return TEXT("middle");
    }
}

FString AZhengDriftUnreal::GetLeftHandActionString(
    EZhengDriftLeftHandAction Action) {
    switch (Action) {
        case EZhengDriftLeftHandAction::NORMAL:
            return TEXT("Normal");
        case EZhengDriftLeftHandAction::PRESS:
            return TEXT("Press");
        default:
            return TEXT("Normal");
    }
}

FString AZhengDriftUnreal::GetRightHandActionString(
    EZhengDriftRightHandAction Action) {
    switch (Action) {
        case EZhengDriftRightHandAction::NORMAL:
            return TEXT("Normal");
        case EZhengDriftRightHandAction::TREMOLO:
            return TEXT("Tremolo");
        default:
            return TEXT("Normal");
    }
}

// ============================================================
// GetSkeletalMeshActorByName
// ============================================================

ASkeletalMeshActor* AZhengDriftUnreal::GetSkeletalMeshActorByName(
    FName ComponentName) const {
    if (ComponentName == TEXT("Zheng")) return Zheng;
    if (ComponentName == TEXT("Performer")) return SkeletalMeshActor;
    return nullptr;
}

// ============================================================
// 状态映射
// ============================================================

TMap<FString, FString>
AZhengDriftUnreal::GetLeftHandControllerToRecorderMapping(
    EZhengDriftHandPosition Position, EZhengDriftLeftHandAction Action) const {
    TMap<FString, FString> Mapping;

    FString PosStr = GetHandPositionString(Position);
    FString ActStr = GetLeftHandActionString(Action);

    // 8 个非 pole 手指简写
    const TArray<FString> ShortNames = {
        TEXT("h"), TEXT("hp"), TEXT("t"), TEXT("tp"),
        TEXT("i"), TEXT("m"),  TEXT("r"), TEXT("p"),
    };

    // 对应控制器名称（同序）
    const TArray<FString> CtrlOrder = {
        TEXT("H_L"), TEXT("HP_L"), TEXT("T_L"), TEXT("TP_L"),
        TEXT("I_L"), TEXT("M_L"),  TEXT("R_L"), TEXT("P_L"),
    };

    for (int32 i = 0; i < ShortNames.Num(); ++i) {
        const FString& ShortName = ShortNames[i];
        const FString& ControllerName = CtrlOrder[i];

        FString RecKey =
            FString::Printf(TEXT("%s_%s_%s_l"), *ActStr, *PosStr, *ShortName);

        const FString* RecValue = LeftHandRecorders.Find(RecKey);
        if (RecValue) {
            Mapping.Add(ControllerName, *RecValue);
        }
    }

    return Mapping;
}

TMap<FString, FString>
AZhengDriftUnreal::GetRightHandControllerToRecorderMapping(
    EZhengDriftHandPosition Position, EZhengDriftRightHandAction Action) const {
    TMap<FString, FString> Mapping;

    FString PosStr = GetHandPositionString(Position);
    FString ActStr = GetRightHandActionString(Action);

    const TArray<FString> ShortNames = {
        TEXT("h"), TEXT("hp"), TEXT("t"), TEXT("tp"),
        TEXT("i"), TEXT("m"),  TEXT("r"), TEXT("p"),
    };
    const TArray<FString> CtrlOrder = {
        TEXT("H_R"), TEXT("HP_R"), TEXT("T_R"), TEXT("TP_R"),
        TEXT("I_R"), TEXT("M_R"),  TEXT("R_R"), TEXT("P_R"),
    };

    for (int32 i = 0; i < ShortNames.Num(); ++i) {
        FString RecKey = FString::Printf(TEXT("%s_%s_%s_r"), *ActStr, *PosStr,
                                         *ShortNames[i]);
        const FString* RecValue = RightHandRecorders.Find(RecKey);
        if (RecValue) {
            Mapping.Add(CtrlOrder[i], *RecValue);
        }
    }

    return Mapping;
}

TMap<FString, FString>
AZhengDriftUnreal::GetLeftHandRecorderToControllerMapping(
    EZhengDriftHandPosition Position, EZhengDriftLeftHandAction Action) const {
    TMap<FString, FString> Reverse;
    for (const auto& Pair :
         GetLeftHandControllerToRecorderMapping(Position, Action)) {
        Reverse.Add(Pair.Value, Pair.Key);
    }
    return Reverse;
}

TMap<FString, FString>
AZhengDriftUnreal::GetRightHandRecorderToControllerMapping(
    EZhengDriftHandPosition Position, EZhengDriftRightHandAction Action) const {
    TMap<FString, FString> Reverse;
    for (const auto& Pair :
         GetRightHandControllerToRecorderMapping(Position, Action)) {
        Reverse.Add(Pair.Value, Pair.Key);
    }
    return Reverse;
}

// ============================================================
// ControlRig 缓存
// ============================================================

UControlRig* AZhengDriftUnreal::GetCachedControlRig(FName ComponentName) {
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("GetCachedControlRig [ZhengDrift]: GEngine is null"));
        return nullptr;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(
            LogTemp, Error,
            TEXT("GetCachedControlRig [ZhengDrift]: CacheSubsystem not found"));
        return nullptr;
    }

    ASkeletalMeshActor* Actor = GetSkeletalMeshActorByName(ComponentName);
    if (!Actor) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("GetCachedControlRig [ZhengDrift]: Actor not found for '%s'"),
            *ComponentName.ToString());
        return nullptr;
    }

    // 根据 ComponentName 确定 RootControlName
    FString RootControlName;
    if (ComponentName == TEXT("Zheng")) {
        RootControlName = TEXT("zheng_root");
    } else if (ComponentName == TEXT("Performer")) {
        RootControlName = TEXT("controller_root");
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("GetCachedControlRig [ZhengDrift]: No LevelSequence found"));
        return nullptr;
    }

    UControlRig* ControlRig =
        CacheSubsystem->GetControlRig(Actor, LevelSequence, RootControlName);

    if (!ControlRig) {
        CacheSubsystem->TriggerRegistrationIfNeeded(Actor, LevelSequence,
                                                    RootControlName);
        ControlRig = CacheSubsystem->GetControlRig(Actor, LevelSequence,
                                                   RootControlName);

        if (!ControlRig) {
            UE_LOG(LogTemp, Error,
                   TEXT("GetCachedControlRig [ZhengDrift]: Still null after "
                        "registration for '%s'"),
                   *Actor->GetName());
        }
    }

    return ControlRig;
}

UControlRigBlueprint* AZhengDriftUnreal::GetCachedControlRigBlueprint(
    FName ComponentName) {
    if (!GEngine) return nullptr;

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) return nullptr;

    ASkeletalMeshActor* Actor = GetSkeletalMeshActorByName(ComponentName);
    if (!Actor) return nullptr;

    // 根据 ComponentName 确定 RootControlName
    FString RootControlName;
    if (ComponentName == TEXT("Zheng")) {
        RootControlName = TEXT("zheng_root");
    } else if (ComponentName == TEXT("Performer")) {
        RootControlName = TEXT("controller_root");
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return nullptr;

    return CacheSubsystem->GetControlRigBlueprint(Actor, LevelSequence,
                                                  RootControlName);
}

void AZhengDriftUnreal::RegisterAllControlRigs() {
    if (!GEngine) return;

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) return;

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return;

    int32 RegisteredCount = 0;

    if (SkeletalMeshActor) {
        CacheSubsystem->TriggerRegistrationIfNeeded(SkeletalMeshActor,
                                                    LevelSequence);
        RegisteredCount++;
        UE_LOG(LogTemp, Warning,
               TEXT("ZhengDrift::RegisterAllControlRigs: Registered Performer "
                    "'%s'"),
               *SkeletalMeshActor->GetName());
    }

    if (Zheng) {
        CacheSubsystem->TriggerRegistrationIfNeeded(Zheng, LevelSequence);
        RegisteredCount++;
        UE_LOG(
            LogTemp, Warning,
            TEXT("ZhengDrift::RegisterAllControlRigs: Registered Zheng '%s'"),
            *Zheng->GetName());
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("ZhengDrift::RegisterAllControlRigs: Registered %d ControlRigs"),
        RegisteredCount);
}

void AZhengDriftUnreal::TriggerControlRigReregistration(
    const FString& ErrorMessage) {
    UE_LOG(LogTemp, Warning,
           TEXT("ZhengDrift::TriggerControlRigReregistration: %s"),
           *ErrorMessage);
    RegisterAllControlRigs();
}

// ============================================================
// ExportRecorderInfo / ImportRecorderInfo
// ============================================================

void AZhengDriftUnreal::ExportRecorderInfo(const FString& FilePath) {
    if (FilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("ZhengDrift::ExportRecorderInfo: FilePath is empty"));
        return;
    }

    TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);

    // 辅助 lambda：将记录器名称列表的 Transform 写入 JSON 对象
    // 字段名 "rotation" 与 .zheng_master 格式保持一致
    auto WriteCategory = [&](const FString& CategoryName,
                             const TMap<FString, FString>& RecorderMap) {
        TSharedPtr<FJsonObject> CatObj = MakeShareable(new FJsonObject);
        for (const auto& Pair : RecorderMap) {
            const FZhengDriftRecorderTransform* T =
                RecorderTransforms.Find(Pair.Value);
            if (T) {
                TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
                Entry->SetArrayField(
                    TEXT("location"),
                    FZhengDriftHelpers::VecToJsonArray(T->Location));
                Entry->SetArrayField(
                    TEXT("rotation"),
                    FZhengDriftHelpers::QuatToJsonArray(T->Rotation));
                CatObj->SetObjectField(*Pair.Value, Entry);
            }
        }
        if (CatObj->Values.Num() > 0) {
            Root->SetObjectField(*CategoryName, CatObj);
        }
    };

    // 脚部控制器单独写入（key 即控制器名，无 Recorder 中间层）
    auto WriteFootControllers = [&]() {
        TSharedPtr<FJsonObject> CatObj = MakeShareable(new FJsonObject);
        for (const auto& Pair : FootControllers) {
            const FZhengDriftRecorderTransform* T =
                RecorderTransforms.Find(Pair.Value);
            if (T) {
                TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
                Entry->SetArrayField(
                    TEXT("location"),
                    FZhengDriftHelpers::VecToJsonArray(T->Location));
                Entry->SetArrayField(
                    TEXT("rotation"),
                    FZhengDriftHelpers::QuatToJsonArray(T->Rotation));
                CatObj->SetObjectField(*Pair.Value, Entry);
            }
        }
        if (CatObj->Values.Num() > 0) {
            Root->SetObjectField(TEXT("FOOT_CONTROLLERS"), CatObj);
        }
    };

    WriteCategory(TEXT("STRING_RECORDERS"), StringPositionRecorders);
    WriteCategory(TEXT("LEFT_HAND_RECORDERS"), LeftHandRecorders);
    WriteCategory(TEXT("RIGHT_HAND_RECORDERS"), RightHandRecorders);
    WriteFootControllers();

    // 双线性映射辅助控制器（只导出 location）
    {
        TSharedPtr<FJsonObject> CatObj = MakeShareable(new FJsonObject);
        for (const auto& Pair : BilinearHelpers) {
            const FZhengDriftRecorderTransform* T =
                RecorderTransforms.Find(Pair.Value);
            if (T) {
                TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
                Entry->SetArrayField(
                    TEXT("location"),
                    FZhengDriftHelpers::VecToJsonArray(T->Location));
                CatObj->SetObjectField(*Pair.Value, Entry);
            }
        }
        if (CatObj->Values.Num() > 0) {
            Root->SetObjectField(TEXT("BILINEAR_HELPERS"), CatObj);
        }
    }

    // OTHER_SETTINGS：源标记
    {
        TSharedPtr<FJsonObject> SettingsObj = MakeShareable(new FJsonObject);
        SettingsObj->SetBoolField(TEXT("is_unreal"), true);
        Root->SetObjectField(TEXT("OTHER_SETTINGS"), SettingsObj);
    }

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    if (FFileHelper::SaveStringToFile(Output, *FilePath)) {
        UE_LOG(LogTemp, Warning,
               TEXT("ZhengDrift::ExportRecorderInfo: Saved to %s"), *FilePath);
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("ZhengDrift::ExportRecorderInfo: Failed to save to %s"),
               *FilePath);
    }
}

bool AZhengDriftUnreal::ImportRecorderInfo(const FString& FilePath) {
    if (FilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("ZhengDrift::ImportRecorderInfo: FilePath is empty"));
        return false;
    }

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("ZhengDrift::ImportRecorderInfo: Failed to load %s"),
               *FilePath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("ZhengDrift::ImportRecorderInfo: JSON parse failed for %s"),
               *FilePath);
        return false;
    }

    int32 ImportedCount = 0;

    // 辅助 lambda：从 JSON 类别对象中读取 Transform 到 RecorderTransforms
    // 兼容 "rotation"（.zheng_master 格式）和 "rotation_quaternion"（旧格式）
    auto ReadCategory = [&](const FString& CategoryName) {
        if (!Root->HasField(*CategoryName)) return;
        TSharedPtr<FJsonObject> CatObj = Root->GetObjectField(*CategoryName);

        for (const auto& ObjPair : CatObj->Values) {
            const FString& RecorderName = ObjPair.Key;
            TSharedPtr<FJsonObject> Entry = ObjPair.Value->AsObject();
            if (!Entry.IsValid()) continue;

            FZhengDriftRecorderTransform T;

            if (Entry->HasField(TEXT("location"))) {
                FZhengDriftHelpers::ReadLocationFromArray(
                    Entry->GetArrayField(TEXT("location")), T.Location);
            }
            // 兼容两种字段名
            if (Entry->HasField(TEXT("rotation"))) {
                FZhengDriftHelpers::ReadRotationFromArray(
                    Entry->GetArrayField(TEXT("rotation")), T.Rotation);
            } else if (Entry->HasField(TEXT("rotation_quaternion"))) {
                FZhengDriftHelpers::ReadRotationFromArray(
                    Entry->GetArrayField(TEXT("rotation_quaternion")),
                    T.Rotation);
            }

            RecorderTransforms.FindOrAdd(RecorderName) = T;
            ImportedCount++;
        }
    };

    ReadCategory(TEXT("STRING_RECORDERS"));
    ReadCategory(TEXT("LEFT_HAND_RECORDERS"));
    ReadCategory(TEXT("RIGHT_HAND_RECORDERS"));
    ReadCategory(TEXT("FOOT_CONTROLLERS"));

    // 双线性映射辅助控制器（只读取 location）
    if (Root->HasField(TEXT("BILINEAR_HELPERS"))) {
        TSharedPtr<FJsonObject> CatObj =
            Root->GetObjectField(TEXT("BILINEAR_HELPERS"));
        for (const auto& ObjPair : CatObj->Values) {
            const FString& HelperName = ObjPair.Key;
            TSharedPtr<FJsonObject> Entry = ObjPair.Value->AsObject();
            if (!Entry.IsValid()) continue;

            FZhengDriftRecorderTransform T;
            if (Entry->HasField(TEXT("location"))) {
                FZhengDriftHelpers::ReadLocationFromArray(
                    Entry->GetArrayField(TEXT("location")), T.Location);
            }
            RecorderTransforms.FindOrAdd(HelperName) = T;
            ImportedCount++;
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("ZhengDrift::ImportRecorderInfo: Imported %d recorders from %s"),
        ImportedCount, *FilePath);

    if (ImportedCount > 0) {
        // 将 STRING_RECORDERS 中的弦位置数据回写到 Control Rig 控制器
        UControlRig* ControlRig = GetCachedControlRig(TEXT("Performer"));
        if (ControlRig) {
            UZhengDriftControlRigProcessor::ApplyStringPositionToControlRig(
                this, ControlRig);
        } else {
            UE_LOG(
                LogTemp, Warning,
                TEXT("ZhengDrift::ImportRecorderInfo: ControlRig not available,"
                     " string positions stored in RecorderTransforms only"));
        }
    }

    return ImportedCount > 0;
}
