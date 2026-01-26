#include "KeyRipplePianoProcessor.h"

#include "Channels/MovieSceneFloatChannel.h"
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

    // 获取 Piano (ASkeletalMeshActor) 的 SkeletalMeshComponent
    USkeletalMeshComponent* SkeletalMeshComp =
        KeyRippleActor->Piano->GetSkeletalMeshComponent();

    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano does not have a SkeletalMeshComponent"));
        return;
    }

    // 报告统计数据
    int32 SuccessCount = 0;
    int32 FailureCount = 0;
    TArray<FString> InvalidMaterials;
    TArray<FString> ProcessedMaterials;

    // 获取 SkeletalMeshComponent 上的材质数量
    int32 NumMaterials = SkeletalMeshComp->GetNumMaterials();

    if (NumMaterials == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano skeletal mesh component has no materials"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Found %d materials on Piano"), NumMaterials);

    // 遍历所有材质槽
    for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < NumMaterials;
         ++MaterialSlotIndex) {
        UMaterialInterface* CurrentMaterial =
            SkeletalMeshComp->GetMaterial(MaterialSlotIndex);

        if (!CurrentMaterial) {
            continue;
        }

        FString MaterialName = CurrentMaterial->GetName();
        if (MaterialName.Contains(TEXT("Animated"))) {
            // 已经是动画轨道中的动态材质，跳过
            ProcessedMaterials.Add(MaterialName);
            continue;
        }

        // 尝试提取 number 值，通过 '_' 分割并取最后一个部分
        int32 KeyNumber = -1;
        TArray<FString> NameParts;
        MaterialName.ParseIntoArray(NameParts, TEXT("_"));

        if (NameParts.Num() > 0) {
            FString LastPart = NameParts[NameParts.Num() - 1];
            if (LastPart.IsNumeric()) {
                KeyNumber = FCString::Atoi(*LastPart);
            } else {
                InvalidMaterials.Add(MaterialName);
                FailureCount++;
                continue;
            }
        } else {
            InvalidMaterials.Add(MaterialName);
            UE_LOG(
                LogTemp, Warning,
                TEXT("  ? Material name does not match expected pattern: %s"),
                *MaterialName);
            FailureCount++;
            continue;
        }

        // 根据 MIDI 音符编号判断黑白键
        int32 ModValue = KeyNumber % 12;
        bool bIsBlackKey = (ModValue == 1 || ModValue == 3 || ModValue == 6 ||
                            ModValue == 8 || ModValue == 10);

        UMaterialInterface* ParentMaterial = bIsBlackKey
                                                 ? KeyRippleActor->KeyMatBlack
                                                 : KeyRippleActor->KeyMatWhite;

        if (!ParentMaterial) {
            UE_LOG(LogTemp, Error, TEXT("Parent material is null for key %d"),
                   KeyNumber);
            FailureCount++;
            continue;
        }

        // 构建目标材质名称（无 MID_ 前缀）
        FString TargetMaterialName =
            FString::Printf(TEXT("MAT_Key_%d"), KeyNumber);

        // 检查是否已存在同名材质
        FString ExistingMaterialPath =
            FString::Printf(TEXT("/Game/Materials/%s"), *TargetMaterialName);
        UMaterialInterface* ExistingMaterial =
            LoadObject<UMaterialInterface>(nullptr, *ExistingMaterialPath);

        UMaterialInterface* FinalMaterial = nullptr;

        // 🔧 FIX: 首先检查是否已经在GeneratedPianoMaterials中存在
        FString KeyNumberStr = FString::FromInt(KeyNumber);
        UMaterialInstanceConstant** ExistingMaterialPtr =
            KeyRippleActor->GeneratedPianoMaterials.Find(KeyNumberStr);

        if (ExistingMaterialPtr && *ExistingMaterialPtr) {
            FinalMaterial = *ExistingMaterialPtr;
        } else if (ExistingMaterial) {
            FinalMaterial = ExistingMaterial;
            // 缓存到GeneratedPianoMaterials中
            if (UMaterialInstanceConstant* MaterialInstance =
                    Cast<UMaterialInstanceConstant>(FinalMaterial)) {
                KeyRippleActor->GeneratedPianoMaterials.Add(KeyNumberStr,
                                                            MaterialInstance);
            }
        } else {
            // 🔧 FIX: 创建新材质实例并保存到正确路径
            FString PackagePath = FString::Printf(TEXT("/Game/Materials/%s"),
                                                  *TargetMaterialName);
            UPackage* Package = CreatePackage(*PackagePath);

            if (!Package) {
                UE_LOG(LogTemp, Error,
                       TEXT("Failed to create package for material: %s"),
                       *PackagePath);
                FailureCount++;
                continue;
            }

            // 在正确的包中创建材质实例
            UMaterialInstanceConstant* MatInstance =
                NewObject<UMaterialInstanceConstant>(
                    Package, *TargetMaterialName, RF_Public | RF_Standalone);

            if (!MatInstance) {
                UE_LOG(LogTemp, Error,
                       TEXT("Failed to create material instance for key %d"),
                       KeyNumber);
                FailureCount++;
                continue;
            }

            // 设置父材质
            MatInstance->SetParentEditorOnly(ParentMaterial);

            // 🔧 ADD: 标记包为脏状态
            Package->MarkPackageDirty();

            // 🔧 ADD: 在编辑器中尝试保存材质到磁盘
            FinalMaterial = MatInstance;

            // 🔧 ADD: 缓存到GeneratedPianoMaterials中
            KeyRippleActor->GeneratedPianoMaterials.Add(KeyNumberStr,
                                                        MatInstance);
        }

        if (!FinalMaterial) {
            UE_LOG(LogTemp, Error,
                   TEXT("Failed to get or create material for key %d"),
                   KeyNumber);
            FailureCount++;
            continue;
        }

        // 直接应用材质到 SkeletalMeshComponent
        SkeletalMeshComp->SetMaterial(MaterialSlotIndex, FinalMaterial);

        ProcessedMaterials.Add(MaterialName);
        SuccessCount++;
    }

    SkeletalMeshComp->MarkPackageDirty();

    // 输出完整报告
    UE_LOG(LogTemp, Warning,
           TEXT("========== UpdatePianoMaterials Report =========="));
    UE_LOG(LogTemp, Warning, TEXT("Total materials processed: %d"),
           SuccessCount + FailureCount);
    UE_LOG(LogTemp, Warning, TEXT("Successfully initialized: %d"),
           SuccessCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed to initialize: %d"), FailureCount);

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

    // 更新钢琴材质
    UpdatePianoMaterials(KeyRippleActor);

    // 初始化钢琴键 Control Rig
    InitPianoKeyControlRig(KeyRippleActor);

    // 🔧 NEW: 初始化钢琴材质参数轨道
    InitPianoMaterialParameterTracks(KeyRippleActor);

    UE_LOG(LogTemp, Warning, TEXT("========== InitPiano Completed =========="));
}

void UKeyRipplePianoProcessor::GenerateMorphTargetAnimationInLevelSequencer(
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
           TEXT("========== GenerateMorphTargetAnimationInLevelSequencer "
                "Started =========="));

#if WITH_EDITOR
    //===== Step 1: Read and parse JSON animation file =====
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *PianoKeyAnimationPath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load JSON file: %s"),
               *PianoKeyAnimationPath);
        return;
    }

    TArray<TSharedPtr<FJsonValue>> KeyDataArray;
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonContent);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) ||
        !RootObject.IsValid()) {
        TArray<TSharedPtr<FJsonValue>> RootArray;
        TSharedRef<TJsonReader<>> ArrayReader =
            TJsonReaderFactory<>::Create(JsonContent);
        if (!FJsonSerializer::Deserialize(ArrayReader, RootArray)) {
            UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON"));
            return;
        }
        KeyDataArray = RootArray;
    } else if (RootObject->HasField(TEXT("keys"))) {
        KeyDataArray = RootObject->GetArrayField(TEXT("keys"));
    } else {
        UE_LOG(LogTemp, Error, TEXT("No keys found in JSON"));
        return;
    }

    if (KeyDataArray.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("No key data found"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Loaded %d morph target entries from JSON"),
           KeyDataArray.Num());

    //===== Step 2: Get Level Sequencer and Piano's Control Rig =====
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
        TArray<TWeakPtr<ISequencer>> WeakSequencers =
            FLevelEditorSequencerIntegration::Get().GetSequencers();

        for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
            if (TSharedPtr<ISequencer> CurrentSequencer = WeakSequencer.Pin()) {
                UMovieSceneSequence* RootSequence =
                    CurrentSequencer->GetRootMovieSceneSequence();

                if (!RootSequence) {
                    continue;
                }

                LevelSequence = Cast<ULevelSequence>(RootSequence);
                if (LevelSequence) {
                    Sequencer = CurrentSequencer;
                    break;
                }
            }
        }
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("No Level Sequence is currently open in Sequencer"));
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return;
    }

    //===== Step 3: Get Piano's Control Rig Instance =====
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!UKeyRippleControlRigProcessor::GetControlRigFromSkeletalMeshActor(
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

    //===== Step 5: Find or create Control Rig track =====
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Error, TEXT("Failed to find Control Rig track"));
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
    // processing =====

    // 🔧 NEW APPROACH: 直接删除所有sections而不是逐个清理通道
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

    //===== Step 8: Batch collect all morph target keyframe data =====
    FFrameNumber seqStart, seqEnd;
    TRange<FFrameNumber> seqrange = MovieScene->GetPlaybackRange();
    seqStart = seqrange.GetLowerBoundValue();
    seqEnd = seqrange.GetUpperBoundValue();
    UE_LOG(LogTemp, Warning, TEXT("MovieScene Playback Range: %d - %d"),
           seqStart.Value, seqEnd.Value);

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    UE_LOG(LogTemp, Warning,
           TEXT("Frame Rate - Tick Resolution: %d/%d, Display Rate: %d/%d"),
           TickResolution.Numerator, TickResolution.Denominator,
           DisplayRate.Numerator, DisplayRate.Denominator);

    // 初始化 MinFrame 和 MaxFrame
    FFrameNumber MinFrame(MAX_int32);
    FFrameNumber MaxFrame(MIN_int32);

    bool bHasFrames = false;

    // 修改数据结构：按 MorphTargetName 聚合所有关键帧数据（避免重复）
    // Map: MorphTargetName -> (FrameNumbers, FloatValues)
    TMap<FString, TPair<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>
        MorphTargetKeyframeData;

    int32 TotalSuccess = 0;
    int32 TotalFailure = 0;

    UE_LOG(LogTemp, Warning, TEXT("Processing morph target data from JSON..."));

    for (const TSharedPtr<FJsonValue>& KeyValue : KeyDataArray) {
        TSharedPtr<FJsonObject> KeyObject = KeyValue->AsObject();
        if (!KeyObject.IsValid()) {
            TotalFailure++;
            continue;
        }

        FString MorphTargetName =
            KeyObject->GetStringField(TEXT("shape_key_name"));

        if (MorphTargetName.IsEmpty()) {
            TotalFailure++;
            continue;
        }

        TArray<TSharedPtr<FJsonValue>> Keyframes =
            KeyObject->GetArrayField(TEXT("keyframes"));

        if (Keyframes.Num() == 0) {
            TotalFailure++;
            UE_LOG(LogTemp, Warning,
                   TEXT("  ! Morph target '%s' has no keyframes"),
                   *MorphTargetName);
            continue;
        }

        // 收集该 MorphTarget 的所有关键帧数据
        TArray<FFrameNumber> FrameNumbers;
        TArray<FMovieSceneFloatValue> FrameValues;

        for (const TSharedPtr<FJsonValue>& KeyframeValue : Keyframes) {
            TSharedPtr<FJsonObject> KeyframeObj = KeyframeValue->AsObject();
            if (KeyframeObj.IsValid()) {
                float Frame = KeyframeObj->GetNumberField(TEXT("frame"));

                float Value =
                    KeyframeObj->GetNumberField(TEXT("shape_key_value"));

                // 转换帧数
                float ScaledFrameNumberFloat =
                    Frame * TickResolution.Numerator * DisplayRate.Denominator /
                    (TickResolution.Denominator * DisplayRate.Numerator);
                int32 ScaledFrameNumber =
                    static_cast<int32>(ScaledFrameNumberFloat);

                FFrameNumber FrameNumber(ScaledFrameNumber);

                FrameNumbers.Add(FrameNumber);
                FrameValues.Add(FMovieSceneFloatValue(Value));

                // 跟踪最小/最大帧数
                if (FrameNumber < MinFrame) {
                    MinFrame = FrameNumber;
                }
                if (FrameNumber > MaxFrame) {
                    MaxFrame = FrameNumber;
                }
                bHasFrames = true;
            }
        }

        // 如果已存在，追加新数据而不是替换（学习演奏动画的正确做法）
        if (MorphTargetKeyframeData.Contains(MorphTargetName)) {
            // 追加数据而不是替换 - 这是关键修复
            auto& ExistingPair = MorphTargetKeyframeData[MorphTargetName];

            // 追加新的FrameNumbers和FrameValues
            ExistingPair.Key.Append(FrameNumbers);
            ExistingPair.Value.Append(FrameValues);
        } else {
            // 第一次遇到此 MorphTargetName，添加新条目
            MorphTargetKeyframeData.Add(
                MorphTargetName,
                TPair<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>(
                    FrameNumbers, FrameValues));
            TotalSuccess++;
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Collected %d unique morph targets, %d failed"),
           MorphTargetKeyframeData.Num(), TotalFailure);

    // Log the actual min/max frames we collected
    if (bHasFrames) {
        UE_LOG(LogTemp, Warning, TEXT("Collected Frame Range: %d - %d"),
               MinFrame.Value, MaxFrame.Value);
        UE_LOG(LogTemp, Warning,
               TEXT("Display vs Tick scaling factor: %d/%d * %d/%d = %.2f"),
               TickResolution.Numerator, TickResolution.Denominator,
               DisplayRate.Denominator, DisplayRate.Numerator,
               (float)TickResolution.Numerator * DisplayRate.Denominator /
                   (TickResolution.Denominator * DisplayRate.Numerator));
    }

    //===== Step 8: Insert keyframes into channels =====
    FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();

    // 对每个 Morph Target，一次性获取 FloatChannel 并写入所有数据
    for (const auto& MorphTargetPair : MorphTargetKeyframeData) {
        const FString& MorphTargetName = MorphTargetPair.Key;
        const TArray<FFrameNumber>& FrameNumbers = MorphTargetPair.Value.Key;
        const TArray<FMovieSceneFloatValue>& FrameValues =
            MorphTargetPair.Value.Value;

        // 查找该 MorphTarget 对应的 FloatChannel
        FName ChannelName(*MorphTargetName);

        TMovieSceneChannelHandle<FMovieSceneFloatChannel> ChannelHandle =
            ChannelProxy.GetChannelByName<FMovieSceneFloatChannel>(ChannelName);

        FMovieSceneFloatChannel* FloatChannel = ChannelHandle.Get();

        if (!FloatChannel) {
            UE_LOG(LogTemp, Warning,
                   TEXT("  ! Channel '%s' not found in section, trying to "
                        "enumerate all channels"),
                   *MorphTargetName);

            // Try to find any available float channel
            TArrayView<const FMovieSceneChannelEntry> AllEntries =
                ChannelProxy.GetAllEntries();

            for (const FMovieSceneChannelEntry& Entry : AllEntries) {
                if (Entry.GetChannelTypeName() ==
                    FMovieSceneFloatChannel::StaticStruct()->GetFName()) {
                    TArrayView<FMovieSceneChannel* const> ChannelsInEntry =
                        Entry.GetChannels();

                    UE_LOG(LogTemp, Warning,
                           TEXT("    📊 Found %d float channels in this entry"),
                           ChannelsInEntry.Num());

#if WITH_EDITOR
                    TArrayView<const FMovieSceneChannelMetaData> MetaDataArray =
                        Entry.GetMetaData();

                    for (int32 i = 0; i < ChannelsInEntry.Num(); ++i) {
                        if (i < MetaDataArray.Num()) {
                            const FMovieSceneChannelMetaData& MetaData =
                                MetaDataArray[i];
                            UE_LOG(LogTemp, Warning,
                                   TEXT("      Channel %d: Name='%s'"), i,
                                   *MetaData.Name.ToString());
                            if (MetaData.Name.ToString() == TEXT("Pressed")) {
                                FloatChannel =
                                    static_cast<FMovieSceneFloatChannel*>(
                                        ChannelsInEntry[i]);
                                UE_LOG(LogTemp, Warning,
                                       TEXT("      ✓ Matched 'Pressed'!"));
                                break;
                            }
                        }
                    }
#endif
                }

                if (FloatChannel) {
                    break;
                }
            }
        }

        if (FloatChannel) {
            // 🔧 ADD: 写入前验证数据
            if (FrameNumbers.Num() != FrameValues.Num()) {
                UE_LOG(LogTemp, Error,
                       TEXT("  ❌ MISMATCH: FrameNumbers.Num()=%d != "
                            "FrameValues.Num()=%d for '%s'"),
                       FrameNumbers.Num(), FrameValues.Num(), *MorphTargetName);
                continue;
            }

            // 一次性调用 AddKeys，不再分多次调用
            FloatChannel->AddKeys(FrameNumbers, FrameValues);
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("  ✗ Failed to find channel for morph target '%s'"),
                   *MorphTargetName);
        }
    }

    //===== Step 9: Update section range =====
    if (bHasFrames) {
        // 🔧 FIX: TRange的上边界是exclusive的，需要+1来包含MaxFrame
        Section->SetRange(TRange<FFrameNumber>(MinFrame, MaxFrame + 1));
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
           TEXT("========== Morph Target Animation Report =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully processed %d morph targets"),
           MorphTargetKeyframeData.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== Morph Target Animation Completed =========="));

    //===== Step 11: Generate material parameter animation =====
    UE_LOG(LogTemp, Warning,
           TEXT("========== Step 11: Generating material parameter animation "
                "=========="));
    int32 MaterialAnimationResult = GenerateMaterialAnimationInLevelSequencer(
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
           TEXT("========== GenerateMorphTargetAnimationInLevelSequencer "
                "Completed =========="));
#endif
}

bool UKeyRipplePianoProcessor::GetPianoMorphTargetNames(
    AKeyRippleUnreal* KeyRippleActor, TArray<FString>& OutMorphTargetNames) {
    OutMorphTargetNames.Empty();

    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleActor is null in GetPianoMorphTargetNames"));
        return false;
    }

    if (!KeyRippleActor->Piano) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano is not assigned in GetPianoMorphTargetNames"));
        return false;
    }

    // 获取 Piano 的 SkeletalMeshComponent
    USkeletalMeshComponent* SkeletalMeshComp =
        KeyRippleActor->Piano->GetSkeletalMeshComponent();

    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano does not have a SkeletalMeshComponent in "
                    "GetPianoMorphTargetNames"));
        return false;
    }

    // 获取骨骼网格资产
    USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMeshAsset();
    if (!SkeletalMesh) {
        UE_LOG(LogTemp, Error,
               TEXT("SkeletalMesh is null in GetPianoMorphTargetNames"));
        return false;
    }

    // 获取所有 Morph Target
    if (SkeletalMesh->GetMorphTargets().Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("Piano SkeletalMesh has no morph targets"));
        return false;
    }

    // 提取 Morph Target 名称
    for (const UMorphTarget* MorphTarget : SkeletalMesh->GetMorphTargets()) {
        if (MorphTarget) {
            FString MorphTargetName = MorphTarget->GetName();
            OutMorphTargetNames.Add(MorphTargetName);
        }
    }

    return OutMorphTargetNames.Num() > 0;
}

bool UKeyRipplePianoProcessor::EnsureRootControlExists(
    AKeyRippleUnreal* KeyRippleActor,
    UControlRigBlueprint* ControlRigBlueprint) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRigBlueprint is null in EnsureRootControlExists"));
        return false;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigBlueprint in "
                    "EnsureRootControlExists"));
        return false;
    }

    FRigElementKey RootControlKey(TEXT("piano_key_root"),
                                  ERigElementType::Control);

    // 检查 Root Control 是否已存在
    if (RigHierarchy->Contains(RootControlKey)) {
        UE_LOG(LogTemp, Warning,
               TEXT("Root control 'piano_key_root' already exists"));
        return true;
    }

    // 如果不存在，创建 Root Control
    URigHierarchyController* HierarchyController =
        RigHierarchy->GetController();
    if (!HierarchyController) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy controller in "
                    "EnsureRootControlExists"));
        return false;
    }

    FRigControlSettings RootControlSettings;
    RootControlSettings.ControlType = ERigControlType::Transform;
    RootControlSettings.DisplayName = FName(TEXT("piano_key_root"));
    RootControlSettings.ShapeName = FName(TEXT("Cube"));

    FTransform InitialTransform = FTransform::Identity;
    FRigControlValue InitialValue;
    InitialValue.SetFromTransform(InitialTransform, ERigControlType::Transform,
                                  ERigControlAxis::X);

    FRigElementKey NewRootControlKey = HierarchyController->AddControl(
        FName(TEXT("piano_key_root")), FRigElementKey(), RootControlSettings,
        InitialValue, FTransform::Identity, FTransform::Identity, true, false);

    if (NewRootControlKey.IsValid()) {
        UE_LOG(LogTemp, Warning,
               TEXT("Successfully created root control 'piano_key_root'"));
        return true;
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to create root control 'piano_key_root'"));
        return false;
    }
}

int32 UKeyRipplePianoProcessor::AddAnimationChannelsToRootControl(
    AKeyRippleUnreal* KeyRippleActor, UControlRigBlueprint* ControlRigBlueprint,
    const TArray<FString>& AnimationChannelNames) {
    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("ControlRigBlueprint is null in "
                    "AddAnimationChannelsToRootControl"));
        return 0;
    }

    if (AnimationChannelNames.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("AnimationChannelNames is empty in "
                    "AddAnimationChannelsToRootControl"));
        return 0;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigBlueprint in "
                    "AddAnimationChannelsToRootControl"));
        return 0;
    }

    URigHierarchyController* HierarchyController =
        RigHierarchy->GetController();
    if (!HierarchyController) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy controller in "
                    "AddAnimationChannelsToRootControl"));
        return 0;
    }

    // 获取 Root Control
    FRigElementKey RootControlKey(TEXT("piano_key_root"),
                                  ERigElementType::Control);

    if (!RigHierarchy->Contains(RootControlKey)) {
        UE_LOG(LogTemp, Error,
               TEXT("Root control 'piano_key_root' does not exist in "
                    "AddAnimationChannelsToRootControl"));
        return 0;
    }

    int32 SuccessCount = 0;
    int32 FailureCount = 0;

    UE_LOG(LogTemp, Warning,
           TEXT("Adding %d animation channels to piano_key_root control"),
           AnimationChannelNames.Num());

    // 获取 Root Control 下所有现有的 Animation Channel
    TArray<FRigElementKey> ExistingChannels =
        RigHierarchy->GetAnimationChannels(RootControlKey, true);

    // 对每个 Morph Target 名称创建一个动画通道
    for (const FString& ChannelName : AnimationChannelNames) {
        FName ChannelFName(*ChannelName);

        // 检查该通道是否已存在
        bool bChannelExists = false;
        for (const FRigElementKey& ExistingKey : ExistingChannels) {
            if (ExistingKey.Name == ChannelFName) {
                const FRigControlElement* ControlElement =
                    RigHierarchy->Find<FRigControlElement>(ExistingKey);
                if (ControlElement && ControlElement->IsAnimationChannel()) {
                    bChannelExists = true;
                    break;
                }
            }
        }

        if (bChannelExists) {
            SuccessCount++;
            continue;
        }

        // 为 Root Control 添加 Animation Channel
        FRigControlSettings ChannelSettings;
        ChannelSettings.ControlType = ERigControlType::Float;
        ChannelSettings.DisplayName = ChannelFName;

        FRigElementKey NewChannelKey = HierarchyController->AddAnimationChannel(
            ChannelFName,
            RootControlKey,  // 父 Control 为 piano_key_root
            ChannelSettings,
            true,  // bSetupUndo
            false  // bPrintPythonCommand
        );

        if (NewChannelKey.IsValid()) {
            SuccessCount++;
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("  ✗ Failed to create animation channel '%s'"),
                   *ChannelName);
            FailureCount++;
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== AddAnimationChannels Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully created: %d channels"),
           SuccessCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed to create: %d channels"),
           FailureCount);
    UE_LOG(LogTemp, Warning,
           TEXT("========== AddAnimationChannels Completed =========="));

    return SuccessCount;
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

    if (!UKeyRippleControlRigProcessor::GetControlRigFromSkeletalMeshActor(
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
    if (!EnsureRootControlExists(KeyRippleActor, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to ensure Root Control exists in "
                    "InitPianoKeyControlRig"));
        return;
    }

    // 步骤 3: 在 Root Control 上添加动画通道
    int32 ChannelsAdded = AddAnimationChannelsToRootControl(
        KeyRippleActor, ControlRigBlueprint, MorphTargetNames);

    if (ChannelsAdded == 0) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("No animation channels were added (they may already exist)"));
    }
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitPianoKeyControlRig Completed =========="));
}

bool UKeyRipplePianoProcessor::MaterialHasPressedParameter(
    UMaterialInterface* Material) {
    if (!Material) {
        return false;
    }

    // 尝试转换为材质实例
    UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
    if (MaterialInstance) {
        // 检查标量参数
        TArray<FMaterialParameterInfo> ParameterInfo;
        TArray<FGuid> ParameterIds;
        MaterialInstance->GetAllScalarParameterInfo(ParameterInfo,
                                                    ParameterIds);

        for (const FMaterialParameterInfo& Info : ParameterInfo) {
            if (Info.Name.ToString() == TEXT("Pressed")) {
                return true;
            }
        }
    }

    // 尝试转换为材质实例常量
    UMaterialInstanceConstant* MaterialInstanceConstant =
        Cast<UMaterialInstanceConstant>(Material);
    if (MaterialInstanceConstant) {
        // 检查标量参数
        TArray<FMaterialParameterInfo> ParameterInfo;
        TArray<FGuid> ParameterIds;
        MaterialInstanceConstant->GetAllScalarParameterInfo(ParameterInfo,
                                                            ParameterIds);

        for (const FMaterialParameterInfo& Info : ParameterInfo) {
            if (Info.Name.ToString() == TEXT("Pressed")) {
                UE_LOG(LogTemp, Warning,
                       TEXT("Material '%s' has Pressed parameter"),
                       *Material->GetName());
                return true;
            }
        }
    }

    return false;
}

bool UKeyRipplePianoProcessor::AddMaterialParameterTrackToControlRig(
    ULevelSequence* LevelSequence,
    USkeletalMeshComponent* SkeletalMeshComponent, int32 MaterialSlotIndex,
    const FString& ParameterName, const FGuid& ObjectBindingID) {
    if (!LevelSequence || !SkeletalMeshComponent ||
        !ObjectBindingID.IsValid()) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "Invalid parameters in AddMaterialParameterTrackToControlRig"));
        return false;
    }

    // 获取 MovieScene
    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("Invalid MovieScene"));
        return false;
    }

    // Step 1: 验证材质槽是否存在并获取材质槽名称
    if (MaterialSlotIndex < 0 ||
        MaterialSlotIndex >= SkeletalMeshComponent->GetNumMaterials()) {
        UE_LOG(LogTemp, Error,
               TEXT("Invalid MaterialSlotIndex: %d (NumMaterials: %d)"),
               MaterialSlotIndex, SkeletalMeshComponent->GetNumMaterials());
        return false;
    }

    // 获取材质槽名称（如果有的话）
    FName MaterialSlotName = NAME_None;
    if (USkeletalMesh* SkeletalMesh =
            SkeletalMeshComponent->GetSkeletalMeshAsset()) {
        const TArray<FSkeletalMaterial>& Materials =
            SkeletalMesh->GetMaterials();
        if (MaterialSlotIndex < Materials.Num()) {
            MaterialSlotName = Materials[MaterialSlotIndex].MaterialSlotName;
        }
    }

    // Step 2: 创建或查找 Component Material Track
    UMovieSceneComponentMaterialTrack* MaterialParameterTrack = nullptr;
    TArray<UMovieSceneTrack*> ExistingMaterialTracks = MovieScene->FindTracks(
        UMovieSceneComponentMaterialTrack::StaticClass(), ObjectBindingID);

    // 检查现有的 Component Material Track 是否匹配我们的材质槽
    for (UMovieSceneTrack* Track : ExistingMaterialTracks) {
        UMovieSceneComponentMaterialTrack* ExistingTrack =
            Cast<UMovieSceneComponentMaterialTrack>(Track);

        if (ExistingTrack) {
            const FComponentMaterialInfo& ExistingInfo =
                ExistingTrack->GetMaterialInfo();

            // 匹配材质槽
            bool bMatches = false;
            if (MaterialSlotName != NAME_None &&
                ExistingInfo.MaterialSlotName == MaterialSlotName) {
                bMatches = true;
            } else if (MaterialSlotName == NAME_None &&
                       ExistingInfo.MaterialSlotIndex == MaterialSlotIndex) {
                bMatches = true;
                UE_LOG(LogTemp, Warning,
                       TEXT("Found existing component material track matching "
                            "slot index: %d"),
                       MaterialSlotIndex);
            }

            if (bMatches) {
                MaterialParameterTrack = ExistingTrack;
                break;
            }
        }
    }

    // 如果没有找到匹配的 Component Material Track，创建新的
    if (!MaterialParameterTrack) {
        MaterialParameterTrack =
            Cast<UMovieSceneComponentMaterialTrack>(MovieScene->AddTrack(
                UMovieSceneComponentMaterialTrack::StaticClass(),
                ObjectBindingID));

        if (!MaterialParameterTrack) {
            UE_LOG(LogTemp, Error,
                   TEXT("Failed to create UMovieSceneComponentMaterialTrack"));
            return false;
        }

        // 构建并设置材质信息
        FComponentMaterialInfo MaterialInfo;
        MaterialInfo.MaterialType = EComponentMaterialType::IndexedMaterial;
        MaterialInfo.MaterialSlotIndex = MaterialSlotIndex;
        MaterialInfo.MaterialSlotName = MaterialSlotName;

        MaterialParameterTrack->SetMaterialInfo(MaterialInfo);

        // 使用简化的轨道显示名称：CM_{slot}_{materialslotName}
        FString TrackDisplayName = FString::Printf(
            TEXT("CM_%d_%s"), MaterialSlotIndex,
            MaterialSlotName != NAME_None ? *MaterialSlotName.ToString()
                                          : TEXT("Unnamed"));
        MaterialParameterTrack->SetDisplayName(
            FText::FromString(TrackDisplayName));

        UE_LOG(LogTemp, Warning, TEXT("Created new ComponentMaterialTrack: %s"),
               *TrackDisplayName);
    }

    // Step 3: 检查是否需要为特定参数创建 Section
    TArray<UMovieSceneSection*> ExistingSections =
        MaterialParameterTrack->GetAllSections();
    UMovieSceneComponentMaterialParameterSection* ParameterSection = nullptr;

    // 查找现有的参数部分，检查是否已有此参数
    bool bParameterExists = false;
    for (UMovieSceneSection* Section : ExistingSections) {
        UMovieSceneComponentMaterialParameterSection* MaterialParamSection =
            Cast<UMovieSceneComponentMaterialParameterSection>(Section);

        if (MaterialParamSection) {
            // 检查是否已存在该参数
            for (const FScalarMaterialParameterInfoAndCurve& ExistingParam :
                 MaterialParamSection->ScalarParameterInfosAndCurves) {
                if (ExistingParam.ParameterInfo.Name == FName(*ParameterName)) {
                    bParameterExists = true;
                    ParameterSection = MaterialParamSection;
                    break;
                }
            }

            if (!ParameterSection) {
                ParameterSection =
                    MaterialParamSection;  // 使用第一个找到的section
            }
        }

        if (bParameterExists) {
            break;
        }
    }

    // Step 4: 如果没有找到合适的Section，创建新的
    if (!ParameterSection) {
        UMovieSceneSection* NewSection =
            MaterialParameterTrack->CreateNewSection();
        if (!NewSection) {
            UE_LOG(LogTemp, Error,
                   TEXT("Failed to create new section for material parameter "
                        "track"));
            return false;
        }

        MaterialParameterTrack->AddSection(*NewSection);

        ParameterSection =
            Cast<UMovieSceneComponentMaterialParameterSection>(NewSection);
        if (!ParameterSection) {
            UE_LOG(LogTemp, Error,
                   TEXT("Failed to cast new section to "
                        "UMovieSceneComponentMaterialParameterSection"));
            return false;
        }

        UE_LOG(LogTemp, Warning,
               TEXT("Created new material parameter section"));
    }

    // Step 5: 添加参数（如果还不存在）
    if (!bParameterExists) {
        FMaterialParameterInfo ParameterInfo;
        ParameterInfo.Name = FName(*ParameterName);

        // 添加初始关键帧来创建参数
        FFrameNumber InitialFrame(0);
        ParameterSection->AddScalarParameterKey(
            ParameterInfo, InitialFrame,
            0.0f,      // 初始值
            TEXT(""),  // LayerName
            TEXT(""),  // AssetName
            EMovieSceneKeyInterpolation::Auto);

        UE_LOG(LogTemp, Warning,
               TEXT("Added scalar parameter '%s' with initial keyframe"),
               *ParameterName);
    }

    // Step 6: 设置Section范围为全部（如果需要）
    if (!ParameterSection->GetRange().HasLowerBound() ||
        !ParameterSection->GetRange().HasUpperBound()) {
        ParameterSection->SetRange(TRange<FFrameNumber>::All());
        UE_LOG(LogTemp, Warning, TEXT("Set parameter section range to All"));
    }

    // Step 7: 验证创建结果
    UMaterialInterface* TargetMaterial =
        SkeletalMeshComponent->GetMaterial(MaterialSlotIndex);
    if (TargetMaterial) {
        // 验证参数是否确实存在于材质中
        if (MaterialHasPressedParameter(TargetMaterial)) {
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("Warning: Material may not have '%s' parameter"),
                   *ParameterName);
        }
    }

    return true;
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

    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
        TArray<TWeakPtr<ISequencer>> WeakSequencers =
            FLevelEditorSequencerIntegration::Get().GetSequencers();

        for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
            if (TSharedPtr<ISequencer> CurrentSequencer = WeakSequencer.Pin()) {
                UMovieSceneSequence* RootSequence =
                    CurrentSequencer->GetRootMovieSceneSequence();

                if (!RootSequence) {
                    continue;
                }

                LevelSequence = Cast<ULevelSequence>(RootSequence);
                if (LevelSequence) {
                    Sequencer = CurrentSequencer;
                    break;
                }
            }
        }
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("No Level Sequence is currently open in Sequencer"));
        return 0;
    }

    FGuid PianoObjectBindingID;

    // 获取 piano 的track

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("Invalid MovieScene in LevelSequence"));
        return 0;
    }

    // 修改为明确的 const 引用
    const TArray<FMovieSceneBinding>& Bindings =
        const_cast<const UMovieScene*>(MovieScene)->GetBindings();

    for (const FMovieSceneBinding& Binding : Bindings) {
        // 使用 Sequencer 的 FindBoundObjects 方法（在 ISequencer 中）
        TArray<UObject*> BoundObjects;

        // 通过 Sequencer 查询该 Binding 绑定的对象
        for (TWeakObjectPtr<> BoundObject : Sequencer->FindBoundObjects(
                 Binding.GetObjectGuid(), Sequencer->GetFocusedTemplateID())) {
            if (BoundObject.IsValid() &&
                BoundObject.Get() == KeyRippleActor->Piano) {
                PianoObjectBindingID = Binding.GetObjectGuid();
            }
        }
    }

    // 获取 Piano 的 SkeletalMeshComponent
    USkeletalMeshComponent* SkeletalMeshComp =
        KeyRippleActor->Piano->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano does not have a SkeletalMeshComponent"));
        return 0;
    }

    // 🔧 正确做法：使用 Sequencer->GetHandleToObject 为组件创建绑定
    FGuid SkeletalMeshCompBindingID;

    if (Sequencer.IsValid()) {
        SkeletalMeshCompBindingID = Sequencer->GetHandleToObject(
            SkeletalMeshComp, true /*bCreateHandle*/);

        if (SkeletalMeshCompBindingID.IsValid()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("✅ Got/Created SkeletalMeshComponent binding via "
                        "Sequencer: %s"),
                   *SkeletalMeshCompBindingID.ToString());
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("❌ Failed to get/create binding via "
                        "Sequencer->GetHandleToObject"));
            return 0;
        }
    } else {
        UE_LOG(
            LogTemp, Error,
            TEXT("❌ Sequencer is not valid, cannot create component binding"));
        return 0;
    }

    // 验证 SkeletalMeshCompBindingID 有效性
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
        if (MaterialHasPressedParameter(CurrentMaterial)) {
            if (AddMaterialParameterTrackToControlRig(
                    LevelSequence, SkeletalMeshComp, MaterialSlotIndex,
                    TEXT("Pressed"), SkeletalMeshCompBindingID)) {
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

int32 UKeyRipplePianoProcessor::GenerateMaterialAnimationInLevelSequencer(
    AKeyRippleUnreal* KeyRippleActor, ULevelSequence* LevelSequence,
    const TMap<FString,
               TPair<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>>&
        MorphTargetKeyframeData,
    FFrameNumber MinFrame, FFrameNumber MaxFrame) {
    if (!KeyRippleActor) {
        UE_LOG(LogTemp, Error,
               TEXT("KeyRippleActor is null in "
                    "GenerateMaterialAnimationInLevelSequencer"));
        return 0;
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("LevelSequence is null in "
                    "GenerateMaterialAnimationInLevelSequencer"));
        return 0;
    }

    if (!KeyRippleActor->Piano) {
        UE_LOG(LogTemp, Error,
               TEXT("Piano is not assigned in "
                    "GenerateMaterialAnimationInLevelSequencer"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateMaterialAnimationInLevelSequencer Started "
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

    // 🔧 FIX: 获取 SkeletalMeshComponent 的 ObjectBindingID
    FGuid SkeletalMeshCompBindingID;
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
                    SkeletalMeshCompBindingID = Sequencer->GetHandleToObject(
                        SkeletalMeshComp, false /*bCreateHandle*/);
                    break;
                }
            }
        }
    }

    if (!SkeletalMeshCompBindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get binding ID for SkeletalMeshComponent"));
        return 0;
    }

    int32 SuccessCount = 0;
    int32 FailureCount = 0;
    int32 NumMaterials = SkeletalMeshComp->GetNumMaterials();

    // 🔧 增强数据结构：为每个 MorphTarget 添加轨道名称信息
    TMap<FString, TPair<FString, TPair<TArray<FFrameNumber>,
                                       TArray<FMovieSceneFloatValue>>>>
        MorphTargetKeyframeDataWithTrackName;

    for (const auto& MorphTargetPair : MorphTargetKeyframeData) {
        const FString& MorphTargetName = MorphTargetPair.Key;
        const TArray<FFrameNumber>& FrameNumbers = MorphTargetPair.Value.Key;
        const TArray<FMovieSceneFloatValue>& FrameValues =
            MorphTargetPair.Value.Value;

        // 从 MorphTargetName 提取钢琴键号
        // 格式例如: "key_75_pressed" -> 需要提取 75
        int32 KeyNumber = -1;
        TArray<FString> NameParts;
        MorphTargetName.ParseIntoArray(NameParts, TEXT("_"));

        // 🔧 FIX: 查找第一个数字部分作为键号
        for (int32 i = 0; i < NameParts.Num(); ++i) {
            if (NameParts[i].IsNumeric()) {
                KeyNumber = FCString::Atoi(*NameParts[i]);
                break;
            }
        }

        // 根据钢琴键号查找对应的材质槽名称
        FString MaterialSlotName = TEXT("");
        if (KeyNumber >= 0) {
            if (USkeletalMesh* SkeletalMesh =
                    SkeletalMeshComp->GetSkeletalMeshAsset()) {
                const TArray<FSkeletalMaterial>& Materials =
                    SkeletalMesh->GetMaterials();
                for (int32 i = 0; i < Materials.Num(); ++i) {
                    FString SlotNameStr =
                        Materials[i].MaterialSlotName.ToString();
                    if (SlotNameStr.EndsWith(FString::FromInt(KeyNumber),
                                             ESearchCase::IgnoreCase)) {
                        MaterialSlotName = SlotNameStr;
                        break;
                    }
                }
            }
        }

        if (!MaterialSlotName.IsEmpty()) {
            MorphTargetKeyframeDataWithTrackName.Add(
                MorphTargetName,
                TPair<FString, TPair<TArray<FFrameNumber>,
                                     TArray<FMovieSceneFloatValue>>>(
                    MaterialSlotName,
                    TPair<TArray<FFrameNumber>, TArray<FMovieSceneFloatValue>>(
                        FrameNumbers, FrameValues)));
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("  ❌ MorphTarget '%s' (Key %d) -> NO MATCHING TRACK!"),
                   *MorphTargetName, KeyNumber);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Processing material parameter animation with %d morph targets "
                "mapped to tracks"),
           MorphTargetKeyframeDataWithTrackName.Num());

    // 为每个材质槽处理动画
    for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < NumMaterials;
         ++MaterialSlotIndex) {
        UMaterialInterface* CurrentMaterial =
            SkeletalMeshComp->GetMaterial(MaterialSlotIndex);

        if (!CurrentMaterial) {
            continue;
        }

        // 检查材质是否有 Pressed 参数
        if (!MaterialHasPressedParameter(CurrentMaterial)) {
            continue;
        }

        // 🔧 FIX: 使用 FindTracks 精确查找绑定到 SkeletalMeshComponent 的轨道
        TArray<UMovieSceneTrack*> ExistingMaterialTracks =
            MovieScene->FindTracks(
                UMovieSceneComponentMaterialTrack::StaticClass(),
                SkeletalMeshCompBindingID);

        UMovieSceneComponentMaterialTrack* TargetMaterialTrack = nullptr;

        // 提取当前材质的 SlotName
        FName CurrentMaterialSlotName = NAME_None;
        if (USkeletalMesh* SkeletalMesh =
                SkeletalMeshComp->GetSkeletalMeshAsset()) {
            const TArray<FSkeletalMaterial>& Materials =
                SkeletalMesh->GetMaterials();
            if (MaterialSlotIndex < Materials.Num()) {
                CurrentMaterialSlotName =
                    Materials[MaterialSlotIndex].MaterialSlotName;
            }
        }

        // 在找到的轨道中查找匹配的材质槽
        for (UMovieSceneTrack* Track : ExistingMaterialTracks) {
            UMovieSceneComponentMaterialTrack* MaterialTrack =
                Cast<UMovieSceneComponentMaterialTrack>(Track);

            if (MaterialTrack) {
                const FComponentMaterialInfo& MaterialInfo =
                    MaterialTrack->GetMaterialInfo();

                bool bMatches = false;

                // 首先尝试通过 MaterialSlotName 匹配（支持动态材质）
                if (CurrentMaterialSlotName != NAME_None &&
                    MaterialInfo.MaterialSlotName == CurrentMaterialSlotName) {
                    bMatches = true;
                } else if (MaterialInfo.MaterialSlotIndex ==
                           MaterialSlotIndex) {
                    bMatches = true;
                }

                if (bMatches) {
                    TargetMaterialTrack = MaterialTrack;
                    break;
                }
            }
        }

        if (!TargetMaterialTrack) {
            UE_LOG(LogTemp, Warning, TEXT("    ❌ No matching track found!"));
            FailureCount++;
            continue;
        }

        // 🔧 FIX: 删除现有 Section 并创建新的，以清除原有关键帧
        TArray<UMovieSceneSection*> ExistingSections =
            TargetMaterialTrack->GetAllSections();

        if (ExistingSections.Num() > 0) {
            for (UMovieSceneSection* ExistingSection : ExistingSections) {
                if (ExistingSection) {
                    TargetMaterialTrack->RemoveSection(*ExistingSection);
                }
            }
        }

        // 创建新的 Section
        UMovieSceneSection* NewSection =
            TargetMaterialTrack->CreateNewSection();
        if (!NewSection) {
            FailureCount++;
            continue;
        }

        TargetMaterialTrack->AddSection(*NewSection);

        UMovieSceneComponentMaterialParameterSection* ParameterSection =
            Cast<UMovieSceneComponentMaterialParameterSection>(NewSection);

        if (!ParameterSection) {
            FailureCount++;
            continue;
        }

        // 🔧 FIX: 使用 AddScalarParameterKey() 逐个添加关键帧到 Pressed 参数
        FMaterialParameterInfo PressedParameterInfo;
        PressedParameterInfo.Name = FName(TEXT("Pressed"));

        // 查找该轨道对应的 MorphTarget 数据
        if (USkeletalMesh* SkeletalMesh =
                SkeletalMeshComp->GetSkeletalMeshAsset()) {
            const TArray<FSkeletalMaterial>& Materials =
                SkeletalMesh->GetMaterials();
            if (MaterialSlotIndex < Materials.Num()) {
                CurrentMaterialSlotName =
                    Materials[MaterialSlotIndex].MaterialSlotName;
            }
        }

        FString CurrentTrackNameStr = CurrentMaterialSlotName.ToString();
        int32 KeysWrittenForThisSlot = 0;

        // 遍历 MorphTargetKeyframeDataWithTrackName，找到对应当前轨道的数据
        for (const auto& EnhancedPair : MorphTargetKeyframeDataWithTrackName) {
            const FString& MorphTargetName = EnhancedPair.Key;
            const FString& MaterialSlotNameForThisKey = EnhancedPair.Value.Key;
            const TArray<FFrameNumber>& FrameNumbers =
                EnhancedPair.Value.Value.Key;
            const TArray<FMovieSceneFloatValue>& FrameValues =
                EnhancedPair.Value.Value.Value;

            // 只处理匹配当前轨道的数据
            if (MaterialSlotNameForThisKey != CurrentTrackNameStr) {
                continue;
            }

            if (FrameNumbers.Num() == 0) {
                continue;
            }

            // 验证数据数组匹配
            if (FrameNumbers.Num() != FrameValues.Num()) {
                UE_LOG(LogTemp, Error,
                       TEXT("  ❌ MISMATCH: FrameNumbers.Num()=%d != "
                            "FrameValues.Num()=%d for '%s'"),
                       FrameNumbers.Num(), FrameValues.Num(), *MorphTargetName);
                FailureCount++;
                continue;
            }

            // 🔧 关键修复：逐个写入关键帧到 ParameterSection
            for (int32 i = 0; i < FrameNumbers.Num(); ++i) {
                ParameterSection->AddScalarParameterKey(
                    PressedParameterInfo, FrameNumbers[i], FrameValues[i].Value,
                    TEXT(""),  // LayerName
                    TEXT(""),  // AssetName
                    EMovieSceneKeyInterpolation::Linear);

                KeysWrittenForThisSlot++;
            }

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
           TEXT("========== Material Animation Report =========="));
    UE_LOG(LogTemp, Warning,
           TEXT("Successfully written to: %d material parameter tracks"),
           SuccessCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed: %d material parameter tracks"),
           FailureCount);
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

#undef LOCTEXT_NAMESPACE
