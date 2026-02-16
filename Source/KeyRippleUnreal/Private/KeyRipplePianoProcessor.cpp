#include "KeyRipplePianoProcessor.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Common/Public/InstrumentAnimationUtility.h"
#include "Common/Public/InstrumentControlRigUtility.h"
#include "Common/Public/InstrumentMaterialUtility.h"
#include "Common/Public/InstrumentMorphTargetUtility.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "Engine/World.h"
#include "EntitySystem/MovieSceneSharedPlaybackState.h"
#include "ISequencer.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "KeyRippleControlRigProcessor.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "Rigs/RigHierarchyController.h"
#include "Sections/MovieSceneComponentMaterialParameterSection.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "KeyRipplePianoProcessor"

void UKeyRipplePianoProcessor::UpdatePianoMaterials(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleActor is null in UpdatePianoMaterials"));
        return;
    }

    if (!KeyRippleActor->Piano) {
        UE_LOG(LogTemp, Error, TEXT("Piano is not assigned in KeyRippleActor"));
        return;
    }

    if (!KeyRippleActor->KeyMatWhite || !KeyRippleActor->KeyMatBlack) {
        UE_LOG(LogTemp, Error,
               TEXT("Key materials are not assigned in KeyRippleActor"));
        return;
    }

    // 获取 Piano 的 SkeletalMeshComponent
    USkeletalMeshComponent* SkeletalMeshComp =
        KeyRippleActor->Piano->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano does not have a SkeletalMeshComponent"));
        return;
    }

    // 使用Common模块的材质更新工具
    FMaterialUpdateSettings Settings;
    Settings.bSkipAnimatedMaterials = true;

    // 定义材质选择器函数
    Settings.MaterialSelector = [KeyRippleActor, SkeletalMeshComp](
                                    const FString& SlotName,
                                    int32 SlotIndex) -> UMaterialInterface* {
        // 尝试提取键号
        int32 KeyNumber = -1;
        TArray<FString> NameParts;
        SlotName.ParseIntoArray(NameParts, TEXT("_"));

        if (NameParts.Num() > 0) {
            FString LastPart = NameParts[NameParts.Num() - 1];
            if (LastPart.IsNumeric()) {
                KeyNumber = FCString::Atoi(*LastPart);
            }
        }

        if (KeyNumber < 0) {
            return nullptr;
        }

        // 判断黑白键
        int32 ModValue = KeyNumber % 12;
        bool bIsBlackKey = (ModValue == 1 || ModValue == 3 || ModValue == 6 ||
                            ModValue == 8 || ModValue == 10);

        UMaterialInterface* ParentMaterial = bIsBlackKey
                                                 ? KeyRippleActor->KeyMatBlack
                                                 : KeyRippleActor->KeyMatWhite;

        if (!ParentMaterial) {
            return nullptr;
        }

        // 构建材质名称
        FString MaterialName = FString::Printf(TEXT("MAT_Key_%d"), KeyNumber);
        FString PackagePath =
            FString::Printf(TEXT("/Game/Materials/%s"), *MaterialName);

        // 使用Common模块创建或获取材质实例
        return UInstrumentMaterialUtility::CreateOrGetMaterialInstance(
            MaterialName, PackagePath, ParentMaterial,
            KeyRippleActor->GeneratedPianoMaterials);
    };

    // 执行材质更新
    int32 UpdatedCount =
        UInstrumentMaterialUtility::UpdateSkeletalMeshMaterials(
            SkeletalMeshComp, Settings,
            KeyRippleActor->GeneratedPianoMaterials);

    // 输出报告
    UE_LOG(LogTemp, Warning,
           TEXT("========== UpdatePianoMaterials Report =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully updated %d materials"),
           UpdatedCount);
    UE_LOG(LogTemp, Warning, TEXT("GeneratedPianoMaterials count: %d"),
           KeyRippleActor->GeneratedPianoMaterials.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== UpdatePianoMaterials Completed =========="));
}

void UKeyRipplePianoProcessor::InitPiano(AKeyRippleUnreal* KeyRippleActor) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error, TEXT("KeyRippleActor is null in InitPiano"));
        return;
    }

    if (!KeyRippleActor->Piano) {
        UE_LOG(LogTemp, Error, TEXT("Piano is not assigned in KeyRippleActor"));
        return;
    }

    if (!KeyRippleActor->KeyMatWhite) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyMatWhite is not assigned in KeyRippleActor"));
        return;
    }

    if (!KeyRippleActor->KeyMatBlack) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyMatBlack is not assigned in KeyRippleActor"));
        return;
    }

    // 清空之前生成的材质实例，防止旧资产残留
    KeyRippleActor->GeneratedPianoMaterials.Empty();

    UE_LOG(LogTemp, Warning, TEXT("========== InitPiano Started =========="));

    // 🔧 新增：清理现有的动画数据
    CleanupExistingPianoAnimations(KeyRippleActor);

    // 更新钢琴材质
    UpdatePianoMaterials(KeyRippleActor);

    // 初始化钢琴键 Control Rig
    InitPianoKeyControlRig(KeyRippleActor);

    // 初始化钢琴材质参数轨道
    InitPianoMaterialParameterTracks(KeyRippleActor);

    UE_LOG(LogTemp, Warning, TEXT("========== InitPiano Completed =========="));
}

void UKeyRipplePianoProcessor::GenerateInstrumentAnimation(
    AKeyRippleUnreal* KeyRippleActor, const FString& PianoKeyAnimationPath) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error, TEXT("KeyRippleActor is null"));
        return;
    }

    if (PianoKeyAnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("PianoKeyAnimationPath is empty"));
        return;
    }

    if (!KeyRippleActor->Piano) {
        UE_LOG(LogTemp, Error, TEXT("Piano is not assigned in KeyRippleActor"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation "
                "Started =========="));

#if WITH_EDITOR
    //===== Step 1: Read and parse JSON animation file using Common utility
    //=====
    TArray<FMorphTargetKeyframeData> KeyframeData;

    // 使用Common模块的通用方法获取LevelSequence和Sequencer
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return;
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    if (!UInstrumentMorphTargetUtility::ParseMorphTargetJSON(
            PianoKeyAnimationPath, KeyframeData, TickResolution, DisplayRate)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to parse morph target JSON file: %s"),
               *PianoKeyAnimationPath);
        return;
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("No morph target data found in JSON"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Loaded %d morph target entries from JSON"),
           KeyframeData.Num());

    //===== Step 2: Get Piano's Control Rig Instance =====

    //===== Step 2.1: Get unique ObjectBindingID for this piano instance using
    // Common utility =====
    FGuid PianoBindingID =
        UInstrumentAnimationUtility::FindSkeletalMeshActorBinding(
            Sequencer, LevelSequence, KeyRippleActor->Piano);

    if (!PianoBindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ObjectBindingID for Piano instance!"));
        return;
    }

    // 输出当前piano实例的ObjectBindingID
    UE_LOG(LogTemp, Warning,
           TEXT("Current Piano SkeletalMeshActor binding ID: %s"),
           *PianoBindingID.ToString());

    //===== Step 3: Get Piano's Control Rig Instance =====
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
            KeyRippleActor->Piano, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig from Piano SkeletalMeshActor"));
        return;
    }

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRig Instance or Blueprint is null"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get hierarchy from ControlRig"));
        return;
    }

    //===== Step 4: Find root control (piano_key_root) =====
    FRigElementKey RootControlKey(TEXT("piano_key_root"),
                                  ERigElementType::Control);
    if (!RigHierarchy->Contains(RootControlKey)) {
        UE_LOG(LogTemp, Error, TEXT("Root control 'piano_key_root' not found"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Found root control: piano_key_root"));

    //===== Step 5: Find or create Control Rig track (must match this ControlRig
    // instance) =====
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Error, TEXT("Failed to find Control Rig track"));
        return;
    }

    // 校验轨道归属（可选，增强健壮性）
    if (ControlRigTrack->GetControlRig() != ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRigTrack does not match current Piano's ControlRig "
                    "instance!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Found Control Rig track"));

    //===== Step 6: Get or create Section =====
    TArray<UMovieSceneSection*> Sections = ControlRigTrack->GetAllSections();
    UMovieSceneSection* Section = nullptr;

    if (Sections.Num() > 0) {
        Section = Sections[0];
    } else {
        Section = ControlRigTrack->CreateNewSection();
        if (Section) {
            ControlRigTrack->AddSection(*Section);
        }
    }

    if (!Section) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get or create Section"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Working with Section"));

    //===== Step 7: Clear all sections and create new one before batch
    TArray<UMovieSceneSection*> AllExistingSections =
        ControlRigTrack->GetAllSections();

    if (AllExistingSections.Num() > 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("Removing %d sections from Piano Control Rig track before "
                    "adding new keyframes"),
               AllExistingSections.Num());

        // 删除所有现有sections
        for (UMovieSceneSection* ExistingSection : AllExistingSections) {
            if (ExistingSection) {
                ControlRigTrack->RemoveSection(*ExistingSection);
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("All existing sections removed"));
    }

    // 创建一个新的空section
    Section = ControlRigTrack->CreateNewSection();
    if (Section) {
        ControlRigTrack->AddSection(*Section);
        UE_LOG(LogTemp, Warning,
               TEXT("Created new empty section for piano morph targets"));
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to create new section for piano morph targets"));
        return;
    }

    //===== Step 8: Calculate frame range from parsed data =====
    FFrameNumber seqStart, seqEnd;
    TRange<FFrameNumber> seqrange = MovieScene->GetPlaybackRange();
    seqStart = seqrange.GetLowerBoundValue();
    seqEnd = seqrange.GetUpperBoundValue();
    UE_LOG(LogTemp, Warning, TEXT("MovieScene Playback Range: %d - %d"),
           seqStart.Value, seqEnd.Value);

    // 初始化 MinFrame 和 MaxFrame
    FFrameNumber MinFrame(MAX_int32);
    FFrameNumber MaxFrame(MIN_int32);
    bool bHasFrames = false;

    // 从已解析的数据中计算帧数范围
    for (const FMorphTargetKeyframeData& Data : KeyframeData) {
        for (const FFrameNumber& FrameNumber : Data.FrameNumbers) {
            if (FrameNumber < MinFrame) {
                MinFrame = FrameNumber;
            }
            if (FrameNumber > MaxFrame) {
                MaxFrame = FrameNumber;
            }
            bHasFrames = true;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Processing morph target data from JSON..."));

    //===== Step 8: Write keyframes using Common utility =====
    int32 WrittenTargets =
        UInstrumentMorphTargetUtility::WriteMorphTargetKeyframes(Section,
                                                                 KeyframeData);

    UE_LOG(LogTemp, Warning,
           TEXT("Successfully wrote keyframes for %d morph targets"),
           WrittenTargets);

    // Log the actual min/max frames we collected
    if (bHasFrames) {
        UE_LOG(LogTemp, Warning, TEXT("Collected Frame Range: %d - %d"),
               MinFrame.Value, MaxFrame.Value);
    }

    //===== Step 9: Update section range =====
    if (bHasFrames) {
        Section->SetRange(TRange<FFrameNumber>(MinFrame, MaxFrame));
        UE_LOG(LogTemp, Warning,
               TEXT("Set section range to [%d, %d) to include all frames from "
                    "%d to %d"),
               MinFrame.Value, (MaxFrame + 1).Value, MinFrame.Value,
               MaxFrame.Value);
    }

    //===== Step 10: Mark as modified and refresh =====
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    // 如果在编辑器中，可能需要刷新Sequencer UI
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif

    // 🔧 ADD: 材质参数关键帧报告
    UE_LOG(LogTemp, Warning,
           TEXT("========== Instrument Animation Report =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed %d morph targets"),
           KeyframeData.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== Instrument Animation Completed =========="));

    //===== Step 11: Generate material parameter animation =====
    UE_LOG(LogTemp, Warning,
           TEXT("========== Step 11: Generating material parameter animation "
                "=========="));

    // 转换KeyframeData为MorphTargetKeyframeData格式
    TMap<FString, TPair<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>
        MorphTargetKeyframeData;
    for (const FMorphTargetKeyframeData& Data : KeyframeData) {
        TArray<FMovieSceneFloatValue> FloatValues;
        FloatValues.Reserve(Data.Values.Num());
        for (float Value : Data.Values) {
            FloatValues.Add(FMovieSceneFloatValue(Value));
        }
        MorphTargetKeyframeData.Add(
            Data.MorphTargetName,
            TPair<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>(
                Data.FrameNumbers, FloatValues));
    }

    int32 MaterialAnimationResult = GenerateInstrumentMaterialAnimation(
        KeyRippleActor, LevelSequence, MorphTargetKeyframeData, MinFrame,
        MaxFrame);

    if (MaterialAnimationResult > 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("✓ Material parameter animation generated successfully for "
                    "%d material tracks"),
               MaterialAnimationResult);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("✗ No material parameter animation was generated"));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentAnimation "
                "Completed =========="));
#endif
}

bool UKeyRipplePianoProcessor::GetPianoMorphTargetNames(
    AKeyRippleUnreal* KeyRippleActor, TArray<FString>& OutMorphTargetNames) {
    OutMorphTargetNames.Empty();

    if (!KeyRippleActor || !KeyRippleActor->Piano) {
        UE_LOG(LogTemp, Error, TEXT("Invalid KeyRippleActor or Piano"));
        return false;
    }

    USkeletalMeshComponent* SkeletalMeshComp =
        KeyRippleActor->Piano->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano does not have a SkeletalMeshComponent"));
        return false;
    }

    return UInstrumentMorphTargetUtility::GetMorphTargetNames(
        SkeletalMeshComp, OutMorphTargetNames);
}

void UKeyRipplePianoProcessor::InitPianoKeyControlRig(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleActor is null in InitPianoKeyControlRig"));
        return;
    }

    if (!KeyRippleActor->Piano) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano is not assigned in InitPianoKeyControlRig"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitPianoKeyControlRig Started =========="));

    // 获取 Control Rig Instance 和 Blueprint
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
            KeyRippleActor->Piano, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig from Piano SkeletalMeshActor in "
                    "InitPianoKeyControlRig"));
        return;
    }

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRigBlueprint is null in InitPianoKeyControlRig"));
        return;
    }

    // 步骤 1: 获取所有 Morph Target 名称
    TArray<FString> MorphTargetNames;
    if (!GetPianoMorphTargetNames(KeyRippleActor, MorphTargetNames)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Morph Target names in "
                    "InitPianoKeyControlRig"));
        return;
    }

    // 步骤 2: 确保 Root Control 存在
    if (!UInstrumentMorphTargetUtility::EnsureRootControlExists(
            ControlRigBlueprint, TEXT("piano_key_root"))) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to ensure Root Control exists in "
                    "InitPianoKeyControlRig"));
        return;
    }

    // 步骤 3: 在 Root Control 上添加动画通道
    FRigElementKey ParentKey(TEXT("piano_key_root"), ERigElementType::Control);
    int32 ChannelsAdded = UInstrumentMorphTargetUtility::AddAnimationChannels(
        ControlRigBlueprint, ParentKey, MorphTargetNames);

    if (ChannelsAdded == 0) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("No animation channels were added (they may already exist)"));
    }
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitPianoKeyControlRig Completed =========="));
}

int32 UKeyRipplePianoProcessor::InitPianoMaterialParameterTracks(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!KeyRippleActor) {
        UE_LOG(
            LogTemp, Error,
            TEXT("KeyRippleActor is null in InitPianoMaterialParameterTracks"));
        return 0;
    }

    if (!KeyRippleActor->Piano) {
        UE_LOG(
            LogTemp, Error,
            TEXT("Piano is not assigned in InitPianoMaterialParameterTracks"));
        return 0;
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("========== InitPianoMaterialParameterTracks Started =========="));

    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        return 0;
    }

    // 使用Common模块的通用方法查找SkeletalMeshActor绑定
    FGuid PianoObjectBindingID =
        UInstrumentAnimationUtility::FindSkeletalMeshActorBinding(
            Sequencer, LevelSequence, KeyRippleActor->Piano);

    if (!PianoObjectBindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to find Piano SkeletalMeshActor binding in Level "
                    "Sequence"));
        return 0;
    }

    // 获取 Piano 的 SkeletalMeshComponent
    USkeletalMeshComponent* SkeletalMeshComp =
        KeyRippleActor->Piano->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano does not have a SkeletalMeshComponent"));
        return 0;
    }

    // 使用Common模块的通用方法获取或创建SkeletalMeshComponent绑定
    FGuid SkeletalMeshCompBindingID =
        UInstrumentAnimationUtility::GetOrCreateComponentBinding(
            Sequencer, SkeletalMeshComp, true);

    if (!SkeletalMeshCompBindingID.IsValid()) {
        UE_LOG(
            LogTemp, Error,
            TEXT("Failed to get or create binding for SkeletalMeshComponent"));
        return 0;
    }

    UE_LOG(LogTemp, Warning, TEXT("Final SkeletalMeshComponent BindingID: %s"),
           *SkeletalMeshCompBindingID.ToString());

    int32 SuccessCount = 0;
    int32 FailureCount = 0;
    int32 NumMaterials = SkeletalMeshComp->GetNumMaterials();

    UE_LOG(LogTemp, Warning,
           TEXT("Checking %d materials for Pressed parameter..."),
           NumMaterials);

    // 现在遍历所有材质槽，只处理材质轨道添加
    for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < NumMaterials;
         ++MaterialSlotIndex) {
        UMaterialInterface* CurrentMaterial =
            SkeletalMeshComp->GetMaterial(MaterialSlotIndex);

        if (!CurrentMaterial) {
            continue;
        }

        FString MaterialName = CurrentMaterial->GetName();

        // 检查材质是否有 Pressed 参数
        if (UInstrumentMaterialUtility::MaterialHasParameter(CurrentMaterial,
                                                             TEXT("Pressed"))) {
            // 查找或创建材质参数轨道
            UMovieSceneComponentMaterialTrack* MaterialTrack =
                UInstrumentAnimationUtility::FindOrCreateComponentMaterialTrack(
                    LevelSequence, SkeletalMeshCompBindingID,
                    MaterialSlotIndex);

            if (MaterialTrack &&
                UInstrumentAnimationUtility::AddMaterialParameter(
                    MaterialTrack, TEXT("Pressed"), 0.0f)) {
                SuccessCount++;
            } else {
                UE_LOG(LogTemp, Warning,
                       TEXT("  ✗ Failed to create material parameter track for "
                            "'%s' (slot %d)"),
                       *MaterialName, MaterialSlotIndex);
                FailureCount++;
            }
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("  - Material '%s' (slot %d) does not have Pressed "
                        "parameter"),
                   *MaterialName, MaterialSlotIndex);
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("========== InitPianoMaterialParameterTracks Report =========="));
    UE_LOG(LogTemp, Warning,
           TEXT("Successfully created: %d material parameter tracks"),
           SuccessCount);
    UE_LOG(LogTemp, Warning,
           TEXT("Failed to create: %d material parameter tracks"),
           FailureCount);
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitPianoMaterialParameterTracks Completed "
                "=========="));

    return SuccessCount;
}

int32 UKeyRipplePianoProcessor::GenerateInstrumentMaterialAnimation(
    AKeyRippleUnreal* KeyRippleActor, ULevelSequence* LevelSequence,
    const TMap<FString,
               TPair<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
        MorphTargetKeyframeData,
    FFrameNumber MinFrame, FFrameNumber MaxFrame) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleActor is null in "
                    "GenerateInstrumentMaterialAnimation"));
        return 0;
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("LevelSequence is null in "
                    "GenerateInstrumentMaterialAnimation"));
        return 0;
    }

    if (!KeyRippleActor->Piano) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano is not assigned in "
                    "GenerateInstrumentMaterialAnimation"));
        return 0;
    }

    // 转换为Common模块使用的数据结构
    TArray<FMaterialParameterKeyframeData> MaterialKeyframeData;

    // 将MorphTarget数据转换为材质参数数据
    for (const auto& MorphTargetPair : MorphTargetKeyframeData) {
        const FString& MorphTargetName = MorphTargetPair.Key;
        const TArray<FFrameNumber>& FrameNumbers = MorphTargetPair.Value.Key;
        const TArray<FMovieSceneFloatValue>& FrameValues =
            MorphTargetPair.Value.Value;

        // 提取钢琴键号
        int32 KeyNumber = -1;
        TArray<FString> NameParts;
        MorphTargetName.ParseIntoArray(NameParts, TEXT("_"));

        for (int32 i = 0; i < NameParts.Num(); ++i) {
            if (NameParts[i].IsNumeric()) {
                KeyNumber = FCString::Atoi(*NameParts[i]);
                break;
            }
        }

        if (KeyNumber >= 0) {
            // 创建材质参数关键帧数据
            FMaterialParameterKeyframeData ParamData;
            ParamData.ParameterName = TEXT("Pressed");  // 固定使用Pressed参数
            ParamData.FrameNumbers = FrameNumbers;

            // 转换FloatValue到普通float数组
            ParamData.Values.Reserve(FrameValues.Num());
            for (const FMovieSceneFloatValue& FloatValue : FrameValues) {
                ParamData.Values.Add(FloatValue.Value);
            }

            MaterialKeyframeData.Add(ParamData);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateInstrumentMaterialAnimation Started "
                "=========="));

#if WITH_EDITOR
    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return 0;
    }

    // 获取 Piano 的 SkeletalMeshComponent
    USkeletalMeshComponent* SkeletalMeshComp =
        KeyRippleActor->Piano->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano does not have a SkeletalMeshComponent"));
        return 0;
    }

    // 获取当前活跃的Sequencer
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
        TArray<TWeakPtr<ISequencer>> WeakSequencers =
            FLevelEditorSequencerIntegration::Get().GetSequencers();

        for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
            if (TSharedPtr<ISequencer> CurrentSequencer = WeakSequencer.Pin()) {
                UMovieSceneSequence* RootSequence =
                    CurrentSequencer->GetRootMovieSceneSequence();

                if (RootSequence && RootSequence == LevelSequence) {
                    Sequencer = CurrentSequencer;
                    break;
                }
            }
        }
    }

    // 验证Sequencer实例是否有效
    if (!Sequencer.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("No active sequencer found for the given LevelSequence"));
        return 0;
    }

    // 验证传入的LevelSequence是否匹配当前Sequencer
    UMovieSceneSequence* RootSequence = Sequencer->GetRootMovieSceneSequence();
    if (!RootSequence || RootSequence != LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("LevelSequence does not match current Sequencer"));
        return 0;
    }

    // 获取SkeletalMeshComponent的绑定ID
    FGuid SkeletalMeshCompBindingID =
        UInstrumentAnimationUtility::GetOrCreateComponentBinding(
            Sequencer, SkeletalMeshComp, true);

    if (!SkeletalMeshCompBindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get binding ID for SkeletalMeshComponent"));
        return 0;
    }

    int32 SuccessCount = 0;
    int32 NumMaterials = SkeletalMeshComp->GetNumMaterials();

    // 为每个有Pressed参数的材质创建轨道并写入关键帧
    for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < NumMaterials;
         ++MaterialSlotIndex) {
        UMaterialInterface* CurrentMaterial =
            SkeletalMeshComp->GetMaterial(MaterialSlotIndex);
        if (!CurrentMaterial ||
            !UInstrumentMaterialUtility::MaterialHasParameter(
                CurrentMaterial, TEXT("Pressed"))) {
            continue;
        }

        // 获取当前材质对应的钢琴键号
        FString MaterialSlotName =
            SkeletalMeshComp->GetMaterialSlotNames()[MaterialSlotIndex]
                .ToString();
        int32 PianoKeyNumber = -1;

        // 从材质槽名称中提取键号（格式：piano_key_88_0 或类似）
        TArray<FString> NameParts;
        MaterialSlotName.ParseIntoArray(NameParts, TEXT("_"));

        for (int32 i = NameParts.Num() - 1; i >= 0; --i) {
            if (NameParts[i].IsNumeric()) {
                PianoKeyNumber = FCString::Atoi(*NameParts[i]);
                break;
            }
        }

        if (PianoKeyNumber < 0) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Could not extract key number from material slot: %s"),
                   *MaterialSlotName);
            continue;
        }

        // 查找该键号对应的关键帧数据 - 修复后的逻辑
        TArray<FMaterialParameterKeyframeData> KeySpecificData;

        // 遍历原始的MorphTargetKeyframeData来查找精确匹配的键
        bool bFoundMatchingKey = false;
        for (const auto& MorphTargetPair : MorphTargetKeyframeData) {
            const FString& MorphTargetName = MorphTargetPair.Key;

            // 从MorphTarget名称中提取键号进行匹配
            TArray<FString> MorphTargetParts;
            MorphTargetName.ParseIntoArray(MorphTargetParts, TEXT("_"));

            int32 MorphTargetKeyNumber = -1;
            for (const FString& Part : MorphTargetParts) {
                if (Part.IsNumeric()) {
                    MorphTargetKeyNumber = FCString::Atoi(*Part);
                    break;
                }
            }

            // 精确匹配：只有当键号完全相同时才处理
            if (MorphTargetKeyNumber == PianoKeyNumber) {
                const TArray<FFrameNumber>& FrameNumbers =
                    MorphTargetPair.Value.Key;
                const TArray<FMovieSceneFloatValue>& FrameValues =
                    MorphTargetPair.Value.Value;

                FMaterialParameterKeyframeData ParamData;
                ParamData.ParameterName = TEXT("Pressed");
                ParamData.FrameNumbers = FrameNumbers;

                // 转换FloatValue到普通float数组
                ParamData.Values.Reserve(FrameValues.Num());
                for (const FMovieSceneFloatValue& FloatValue : FrameValues) {
                    ParamData.Values.Add(FloatValue.Value);
                }

                KeySpecificData.Add(ParamData);
                bFoundMatchingKey = true;
                break;  // 找到匹配项后立即退出循环，避免处理其他键的数据
            }
        }

        // 如果没有找到对应的关键帧数据，跳过该材质
        if (!bFoundMatchingKey) {
            UE_LOG(LogTemp, Verbose,
                   TEXT("No animation data found for piano key %d (material "
                        "slot %d: %s)"),
                   PianoKeyNumber, MaterialSlotIndex, *MaterialSlotName);
            continue;
        }

        // 验证：确保只为当前键写入数据
        if (KeySpecificData.Num() != 1) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Unexpected: Found %d matching entries for key %d, "
                        "expected exactly 1"),
                   KeySpecificData.Num(), PianoKeyNumber);
        }

        // 查找或创建材质参数轨道
        UMovieSceneComponentMaterialTrack* MaterialTrack =
            UInstrumentAnimationUtility::FindOrCreateComponentMaterialTrack(
                LevelSequence, SkeletalMeshCompBindingID, MaterialSlotIndex);

        if (!MaterialTrack) {
            continue;
        }

        // 确保有Pressed参数
        if (!UInstrumentAnimationUtility::AddMaterialParameter(
                MaterialTrack, TEXT("Pressed"), 0.0f)) {
            continue;
        }

        // 清理现有Section并创建新的
        UMovieSceneSection* NewSection =
            UInstrumentAnimationUtility::ResetTrackSections(MaterialTrack);
        if (!NewSection) {
            continue;
        }

        UMovieSceneComponentMaterialParameterSection* ParameterSection =
            Cast<UMovieSceneComponentMaterialParameterSection>(NewSection);
        if (!ParameterSection) {
            continue;
        }

        // 只写入该键对应的关键帧数据
        int32 WrittenParams =
            UInstrumentAnimationUtility::WriteMaterialParameterKeyframes(
                ParameterSection, KeySpecificData);

        if (WrittenParams > 0) {
            SuccessCount++;
            UE_LOG(LogTemp, Log,
                   TEXT("Applied animation to piano key %d (material slot %d)"),
                   PianoKeyNumber, MaterialSlotIndex);

            // 设置Section范围
            if (MinFrame.Value != MAX_int32 && MaxFrame.Value != MIN_int32) {
                ParameterSection->SetRange(
                    TRange<FFrameNumber>(MinFrame, MaxFrame + 1));
            }
        }
    }

    // 标记为已修改
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif

    UE_LOG(LogTemp, Warning,
           TEXT("========== Material Animation Report =========="));
    UE_LOG(LogTemp, Warning,
           TEXT("Successfully written to: %d material parameter tracks"),
           SuccessCount);
    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateMaterialAnimationInLevelSequencer "
                "Completed =========="));

    return SuccessCount;

#else
    UE_LOG(LogTemp, Warning,
           TEXT("Material animation generation requires editor support"));
    return 0;
#endif
}

void UKeyRipplePianoProcessor::CleanupExistingPianoAnimations(
    AKeyRippleUnreal* KeyRippleActor) {
    if (!KeyRippleActor || !KeyRippleActor->Piano) {
        UE_LOG(LogTemp, Warning,
               TEXT("Invalid KeyRippleActor or Piano in CleanupExistingPianoAnimations"));
        return;
    }

    // 使用Common模块的通用清理方法
    UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
        KeyRippleActor->Piano);
}

#undef LOCTEXT_NAMESPACE
