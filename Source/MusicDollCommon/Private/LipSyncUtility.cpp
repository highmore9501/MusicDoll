#include "LipSyncUtility.h"

#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "EdGraphSchema_K2.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#if WITH_EDITOR
#include "LevelSequenceEditorBlueprintLibrary.h"
#endif

// ===== 常量定义 =====

const FName ULipSyncUtility::LipSyncMappingVariableName =
    FName(TEXT("LipSyncMapping"));
const FString ULipSyncUtility::LipSyncControlName = TEXT("lip_sync");

// ===== 映射表管理 =====

bool ULipSyncUtility::AddLipSyncMappingVariable(
    UControlRigBlueprint* ControlRigBlueprint) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] AddLipSyncMappingVariable: "
                    "ControlRigBlueprint is null"));
        return false;
    }

    // 检查变量是否已存在
    int32 ExistingVarIndex = INDEX_NONE;
    for (int32 i = 0; i < ControlRigBlueprint->NewVariables.Num(); ++i) {
        if (ControlRigBlueprint->NewVariables[i].VarName ==
            LipSyncMappingVariableName) {
            ExistingVarIndex = i;
            break;
        }
    }

    // 如果变量已存在，删除旧变量以便重建
    if (ExistingVarIndex != INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("[LipSyncUtility] Variable '%s' already exists, removing "
                    "old variable to recreate"),
               *LipSyncMappingVariableName.ToString());
        ControlRigBlueprint->NewVariables.RemoveAt(ExistingVarIndex);
    }

    ControlRigBlueprint->SetFlags(RF_Transactional);

    // 创建新变量描述
    FBPVariableDescription NewVariable;
    NewVariable.VarName = LipSyncMappingVariableName;
    NewVariable.VarGuid = FGuid::NewGuid();
    NewVariable.RepNotifyFunc = NAME_None;
    NewVariable.Category = FText::FromString(TEXT("Lip Sync Mapping"));
    NewVariable.FriendlyName = FName::NameToDisplayString(
        LipSyncMappingVariableName.ToString(), false);

    // 设置变量类型为 TArray<FLipSyncMappingPair>
    NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    NewVariable.VarType.PinSubCategoryObject =
        FLipSyncMappingPair::StaticStruct();
    NewVariable.VarType.ContainerType = EPinContainerType::Array;

    // 设置属性标志
    NewVariable.PropertyFlags |=
        (CPF_Edit | CPF_BlueprintVisible | CPF_DisableEditOnInstance);
    NewVariable.VarType.bIsConst = false;
    NewVariable.VarType.bIsWeakPointer = false;
    NewVariable.VarType.bIsReference = false;

    // 添加变量到蓝图
    ControlRigBlueprint->NewVariables.Add(NewVariable);

    // 标记蓝图为结构化修改，触发重新编译
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
        ControlRigBlueprint);

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] Successfully added variable '%s' to "
                "ControlRigBlueprint"),
           *LipSyncMappingVariableName.ToString());

    return true;
}

bool ULipSyncUtility::GetLipSyncMapping(
    UControlRigBlueprint* ControlRigBlueprint,
    TArray<FLipSyncMappingPair>& OutMapping) {
    OutMapping.Reset();

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GetLipSyncMapping: ControlRigBlueprint "
                    "is null"));
        return false;
    }

    // 查找变量描述
    bool bVarExists = false;
    for (const FBPVariableDescription& Var :
         ControlRigBlueprint->NewVariables) {
        if (Var.VarName == LipSyncMappingVariableName) {
            bVarExists = true;
            break;
        }
    }

    if (!bVarExists) {
        UE_LOG(LogTemp, Warning,
               TEXT("[LipSyncUtility] GetLipSyncMapping: Variable '%s' not "
                    "found in NewVariables"),
               *LipSyncMappingVariableName.ToString());
        return false;
    }

    // 获取生成类
    UClass* GeneratedClass = ControlRigBlueprint->GeneratedClass;
    if (!GeneratedClass) {
        GeneratedClass = ControlRigBlueprint->SkeletonGeneratedClass;
    }

    if (!GeneratedClass) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GetLipSyncMapping: Failed to get "
                    "GeneratedClass or SkeletonGeneratedClass"));
        return false;
    }

    // 获取默认对象
    UObject* DefaultObject = GeneratedClass->GetDefaultObject();
    if (!DefaultObject) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GetLipSyncMapping: Failed to get "
                    "DefaultObject"));
        return false;
    }

    // 通过反射查找属性
    FProperty* Property =
        GeneratedClass->FindPropertyByName(LipSyncMappingVariableName);
    if (!Property) {
        // 变量存在但尚未编译成属性，返回 true 表示空映射
        UE_LOG(LogTemp, Warning,
               TEXT("[LipSyncUtility] GetLipSyncMapping: Property '%s' not "
                    "found - variable not yet compiled"),
               *LipSyncMappingVariableName.ToString());
        return true;
    }

    FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
    if (!ArrayProperty) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GetLipSyncMapping: Property '%s' is not "
                    "an array"),
               *LipSyncMappingVariableName.ToString());
        return false;
    }

    FStructProperty* StructProperty =
        CastField<FStructProperty>(ArrayProperty->Inner);
    if (!StructProperty ||
        StructProperty->Struct != FLipSyncMappingPair::StaticStruct()) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GetLipSyncMapping: Array does not "
                    "contain FLipSyncMappingPair"));
        return false;
    }

    // 读取数组
    FScriptArrayHelper ArrayHelper(
        ArrayProperty,
        ArrayProperty->ContainerPtrToValuePtr<void>(DefaultObject));

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] GetLipSyncMapping: Found %d items in "
                "LipSyncMapping"),
           ArrayHelper.Num());

    for (int32 i = 0; i < ArrayHelper.Num(); ++i) {
        FLipSyncMappingPair* PairPtr =
            reinterpret_cast<FLipSyncMappingPair*>(ArrayHelper.GetRawPtr(i));
        if (PairPtr) {
            OutMapping.Add(*PairPtr);
        }
    }

    return true;
}

bool ULipSyncUtility::SetLipSyncMapping(
    UControlRigBlueprint* ControlRigBlueprint,
    const TArray<FLipSyncMappingPair>& InMapping) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] SetLipSyncMapping: ControlRigBlueprint "
                    "is null"));
        return false;
    }

    // 查找变量描述
    FBPVariableDescription* TargetVar = nullptr;
    for (FBPVariableDescription& Var : ControlRigBlueprint->NewVariables) {
        if (Var.VarName == LipSyncMappingVariableName) {
            TargetVar = &Var;
            break;
        }
    }

    if (!TargetVar) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] SetLipSyncMapping: Variable '%s' not "
                    "found. Call AddLipSyncMappingVariable first."),
               *LipSyncMappingVariableName.ToString());
        return false;
    }

    // 获取生成类
    UClass* GeneratedClass = ControlRigBlueprint->GeneratedClass;
    if (!GeneratedClass) {
        GeneratedClass = ControlRigBlueprint->SkeletonGeneratedClass;
    }

    if (!GeneratedClass) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] SetLipSyncMapping: Failed to get "
                    "GeneratedClass"));
        return false;
    }

    UObject* DefaultObject = GeneratedClass->GetDefaultObject();
    if (!DefaultObject) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] SetLipSyncMapping: Failed to get "
                    "DefaultObject"));
        return false;
    }

    FProperty* Property =
        GeneratedClass->FindPropertyByName(LipSyncMappingVariableName);
    if (!Property) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] SetLipSyncMapping: Property '%s' not "
                    "found - Blueprint needs compilation"),
               *LipSyncMappingVariableName.ToString());
        return false;
    }

    FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
    if (!ArrayProperty) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] SetLipSyncMapping: Property is not an "
                    "array"));
        return false;
    }

    FStructProperty* StructProperty =
        CastField<FStructProperty>(ArrayProperty->Inner);
    if (!StructProperty ||
        StructProperty->Struct != FLipSyncMappingPair::StaticStruct()) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] SetLipSyncMapping: Array does not "
                    "contain FLipSyncMappingPair"));
        return false;
    }

    DefaultObject->Modify();

    FScriptArrayHelper ArrayHelper(
        ArrayProperty,
        ArrayProperty->ContainerPtrToValuePtr<void>(DefaultObject));

    // 清空现有数组
    ArrayHelper.EmptyValues();

    // 写入新的映射
    for (const FLipSyncMappingPair& Pair : InMapping) {
        int32 NewIndex = ArrayHelper.AddValue();
        FLipSyncMappingPair* NewPair = reinterpret_cast<FLipSyncMappingPair*>(
            ArrayHelper.GetRawPtr(NewIndex));
        if (NewPair) {
            *NewPair = Pair;
        }
    }

    ControlRigBlueprint->MarkPackageDirty();

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] SetLipSyncMapping: Successfully saved %d "
                "mappings"),
           InMapping.Num());

    return true;
}

// ===== Control Rig 操作 =====

int32 ULipSyncUtility::SetupLipSyncControl(
    UControlRigBlueprint* ControlRigBlueprint,
    const TArray<FLipSyncMappingPair>& Mapping) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] SetupLipSyncControl: ControlRigBlueprint "
                    "is null"));
        return 0;
    }

    // Step 1: 确保 lip_sync Control 存在于根层级
    UE_LOG(
        LogTemp, Log,
        TEXT("[LipSyncUtility] SetupLipSyncControl: Creating '%s' control..."),
        *LipSyncControlName);

    // 检查是否已存在
    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (RigHierarchy) {
        FRigElementKey ExistingKey(*LipSyncControlName,
                                   ERigElementType::Control);
        if (RigHierarchy->Contains(ExistingKey)) {
            UE_LOG(LogTemp, Log,
                   TEXT("[LipSyncUtility] Control '%s' already exists"),
                   *LipSyncControlName);
        } else {
            if (!FControlRigCreationUtility::CreateControl(
                    ControlRigBlueprint, LipSyncControlName, TEXT(""))) {
                UE_LOG(LogTemp, Error,
                       TEXT("[LipSyncUtility] Failed to create '%s' control"),
                       *LipSyncControlName);
                return 0;
            }
            UE_LOG(LogTemp, Log,
                   TEXT("[LipSyncUtility] Created '%s' control successfully"),
                   *LipSyncControlName);
        }
    }

    // Step 2: 收集需要创建 Channel 的 Morph Target 名称（去重）
    TSet<FString> UniqueMorphTargets;
    for (const FLipSyncMappingPair& Pair : Mapping) {
        if (!Pair.MorphTargetName.IsEmpty()) {
            UniqueMorphTargets.Add(Pair.MorphTargetName);
        }
    }

    if (UniqueMorphTargets.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[LipSyncUtility] SetupLipSyncControl: No valid morph "
                    "targets in mapping"));
        return 0;
    }

    TArray<FString> ChannelNames = UniqueMorphTargets.Array();

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] SetupLipSyncControl: Creating %d animation "
                "channels under '%s'"),
           ChannelNames.Num(), *LipSyncControlName);

    // Step 3: 批量创建 Float Animation Channel
    FRigElementKey ParentKey(*LipSyncControlName, ERigElementType::Control);
    int32 SuccessCount = UInstrumentMorphTargetUtility::AddAnimationChannels(
        ControlRigBlueprint, ParentKey, ChannelNames, ERigControlType::Float);

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] SetupLipSyncControl: Created %d channels"),
           SuccessCount);

    return SuccessCount;
}

// ===== JSON 解析 =====

bool ULipSyncUtility::ParseLipSyncJson(const FString& FilePath,
                                       TArray<FLipSyncMouthCue>& OutCues,
                                       float& OutDuration) {
    OutCues.Reset();
    OutDuration = 0.0f;

    // 读取文件
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath)) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[LipSyncUtility] ParseLipSyncJson: Failed to load file: %s"),
            *FilePath);
        return false;
    }

    // 解析 JSON
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) ||
        !JsonObject.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ParseLipSyncJson: Failed to parse JSON"));
        return false;
    }

    // 读取 metadata.duration
    const TSharedPtr<FJsonObject>* MetadataObj = nullptr;
    if (JsonObject->TryGetObjectField(TEXT("metadata"), MetadataObj) &&
        MetadataObj->IsValid()) {
        (*MetadataObj)->TryGetNumberField(TEXT("duration"), OutDuration);
    }

    // 读取 mouthCues 数组
    const TArray<TSharedPtr<FJsonValue>>* CuesArray = nullptr;
    if (!JsonObject->TryGetArrayField(TEXT("mouthCues"), CuesArray) ||
        !CuesArray) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ParseLipSyncJson: 'mouthCues' array not "
                    "found"));
        return false;
    }

    for (const TSharedPtr<FJsonValue>& CueValue : *CuesArray) {
        const TSharedPtr<FJsonObject>* CueObj = nullptr;
        if (!CueValue->TryGetObject(CueObj) || !CueObj->IsValid()) {
            continue;
        }

        FLipSyncMouthCue Cue;
        (*CueObj)->TryGetNumberField(TEXT("start"), Cue.Start);
        (*CueObj)->TryGetNumberField(TEXT("end"), Cue.End);
        (*CueObj)->TryGetStringField(TEXT("value"), Cue.Value);

        OutCues.Add(Cue);
    }

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] ParseLipSyncJson: Parsed %d cues, "
                "duration=%.2fs"),
           OutCues.Num(), OutDuration);

    return OutCues.Num() > 0;
}

// ===== 关键帧转换 =====

float ULipSyncUtility::ComputeChangeDuration(float CueDuration) {
    if (CueDuration >= 1.0f) {
        return 0.5f;
    }
    return CueDuration / 3.0f;
}

FFrameNumber ULipSyncUtility::SecondsToFrameNumber(float Seconds,
                                                   FFrameRate TickResolution,
                                                   FFrameRate DisplayRate) {
    // 使用 UE 标准的时间到帧转换
    const FFrameTime FrameTime = Seconds * TickResolution;
    return FrameTime.GetFrame();
}

bool ULipSyncUtility::ConvertCuesToKeyframeData(
    const TArray<FLipSyncMouthCue>& Cues,
    const TArray<FLipSyncMappingPair>& Mapping, FFrameRate TickResolution,
    FFrameRate DisplayRate, TArray<FMorphTargetKeyframeData>& OutKeyframeData) {
    OutKeyframeData.Empty();

    if (Cues.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[LipSyncUtility] ConvertCuesToKeyframeData: Cues array is "
                    "empty"));
        return false;
    }

    if (Mapping.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[LipSyncUtility] ConvertCuesToKeyframeData: Mapping array "
                    "is empty"));
        return false;
    }

    // 构建 Phoneme → MorphTargetName 的快速查找表
    TMap<FString, FString> PhonemeToMorphTarget;
    for (const FLipSyncMappingPair& Pair : Mapping) {
        if (!Pair.Phoneme.IsEmpty() && !Pair.MorphTargetName.IsEmpty()) {
            PhonemeToMorphTarget.Add(Pair.Phoneme.ToUpper(),
                                     Pair.MorphTargetName);
        }
    }

    // 按 MorphTargetName 分组收集关键帧
    // Key: MorphTargetName, Value: 累计的 FrameNumbers 和 Values
    TMap<FString, TArray<FFrameNumber>> MorphTargetFrames;
    TMap<FString, TArray<float>> MorphTargetValues;

    for (int32 i = 0; i < Cues.Num(); ++i) {
        const FLipSyncMouthCue& Cue = Cues[i];

        // X 不执行任何操作，直接跳过
        if (Cue.Value.ToUpper() == TEXT("X")) {
            continue;
        }

        // 查找映射
        const FString* MorphTargetNamePtr =
            PhonemeToMorphTarget.Find(Cue.Value.ToUpper());
        if (!MorphTargetNamePtr) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[LipSyncUtility] ConvertCuesToKeyframeData: No "
                        "mapping for phoneme '%s'"),
                   *Cue.Value);
            continue;
        }

        const float CueDuration = Cue.GetDuration();
        const float ChangeDuration = ComputeChangeDuration(CueDuration);

        // 查找下一个非 X cue 来计算 next_change_duration
        float NextChangeDuration =
            ChangeDuration;  // 默认使用当前 cue 的 change_duration
        for (int32 j = i + 1; j < Cues.Num(); ++j) {
            if (Cues[j].Value.ToUpper() != TEXT("X")) {
                NextChangeDuration =
                    ComputeChangeDuration(Cues[j].GetDuration());
                break;
            }
        }

        // 计算四帧时间和值
        const float T0 =
            FMath::Max(0.0f, Cue.Start - ChangeDuration);  // 淡入开始
        const float T1 = Cue.Start;                        // 口型激活
        const float T2 = Cue.End;                          // 口型保持结束
        const float T3 = Cue.End + NextChangeDuration;     // 淡出结束

        TArray<FFrameNumber>& Frames =
            MorphTargetFrames.FindOrAdd(*MorphTargetNamePtr);
        TArray<float>& Values =
            MorphTargetValues.FindOrAdd(*MorphTargetNamePtr);

        // 四帧：0.0 → 1.0 → 1.0 → 0.0
        Frames.Add(SecondsToFrameNumber(T0, TickResolution, DisplayRate));
        Values.Add(0.0f);

        Frames.Add(SecondsToFrameNumber(T1, TickResolution, DisplayRate));
        Values.Add(1.0f);

        Frames.Add(SecondsToFrameNumber(T2, TickResolution, DisplayRate));
        Values.Add(1.0f);

        Frames.Add(SecondsToFrameNumber(T3, TickResolution, DisplayRate));
        Values.Add(0.0f);
    }

    // 转换为输出格式
    for (const auto& Pair : MorphTargetFrames) {
        const FString& MorphTargetName = Pair.Key;
        const TArray<FFrameNumber>& Frames = Pair.Value;
        const TArray<float>* ValuesPtr =
            MorphTargetValues.Find(MorphTargetName);

        if (!ValuesPtr || Frames.Num() == 0) {
            continue;
        }

        FMorphTargetKeyframeData Data(MorphTargetName);
        Data.FrameNumbers = Frames;
        Data.Values = *ValuesPtr;
        OutKeyframeData.Add(Data);
    }

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] ConvertCuesToKeyframeData: Converted %d cues "
                "→ %d morph targets"),
           Cues.Num(), OutKeyframeData.Num());

    return OutKeyframeData.Num() > 0;
}

// ===== 写入 Sequencer（定向清理，不破坏其他轨道）=====

int32 ULipSyncUtility::WriteLipSyncToControlRig(
    ASkeletalMeshActor* Performer,
    const TArray<FMorphTargetKeyframeData>& KeyframeData,
    ULevelSequence* LevelSequence, const FString& RootControlName,
    int32 FramePadding) {
    if (!Performer) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: Performer is "
                    "null"));
        return 0;
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: LevelSequence "
                    "is null"));
        return 0;
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: KeyframeData "
                    "is empty"));
        return 0;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: MovieScene is "
                    "null"));
        return 0;
    }

    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: GEngine not "
                    "available"));
        return 0;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: CacheSubsystem "
                    "not available"));
        return 0;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(Performer, LevelSequence);
    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: Failed to get "
                    "ControlRigInstance"));
        return 0;
    }

    // Step 1: 查找或创建 ControlRigParameterTrack
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: Failed to find "
                    "ControlRig track"));
        return 0;
    }

    // Step 2: 查找现有 Section（不删除！直接用第一个可用的 Section）
    UMovieSceneControlRigParameterSection* Section = nullptr;
    TArray<UMovieSceneSection*> AllSections = ControlRigTrack->GetAllSections();
    for (UMovieSceneSection* S : AllSections) {
        UMovieSceneControlRigParameterSection* CRSection =
            Cast<UMovieSceneControlRigParameterSection>(S);
        if (CRSection) {
            Section = CRSection;
            break;
        }
    }

    // 如果没有现有 Section，创建一个新的
    if (!Section) {
        Section = Cast<UMovieSceneControlRigParameterSection>(
            ControlRigTrack->CreateNewSection());
        if (!Section) {
            UE_LOG(LogTemp, Error,
                   TEXT("[LipSyncUtility] WriteLipSyncToControlRig: Failed to "
                        "create new section"));
            return 0;
        }
        ControlRigTrack->AddSection(*Section);
        UE_LOG(LogTemp, Log,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: Created new "
                    "section"));
    }

    // Step 3: 只清理目标 morph target 对应的 Float Channel
    // 对每个 morph target，用 bReconstructChannel=true
    // 重新创建通道，清空旧关键帧
    TSet<FString> TargetMorphTargets;
    for (const FMorphTargetKeyframeData& Data : KeyframeData) {
        TargetMorphTargets.Add(Data.MorphTargetName);
    }

    int32 ClearedCount = 0;
    for (const FString& MorphTargetName : TargetMorphTargets) {
        FName ChannelFName(*MorphTargetName);
        if (Section->HasScalarParameter(ChannelFName)) {
            // bReconstructChannel=true 会删除旧通道并重建，从而清空所有关键帧
            Section->AddScalarParameter(ChannelFName, TOptional<float>(0.0f),
                                        true);
            ClearedCount++;
        } else {
            Section->AddScalarParameter(ChannelFName, TOptional<float>(0.0f),
                                        false);
        }
    }

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] WriteLipSyncToControlRig: Cleared/recreated "
                "%d channels"),
           ClearedCount);

    // Step 4: 计算帧数范围
    FFrameNumber MinFrame(MAX_int32);
    FFrameNumber MaxFrame(MIN_int32);
    bool bHasFrames = false;

    for (const FMorphTargetKeyframeData& Data : KeyframeData) {
        for (const FFrameNumber& FrameNumber : Data.FrameNumbers) {
            if (FrameNumber < MinFrame) MinFrame = FrameNumber;
            if (FrameNumber > MaxFrame) MaxFrame = FrameNumber;
            bHasFrames = true;
        }
    }

    // Step 5: 写入关键帧（复用已有方法）
    int32 WrittenTargets =
        UInstrumentMorphTargetUtility::WriteMorphTargetKeyframes(Section,
                                                                 KeyframeData);

    // Step 6: 更新 Section Range
    if (bHasFrames) {
        FFrameRate TickResolution = MovieScene->GetTickResolution();
        FFrameRate DisplayRate = MovieScene->GetDisplayRate();
        int32 PaddingInInternalFrames =
            FramePadding * TickResolution.Numerator * DisplayRate.Denominator /
            (TickResolution.Denominator * DisplayRate.Numerator);

        Section->SetRange(
            TRange<FFrameNumber>(MinFrame, MaxFrame + PaddingInInternalFrames));
        UE_LOG(LogTemp, Log,
               TEXT("[LipSyncUtility] Set section range to [%d, %d)"),
               MinFrame.Value, (MaxFrame + PaddingInInternalFrames).Value);
    }

    // Step 7: 标记修改并刷新
    Section->Modify();
    ControlRigTrack->Modify();
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();

    {
        TSharedPtr<ISequencer> ActiveSequencer = nullptr;
        ULevelSequence* ActiveLevelSequence = nullptr;
        if (UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
                ActiveLevelSequence, ActiveSequencer)) {
            if (ActiveSequencer.IsValid() &&
                ActiveLevelSequence == LevelSequence) {
                ActiveSequencer->NotifyMovieSceneDataChanged(
                    EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
                ActiveSequencer->ForceEvaluate();
            }
        }
    }

    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] WriteLipSyncToControlRig: Successfully wrote "
                "%d morph targets"),
           WrittenTargets);

    return WrittenTargets;
}

// ===== 完整写入流程 =====

int32 ULipSyncUtility::GenerateLipSyncFromJson(
    ASkeletalMeshActor* Performer, UControlRigBlueprint* ControlRigBlueprint,
    const FString& JsonFilePath) {
    if (!Performer) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "[LipSyncUtility] GenerateLipSyncFromJson: Performer is null"));
        return 0;
    }

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GenerateLipSyncFromJson: "
                    "ControlRigBlueprint is null"));
        return 0;
    }

    // Step 1: 解析 JSON 文件
    TArray<FLipSyncMouthCue> Cues;
    float Duration = 0.0f;
    if (!ParseLipSyncJson(JsonFilePath, Cues, Duration)) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GenerateLipSyncFromJson: Failed to parse "
                    "JSON"));
        return 0;
    }

    // Step 2: 读取映射表
    TArray<FLipSyncMappingPair> Mapping;
    if (!GetLipSyncMapping(ControlRigBlueprint, Mapping)) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GenerateLipSyncFromJson: Failed to get "
                    "mapping"));
        return 0;
    }

    // Step 3: 获取 LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GenerateLipSyncFromJson: No active "
                    "LevelSequence"));
        return 0;
    }

    // Step 4: 获取 TickResolution 和 DisplayRate
    const FFrameRate TickResolution =
        LevelSequence->GetMovieScene()->GetTickResolution();
    const FFrameRate DisplayRate =
        LevelSequence->GetMovieScene()->GetDisplayRate();

    // Step 5: 转换 Cues → KeyframeData
    TArray<FMorphTargetKeyframeData> KeyframeData;
    if (!ConvertCuesToKeyframeData(Cues, Mapping, TickResolution, DisplayRate,
                                   KeyframeData)) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GenerateLipSyncFromJson: Failed to "
                    "convert cues to keyframe data"));
        return 0;
    }

    // Step 6: 调用定向写入（只清理 lip_sync 下的通道，不破坏其他轨道）
    const int32 WrittenCount = WriteLipSyncToControlRig(
        Performer, KeyframeData, LevelSequence, LipSyncControlName);

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] GenerateLipSyncFromJson: Complete! Written "
                "%d morph targets"),
           WrittenCount);

    return WrittenCount;
}
