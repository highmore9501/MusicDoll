#include "StringFlowMusicInstrumentProcessor.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Common/Public/InstrumentAnimationUtility.h"
#include "Common/Public/InstrumentControlRigUtility.h"
#include "Common/Public/InstrumentMaterialUtility.h"
#include "Common/Public/InstrumentMorphTargetUtility.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "ISequencer.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "Rigs/RigHierarchyController.h"
#include "Sections/MovieSceneComponentMaterialParameterSection.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StringFlowAnimationProcessor.h"
#include "StringFlowControlRigProcessor.h"
#include "StringFlowUnreal.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "StringFlowMusicInstrumentProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 0. InitializeStringInstrument - 初始化弦乐器（主入口方法）
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UStringFlowMusicInstrumentProcessor::InitializeStringInstrument(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowActor is null in InitializeStringInstrument"));
        return;
    }

    if (!StringFlowActor->StringInstrument) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument is not assigned in StringFlowActor"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringInstrument Started =========="));

    // 清理现有的动画数据
    CleanupExistingStringAnimations(StringFlowActor);

    // 初始化弦材质，这里实际还需要先去在模型上生成不同弦的材质，以后再去测试和处理
    InitializeStringMaterials(StringFlowActor);

    // 初始化弦振动Control Rig通道
    InitializeStringVibrationAnimationChannels(StringFlowActor);

    // 初始化弦材质参数轨道
    InitializeStringMaterialAnimationTracks(StringFlowActor);

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringInstrument Completed =========="));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 1. InitializeStringMaterials - 初始化弦乐器弦的材质
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UStringFlowMusicInstrumentProcessor::InitializeStringMaterials(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowActor is null in InitializeStringMaterials"));
        return;
    }

    if (!StringFlowActor->StringInstrument) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument is not assigned in StringFlowActor"));
        return;
    }

    USkeletalMeshComponent* SkeletalMeshComp =
        StringFlowActor->StringInstrument->GetSkeletalMeshComponent();

    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument does not have a SkeletalMeshComponent"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterials Started =========="));

    // 使用Common模块的批量材质更新方法
    FMaterialUpdateSettings Settings;
    Settings.bSkipAnimatedMaterials = true;

    // 定义材质选择器
    Settings.MaterialSelector = [StringFlowActor, SkeletalMeshComp](
                                    const FString& SlotName,
                                    int32 SlotIndex) -> UMaterialInterface* {
        // 构建材质名称和路径
        FString StringIndexStr = FString::FromInt(SlotIndex);
        FString TargetMaterialName =
            FString::Printf(TEXT("MAT_String_%s"), *StringIndexStr);
        FString PackagePath =
            FString::Printf(TEXT("/Game/Materials/%s"), *TargetMaterialName);

        UMaterialInterface* CurrentMaterial =
            SkeletalMeshComp->GetMaterial(SlotIndex);
        if (!CurrentMaterial) {
            return nullptr;
        }

        // 使用Common模块创建或获取材质实例
        return UInstrumentMaterialUtility::CreateOrGetMaterialInstance(
            TargetMaterialName, PackagePath, CurrentMaterial,
            StringFlowActor->GeneratedMaterials);
    };

    // 执行批量更新
    int32 UpdatedCount =
        UInstrumentMaterialUtility::UpdateSkeletalMeshMaterials(
            SkeletalMeshComp, Settings, StringFlowActor->GeneratedMaterials);

    SkeletalMeshComp->MarkPackageDirty();

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterials Report =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully updated: %d materials"),
           UpdatedCount);
    UE_LOG(LogTemp, Warning, TEXT("GeneratedMaterials count: %d"),
           StringFlowActor->GeneratedMaterials.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterials Completed =========="));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 2. InitializeStringMaterialAnimationTracks - 初始化弦材质参数动画轨道
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32 UStringFlowMusicInstrumentProcessor::
    InitializeStringMaterialAnimationTracks(
        AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowActor is null in "
                    "InitializeStringMaterialAnimationTracks"));
        return 0;
    }

    if (!StringFlowActor->StringInstrument) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument is not assigned in "
                    "InitializeStringMaterialAnimationTracks"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterialAnimationTracks Started "
                "=========="));

    // 使用Common模块的方法获取LevelSequence和Sequencer
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error, TEXT("请确保已打开Level Sequence"));
        return 0;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("Invalid MovieScene in LevelSequence"));
        return 0;
    }

    USkeletalMeshComponent* SkeletalMeshComp =
        StringFlowActor->StringInstrument->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument does not have a SkeletalMeshComponent"));
        return 0;
    }

    // 使用Common模块的方法获取组件绑定
    FGuid SkeletalMeshCompBindingID =
        UInstrumentAnimationUtility::GetOrCreateComponentBinding(
            Sequencer, SkeletalMeshComp, true);

    if (!SkeletalMeshCompBindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get SkeletalMeshComponent binding"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("✅ Got/Created SkeletalMeshComponent binding: %s"),
           *SkeletalMeshCompBindingID.ToString());

    int32 SuccessCount = 0;
    int32 FailureCount = 0;
    int32 NumMaterials = SkeletalMeshComp->GetNumMaterials();

    UE_LOG(LogTemp, Warning,
           TEXT("Checking %d materials for Vibration parameter..."),
           NumMaterials);

    // 遍历所有材质槽，为每根弦的材质创建参数轨道
    for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < NumMaterials;
         ++MaterialSlotIndex) {
        UMaterialInterface* CurrentMaterial =
            SkeletalMeshComp->GetMaterial(MaterialSlotIndex);

        if (!CurrentMaterial) {
            continue;
        }

        FString MaterialName = CurrentMaterial->GetName();

        // 使用Common模块方法检查材质是否有Vibration参数
        if (UInstrumentMaterialUtility::MaterialHasParameter(
                CurrentMaterial, TEXT("Vibration"))) {
            // 使用Common模块方法查找或创建材质轨道
            UMovieSceneComponentMaterialTrack* MaterialTrack =
                UInstrumentAnimationUtility::FindOrCreateComponentMaterialTrack(
                    LevelSequence, SkeletalMeshCompBindingID,
                    MaterialSlotIndex);

            if (MaterialTrack) {
                // 使用Common模块方法添加参数
                if (UInstrumentAnimationUtility::AddMaterialParameter(
                        MaterialTrack, TEXT("Vibration"), 0.0f)) {
                    SuccessCount++;
                    UE_LOG(LogTemp, Log,
                           TEXT("  ✓ Created material parameter track for "
                                "'%s' (slot %d)"),
                           *MaterialName, MaterialSlotIndex);
                } else {
                    FailureCount++;
                    UE_LOG(LogTemp, Warning,
                           TEXT("  ✗ Failed to add Vibration parameter to "
                                "track for "
                                "'%s' (slot %d)"),
                           *MaterialName, MaterialSlotIndex);
                }
            } else {
                FailureCount++;
                UE_LOG(LogTemp, Warning,
                       TEXT("  ✗ Failed to create material parameter track for "
                            "'%s' (slot %d)"),
                       *MaterialName, MaterialSlotIndex);
            }
        } else {
            UE_LOG(LogTemp, Verbose,
                   TEXT("  - Material '%s' (slot %d) does not have Vibration "
                        "parameter"),
                   *MaterialName, MaterialSlotIndex);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterialAnimationTracks Report "
                "=========="));
    UE_LOG(LogTemp, Warning,
           TEXT("Successfully created: %d material parameter tracks"),
           SuccessCount);
    UE_LOG(LogTemp, Warning,
           TEXT("Failed to create: %d material parameter tracks"),
           FailureCount);
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringMaterialAnimationTracks Completed "
                "=========="));

    return SuccessCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 3. InitializeStringVibrationAnimationChannels - 初始化弦振动动画通道
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UStringFlowMusicInstrumentProcessor::
    InitializeStringVibrationAnimationChannels(
        AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowActor is null in "
                    "InitializeStringVibrationAnimationChannels"));
        return;
    }

    if (!StringFlowActor->StringInstrument) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument is not assigned in "
                    "InitializeStringVibrationAnimationChannels"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels Started "
                "=========="));

    // 获取 Control Rig Instance 和 Blueprint
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
            StringFlowActor->StringInstrument, ControlRigInstance,
            ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig from StringInstrument in "
                    "InitializeStringVibrationAnimationChannels"));
        return;
    }

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRigBlueprint is null in "
                    "InitializeStringVibrationAnimationChannels"));
        return;
    }

    // 生成所有需要的通道名称
    TArray<FString> ChannelNamesToCreate;

    const int32 MaxStringIndex = 3;
    const int32 MinFretNumber = 2;
    const int32 MaxFretNumber = 21;

    for (int32 StringIndex = 0; StringIndex <= MaxStringIndex; ++StringIndex) {
        // 添加Basis通道（空弦音）
        FString BasisChannelStr =
            FString::Printf(TEXT("s%dBasis"), StringIndex);
        ChannelNamesToCreate.Add(BasisChannelStr);

        // 添加Fret通道
        for (int32 FretNumber = MinFretNumber; FretNumber <= MaxFretNumber;
             ++FretNumber) {
            FString FretChannelStr =
                FString::Printf(TEXT("s%dfret%d"), StringIndex, FretNumber);
            ChannelNamesToCreate.Add(FretChannelStr);
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("Creating vibration animation channels for %d channel names..."),
        ChannelNamesToCreate.Num());

    // 使用Common模块的通用方法：确保Root Control存在
    if (!UInstrumentMorphTargetUtility::EnsureRootControlExists(
            ControlRigBlueprint, TEXT("violin_root"))) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to ensure Root Control 'violin_root' exists"));
        return;
    }

    // 使用Common模块的通用方法：批量添加动画通道
    FRigElementKey ParentKey(TEXT("violin_root"), ERigElementType::Control);
    int32 ChannelsAdded = UInstrumentMorphTargetUtility::AddAnimationChannels(
        ControlRigBlueprint, ParentKey, ChannelNamesToCreate);

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels "
                "Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully created/verified: %d channels"),
           ChannelsAdded);
    UE_LOG(
        LogTemp, Warning,
        TEXT("Expected total: %d channels (4 strings × (1 basis + 20 frets))"),
        4 * (1 + (MaxFretNumber - MinFretNumber + 1)));
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeStringVibrationAnimationChannels "
                "Completed =========="));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 4. LoadAndGenerateStringVibrationAnimation - 从JSON加载数据并写入morph
// target轨道
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UStringFlowMusicInstrumentProcessor::
    LoadAndGenerateStringVibrationAnimation(
        AStringFlowUnreal* StringFlowActor,
        const FString& StringVibrationDataPath,
        TMap<FString,
             TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
            OutVibrationKeyframeData) {
    OutVibrationKeyframeData.Empty();

    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowActor is null in "
                    "LoadAndGenerateStringVibrationAnimation"));
        return false;
    }

    if (StringVibrationDataPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("StringVibrationDataPath is empty in "
                    "LoadAndGenerateStringVibrationAnimation"));
        return false;
    }

    if (!StringFlowActor->StringInstrument) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument is not assigned in "
                    "LoadAndGenerateStringVibrationAnimation"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== LoadAndGenerateStringVibrationAnimation Started "
                "=========="));

#if WITH_EDITOR
    // 使用Common模块解析JSON数据
    TArray<FMorphTargetKeyframeData> KeyframeData;

    // 使用Common模块的方法获取LevelSequence和Sequencer
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error, TEXT("请确保已打开Level Sequence"));
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return false;
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    // 使用Common模块方法解析JSON
    if (!UInstrumentMorphTargetUtility::ParseMorphTargetJSON(
            StringVibrationDataPath, KeyframeData, TickResolution,
            DisplayRate)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse vibration JSON file: %s"),
               *StringVibrationDataPath);
        return false;
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("No vibration data found"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Loaded %d vibration entries from JSON"),
           KeyframeData.Num());

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    // 使用Common模块的通用方法获取Control Rig
    if (!FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
            StringFlowActor->StringInstrument, ControlRigInstance,
            ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig from StringInstrument"));
        return false;
    }

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig Instance or Blueprint is null"));
        return false;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get hierarchy from ControlRig"));
        return false;
    }

    // 查找或创建 Control Rig 轨道
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Error, TEXT("Failed to find Control Rig track"));
        return false;
    }

    // 删除所有现有 sections
    TArray<UMovieSceneSection*> AllExistingSections =
        ControlRigTrack->GetAllSections();

    if (AllExistingSections.Num() > 0) {
        for (UMovieSceneSection* ExistingSection : AllExistingSections) {
            if (ExistingSection) {
                ControlRigTrack->RemoveSection(*ExistingSection);
            }
        }
    }

    // 创建新的 section
    UMovieSceneSection* Section = ControlRigTrack->CreateNewSection();
    if (Section) {
        ControlRigTrack->AddSection(*Section);
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to create new section for vibration animation"));
        return false;
    }

    // 收集 frame 范围信息
    FFrameNumber MinFrame(MAX_int32);
    FFrameNumber MaxFrame(MIN_int32);
    bool bHasFrames = false;

    UE_LOG(LogTemp, Warning, TEXT("Processing vibration data from JSON..."));

    // 使用Common模块解析后的数据
    for (const FMorphTargetKeyframeData& MorphData : KeyframeData) {
        // 从MorphTarget名称中提取弦索引
        FString ChannelName = MorphData.MorphTargetName;

        // 转换FrameNumbers和Values格式
        TArray<FFrameNumber> FrameNumbers = MorphData.FrameNumbers;
        TArray<FMovieSceneFloatValue> FrameValues;
        FrameValues.Reserve(MorphData.Values.Num());

        for (float Value : MorphData.Values) {
            FrameValues.Add(FMovieSceneFloatValue(Value));
        }

        if (FrameNumbers.Num() > 0) {
            // 更新帧范围
            for (const FFrameNumber& FrameNum : FrameNumbers) {
                if (FrameNum < MinFrame) {
                    MinFrame = FrameNum;
                }
                if (FrameNum > MaxFrame) {
                    MaxFrame = FrameNum;
                }
                bHasFrames = true;
            }

            if (OutVibrationKeyframeData.Contains(ChannelName)) {
                auto& ExistingPair = OutVibrationKeyframeData[ChannelName];
                ExistingPair.Get<0>().Append(FrameNumbers);
                ExistingPair.Get<1>().Append(FrameValues);
            } else {
                OutVibrationKeyframeData.Add(
                    ChannelName,
                    TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>(
                        FrameNumbers, FrameValues));
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Collected %d unique vibration channels"),
           OutVibrationKeyframeData.Num());

    // 使用Common模块方法批量写入关键帧
    TArray<FMorphTargetKeyframeData> MorphTargetData;
    MorphTargetData.Reserve(OutVibrationKeyframeData.Num());

    // 转换数据格式
    for (const auto& VibrationPair : OutVibrationKeyframeData) {
        const FString& ChannelName = VibrationPair.Key;
        const TArray<FFrameNumber>& FrameNumbers = VibrationPair.Value.Get<0>();
        const TArray<FMovieSceneFloatValue>& FrameValues =
            VibrationPair.Value.Get<1>();

        FMorphTargetKeyframeData Data(ChannelName);
        Data.FrameNumbers = FrameNumbers;

        // 转换FMovieSceneFloatValue到float
        Data.Values.Reserve(FrameValues.Num());
        for (const FMovieSceneFloatValue& FloatValue : FrameValues) {
            Data.Values.Add(FloatValue.Value);
        }

        MorphTargetData.Add(Data);
    }

    // 使用Common模块方法写入关键帧
    int32 WrittenTargets =
        UInstrumentMorphTargetUtility::WriteMorphTargetKeyframes(
            Section, MorphTargetData);

    UE_LOG(LogTemp, Warning,
           TEXT("  ✓ Successfully wrote keyframes for %d channels"),
           WrittenTargets);

    // 更新 section 范围
    if (bHasFrames) {
        Section->SetRange(TRange<FFrameNumber>(MinFrame, MaxFrame + 1));
    }

    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif

    UE_LOG(LogTemp, Warning,
           TEXT("========== LoadAndGenerateStringVibrationAnimation Completed "
                "=========="));

    return true;

#else
    return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 5. SyncVibrationToMaterialAnimation - 将振动数据同步写入材质动画轨道
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32 UStringFlowMusicInstrumentProcessor::SyncVibrationToMaterialAnimation(
    AStringFlowUnreal* StringFlowActor, ULevelSequence* LevelSequence,
    const TMap<FString,
               TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
        VibrationKeyframeData,
    FFrameNumber MinFrame, FFrameNumber MaxFrame) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowActor is null in "
                    "SyncVibrationToMaterialAnimation"));
        return 0;
    }

    if (!LevelSequence) {
        UE_LOG(
            LogTemp, Error,
            TEXT("LevelSequence is null in SyncVibrationToMaterialAnimation"));
        return 0;
    }

    if (!StringFlowActor->StringInstrument) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument is not assigned in "
                    "SyncVibrationToMaterialAnimation"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== SyncVibrationToMaterialAnimation Started "
                "=========="));

#if WITH_EDITOR
    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return 0;
    }

    USkeletalMeshComponent* SkeletalMeshComp =
        StringFlowActor->StringInstrument->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument does not have a SkeletalMeshComponent"));
        return 0;
    }

    // 使用Common模块的方法获取Sequencer
    TSharedPtr<ISequencer> Sequencer = nullptr;
    ULevelSequence* ActiveLevelSequence = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            ActiveLevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error, TEXT("请确保已打开Level Sequence"));
        return 0;
    }

    // 验证LevelSequence匹配
    if (ActiveLevelSequence != LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("LevelSequence does not match current Sequencer"));
        return 0;
    }

    // 使用Common模块的方法获取组件绑定
    FGuid SkeletalMeshCompBindingID =
        UInstrumentAnimationUtility::GetOrCreateComponentBinding(
            Sequencer, SkeletalMeshComp, false);

    if (!SkeletalMeshCompBindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get SkeletalMeshComponent binding"));
        return 0;
    }

    int32 SuccessCount = 0;
    int32 FailureCount = 0;
    int32 NumMaterials = SkeletalMeshComp->GetNumMaterials();

    // 为每个材质槽处理动画
    for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < NumMaterials;
         ++MaterialSlotIndex) {
        UMaterialInterface* CurrentMaterial =
            SkeletalMeshComp->GetMaterial(MaterialSlotIndex);

        if (!CurrentMaterial) {
            continue;
        }

        // 检查材质是否有Vibration参数
        if (!UInstrumentMaterialUtility::MaterialHasParameter(
                CurrentMaterial, TEXT("Vibration"))) {
            continue;
        }

        // 使用Common模块方法查找或创建材质轨道
        UMovieSceneComponentMaterialTrack* MaterialTrack =
            UInstrumentAnimationUtility::FindOrCreateComponentMaterialTrack(
                LevelSequence, SkeletalMeshCompBindingID, MaterialSlotIndex);

        if (!MaterialTrack) {
            FailureCount++;
            continue;
        }

        // 使用Common模块方法重置轨道sections（这会删除所有现有Section并创建新的空Section）
        UMovieSceneSection* NewSection =
            UInstrumentAnimationUtility::ResetTrackSections(MaterialTrack);

        if (!NewSection) {
            FailureCount++;
            continue;
        }

        UMovieSceneComponentMaterialParameterSection* ParameterSection =
            Cast<UMovieSceneComponentMaterialParameterSection>(NewSection);

        if (!ParameterSection) {
            FailureCount++;
            continue;
        }

        // 在新创建的空Section中添加Vibration参数（初始值为0）
        FMaterialParameterInfo ParameterInfo;
        ParameterInfo.Name = FName(TEXT("Vibration"));

        ParameterSection->AddScalarParameterKey(
            ParameterInfo, FFrameNumber(0), 0.0f, TEXT(""), TEXT(""),
            EMovieSceneKeyInterpolation::Auto);

        // 准备关键帧数据
        TArray<FMaterialParameterKeyframeData> KeyframeData;

        // 遍历振动数据，找到对应当前槽的数据
        for (const auto& VibrationPair : VibrationKeyframeData) {
            const FString& ChannelName = VibrationPair.Key;
            const TArray<FFrameNumber>& FrameNumbers =
                VibrationPair.Value.Get<0>();
            const TArray<FMovieSceneFloatValue>& FrameValues =
                VibrationPair.Value.Get<1>();

            // 从通道名提取弦索引并匹配到材质
            // 新格式: "s{string_index}fret{fret_number}" 或
            // "s{string_index}Basis"
            int32 StringIndex = -1;

            // 先尝试提取字符串中第一个数字作为弦索引
            // 格式: s0fret2, s1Basis 等
            if (ChannelName.Len() > 1 && ChannelName[0] == TCHAR('s')) {
                StringIndex = FCString::Atoi(*ChannelName.Mid(1, 1));
            }

            // 检查该弦是否映射到当前材质槽
            if (StringIndex >= 0 && StringIndex == MaterialSlotIndex) {
                if (FrameNumbers.Num() > 0 &&
                    FrameNumbers.Num() == FrameValues.Num()) {
                    FMaterialParameterKeyframeData ParamData(TEXT("Vibration"));
                    ParamData.FrameNumbers = FrameNumbers;

                    // 转换FMovieSceneFloatValue到float
                    ParamData.Values.Reserve(FrameValues.Num());
                    for (const FMovieSceneFloatValue& FloatValue :
                         FrameValues) {
                        ParamData.Values.Add(FloatValue.Value);
                    }

                    KeyframeData.Add(ParamData);
                }
            }
        }

        // 使用Common模块方法批量写入关键帧
        if (KeyframeData.Num() > 0) {
            int32 WrittenParams =
                UInstrumentAnimationUtility::WriteMaterialParameterKeyframes(
                    ParameterSection, KeyframeData);

            if (WrittenParams > 0) {
                SuccessCount++;
                UE_LOG(LogTemp, Warning,
                       TEXT("  ✓ Synced vibration data to material slot "
                            "%d (%d parameters)"),
                       MaterialSlotIndex, WrittenParams);
            } else {
                FailureCount++;
            }
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("  ⚠ No vibration data found for material slot %d"),
                   MaterialSlotIndex);
            // 即使没有数据，我们也成功创建了轨道，所以这不是失败
            SuccessCount++;
        }

        // 设置 Section 范围
        if (MinFrame.Value != MAX_int32 && MaxFrame.Value != MIN_int32) {
            ParameterSection->SetRange(
                TRange<FFrameNumber>(MinFrame, MaxFrame + 1));
        }
    }

    // 标记为已修改
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif

    UE_LOG(LogTemp, Warning,
           TEXT("========== SyncVibrationToMaterialAnimation Summary "
                "=========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully synced to: %d material tracks"),
           SuccessCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed: %d material tracks"), FailureCount);
    UE_LOG(LogTemp, Warning,
           TEXT("========== SyncVibrationToMaterialAnimation Completed "
                "=========="));

    return SuccessCount;

#else
    UE_LOG(LogTemp, Warning,
           TEXT("Material animation sync requires editor support"));
    return 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 6. CleanupExistingStringAnimations - 清理现有的弦乐器动画数据
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UStringFlowMusicInstrumentProcessor::CleanupExistingStringAnimations(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor || !StringFlowActor->StringInstrument) {
        UE_LOG(LogTemp, Warning,
               TEXT("Invalid StringFlowActor or StringInstrument in "
                    "CleanupExistingStringAnimations"));
        return;
    }

    // 使用Common模块的通用清理方法清理Sequencer中的轨道
    UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
        StringFlowActor->StringInstrument);
}

void UStringFlowMusicInstrumentProcessor::GenerateInstrumentAnimation(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowActor is null in GenerateInstrumentAnimation"));
        return;
    }

    if (!StringFlowActor->StringInstrument) {
        UE_LOG(LogTemp, Error,
               TEXT("StringInstrument is not assigned in "
                    "GenerateInstrumentAnimation"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Started =========="));

#if WITH_EDITOR
    // 获取配置文件路径
    FString LeftHandAnimationPath;
    FString RightHandAnimationPath;
    FString StringVibrationPath;

    if (!UStringFlowAnimationProcessor::ParseStringFlowConfigFile(
            StringFlowActor, LeftHandAnimationPath, RightHandAnimationPath,
            StringVibrationPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse StringFlow config file in "
                    "GenerateInstrumentAnimation"));
        return;
    }

    if (StringVibrationPath.IsEmpty()) {
        UE_LOG(LogTemp, Warning,
               TEXT("String vibration path is empty, skipping instrument "
                    "animation"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Generating instrument animation from: %s"),
           *StringVibrationPath);

    // 使用新的Morph Target生成方法
    TMap<FString, TTuple<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>
        VibrationKeyframeData;

    if (!LoadAndGenerateStringVibrationAnimation(
            StringFlowActor, StringVibrationPath, VibrationKeyframeData)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to load and generate string vibration animation"));
        return;
    }

    // 使用Common模块的方法获取LevelSequence和Sequencer
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error, TEXT("请确保已打开Level Sequence"));
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return;
    }

    // 计算帧范围
    FFrameNumber MinFrame = FFrameNumber(INT_MAX);
    FFrameNumber MaxFrame = FFrameNumber(INT_MIN);

    for (const auto& Pair : VibrationKeyframeData) {
        const TArray<FFrameNumber>& FrameNumbers = Pair.Value.Key;
        if (FrameNumbers.Num() > 0) {
            MinFrame = FMath::Min(MinFrame, FrameNumbers[0]);
            MaxFrame = FMath::Max(MaxFrame, FrameNumbers.Last());
        }
    }

    if (MinFrame.Value == INT_MAX || MaxFrame.Value == INT_MIN) {
        UE_LOG(LogTemp, Error, TEXT("Invalid frame range"));
        return;
    }

    // 同步到材质动画
    int32 MaterialTracksUpdated = SyncVibrationToMaterialAnimation(
        StringFlowActor, LevelSequence, VibrationKeyframeData, MinFrame,
        MaxFrame);

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Report =========="));
    UE_LOG(LogTemp, Warning,
           TEXT("Successfully processed string vibration data"));
    UE_LOG(LogTemp, Warning, TEXT("Material tracks updated: %d"),
           MaterialTracksUpdated);
    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation Completed =========="));

#endif
}

void UStringFlowMusicInstrumentProcessor::GenerateInstrumentMaterialAnimation(
    AStringFlowUnreal* StringFlowActor,
    const FString& InstrumentAnimationDataPath) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error,
               TEXT("StringFlowActor is null in "
                    "GenerateInstrumentMaterialAnimation"));
        return;
    }

    if (InstrumentAnimationDataPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("InstrumentAnimationDataPath is empty in "
                    "GenerateInstrumentMaterialAnimation"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentMaterialAnimation Started "
                "=========="));
    UE_LOG(LogTemp, Warning,
           TEXT("Instrument material animation generation is not yet "
                "implemented."));
    UE_LOG(LogTemp, Warning,
           TEXT("This functionality will be provided by a dedicated module."));
    UE_LOG(LogTemp, Warning, TEXT("Input path: %s"),
           *InstrumentAnimationDataPath);
    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentMaterialAnimation Completed "
                "=========="));
}

#undef LOCTEXT_NAMESPACE
