#include "HarpGlideUnreal.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "HarpGlideControlRigProcessor.h"
#include "InstrumentAnimationUtility.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

// ============================================================
// 辅助命名空间：JSON 读写工具
// ============================================================

struct FHarpGlideJsonHelpers {
    static bool ReadLocationFromArray(const TArray<TSharedPtr<FJsonValue>>& Arr,
                                      FVector& Out) {
        if (Arr.Num() != 3) return false;
        Out.X = Arr[0]->AsNumber();
        Out.Y = Arr[1]->AsNumber();
        Out.Z = Arr[2]->AsNumber();
        return true;
    }

    // JSON 四元数 [w,x,y,z] → UE FQuat (X,Y,Z,W)
    static bool ReadRotationFromArray(const TArray<TSharedPtr<FJsonValue>>& Arr,
                                      FQuat& Out) {
        if (Arr.Num() != 4) return false;
        // JSON: [w, x, y, z], FQuat 构造: FQuat(X, Y, Z, W)
        Out.X = Arr[1]->AsNumber();
        Out.Y = Arr[2]->AsNumber();
        Out.Z = Arr[3]->AsNumber();
        Out.W = Arr[0]->AsNumber();
        return true;
    }

    static TArray<TSharedPtr<FJsonValue>> VecToJsonArray(const FVector& V) {
        return {MakeShareable(new FJsonValueNumber(V.X)),
                MakeShareable(new FJsonValueNumber(V.Y)),
                MakeShareable(new FJsonValueNumber(V.Z))};
    }

    // UE FQuat (X,Y,Z,W) → JSON [w,x,y,z]
    static TArray<TSharedPtr<FJsonValue>> QuatToJsonArray(const FQuat& Q) {
        return {MakeShareable(new FJsonValueNumber(Q.W)),
                MakeShareable(new FJsonValueNumber(Q.X)),
                MakeShareable(new FJsonValueNumber(Q.Y)),
                MakeShareable(new FJsonValueNumber(Q.Z))};
    }
};

// ============================================================
// 构造函数
// ============================================================

AHarpGlideUnreal::AHarpGlideUnreal() {
    PrimaryActorTick.bCanEverTick = true;

    StringNumber = 47;
    LeftFar = 0;
    LeftNear = 0;
    LeftMidFar = 0;
    LeftMidNear = 0;
    RightFar = 0;
    RightNear = 0;
    bIsUnreal = true;

    CurrentLeftHandPose = EHarpGlideHandPose::FAR;
    CurrentRightHandPose = EHarpGlideHandPose::FAR;
    CurrentTiltState = EHarpGlideTiltState::MID;
    CurrentPedalNote = EHarpGlidePedalNote::D;
    CurrentPedalState = EHarpGlidePedalState::NATURAL;

    InitializeControllersAndRecorders();
}

void AHarpGlideUnreal::BeginPlay() { Super::BeginPlay(); }

// ============================================================
// Tick
// ============================================================

void AHarpGlideUnreal::Tick(float DeltaTime) { Super::Tick(DeltaTime); }

// ============================================================
// InitializeControllersAndRecorders
// 直接翻译自 Blender 插件:
//   harp_controllers.py ControllerMixin.__init__
//   harp_recorders.py RecorderMixin.__init__
// ============================================================

void AHarpGlideUnreal::InitializeControllersAndRecorders() {
    // ========== 身体控制器（2 个） ==========
    BodyControllers.Empty();
    BodyControllers.Add(TEXT("head"), TEXT("Head"));
    BodyControllers.Add(TEXT("shoulder_harp"), TEXT("Shoulder_Harp"));

    // ========== 左手控制器（7 个） ==========
    LeftHandControllers.Empty();
    LeftHandControllers.Add(TEXT("left_hand_controller"), TEXT("H_L"));
    LeftHandControllers.Add(TEXT("left_hand_ik_pivot"), TEXT("HP_L"));
    LeftHandControllers.Add(TEXT("left_thumb_controller"), TEXT("T_L"));
    LeftHandControllers.Add(TEXT("left_index_controller"), TEXT("I_L"));
    LeftHandControllers.Add(TEXT("left_middle_controller"), TEXT("M_L"));
    LeftHandControllers.Add(TEXT("left_ring_controller"), TEXT("R_L"));
    LeftHandControllers.Add(TEXT("left_little_controller"), TEXT("P_L"));

    // ========== 右手控制器（7 个） ==========
    RightHandControllers.Empty();
    RightHandControllers.Add(TEXT("right_hand_controller"), TEXT("H_R"));
    RightHandControllers.Add(TEXT("right_hand_ik_pivot"), TEXT("HP_R"));
    RightHandControllers.Add(TEXT("right_thumb_controller"), TEXT("T_R"));
    RightHandControllers.Add(TEXT("right_index_controller"), TEXT("I_R"));
    RightHandControllers.Add(TEXT("right_middle_controller"), TEXT("M_R"));
    RightHandControllers.Add(TEXT("right_ring_controller"), TEXT("R_R"));
    RightHandControllers.Add(TEXT("right_little_controller"), TEXT("P_R"));

    // ========== 脚部控制器（4 个） ==========
    FootControllers.Empty();
    FootControllers.Add(TEXT("left_foot_controller"), TEXT("F_L"));
    FootControllers.Add(TEXT("left_foot_pole"), TEXT("FP_L"));
    FootControllers.Add(TEXT("right_foot_controller"), TEXT("F_R"));
    FootControllers.Add(TEXT("right_foot_pole"), TEXT("FP_R"));

    // ========== Target 控制器（3 个） ==========
    TargetControllers.Empty();
    TargetControllers.Add(TEXT("mid_hand"), TEXT("Mid_Hand"));
    TargetControllers.Add(TEXT("look_at"), TEXT("Look_At"));

    // ========== 竖琴支点控制器（1 个） ==========
    HarpPivotControllers.Empty();
    HarpPivotControllers.Add(TEXT("harp_pivot"), TEXT("harp_pivot"));

    // ========== 手指极向量控制器（10 个，仅手动调节，不参与 Save/Load）
    // ==========
    HandPoleControllers.Empty();
    HandPoleControllers.Add(TEXT("left_thumb_pole"), TEXT("T_L_pole"));
    HandPoleControllers.Add(TEXT("left_index_pole"), TEXT("I_L_pole"));
    HandPoleControllers.Add(TEXT("left_middle_pole"), TEXT("M_L_pole"));
    HandPoleControllers.Add(TEXT("left_ring_pole"), TEXT("R_L_pole"));
    HandPoleControllers.Add(TEXT("left_little_pole"), TEXT("P_L_pole"));
    HandPoleControllers.Add(TEXT("right_thumb_pole"), TEXT("T_R_pole"));
    HandPoleControllers.Add(TEXT("right_index_pole"), TEXT("I_R_pole"));
    HandPoleControllers.Add(TEXT("right_middle_pole"), TEXT("M_R_pole"));
    HandPoleControllers.Add(TEXT("right_ring_pole"), TEXT("R_R_pole"));
    HandPoleControllers.Add(TEXT("right_little_pole"), TEXT("P_R_pole"));

    // ========== 弦位置记录器（94 个：47弦 × 2点 head/end） ==========
    // 命名：s{n}head / s{n}end（n = 0..StringNumber-1）
    // Blender: self.string_recorders[f's{n}_head'] = f's{n}head'
    StringPositionRecorders.Empty();
    for (int32 i = 0; i < StringNumber; ++i) {
        StringPositionRecorders.Add(FString::Printf(TEXT("s%d_head"), i),
                                    FString::Printf(TEXT("s%dhead"), i));
        StringPositionRecorders.Add(FString::Printf(TEXT("s%d_end"), i),
                                    FString::Printf(TEXT("s%dend"), i));
    }

    // ========== 踏板位置记录器（35 个：7唱名 × 5档位） ==========
    // 命名：pedal_{note}_state{n}（n = 0..4）
    // Blender: self.pedal_position_recorders[key] = key
    PedalPositionRecorders.Empty();
    const TArray<FString> PedalNotes = {TEXT("D"), TEXT("C"), TEXT("B"),
                                        TEXT("E"), TEXT("F"), TEXT("G"),
                                        TEXT("A")};
    for (const FString& Note : PedalNotes) {
        for (int32 State = 0; State <= 4; ++State) {
            FString Key =
                FString::Printf(TEXT("pedal_%s_state%d"), *Note, State);
            PedalPositionRecorders.Add(Key, Key);
        }
    }

    // ========== 竖琴支点状态记录器（3 个） ==========
    // 命名：harp_pivot_near / harp_pivot_mid / harp_pivot_far
    HarpPivotRecorders.Empty();
    HarpPivotRecorders.Add(TEXT("harp_pivot_near"), TEXT("harp_pivot_near"));
    HarpPivotRecorders.Add(TEXT("harp_pivot_mid"), TEXT("harp_pivot_mid"));
    HarpPivotRecorders.Add(TEXT("harp_pivot_far"), TEXT("harp_pivot_far"));

    // ========== 手部姿势记录器生成 ==========
    // 姿势值：far / near / attack / rest
    // 命名：{ControllerName}_{pose}
    // Blender: _generate_hand_recorders() → key =
    // f'{ctrl_name}_{pose_state.value}'
    const TArray<FString> PoseSuffixes = {TEXT("far"), TEXT("near"),
                                          TEXT("attack"), TEXT("rest")};

    auto GenerateHandRecorders = [&](TMap<FString, FString>& OutMap,
                                     const TMap<FString, FString>& CtrlMap) {
        OutMap.Empty();
        for (const FString& Suffix : PoseSuffixes) {
            for (const auto& CtrlPair : CtrlMap) {
                const FString& CtrlName = CtrlPair.Value;
                FString RecKey =
                    FString::Printf(TEXT("%s_%s"), *CtrlName, *Suffix);
                OutMap.Add(RecKey, RecKey);
            }
        }
    };

    // 左手姿势记录器（28 个：7控制器 × 4姿势）
    GenerateHandRecorders(LeftHandRecorders, LeftHandControllers);

    // 右手姿势记录器（28 个）
    GenerateHandRecorders(RightHandRecorders, RightHandControllers);

    // ========== 头部姿势记录器（4 个：1控制器 × 4姿势） ==========
    HeadRecorders.Empty();
    for (const FString& Suffix : PoseSuffixes) {
        FString Key = FString::Printf(TEXT("Head_%s"), *Suffix);
        HeadRecorders.Add(Key, Key);
    }

    // ========== 脚部休息记录器（2 个） ==========
    FootRestRecorders.Empty();
    FootRestRecorders.Add(TEXT("F_rest_L"), TEXT("F_rest_L"));
    FootRestRecorders.Add(TEXT("F_rest_R"), TEXT("F_rest_R"));

    // ========== RecorderTransforms 默认条目初始化 ==========
    RecorderTransforms.Empty();
    FHarpGlideRecorderTransform DefaultTransform;

    auto AddDefaults = [&](const TMap<FString, FString>& Map) {
        for (const auto& Pair : Map) {
            RecorderTransforms.FindOrAdd(Pair.Value) = DefaultTransform;
        }
    };

    AddDefaults(StringPositionRecorders);
    AddDefaults(PedalPositionRecorders);
    AddDefaults(HarpPivotRecorders);
    AddDefaults(LeftHandRecorders);
    AddDefaults(RightHandRecorders);
    AddDefaults(HeadRecorders);
    AddDefaults(FootRestRecorders);

    UE_LOG(LogTemp, Warning,
           TEXT("HarpGlideUnreal: InitializeControllersAndRecorders completed."
                " Ctrl=%d Rec=%d StringPos=%d PedalPos=%d HandRec=%d"),
           BodyControllers.Num() + LeftHandControllers.Num() +
               RightHandControllers.Num() + FootControllers.Num() +
               TargetControllers.Num() + HarpPivotControllers.Num() +
               HandPoleControllers.Num() + BilinearHelpers.Num(),
           RecorderTransforms.Num(), StringPositionRecorders.Num(),
           PedalPositionRecorders.Num(),
           LeftHandRecorders.Num() + RightHandRecorders.Num());
}

// ============================================================
// 状态映射方法
// ============================================================

TMap<FString, FString> AHarpGlideUnreal::GetLeftHandControllerToRecorderMapping(
    EHarpGlideHandPose Pose) const {
    TMap<FString, FString> Result;
    FString PoseStr = GetHandPoseString(Pose);

    for (const auto& Pair : LeftHandControllers) {
        const FString& Key = Pair.Key;
        const FString& CtrlName = Pair.Value;

        // 排除 pole 型控制器（不参与 Save/Load）
        if (Key.Contains(TEXT("_pole"))) continue;

        FString RecName = FString::Printf(TEXT("%s_%s"), *CtrlName, *PoseStr);
        Result.Add(CtrlName, RecName);
    }
    // 左手始终同步头部
    FString HeadRecName = FString::Printf(TEXT("Head_%s"), *PoseStr);
    Result.Add(TEXT("Head"), HeadRecName);

    return Result;
}

TMap<FString, FString>
AHarpGlideUnreal::GetRightHandControllerToRecorderMapping(
    EHarpGlideHandPose Pose) const {
    TMap<FString, FString> Result;
    FString PoseStr = GetHandPoseString(Pose);

    for (const auto& Pair : RightHandControllers) {
        const FString& Key = Pair.Key;
        const FString& CtrlName = Pair.Value;

        if (Key.Contains(TEXT("_pole"))) continue;

        FString RecName = FString::Printf(TEXT("%s_%s"), *CtrlName, *PoseStr);
        Result.Add(CtrlName, RecName);
    }
    // 右手也同步头部
    FString HeadRecName = FString::Printf(TEXT("Head_%s"), *PoseStr);
    Result.Add(TEXT("Head"), HeadRecName);

    return Result;
}

TMap<FString, FString> AHarpGlideUnreal::GetLeftHandRecorderToControllerMapping(
    EHarpGlideHandPose Pose) const {
    TMap<FString, FString> Result;
    FString PoseStr = GetHandPoseString(Pose);

    for (const auto& Pair : LeftHandControllers) {
        const FString& Key = Pair.Key;
        const FString& CtrlName = Pair.Value;

        if (Key.Contains(TEXT("_pole"))) continue;

        FString RecName = FString::Printf(TEXT("%s_%s"), *CtrlName, *PoseStr);
        Result.Add(RecName, CtrlName);
    }
    // 头部反向映射
    FString HeadRecName = FString::Printf(TEXT("Head_%s"), *PoseStr);
    Result.Add(HeadRecName, TEXT("Head"));

    return Result;
}

TMap<FString, FString>
AHarpGlideUnreal::GetRightHandRecorderToControllerMapping(
    EHarpGlideHandPose Pose) const {
    TMap<FString, FString> Result;
    FString PoseStr = GetHandPoseString(Pose);

    for (const auto& Pair : RightHandControllers) {
        const FString& Key = Pair.Key;
        const FString& CtrlName = Pair.Value;

        if (Key.Contains(TEXT("_pole"))) continue;

        FString RecName = FString::Printf(TEXT("%s_%s"), *CtrlName, *PoseStr);
        Result.Add(RecName, CtrlName);
    }
    FString HeadRecName = FString::Printf(TEXT("Head_%s"), *PoseStr);
    Result.Add(HeadRecName, TEXT("Head"));

    return Result;
}

// ============================================================
// ControlRig 缓存 — 通过 UControlRigCacheSubsystem 管理
// ============================================================

UControlRig* AHarpGlideUnreal::GetCachedControlRig(FName ComponentName) {
    if (!GEngine) return nullptr;

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) return nullptr;

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return nullptr;

    ASkeletalMeshActor* TargetActor = GetSkeletalMeshActorByName(ComponentName);
    if (!TargetActor) return nullptr;

    return CacheSubsystem->GetControlRig(TargetActor, LevelSequence);
}

UControlRigBlueprint* AHarpGlideUnreal::GetCachedControlRigBlueprint(
    FName ComponentName) {
    if (!GEngine) return nullptr;

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) return nullptr;

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return nullptr;

    ASkeletalMeshActor* TargetActor = GetSkeletalMeshActorByName(ComponentName);
    if (!TargetActor) return nullptr;

    return CacheSubsystem->GetControlRigBlueprint(TargetActor, LevelSequence);
}

void AHarpGlideUnreal::RegisterAllControlRigs() {
    if (!GEngine) return;

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) return;

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return;

    // 注册演奏者 CR
    if (SkeletalMeshActor) {
        CacheSubsystem->TriggerRegistrationIfNeeded(SkeletalMeshActor,
                                                    LevelSequence);
    }

    // 注册竖琴 CR
    if (Harp) {
        CacheSubsystem->TriggerRegistrationIfNeeded(Harp, LevelSequence);
    }
}

void AHarpGlideUnreal::TriggerControlRigReregistration(
    const FString& ErrorMessage) {
    UE_LOG(LogTemp, Error,
           TEXT("HarpGlide TriggerControlRigReregistration: %s"),
           *ErrorMessage);

    if (!GEngine) return;

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) return;

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return;

    // 重新注册演奏者 CR
    if (SkeletalMeshActor) {
        CacheSubsystem->TriggerRegistrationIfNeeded(SkeletalMeshActor,
                                                    LevelSequence);
    }
}

ASkeletalMeshActor* AHarpGlideUnreal::GetSkeletalMeshActorByName(
    FName ComponentName) const {
    if (ComponentName == TEXT("Harp")) {
        return Harp;
    }
    if (ComponentName == TEXT("Performer")) {
        return SkeletalMeshActor;
    }
    return nullptr;
}

// ============================================================
// 导入/导出（.harpist 格式）
// ============================================================

void AHarpGlideUnreal::ExportRecorderInfo(const FString& FilePath) {
    if (FilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("HarpGlide::ExportRecorderInfo: FilePath is empty"));
        return;
    }

    TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);

    // config 段：导出场景配置参数
    TSharedPtr<FJsonObject> ConfigObj = MakeShareable(new FJsonObject);
    ConfigObj->SetNumberField(TEXT("string_count"), StringNumber);
    ConfigObj->SetNumberField(TEXT("left_far"), LeftFar);
    ConfigObj->SetNumberField(TEXT("left_near"), LeftNear);
    ConfigObj->SetNumberField(TEXT("left_mid_far"), LeftMidFar);
    ConfigObj->SetNumberField(TEXT("left_mid_near"), LeftMidNear);
    ConfigObj->SetNumberField(TEXT("right_far"), RightFar);
    ConfigObj->SetNumberField(TEXT("right_near"), RightNear);
    ConfigObj->SetBoolField(TEXT("is_unreal"), true);
    Root->SetObjectField(TEXT("config"), ConfigObj);

    // 辅助 lambda：将记录器组的 Transform 写入 JSON
    auto WriteCategory = [&](const FString& CategoryName,
                             const TMap<FString, FString>& RecorderMap,
                             bool bIncludeRotation = true) {
        TSharedPtr<FJsonObject> CatObj = MakeShareable(new FJsonObject);
        for (const auto& Pair : RecorderMap) {
            const FHarpGlideRecorderTransform* T =
                RecorderTransforms.Find(Pair.Value);
            if (T) {
                TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
                Entry->SetArrayField(
                    TEXT("location"),
                    FHarpGlideJsonHelpers::VecToJsonArray(T->Location));
                if (bIncludeRotation) {
                    Entry->SetArrayField(
                        TEXT("rotation"),
                        FHarpGlideJsonHelpers::QuatToJsonArray(T->Rotation));
                }
                CatObj->SetObjectField(*Pair.Value, Entry);
            }
        }
        if (CatObj->Values.Num() > 0) {
            Root->SetObjectField(*CategoryName, CatObj);
        }
    };

    // 脚部控制器单独写入（key=控制器名）
    auto WriteFootControllers = [&]() {
        TSharedPtr<FJsonObject> CatObj = MakeShareable(new FJsonObject);
        for (const auto& Pair : FootControllers) {
            const FHarpGlideRecorderTransform* T =
                RecorderTransforms.Find(Pair.Value);
            if (T) {
                TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
                Entry->SetArrayField(
                    TEXT("location"),
                    FHarpGlideJsonHelpers::VecToJsonArray(T->Location));
                Entry->SetArrayField(
                    TEXT("rotation"),
                    FHarpGlideJsonHelpers::QuatToJsonArray(T->Rotation));
                CatObj->SetObjectField(*Pair.Value, Entry);
            }
        }
        if (CatObj->Values.Num() > 0) {
            Root->SetObjectField(TEXT("FOOT_CONTROLLERS"), CatObj);
        }
    };

    WriteCategory(TEXT("STRING_RECORDERS"), StringPositionRecorders);
    WriteCategory(TEXT("PEDAL_POSITION_RECORDERS"), PedalPositionRecorders);
    WriteCategory(TEXT("HARP_PIVOT_RECORDERS"), HarpPivotRecorders);
    WriteCategory(TEXT("LEFT_HAND_RECORDERS"), LeftHandRecorders);
    WriteCategory(TEXT("RIGHT_HAND_RECORDERS"), RightHandRecorders);
    WriteCategory(TEXT("HEAD_RECORDERS"), HeadRecorders);
    WriteCategory(TEXT("FOOT_REST_RECORDERS"), FootRestRecorders);
    WriteFootControllers();

    // 双线性映射辅助控制器（只导出 location）
    WriteCategory(TEXT("BILINEAR_HELPERS"), BilinearHelpers, false);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    if (FFileHelper::SaveStringToFile(Output, *FilePath)) {
        UE_LOG(LogTemp, Warning,
               TEXT("HarpGlide::ExportRecorderInfo: Saved to %s"), *FilePath);
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("HarpGlide::ExportRecorderInfo: Failed to save to %s"),
               *FilePath);
    }
}

bool AHarpGlideUnreal::ImportRecorderInfo(const FString& FilePath) {
    if (FilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("HarpGlide::ImportRecorderInfo: FilePath is empty"));
        return false;
    }

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("HarpGlide::ImportRecorderInfo: Failed to load %s"),
               *FilePath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("HarpGlide::ImportRecorderInfo: JSON parse failed for %s"),
               *FilePath);
        return false;
    }

    int32 ImportedCount = 0;

    // 辅助 lambda：从 JSON 类别对象读取 Transform 到 RecorderTransforms
    auto ReadCategory = [&](const FString& CategoryName) {
        if (!Root->HasField(*CategoryName)) return;
        TSharedPtr<FJsonObject> CatObj = Root->GetObjectField(*CategoryName);

        for (const auto& ObjPair : CatObj->Values) {
            const FString& RecorderName = ObjPair.Key;
            TSharedPtr<FJsonObject> Entry = ObjPair.Value->AsObject();
            if (!Entry.IsValid()) continue;

            FHarpGlideRecorderTransform T;

            if (Entry->HasField(TEXT("location"))) {
                FHarpGlideJsonHelpers::ReadLocationFromArray(
                    Entry->GetArrayField(TEXT("location")), T.Location);
            }
            // 兼容 rotation / rotation_quaternion 两种字段名
            if (Entry->HasField(TEXT("rotation"))) {
                FHarpGlideJsonHelpers::ReadRotationFromArray(
                    Entry->GetArrayField(TEXT("rotation")), T.Rotation);
            } else if (Entry->HasField(TEXT("rotation_quaternion"))) {
                FHarpGlideJsonHelpers::ReadRotationFromArray(
                    Entry->GetArrayField(TEXT("rotation_quaternion")),
                    T.Rotation);
            }

            RecorderTransforms.FindOrAdd(RecorderName) = T;
            ImportedCount++;
        }
    };

    // 读取 config 段
    if (Root->HasField(TEXT("config"))) {
        TSharedPtr<FJsonObject> ConfigObj =
            Root->GetObjectField(TEXT("config"));
        int32 LoadedStringCount = StringNumber;
        ConfigObj->TryGetNumberField(TEXT("string_count"), LoadedStringCount);
        if (LoadedStringCount != StringNumber) {
            UE_LOG(LogTemp, Warning,
                   TEXT("HarpGlide::ImportRecorderInfo: "
                        "config.string_count=%d, current StringNumber=%d"),
                   LoadedStringCount, StringNumber);
        }
        // 读取手部位置参数
        ConfigObj->TryGetNumberField(TEXT("left_far"), LeftFar);
        ConfigObj->TryGetNumberField(TEXT("left_near"), LeftNear);
        ConfigObj->TryGetNumberField(TEXT("left_mid_far"), LeftMidFar);
        ConfigObj->TryGetNumberField(TEXT("left_mid_near"), LeftMidNear);
        ConfigObj->TryGetNumberField(TEXT("right_far"), RightFar);
        ConfigObj->TryGetNumberField(TEXT("right_near"), RightNear);
        UE_LOG(LogTemp, Warning,
               TEXT("HarpGlide::ImportRecorderInfo: Config imported"));
    }

    ReadCategory(TEXT("STRING_RECORDERS"));
    ReadCategory(TEXT("PEDAL_POSITION_RECORDERS"));
    ReadCategory(TEXT("HARP_PIVOT_RECORDERS"));
    ReadCategory(TEXT("LEFT_HAND_RECORDERS"));
    ReadCategory(TEXT("RIGHT_HAND_RECORDERS"));
    ReadCategory(TEXT("HEAD_RECORDERS"));
    ReadCategory(TEXT("FOOT_REST_RECORDERS"));
    ReadCategory(TEXT("FOOT_CONTROLLERS"));
    ReadCategory(TEXT("BILINEAR_HELPERS"));

    UE_LOG(LogTemp, Warning,
           TEXT("HarpGlide::ImportRecorderInfo: Imported %d recorders from %s"),
           ImportedCount, *FilePath);

    if (ImportedCount > 0) {
        // 将 STRING_RECORDERS 中的弦位置数据回写到 Control Rig 控制器
        UControlRig* ControlRig = GetCachedControlRig(TEXT("Performer"));
        if (ControlRig) {
            UHarpGlideControlRigProcessor::ApplyStringPositionToControlRig(
                this, ControlRig);
        } else {
            UE_LOG(
                LogTemp, Warning,
                TEXT("HarpGlide::ImportRecorderInfo: ControlRig not "
                     "available, string positions in RecorderTransforms only"));
        }

        MarkPackageDirty();
    }

    return ImportedCount > 0;
}

// ============================================================
// 静态辅助
// ============================================================

FString AHarpGlideUnreal::GetHandPoseString(EHarpGlideHandPose Pose) {
    switch (Pose) {
        case EHarpGlideHandPose::FAR:
            return TEXT("far");
        case EHarpGlideHandPose::NEAR:
            return TEXT("near");
        case EHarpGlideHandPose::ATTACK:
            return TEXT("attack");
        case EHarpGlideHandPose::REST:
            return TEXT("rest");
        default:
            return TEXT("far");
    }
}

FString AHarpGlideUnreal::GetPedalNoteString(EHarpGlidePedalNote Note) {
    switch (Note) {
        case EHarpGlidePedalNote::D:
            return TEXT("D");
        case EHarpGlidePedalNote::C:
            return TEXT("C");
        case EHarpGlidePedalNote::B:
            return TEXT("B");
        case EHarpGlidePedalNote::E:
            return TEXT("E");
        case EHarpGlidePedalNote::F:
            return TEXT("F");
        case EHarpGlidePedalNote::G:
            return TEXT("G");
        case EHarpGlidePedalNote::A:
            return TEXT("A");
        default:
            return TEXT("D");
    }
}

FString AHarpGlideUnreal::GetPedalStateString(EHarpGlidePedalState State) {
    switch (State) {
        case EHarpGlidePedalState::FLAT:
            return TEXT("state0");
        case EHarpGlidePedalState::STATE_1:
            return TEXT("state1");
        case EHarpGlidePedalState::NATURAL:
            return TEXT("state2");
        case EHarpGlidePedalState::STATE_3:
            return TEXT("state3");
        case EHarpGlidePedalState::SHARP:
            return TEXT("state4");
        default:
            return TEXT("state2");
    }
}

FString AHarpGlideUnreal::GetTiltStateString(EHarpGlideTiltState State) {
    switch (State) {
        case EHarpGlideTiltState::NEAR:
            return TEXT("near");
        case EHarpGlideTiltState::MID:
            return TEXT("mid");
        case EHarpGlideTiltState::FAR:
            return TEXT("far");
        default:
            return TEXT("mid");
    }
}
