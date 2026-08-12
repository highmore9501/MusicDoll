#include "LipSyncUtility.h"

#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
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

    // 清理 lip_sync control 下多余的 float channel（不在 mapping 中的）
    {
        URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
        if (RigHierarchy) {
            FRigElementKey LipSyncKey(*LipSyncControlName,
                                      ERigElementType::Control);
            if (RigHierarchy->Contains(LipSyncKey)) {
                URigHierarchyController* HierarchyController =
                    RigHierarchy->GetController();
                if (HierarchyController) {
                    // 收集 mapping 中的有效 morph target 名称
                    TSet<FString> ValidMorphTargetNames;
                    for (const FLipSyncMappingPair& Pair : InMapping) {
                        if (!Pair.MorphTargetName.IsEmpty()) {
                            ValidMorphTargetNames.Add(Pair.MorphTargetName);
                        }
                    }

                    if (ValidMorphTargetNames.Num() > 0) {
                        // 遍历所有 control，找出 lip_sync 下不在 mapping 中的
                        // float animation channel 并删除
                        TArray<FRigElementKey> AllKeys =
                            RigHierarchy->GetAllKeys(false);
                        int32 RemovedCount = 0;

                        for (const FRigElementKey& Key : AllKeys) {
                            if (Key.Type != ERigElementType::Control) continue;

                            FRigElementKey ParentKey =
                                RigHierarchy->GetFirstParent(Key);
                            if (ParentKey != LipSyncKey) continue;

                            const FRigControlElement* ControlElement =
                                RigHierarchy->Find<FRigControlElement>(Key);
                            if (!ControlElement ||
                                !ControlElement->IsAnimationChannel())
                                continue;
                            if (ControlElement->Settings.ControlType !=
                                ERigControlType::Float)
                                continue;

                            const FString ChannelName = Key.Name.ToString();
                            if (!ValidMorphTargetNames.Contains(ChannelName)) {
                                if (HierarchyController->RemoveElement(
                                        Key, true, false)) {
                                    RemovedCount++;
                                    UE_LOG(LogTemp, Log,
                                           TEXT("[LipSyncUtility] "
                                                "SetLipSyncMapping: Removed "
                                                "orphaned channel '%s'"),
                                           *ChannelName);
                                }
                            }
                        }

                        if (RemovedCount > 0) {
                            UE_LOG(LogTemp, Log,
                                   TEXT("[LipSyncUtility] "
                                        "SetLipSyncMapping: Cleaned up %d "
                                        "orphaned channels"),
                                   RemovedCount);
                            FBlueprintEditorUtils::
                                MarkBlueprintAsStructurallyModified(
                                    ControlRigBlueprint);
                        }
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] SetLipSyncMapping: Successfully saved %d "
                "mappings"),
           InMapping.Num());

    return true;
}

// ===== Control Rig 操作 =====

bool ULipSyncUtility::InitializeLipSyncControl(
    UControlRigBlueprint* ControlRigBlueprint) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] InitializeLipSyncControl: "
                    "ControlRigBlueprint is null"));
        return false;
    }

    // --- 第一层：检查 LipSyncMapping 变量是否存在，不存在则创建 ---
    {
        bool bVarExists = false;
        for (const FBPVariableDescription& Var :
             ControlRigBlueprint->NewVariables) {
            if (Var.VarName == LipSyncMappingVariableName) {
                bVarExists = true;
                break;
            }
        }

        if (!bVarExists) {
            if (!AddLipSyncMappingVariable(ControlRigBlueprint)) {
                UE_LOG(LogTemp, Error,
                       TEXT("[LipSyncUtility] "
                            "InitializeLipSyncControl: Failed to add "
                            "LipSyncMapping variable"));
                return false;
            }

            UE_LOG(LogTemp, Log,
                   TEXT("[LipSyncUtility] InitializeLipSyncControl: "
                        "Created LipSyncMapping variable"));
        } else {
            UE_LOG(LogTemp, Log,
                   TEXT("[LipSyncUtility] InitializeLipSyncControl: "
                        "LipSyncMapping variable already exists"));
        }
    }

    // --- 第二层：检查 lip_sync Control 是否存在 ---
    {
        URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
        if (!RigHierarchy) {
            UE_LOG(LogTemp, Error,
                   TEXT("[LipSyncUtility] InitializeLipSyncControl: Failed "
                        "to get RigHierarchy"));
            return false;
        }

        FRigElementKey ExistingKey(*LipSyncControlName,
                                   ERigElementType::Control);
        if (!RigHierarchy->Contains(ExistingKey)) {
            // Control 不存在 → 创建 Control
            if (!FControlRigCreationUtility::CreateControl(
                    ControlRigBlueprint, LipSyncControlName, TEXT(""))) {
                UE_LOG(LogTemp, Error,
                       TEXT("[LipSyncUtility] "
                            "InitializeLipSyncControl: Failed to create "
                            "'%s' control"),
                       *LipSyncControlName);
                return false;
            }

            UE_LOG(LogTemp, Log,
                   TEXT("[LipSyncUtility] "
                        "InitializeLipSyncControl: Created '%s' control"),
                   *LipSyncControlName);
            return true;
        }
    }

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] InitializeLipSyncControl: Control '%s' "
                "already exists"),
           *LipSyncControlName);

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] InitializeLipSyncControl: Already "
                "initialized (both variable and control exist). "
                "Use ApplyMappingToRig to create channels."));

    return true;
}

// ===== Mapping应用到Rig =====

int32 ULipSyncUtility::ApplyMappingToRig(
    UControlRigBlueprint* ControlRigBlueprint) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ApplyMappingToRig: "
                    "ControlRigBlueprint is null"));
        return 0;
    }

    // 读取 mapping 变量
    TArray<FLipSyncMappingPair> Mapping;
    if (!GetLipSyncMapping(ControlRigBlueprint, Mapping)) {
        UE_LOG(LogTemp, Warning,
               TEXT("[LipSyncUtility] ApplyMappingToRig: Failed to read "
                    "mapping (Blueprint may need compilation)"));
        return 0;
    }

    // 收集需要创建 Channel 的 Morph Target 名称（去重）
    TSet<FString> UniqueMorphTargets;
    for (const FLipSyncMappingPair& Pair : Mapping) {
        if (!Pair.MorphTargetName.IsEmpty()) {
            UniqueMorphTargets.Add(Pair.MorphTargetName);
        }
    }

    if (UniqueMorphTargets.Num() == 0) {
        UE_LOG(LogTemp, Log,
               TEXT("[LipSyncUtility] ApplyMappingToRig: Mapping is empty, "
                    "no channels to create"));
        return 0;
    }

    TArray<FString> ChannelNames = UniqueMorphTargets.Array();

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] ApplyMappingToRig: Creating %d animation "
                "channels under '%s'"),
           ChannelNames.Num(), *LipSyncControlName);

    FRigElementKey ParentKey(*LipSyncControlName, ERigElementType::Control);
    int32 CreatedCount = UInstrumentMorphTargetUtility::AddAnimationChannels(
        ControlRigBlueprint, ParentKey, ChannelNames, ERigControlType::Float);

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] ApplyMappingToRig: Created %d channels"),
           CreatedCount);

    return CreatedCount;
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

// ===== TSV 解析（Cherry 格式）=====

bool ULipSyncUtility::ParseLipSyncTsv(const FString& FilePath,
                                      TArray<FLipSyncMouthCue>& OutCues,
                                      float& OutDuration) {
    OutCues.Reset();
    OutDuration = 0.0f;

    // 读取文件
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath)) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[LipSyncUtility] ParseLipSyncTsv: Failed to load file: %s"),
            *FilePath);
        return false;
    }

    // 按行分割
    TArray<FString> Lines;
    FileContent.ParseIntoArrayLines(Lines);

    if (Lines.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ParseLipSyncTsv: File is empty"));
        return false;
    }

    // 解析每一行：<时间>\t<口型字母>
    struct FTimestampedPhoneme {
        float Time;
        FString Phoneme;
    };
    TArray<FTimestampedPhoneme> Entries;

    for (const FString& Line : Lines) {
        FString TrimmedLine = Line.TrimStartAndEnd();
        if (TrimmedLine.IsEmpty()) {
            continue;
        }

        TArray<FString> Columns;
        TrimmedLine.ParseIntoArray(Columns, TEXT("\t"), false);

        if (Columns.Num() < 2) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[LipSyncUtility] ParseLipSyncTsv: Skipping malformed "
                        "line: %s"),
                   *TrimmedLine);
            continue;
        }

        float Time = FCString::Atof(*Columns[0]);
        FString Phoneme = Columns[1].TrimStartAndEnd().ToUpper();

        FTimestampedPhoneme Entry;
        Entry.Time = Time;
        Entry.Phoneme = Phoneme;
        Entries.Add(Entry);
    }

    if (Entries.Num() == 0) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[LipSyncUtility] ParseLipSyncTsv: No valid entries found"));
        return false;
    }

    // 转换为 FLipSyncMouthCue：每行的口型持续到下一行的开始时间
    // 最后一行使用默认的短暂持续时间
    const float DefaultLastDuration = 0.1f;

    for (int32 i = 0; i < Entries.Num(); ++i) {
        FLipSyncMouthCue Cue;
        Cue.Start = Entries[i].Time;
        Cue.Value = Entries[i].Phoneme;

        if (i + 1 < Entries.Num()) {
            Cue.End = Entries[i + 1].Time;
        } else {
            Cue.End = Entries[i].Time + DefaultLastDuration;
        }

        OutCues.Add(Cue);
    }

    // 计算总时长：最后一行的时间 + 默认持续时长
    OutDuration = Entries.Last().Time + DefaultLastDuration;

    UE_LOG(LogTemp, Log,
           TEXT("[LipSyncUtility] ParseLipSyncTsv: Parsed %d cues from %d "
                "entries, duration=%.2fs"),
           OutCues.Num(), Entries.Num(), OutDuration);

    return OutCues.Num() > 0;
}

// ===== 自动检测文件类型 =====

bool ULipSyncUtility::ParseLipSyncFile(const FString& FilePath,
                                       TArray<FLipSyncMouthCue>& OutCues,
                                       float& OutDuration) {
    OutCues.Reset();
    OutDuration = 0.0f;

    // 根据文件扩展名自动选择解析器
    FString Extension = FPaths::GetExtension(FilePath).ToLower();

    if (Extension == TEXT("tsv")) {
        UE_LOG(LogTemp, Log,
               TEXT("[LipSyncUtility] ParseLipSyncFile: Detected TSV format"));
        return ParseLipSyncTsv(FilePath, OutCues, OutDuration);
    } else if (Extension == TEXT("json")) {
        UE_LOG(LogTemp, Log,
               TEXT("[LipSyncUtility] ParseLipSyncFile: Detected JSON format"));
        return ParseLipSyncJson(FilePath, OutCues, OutDuration);
    } else {
        // 未知扩展名，先尝试 JSON 再尝试 TSV
        UE_LOG(LogTemp, Log,
               TEXT("[LipSyncUtility] ParseLipSyncFile: Unknown extension "
                    "'%s', trying JSON first"),
               *Extension);

        if (ParseLipSyncJson(FilePath, OutCues, OutDuration)) {
            return true;
        }

        UE_LOG(
            LogTemp, Log,
            TEXT("[LipSyncUtility] ParseLipSyncFile: JSON failed, trying TSV"));

        return ParseLipSyncTsv(FilePath, OutCues, OutDuration);
    }
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
                    "is empty, no keyframes will be generated"));
        return false;
    }

    // 构建 Phoneme → MorphTargetName 的快速查找表
    // 只包含有效映射条目，缺失的口型将被跳过（部分映射容错）
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
        const float CurrentChangeDur =
            ComputeChangeDuration(CueDuration);  // current_change_dur

        // pre_change_dur 源于前一个口型的持续时间
        float PreChangeDur = CurrentChangeDur;
        if (i > 0) {
            PreChangeDur = ComputeChangeDuration(Cues[i - 1].GetDuration());
        }

        // 计算四帧时间和值
        // t1 = max(0, start - pre_change_dur)    —
        // 淡入开始（速度取决于前一口型） t2 = start — 口型激活 t3 = end -
        // current_change_dur          — 淡出开始（速度取决于当前口型） t4 = end
        // — 口型结束 注意：X 虽然不生成关键帧，但 change_duration
        // 计算将其视为普通口型参与
        const float T1 =
            FMath::Max(0.0f, Cue.Start - PreChangeDur);  // 淡入开始
        const float T2 = Cue.Start;                      // 口型激活
        const float T3 = Cue.End - CurrentChangeDur;     // 淡出开始
        const float T4 = Cue.End;                        // 口型结束

        TArray<FFrameNumber>& Frames =
            MorphTargetFrames.FindOrAdd(*MorphTargetNamePtr);
        TArray<float>& Values =
            MorphTargetValues.FindOrAdd(*MorphTargetNamePtr);

        // 四帧：0.0 → 1.0 → 1.0 → 0.0
        Frames.Add(SecondsToFrameNumber(T1, TickResolution, DisplayRate));
        Values.Add(0.0f);

        Frames.Add(SecondsToFrameNumber(T2, TickResolution, DisplayRate));
        Values.Add(1.0f);

        Frames.Add(SecondsToFrameNumber(T3, TickResolution, DisplayRate));
        Values.Add(1.0f);

        Frames.Add(SecondsToFrameNumber(T4, TickResolution, DisplayRate));
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

    // Step 3: 清理 lip_sync control 下所有 float channel 的关键帧数据
    // 从 ControlRigInstance 的 Hierarchy 中获取 lip_sync 的所有子级 float
    // animation channel，对每个 channel 用 bReconstructChannel=true 重建，
    // 从而清空旧关键帧，确保不会被之前写入的数据干扰
    {
        URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
        int32 ClearedCount = 0;

        if (RigHierarchy) {
            FRigElementKey LipSyncKey(*RootControlName,
                                      ERigElementType::Control);
            TArray<FRigElementKey> AllKeys = RigHierarchy->GetAllKeys(false);
            for (const FRigElementKey& Key : AllKeys) {
                if (Key.Type != ERigElementType::Control) continue;

                FRigElementKey ParentKey = RigHierarchy->GetFirstParent(Key);
                if (ParentKey != LipSyncKey) continue;

                const FRigControlElement* ControlElement =
                    RigHierarchy->Find<FRigControlElement>(Key);
                if (!ControlElement || !ControlElement->IsAnimationChannel())
                    continue;
                if (ControlElement->Settings.ControlType !=
                    ERigControlType::Float)
                    continue;

                FName ChannelFName = Key.Name;
                // bReconstructChannel=true 删除旧通道并重建，清空所有关键帧
                Section->AddScalarParameter(ChannelFName,
                                            TOptional<float>(0.0f), true);
                ClearedCount++;
            }
        }

        UE_LOG(LogTemp, Log,
               TEXT("[LipSyncUtility] WriteLipSyncToControlRig: Cleared %d "
                    "float channels under '%s'"),
               ClearedCount, *RootControlName);
    }

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

    // Step 6: 扩展 Section Range（而非覆盖），始终从零帧开始
    if (bHasFrames) {
        FFrameRate TickResolution = MovieScene->GetTickResolution();
        FFrameRate DisplayRate = MovieScene->GetDisplayRate();
        int32 PaddingInInternalFrames =
            FramePadding * TickResolution.Numerator * DisplayRate.Denominator /
            (TickResolution.Denominator * DisplayRate.Numerator);

        // 扩展已有范围（而非覆盖），始终从零帧开始
        FFrameNumber NewEnd = MaxFrame + PaddingInInternalFrames;
        if (!Section->GetRange().IsEmpty() &&
            Section->GetRange().HasUpperBound()) {
            NewEnd =
                FMath::Max(Section->GetRange().GetUpperBoundValue(), NewEnd);
        }
        Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0), NewEnd));
        UE_LOG(LogTemp, Log,
               TEXT("[LipSyncUtility] Expanded section range to [0, %d)"),
               NewEnd.Value);
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

// ===== 关键帧清理 =====

bool ULipSyncUtility::ClearLipSyncKeyframes(ASkeletalMeshActor* Performer,
                                            const FString& RootControlName) {
    if (!Performer) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ClearLipSyncKeyframes: Performer is "
                    "null"));
        return false;
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ClearLipSyncKeyframes: No active "
                    "LevelSequence"));
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ClearLipSyncKeyframes: MovieScene is "
                    "null"));
        return false;
    }

    // 获取 ControlRig 实例
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ClearLipSyncKeyframes: GEngine not "
                    "available"));
        return false;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ClearLipSyncKeyframes: CacheSubsystem "
                    "not available"));
        return false;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(Performer, LevelSequence);
    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ClearLipSyncKeyframes: Failed to get "
                    "ControlRigInstance"));
        return false;
    }

    // 查找 ControlRig 轨道
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);
    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] ClearLipSyncKeyframes: Failed to find "
                    "ControlRig track"));
        return false;
    }

    // 查找已有的 Section
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

    if (!Section) {
        UE_LOG(LogTemp, Warning,
               TEXT("[LipSyncUtility] ClearLipSyncKeyframes: No existing "
                    "section found"));
        return false;
    }

    // 清空 RootControlName 下所有 Float Animation Channel 的关键帧
    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    int32 ClearedCount = 0;

    if (RigHierarchy) {
        FRigElementKey RootKey(*RootControlName, ERigElementType::Control);
        TArray<FRigElementKey> AllKeys = RigHierarchy->GetAllKeys(false);
        for (const FRigElementKey& Key : AllKeys) {
            if (Key.Type != ERigElementType::Control) continue;

            FRigElementKey ParentKey = RigHierarchy->GetFirstParent(Key);
            if (ParentKey != RootKey) continue;

            const FRigControlElement* ControlElement =
                RigHierarchy->Find<FRigControlElement>(Key);
            if (!ControlElement || !ControlElement->IsAnimationChannel())
                continue;
            if (ControlElement->Settings.ControlType != ERigControlType::Float)
                continue;

            // 通过通道代理找到已有 Float Channel 并 Reset() 清空关键帧
            // 注意：AddScalarParameter 对已有通道无效（HasScalarParameter 返回
            // true 时跳过）
            FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
            TArrayView<FMovieSceneFloatChannel*> FloatChannels =
                ChannelProxy.GetChannels<FMovieSceneFloatChannel>();
            for (FMovieSceneFloatChannel* FloatChannel : FloatChannels) {
                if (!FloatChannel) continue;
                UE::MovieScene::FControlRigChannelMetaData MetaData =
                    Section->GetChannelMetaData(FloatChannel);
                if (MetaData && MetaData.GetControlName() == Key.Name) {
                    FloatChannel->Reset();
                    ClearedCount++;
                    break;
                }
            }
        }
    }

    // 通知 Sequencer 刷新
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
           TEXT("[LipSyncUtility] ClearLipSyncKeyframes: Cleared %d float "
                "channels under '%s'"),
           ClearedCount, *RootControlName);

    return ClearedCount > 0;
}

// ===== 完整写入流程 =====

int32 ULipSyncUtility::GenerateLipSyncFromJson(
    ASkeletalMeshActor* Performer, UControlRigBlueprint* ControlRigBlueprint,
    const FString& FilePath) {
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

    // Step 1: 自动检测格式并解析口型文件（支持 .json 和 .tsv）
    TArray<FLipSyncMouthCue> Cues;
    float Duration = 0.0f;
    if (!ParseLipSyncFile(FilePath, Cues, Duration)) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GenerateLipSyncFromJson: Failed to parse "
                    "file: %s"),
               *FilePath);
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

    // Step 5: 转换 Cues → KeyframeData（部分映射缺失时只跳过无映射的口型）
    TArray<FMorphTargetKeyframeData> KeyframeData;
    ConvertCuesToKeyframeData(Cues, Mapping, TickResolution, DisplayRate,
                              KeyframeData);

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("[LipSyncUtility] GenerateLipSyncFromJson: No keyframe "
                    "data generated (check mapping configuration)"));
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
