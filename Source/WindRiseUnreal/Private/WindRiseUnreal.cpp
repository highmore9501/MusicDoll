#include "WindRiseUnreal.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "Misc/FileHelper.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "WindRiseAnimationProcessor.h"
#include "WindRiseControlRigProcessor.h"
#include "WindRiseMusicInstrumentProcessor.h"

#define LOCTEXT_NAMESPACE "WindRiseUnreal"

// ============================================================
// 辅助命名空间：JSON 读写工具
// ============================================================

struct FWindRiseHelpers {
    static bool ReadLocationFromArray(const TArray<TSharedPtr<FJsonValue>>& Arr,
                                      FVector& Out) {
        if (Arr.Num() != 3) return false;
        Out.X = Arr[0]->AsNumber();
        Out.Y = Arr[1]->AsNumber();
        Out.Z = Arr[2]->AsNumber();
        return true;
    }

    // JSON 四元数格式 [w, x, y, z] → UE FQuat [w, x, y, z]
    static bool ReadRotationFromArray(const TArray<TSharedPtr<FJsonValue>>& Arr,
                                      FQuat& Out) {
        if (Arr.Num() != 4) return false;
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
        // 输出格式 [w, x, y, z]
        return {MakeShareable(new FJsonValueNumber(Q.W)),
                MakeShareable(new FJsonValueNumber(Q.X)),
                MakeShareable(new FJsonValueNumber(Q.Y)),
                MakeShareable(new FJsonValueNumber(Q.Z))};
    }

    /** 从 FTransform 读取控制器数据为 FWindRiseNoteState 格式 */
    static TSharedPtr<FJsonObject> TransformToControllerJson(
        const FString& CtrlName, const FTransform& Transform) {
        TSharedPtr<FJsonObject> CtrlObj = MakeShareable(new FJsonObject);
        TSharedPtr<FJsonObject> LocObj = MakeShareable(new FJsonObject);

        FVector Loc = Transform.GetLocation();
        FQuat Rot = Transform.GetRotation();

        CtrlObj->SetArrayField(TEXT("location"),
                               {MakeShareable(new FJsonValueNumber(Loc.X)),
                                MakeShareable(new FJsonValueNumber(Loc.Y)),
                                MakeShareable(new FJsonValueNumber(Loc.Z))});
        CtrlObj->SetArrayField(TEXT("rotation"),
                               {MakeShareable(new FJsonValueNumber(Rot.W)),
                                MakeShareable(new FJsonValueNumber(Rot.X)),
                                MakeShareable(new FJsonValueNumber(Rot.Y)),
                                MakeShareable(new FJsonValueNumber(Rot.Z))});
        return CtrlObj;
    }

    /** 从 JSON 读取控制器数据到 FTransform */
    static bool ControllerJsonToTransform(
        const TSharedPtr<FJsonObject>& CtrlObj, FTransform& OutTransform) {
        if (!CtrlObj) return false;

        const TArray<TSharedPtr<FJsonValue>>* LocArr = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* RotArr = nullptr;

        if (!CtrlObj->TryGetArrayField(TEXT("location"), LocArr) ||
            !CtrlObj->TryGetArrayField(TEXT("rotation"), RotArr)) {
            return false;
        }

        FVector Loc;
        FQuat Rot;
        if (!ReadLocationFromArray(*LocArr, Loc)) return false;
        if (!ReadRotationFromArray(*RotArr, Rot)) return false;

        OutTransform = FTransform(Rot, Loc, FVector(1.0f));
        return true;
    }
};

// ============================================================
// MIDI 音名常量
// ============================================================
static const TArray<FString> SEMITONE_NAMES = {
    TEXT("C"),  TEXT("C#"), TEXT("D"),  TEXT("D#"), TEXT("E"),  TEXT("F"),
    TEXT("F#"), TEXT("G"),  TEXT("G#"), TEXT("A"),  TEXT("A#"), TEXT("B"),
};

// ============================================================
// 构造函数
// ============================================================

AWindRiseUnreal::AWindRiseUnreal() {
    PrimaryActorTick.bCanEverTick = true;

    MinNote = 60;
    MaxNote = 84;
    CurrentNote = 60;

    InitializeControllersAndRecorders();
}

void AWindRiseUnreal::BeginPlay() { Super::BeginPlay(); }

void AWindRiseUnreal::Tick(float DeltaTime) { Super::Tick(DeltaTime); }

// ============================================================
// InitializeControllersAndRecorders
// ============================================================

void AWindRiseUnreal::InitializeControllersAndRecorders() {
    UWindRiseControlRigProcessor::InitializeControllers(this);
}

// ============================================================
// 状态管理
// ============================================================

void AWindRiseUnreal::SaveNoteState(int32 MidiNote) {
    if (!SkeletalMeshActor || !SkeletalMeshActor->GetSkeletalMeshComponent()) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("WindRiseUnreal: SaveNoteState - No Performer SkeletalMesh"));
        return;
    }

    FWindRiseNoteState State;
    State.Note = MidiNote;
    State.Name = NoteNumberToName(MidiNote);

    USkeletalMeshComponent* PerformerSkel =
        SkeletalMeshActor->GetSkeletalMeshComponent();
    USkeletalMeshComponent* InstrumentSkel =
        InstrumentMesh ? InstrumentMesh->GetSkeletalMeshComponent() : nullptr;

    // ── ① 获取所有控制器的 CR 变换 ──
    UControlRig* CR = GetCachedControlRig(TEXT("Performer"));
    if (CR) {
        CR->Evaluate_AnyThread();
    }
    UWindRiseControlRigProcessor::CaptureControllers(this, CR, State);

    // ── ② 获取人物 MT（仅非零值）──
    State.CharacterMT.Empty();
    for (int32 i = 0; i < CharacterMorphTargets.Num(); ++i) {
        float Val =
            PerformerSkel->GetMorphTarget(FName(*CharacterMorphTargets[i]));
        if (FMath::Abs(Val) > 0.001f) {
            State.CharacterMT.Add(
                FWindRiseMorphTargetValue(CharacterMorphTargets[i], Val));
        }
    }

    // ── ③ 获取乐器 MT（仅非零值）──
    State.InstrumentMT.Empty();
    if (InstrumentSkel) {
        for (int32 i = 0; i < InstrumentMorphTargets.Num(); ++i) {
            float Val = InstrumentSkel->GetMorphTarget(
                FName(*InstrumentMorphTargets[i]));
            if (FMath::Abs(Val) > 0.001f) {
                State.InstrumentMT.Add(
                    FWindRiseMorphTargetValue(InstrumentMorphTargets[i], Val));
            }
        }
    }

    // ── ④ 存入 NoteStates ──
    NoteStates.Add(MidiNote, State);
    Modify();

    // ── ⑤ 在 Sequencer 中为手部控件写入关键帧 ──
    {  // 影响 HandControllers: H_L, HP_L, T_L, I_L, M_L, R_L, P_L,
        //                    H_R, HP_R, T_R, I_R, M_R, R_R, P_R (14 个)
        TArray<FString> CtrlNames;
        for (const auto& Pair : HandControllers) {
            CtrlNames.Add(Pair.Value);
        }
        UInstrumentAnimationUtility::InsertCurrentPoseKeyframes(CR, CtrlNames);
    }

    UE_LOG(LogTemp, Log,
           TEXT("WindRiseUnreal: Saved state for %s (%d) - %d controllers, "
                "%d char MT, %d inst MT"),
           *State.Name, MidiNote, State.Controllers.Num(),
           State.CharacterMT.Num(), State.InstrumentMT.Num());
}

void AWindRiseUnreal::LoadNoteState(int32 MidiNote) {
    FWindRiseNoteState* State = NoteStates.Find(MidiNote);
    if (!State) {
        UE_LOG(LogTemp, Warning,
               TEXT("WindRiseUnreal: No saved state for note %d"), MidiNote);
        return;
    }

    // ── ① 恢复控制器变换 ──
    UControlRig* CR = GetCachedControlRig(TEXT("Performer"));
    UWindRiseControlRigProcessor::RestoreControllers(this, CR, *State);
    // 注意：这里不能调用 Evaluate_AnyThread()，否则 Sequencer 会用当前帧
    // 的旧关键帧覆盖刚恢复的目标值；传播由 InsertCurrentPoseKeyframes 完成。

    // ── ② 在 Sequencer 中为手部控件写入关键帧 ──
    {  // 影响 HandControllers: H_L, HP_L, T_L, I_L, M_L, R_L, P_L,
        //                    H_R, HP_R, T_R, I_R, M_R, R_R, P_R (14 个)
        TArray<FString> CtrlNames;
        for (const auto& Pair : HandControllers) {
            CtrlNames.Add(Pair.Value);
        }
        UInstrumentAnimationUtility::InsertCurrentPoseKeyframes(CR, CtrlNames);
    }

    // ── ③ 恢复人物 MT ──
    UWindRiseControlRigProcessor::RestoreCharacterMorphTargets(this, *State);

    // ── ④ 恢复乐器 MT ──
    UWindRiseMusicInstrumentProcessor::RestoreInstrumentMorphTargets(this,
                                                                     *State);

    UE_LOG(LogTemp, Log,
           TEXT("WindRiseUnreal: Loaded state for %s (%d) - %d controllers"),
           *State->Name, MidiNote, State->Controllers.Num());
}

// ============================================================
// RestOffset 捕获
// ============================================================

void AWindRiseUnreal::CaptureRestOffset() {
    if (!SkeletalMeshActor || !SkeletalMeshActor->GetSkeletalMeshComponent()) {
        UE_LOG(LogTemp, Warning,
               TEXT("WindRiseUnreal: CaptureRestOffset - No Performer "
                    "SkeletalMesh"));
        return;
    }

    UControlRig* CR = GetCachedControlRig(TEXT("Performer"));
    if (!CR) {
        UE_LOG(LogTemp, Warning,
               TEXT("WindRiseUnreal: CaptureRestOffset - No Performer "
                    "ControlRig"));
        return;
    }

    URigHierarchy* RigHierarchy = CR->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Warning,
               TEXT("WindRiseUnreal: CaptureRestOffset - No RigHierarchy"));
        return;
    }

    CR->Evaluate_AnyThread();

    if (FInstrumentControlRigUtility::GetControlLocalTransform(
            RigHierarchy, TEXT("controller_root_offset"), RestOffset)) {
        Modify();
        UE_LOG(LogTemp, Log,
               TEXT("WindRiseUnreal: Captured RestOffset - Loc: %s, Rot: %s"),
               *RestOffset.GetLocation().ToString(),
               *RestOffset.GetRotation().ToString());
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("WindRiseUnreal: CaptureRestOffset - Failed to get "
                "controller_root_offset transform"));
}

// ============================================================
// ControlRig 操作
// ============================================================

void AWindRiseUnreal::InitializePerformerControlRig() {
    UWindRiseControlRigProcessor::InitializePerformerControlRig(this);
}

void AWindRiseUnreal::InitializeInstrumentControlRig() {
    UWindRiseMusicInstrumentProcessor::InitializeInstrumentControlRig(this);
}

void AWindRiseUnreal::CheckControlRigStatus() {
    UWindRiseControlRigProcessor::CheckControlRigStatus(this);
}

// ============================================================
// .wind 导入/导出
// ============================================================

void AWindRiseUnreal::ImportWindFile(const FString& FilePath) {
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *FilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("WindRiseUnreal: Failed to load .wind file: %s"),
               *FilePath);
        return;
    }

    TSharedPtr<FJsonObject> RootObj;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);
    if (!FJsonSerializer::Deserialize(Reader, RootObj)) {
        UE_LOG(LogTemp, Error,
               TEXT("WindRiseUnreal: Failed to parse .wind JSON"));
        return;
    }

    // ── 解析 config ──
    const TSharedPtr<FJsonObject>* ConfigObj = nullptr;
    if (RootObj->TryGetObjectField(TEXT("config"), ConfigObj)) {
        (*ConfigObj)
            ->TryGetStringField(TEXT("instrument_type"), InstrumentType);
        (*ConfigObj)->TryGetStringField(TEXT("description"), Description);

        // is_unreal — Unreal 端导出的标记，目前仅用于标识，无功能逻辑
        bool bIsUnreal = false;
        (*ConfigObj)->TryGetBoolField(TEXT("is_unreal"), bIsUnreal);

        int32 MinVal = 60, MaxVal = 84;
        if ((*ConfigObj)->TryGetNumberField(TEXT("min_note"), MinVal))
            MinNote = MinVal;
        if ((*ConfigObj)->TryGetNumberField(TEXT("max_note"), MaxVal))
            MaxNote = MaxVal;

        // force_shape_keys → CharacterMorphTargets
        const TArray<TSharedPtr<FJsonValue>>* FSKArr = nullptr;
        if ((*ConfigObj)->TryGetArrayField(TEXT("force_shape_keys"), FSKArr)) {
            CharacterMorphTargets.Empty();
            for (const auto& Val : *FSKArr) {
                CharacterMorphTargets.Add(Val->AsString());
            }
        }

        // instrument_shape_keys → InstrumentMorphTargets
        const TArray<TSharedPtr<FJsonValue>>* ISKArr = nullptr;
        if ((*ConfigObj)
                ->TryGetArrayField(TEXT("instrument_shape_keys"), ISKArr)) {
            InstrumentMorphTargets.Empty();
            for (const auto& Val : *ISKArr) {
                InstrumentMorphTargets.Add(Val->AsString());
            }
        }

        // instrument_mesh_name — 尝试按名称查找乐器 Mesh
        FString InstMeshName;
        if ((*ConfigObj)
                ->TryGetStringField(TEXT("instrument_mesh_name"),
                                    InstMeshName) &&
            !InstMeshName.IsEmpty()) {
            AActor* Found = nullptr;
            // 按名称搜索场景中的 Actor
            for (TActorIterator<ASkeletalMeshActor> It(GetWorld()); It; ++It) {
                if (It->GetName() == InstMeshName) {
                    InstrumentMesh = *It;
                    break;
                }
            }
        }
    }

    // ── 解析 rest_offset（可选字段）──
    const TSharedPtr<FJsonObject>* RestOffsetObj = nullptr;
    if (RootObj->TryGetObjectField(TEXT("rest_offset"), RestOffsetObj)) {
        FVector Loc(0.f);
        FQuat Rot(FQuat::Identity);
        bool bReadLoc = false, bReadRot = false;

        const TArray<TSharedPtr<FJsonValue>>* LocArr = nullptr;
        if ((*RestOffsetObj)->TryGetArrayField(TEXT("location"), LocArr)) {
            bReadLoc = FWindRiseHelpers::ReadLocationFromArray(*LocArr, Loc);
        }

        const TArray<TSharedPtr<FJsonValue>>* RotArr = nullptr;
        if ((*RestOffsetObj)->TryGetArrayField(TEXT("rotation"), RotArr)) {
            bReadRot = FWindRiseHelpers::ReadRotationFromArray(*RotArr, Rot);
        }

        if (bReadLoc && bReadRot) {
            RestOffset = FTransform(Rot, Loc, FVector(1.0f));
        }
    }

    // ── 解析 note_info ──
    NoteStates.Empty();
    const TArray<TSharedPtr<FJsonValue>>* NoteInfoArr = nullptr;
    if (RootObj->TryGetArrayField(TEXT("note_info"), NoteInfoArr)) {
        for (const auto& Item : *NoteInfoArr) {
            const TSharedPtr<FJsonObject>* NoteObj = nullptr;
            if (!Item->TryGetObject(NoteObj)) continue;

            FWindRiseNoteState State;
            int32 NoteNum = 0;
            (*NoteObj)->TryGetNumberField(TEXT("note"), NoteNum);
            State.Note = NoteNum;
            (*NoteObj)->TryGetStringField(TEXT("name"), State.Name);

            // ── controllers ──
            const TSharedPtr<FJsonObject>* ControllersObj = nullptr;
            if ((*NoteObj)->TryGetObjectField(TEXT("controllers"),
                                              ControllersObj)) {
                for (const auto& CtrlPair : ControllersObj->Get()->Values) {
                    const TSharedPtr<FJsonObject>* CtrlValObj = nullptr;
                    if (!CtrlPair.Value->TryGetObject(CtrlValObj)) continue;

                    FTransform CtrlTransform;
                    if (FWindRiseHelpers::ControllerJsonToTransform(
                            *CtrlValObj, CtrlTransform)) {
                        State.Controllers.Add(CtrlPair.Key, CtrlTransform);
                    }
                }
            }

            // ── character_shape_keys ──
            const TArray<TSharedPtr<FJsonValue>>* CharSKArr = nullptr;
            if ((*NoteObj)->TryGetArrayField(TEXT("character_shape_keys"),
                                             CharSKArr)) {
                for (const auto& SKVal : *CharSKArr) {
                    const TSharedPtr<FJsonObject>* SKObj = nullptr;
                    if (!SKVal->TryGetObject(SKObj)) continue;

                    int32 SKIndex = 0;
                    float SKValue = 0.0f;
                    (*SKObj)->TryGetNumberField(TEXT("shape_key_index"),
                                                SKIndex);
                    (*SKObj)->TryGetNumberField(TEXT("value"), SKValue);

                    if (CharacterMorphTargets.IsValidIndex(SKIndex)) {
                        State.CharacterMT.Add(FWindRiseMorphTargetValue(
                            CharacterMorphTargets[SKIndex], SKValue));
                    }
                }
            }

            // ── instrument_shape_keys ──
            const TArray<TSharedPtr<FJsonValue>>* InstSKArr = nullptr;
            if ((*NoteObj)->TryGetArrayField(TEXT("instrument_shape_keys"),
                                             InstSKArr)) {
                for (const auto& SKVal : *InstSKArr) {
                    const TSharedPtr<FJsonObject>* SKObj = nullptr;
                    if (!SKVal->TryGetObject(SKObj)) continue;

                    int32 SKIndex = 0;
                    float SKValue = 0.0f;
                    (*SKObj)->TryGetNumberField(TEXT("shape_key_index"),
                                                SKIndex);
                    (*SKObj)->TryGetNumberField(TEXT("value"), SKValue);

                    if (InstrumentMorphTargets.IsValidIndex(SKIndex)) {
                        State.InstrumentMT.Add(FWindRiseMorphTargetValue(
                            InstrumentMorphTargets[SKIndex], SKValue));
                    }
                }
            }

            NoteStates.Add(NoteNum, State);
        }
    }

    Modify();

    UE_LOG(LogTemp, Log,
           TEXT("WindRiseUnreal: Imported .wind from %s (%d notes)"), *FilePath,
           NoteStates.Num());

    FNotificationInfo Info(FText::Format(
        LOCTEXT("ImportWindSuccess", "Imported .wind: {0} ({1} notes)"),
        FText::FromString(FPaths::GetCleanFilename(FilePath)),
        FText::AsNumber(NoteStates.Num())));
    Info.ExpireDuration = 5.0f;
    FSlateNotificationManager::Get().AddNotification(Info);
}

void AWindRiseUnreal::ExportWindFile(const FString& FilePath) {
    TSharedPtr<FJsonObject> RootObj = MakeShareable(new FJsonObject);

    // ── config ──
    TSharedPtr<FJsonObject> ConfigObj = MakeShareable(new FJsonObject);
    ConfigObj->SetStringField(TEXT("instrument_type"), InstrumentType);
    ConfigObj->SetNumberField(TEXT("min_note"), MinNote);
    ConfigObj->SetNumberField(TEXT("max_note"), MaxNote);
    ConfigObj->SetStringField(TEXT("description"), Description);
    ConfigObj->SetBoolField(TEXT("is_unreal"), true);

    TArray<TSharedPtr<FJsonValue>> FSKArr;
    for (const FString& Name : CharacterMorphTargets) {
        FSKArr.Add(MakeShareable(new FJsonValueString(Name)));
    }
    ConfigObj->SetArrayField(TEXT("force_shape_keys"), FSKArr);

    TArray<TSharedPtr<FJsonValue>> ISKArr;
    for (const FString& Name : InstrumentMorphTargets) {
        ISKArr.Add(MakeShareable(new FJsonValueString(Name)));
    }
    ConfigObj->SetArrayField(TEXT("instrument_shape_keys"), ISKArr);

    RootObj->SetObjectField(TEXT("config"), ConfigObj);

    // ── rest_offset（可选，非单位置时导出）──
    if (!RestOffset.Equals(FTransform::Identity)) {
        TSharedPtr<FJsonObject> RestOffsetObj = MakeShareable(new FJsonObject);
        RestOffsetObj->SetArrayField(
            TEXT("location"),
            FWindRiseHelpers::VecToJsonArray(RestOffset.GetLocation()));
        RestOffsetObj->SetArrayField(
            TEXT("rotation"),
            FWindRiseHelpers::QuatToJsonArray(RestOffset.GetRotation()));
        RootObj->SetObjectField(TEXT("rest_offset"), RestOffsetObj);
    }

    // ── note_info ──
    TArray<TSharedPtr<FJsonValue>> NoteInfoArr;
    for (const auto& NotePair : NoteStates) {
        const FWindRiseNoteState& State = NotePair.Value;
        TSharedPtr<FJsonObject> NoteObj = MakeShareable(new FJsonObject);
        NoteObj->SetNumberField(TEXT("note"), State.Note);
        NoteObj->SetStringField(TEXT("name"), State.Name);

        // controllers
        TSharedPtr<FJsonObject> ControllersObj = MakeShareable(new FJsonObject);
        for (const auto& CtrlPair : State.Controllers) {
            TSharedPtr<FJsonObject> CtrlObj =
                FWindRiseHelpers::TransformToControllerJson(CtrlPair.Key,
                                                            CtrlPair.Value);
            ControllersObj->SetObjectField(CtrlPair.Key, CtrlObj);
        }
        NoteObj->SetObjectField(TEXT("controllers"), ControllersObj);

        // character shape keys
        TArray<TSharedPtr<FJsonValue>> CharSKArr;
        for (const FWindRiseMorphTargetValue& MT : State.CharacterMT) {
            TSharedPtr<FJsonObject> SKObj = MakeShareable(new FJsonObject);
            int32 SKIndex =
                CharacterMorphTargets.IndexOfByKey(MT.MorphTargetName);
            SKObj->SetNumberField(TEXT("shape_key_index"), SKIndex);
            SKObj->SetNumberField(TEXT("value"), MT.Value);
            CharSKArr.Add(MakeShareable(new FJsonValueObject(SKObj)));
        }
        NoteObj->SetArrayField(TEXT("character_shape_keys"), CharSKArr);

        // instrument shape keys
        TArray<TSharedPtr<FJsonValue>> InstSKArr;
        for (const FWindRiseMorphTargetValue& MT : State.InstrumentMT) {
            TSharedPtr<FJsonObject> SKObj = MakeShareable(new FJsonObject);
            int32 SKIndex =
                InstrumentMorphTargets.IndexOfByKey(MT.MorphTargetName);
            SKObj->SetNumberField(TEXT("shape_key_index"), SKIndex);
            SKObj->SetNumberField(TEXT("value"), MT.Value);
            InstSKArr.Add(MakeShareable(new FJsonValueObject(SKObj)));
        }
        NoteObj->SetArrayField(TEXT("instrument_shape_keys"), InstSKArr);

        NoteInfoArr.Add(MakeShareable(new FJsonValueObject(NoteObj)));
    }
    RootObj->SetArrayField(TEXT("note_info"), NoteInfoArr);

    // ── 序列化写入 ──
    FString JsonContent;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&JsonContent);
    if (FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer)) {
        FString OutPath = FilePath;
        if (!OutPath.EndsWith(TEXT(".wind"))) {
            OutPath += TEXT(".wind");
        }
        if (FFileHelper::SaveStringToFile(
                JsonContent, *OutPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
            UE_LOG(LogTemp, Log,
                   TEXT("WindRiseUnreal: Exported .wind to %s (%d notes)"),
                   *OutPath, NoteStates.Num());

            FNotificationInfo Info(FText::Format(
                LOCTEXT("ExportWindSuccess", "Exported .wind: {0} ({1} notes)"),
                FText::FromString(FPaths::GetCleanFilename(OutPath)),
                FText::AsNumber(NoteStates.Num())));
            Info.ExpireDuration = 5.0f;
            FSlateNotificationManager::Get().AddNotification(Info);
        }
    }
}

// ============================================================
// 动画生成
// ============================================================

void AWindRiseUnreal::GenerateAnimationFromWindRise(
    const FString& WindRiseFilePath) {
    UWindRiseAnimationProcessor::GenerateAnimationFromWindRise(
        this, WindRiseFilePath);
}

// ============================================================
// Morph Target 辅助
// ============================================================

void AWindRiseUnreal::SetCharacterMTValue(int32 Index, float Value) {
    UWindRiseControlRigProcessor::SetCharacterMTValue(this, Index, Value);
}

void AWindRiseUnreal::SetInstrumentMTValue(int32 Index, float Value) {
    UWindRiseMusicInstrumentProcessor::SetInstrumentMTValue(this, Index, Value);
}

void AWindRiseUnreal::ResetAllCharacterMT() {
    UWindRiseControlRigProcessor::ResetAllCharacterMT(this);
}

void AWindRiseUnreal::ResetAllInstrumentMT() {
    UWindRiseMusicInstrumentProcessor::ResetAllInstrumentMT(this);
}

// ============================================================
// ControlRig 缓存
// ============================================================

UControlRig* AWindRiseUnreal::GetCachedControlRig(FName ComponentName) {
    if (!GEngine) return nullptr;

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) return nullptr;

    ASkeletalMeshActor* Actor = GetSkeletalMeshActorByName(ComponentName);
    if (!Actor) return nullptr;

    FString RootControlName;
    if (ComponentName == TEXT("Performer")) {
        RootControlName = TEXT("Breath_Control");
    } else if (ComponentName == TEXT("Instrument")) {
        RootControlName = TEXT("wind_root");
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return nullptr;

    UControlRig* ControlRig =
        CacheSubsystem->GetControlRig(Actor, LevelSequence, RootControlName);

    if (!ControlRig) {
        CacheSubsystem->TriggerRegistrationIfNeeded(Actor, LevelSequence,
                                                    RootControlName);
        ControlRig = CacheSubsystem->GetControlRig(Actor, LevelSequence,
                                                   RootControlName);
    }

    return ControlRig;
}

UControlRigBlueprint* AWindRiseUnreal::GetCachedControlRigBlueprint(
    FName ComponentName) {
    if (!GEngine) return nullptr;

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) return nullptr;

    ASkeletalMeshActor* Actor = GetSkeletalMeshActorByName(ComponentName);
    if (!Actor) return nullptr;

    FString RootControlName;
    if (ComponentName == TEXT("Performer")) {
        RootControlName = TEXT("Breath_Control");
    } else if (ComponentName == TEXT("Instrument")) {
        RootControlName = TEXT("wind_root");
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) return nullptr;

    return CacheSubsystem->GetControlRigBlueprint(Actor, LevelSequence,
                                                  RootControlName);
}

void AWindRiseUnreal::TriggerControlRigReregistration(
    const FString& ErrorMessage) {
    UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMessage);
    // TODO: Show a warning to the user in the UI
}

ASkeletalMeshActor* AWindRiseUnreal::GetSkeletalMeshActorByName(
    FName ComponentName) const {
    if (ComponentName == TEXT("Performer")) return SkeletalMeshActor;
    if (ComponentName == TEXT("Instrument")) return InstrumentMesh;
    return nullptr;
}

// ============================================================
// 静态辅助
// ============================================================

FString AWindRiseUnreal::NoteNumberToName(int32 NoteNumber) {
    if (NoteNumber < 0 || NoteNumber > 127) {
        return FString::Printf(TEXT("Unknown(%d)"), NoteNumber);
    }
    int32 Octave = (NoteNumber / 12) - 1;
    return FString::Printf(TEXT("%s%d"), *SEMITONE_NAMES[NoteNumber % 12],
                           Octave);
}

#undef LOCTEXT_NAMESPACE
