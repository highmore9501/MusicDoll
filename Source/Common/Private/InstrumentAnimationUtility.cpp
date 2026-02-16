#include "InstrumentAnimationUtility.h"

#include "Channels/MovieSceneFloatChannel.h"
#include "Common/Public/InstrumentControlRigUtility.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Sections/MovieSceneComponentMaterialParameterSection.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Tracks/MovieSceneMaterialTrack.h"

// ========== Sequencer 集成 ==========

bool UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
    ULevelSequence*& OutLevelSequence, TSharedPtr<ISequencer>& OutSequencer) {
    OutLevelSequence = nullptr;
    OutSequencer = nullptr;

    if (!FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
        UE_LOG(LogTemp, Error, TEXT("LevelEditor module is not loaded"));
        return false;
    }

    TArray<TWeakPtr<ISequencer>> WeakSequencers =
        FLevelEditorSequencerIntegration::Get().GetSequencers();

    for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
        if (TSharedPtr<ISequencer> CurrentSequencer = WeakSequencer.Pin()) {
            UMovieSceneSequence* RootSequence =
                CurrentSequencer->GetRootMovieSceneSequence();

            if (!RootSequence) {
                continue;
            }

            ULevelSequence* LevelSequence = Cast<ULevelSequence>(RootSequence);
            if (LevelSequence) {
                OutLevelSequence = LevelSequence;
                OutSequencer = CurrentSequencer;
                return true;
            }
        }
    }

    UE_LOG(LogTemp, Error,
           TEXT("No Sequencer is open or no Level Sequence found."));
    return false;
}

// ========== Component Material Track 管理 ==========

UMovieSceneComponentMaterialTrack*
UInstrumentAnimationUtility::FindOrCreateComponentMaterialTrack(
    ULevelSequence* LevelSequence, const FGuid& ObjectBindingID,
    int32 MaterialSlotIndex, FName MaterialSlotName) {
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] LevelSequence is null"));
        return nullptr;
    }

    if (!ObjectBindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] ObjectBindingID is invalid"));
        return nullptr;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] MovieScene is null"));
        return nullptr;
    }

    // 1. 查找现有的Component Material Track
    TArray<UMovieSceneTrack*> ExistingMaterialTracks = MovieScene->FindTracks(
        UMovieSceneComponentMaterialTrack::StaticClass(), ObjectBindingID);

    for (UMovieSceneTrack* Track : ExistingMaterialTracks) {
        UMovieSceneComponentMaterialTrack* MaterialTrack =
            Cast<UMovieSceneComponentMaterialTrack>(Track);

        if (MaterialTrack) {
            const FComponentMaterialInfo& MaterialInfo =
                MaterialTrack->GetMaterialInfo();

            bool bMatches = false;

            // 优先通过MaterialSlotName匹配
            if (MaterialSlotName != NAME_None &&
                MaterialInfo.MaterialSlotName == MaterialSlotName) {
                bMatches = true;
            }
            // 如果没有提供MaterialSlotName，则通过索引匹配
            else if (MaterialSlotName == NAME_None &&
                     MaterialInfo.MaterialSlotIndex == MaterialSlotIndex) {
                bMatches = true;
            }

            if (bMatches) {
                UE_LOG(LogTemp, Log,
                       TEXT("[InstrumentAnimationUtility] Found existing "
                            "ComponentMaterialTrack for slot %d"),
                       MaterialSlotIndex);
                return MaterialTrack;
            }
        }
    }

    // 2. 创建新的Component Material Track
    UMovieSceneComponentMaterialTrack* NewMaterialTrack =
        Cast<UMovieSceneComponentMaterialTrack>(MovieScene->AddTrack(
            UMovieSceneComponentMaterialTrack::StaticClass(), ObjectBindingID));

    if (!NewMaterialTrack) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] Failed to create "
                    "ComponentMaterialTrack"));
        return nullptr;
    }

    // 3. 设置材质信息
    FComponentMaterialInfo MaterialInfo;
    MaterialInfo.MaterialType = EComponentMaterialType::IndexedMaterial;
    MaterialInfo.MaterialSlotIndex = MaterialSlotIndex;
    MaterialInfo.MaterialSlotName = MaterialSlotName;

    NewMaterialTrack->SetMaterialInfo(MaterialInfo);

    // 4. 设置显示名称
    FString TrackDisplayName = FString::Printf(
        TEXT("CM_%d_%s"), MaterialSlotIndex,
        MaterialSlotName != NAME_None ? *MaterialSlotName.ToString()
                                      : TEXT("Unnamed"));

    NewMaterialTrack->SetDisplayName(FText::FromString(TrackDisplayName));

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentAnimationUtility] Created new "
                "ComponentMaterialTrack: %s"),
           *TrackDisplayName);

    return NewMaterialTrack;
}

bool UInstrumentAnimationUtility::AddMaterialParameter(
    UMovieSceneComponentMaterialTrack* Track, const FString& ParameterName,
    float InitialValue) {
    if (!Track) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] Track is null"));
        return false;
    }

    if (ParameterName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] ParameterName is empty"));
        return false;
    }

    // 1. 获取或创建Section
    TArray<UMovieSceneSection*> ExistingSections = Track->GetAllSections();
    UMovieSceneComponentMaterialParameterSection* ParameterSection = nullptr;

    bool bParameterExists = false;

    // 检查现有Section中是否已有此参数
    for (UMovieSceneSection* Section : ExistingSections) {
        UMovieSceneComponentMaterialParameterSection* MaterialParamSection =
            Cast<UMovieSceneComponentMaterialParameterSection>(Section);

        if (MaterialParamSection) {
            // 检查参数是否存在
            for (const FScalarMaterialParameterInfoAndCurve& ExistingParam :
                 MaterialParamSection->ScalarParameterInfosAndCurves) {
                if (ExistingParam.ParameterInfo.Name == FName(*ParameterName)) {
                    bParameterExists = true;
                    ParameterSection = MaterialParamSection;
                    break;
                }
            }

            if (!ParameterSection) {
                ParameterSection = MaterialParamSection;
            }
        }

        if (bParameterExists) {
            break;
        }
    }

    // 2. 如果没有找到Section，创建新的
    if (!ParameterSection) {
        UMovieSceneSection* NewSection = Track->CreateNewSection();
        if (!NewSection) {
            UE_LOG(LogTemp, Error,
                   TEXT("[InstrumentAnimationUtility] Failed to create new "
                        "section"));
            return false;
        }

        Track->AddSection(*NewSection);

        ParameterSection =
            Cast<UMovieSceneComponentMaterialParameterSection>(NewSection);
        if (!ParameterSection) {
            UE_LOG(LogTemp, Error,
                   TEXT("[InstrumentAnimationUtility] Failed to cast section "
                        "to MaterialParameterSection"));
            return false;
        }

        UE_LOG(LogTemp, Log,
               TEXT("[InstrumentAnimationUtility] Created new material "
                    "parameter section"));
    }

    // 3. 添加参数（如果不存在）
    if (!bParameterExists) {
        FMaterialParameterInfo ParameterInfo;
        ParameterInfo.Name = FName(*ParameterName);

        FFrameNumber InitialFrame(0);
        ParameterSection->AddScalarParameterKey(
            ParameterInfo, InitialFrame, InitialValue, TEXT(""), TEXT(""),
            EMovieSceneKeyInterpolation::Auto);

        UE_LOG(LogTemp, Log,
               TEXT("[InstrumentAnimationUtility] Added scalar parameter '%s' "
                    "with initial value %.2f"),
               *ParameterName, InitialValue);
    } else {
        UE_LOG(
            LogTemp, Verbose,
            TEXT("[InstrumentAnimationUtility] Parameter '%s' already exists"),
            *ParameterName);
    }

    // 4. 设置Section范围（如果需要）
    if (!ParameterSection->GetRange().HasLowerBound() ||
        !ParameterSection->GetRange().HasUpperBound()) {
        ParameterSection->SetRange(TRange<FFrameNumber>::All());
    }

    return true;
}

int32 UInstrumentAnimationUtility::WriteMaterialParameterKeyframes(
    UMovieSceneComponentMaterialParameterSection* Section,
    const TArray<FMaterialParameterKeyframeData>& KeyframeData) {
    if (!Section) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] Section is null"));
        return 0;
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentAnimationUtility] KeyframeData is empty"));
        return 0;
    }

    int32 SuccessCount = 0;

    for (const FMaterialParameterKeyframeData& Data : KeyframeData) {
        if (Data.ParameterName.IsEmpty()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentAnimationUtility] Skipping keyframe data "
                        "with empty parameter name"));
            continue;
        }

        if (Data.FrameNumbers.Num() != Data.Values.Num()) {
            UE_LOG(LogTemp, Error,
                   TEXT("[InstrumentAnimationUtility] FrameNumbers and Values "
                        "count mismatch for parameter '%s': %d vs %d"),
                   *Data.ParameterName, Data.FrameNumbers.Num(),
                   Data.Values.Num());
            continue;
        }

        if (Data.FrameNumbers.Num() == 0) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentAnimationUtility] No keyframes to write "
                        "for parameter '%s'"),
                   *Data.ParameterName);
            continue;
        }

        FMaterialParameterInfo ParameterInfo;
        ParameterInfo.Name = FName(*Data.ParameterName);

        // 逐个写入关键帧
        for (int32 i = 0; i < Data.FrameNumbers.Num(); ++i) {
            Section->AddScalarParameterKey(ParameterInfo, Data.FrameNumbers[i],
                                           Data.Values[i], TEXT(""), TEXT(""),
                                           EMovieSceneKeyInterpolation::Linear);
        }

        SuccessCount++;

        UE_LOG(LogTemp, Log,
               TEXT("[InstrumentAnimationUtility] Wrote %d keyframes for "
                    "parameter '%s'"),
               Data.FrameNumbers.Num(), *Data.ParameterName);
    }

    UE_LOG(
        LogTemp, Log,
        TEXT("[InstrumentAnimationUtility] Wrote keyframes for %d parameters"),
        SuccessCount);

    return SuccessCount;
}

// ========== 组件绑定管理 ==========

FGuid UInstrumentAnimationUtility::FindSkeletalMeshActorBinding(
    TSharedPtr<ISequencer> Sequencer, ULevelSequence* LevelSequence,
    ASkeletalMeshActor* SkeletalMeshActor) {
    if (!Sequencer.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] Sequencer is not valid"));
        return FGuid();
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] LevelSequence is null"));
        return FGuid();
    }

    if (!SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] SkeletalMeshActor is null"));
        return FGuid();
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] Invalid MovieScene in "
                    "LevelSequence"));
        return FGuid();
    }

    FGuid FoundBindingID;

    // 遍历所有绑定查找匹配的SkeletalMeshActor
    const TArray<FMovieSceneBinding>& Bindings =
        const_cast<const UMovieScene*>(MovieScene)->GetBindings();

    for (const FMovieSceneBinding& Binding : Bindings) {
        // 使用Sequencer的FindBoundObjects方法查询该Binding绑定的对象
        TArrayView<TWeakObjectPtr<UObject>> BoundObjects =
            Sequencer->FindBoundObjects(Binding.GetObjectGuid(),
                                        Sequencer->GetFocusedTemplateID());

        for (const TWeakObjectPtr<UObject>& BoundObject : BoundObjects) {
            if (BoundObject.IsValid() &&
                BoundObject.Get() == SkeletalMeshActor) {
                FoundBindingID = Binding.GetObjectGuid();
                UE_LOG(LogTemp, Log,
                       TEXT("[InstrumentAnimationUtility] Found "
                            "SkeletalMeshActor binding: %s (GUID: %s)"),
                       *SkeletalMeshActor->GetName(),
                       *FoundBindingID.ToString());
                return FoundBindingID;
            }
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentAnimationUtility] SkeletalMeshActor '%s' not found "
                "in Level Sequence bindings"),
           *SkeletalMeshActor->GetName());

    return FGuid();
}

FGuid UInstrumentAnimationUtility::GetOrCreateComponentBinding(
    TSharedPtr<ISequencer> Sequencer, UActorComponent* Component,
    bool bCreateIfNotFound) {
    if (!Sequencer.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] Sequencer is not valid"));
        return FGuid();
    }

    if (!Component) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] Component is null"));
        return FGuid();
    }

    // 使用ISequencer::GetHandleToObject获取或创建绑定
    FGuid BindingID =
        Sequencer->GetHandleToObject(Component, bCreateIfNotFound);

    if (!BindingID.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] Failed to get/create binding "
                    "for component: %s"),
               *Component->GetName());
        return FGuid();
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentAnimationUtility] Got/Created component binding: "
                "%s (GUID: %s)"),
           *Component->GetName(), *BindingID.ToString());

    return BindingID;
}

// ========== Section 管理 ==========

UMovieSceneSection* UInstrumentAnimationUtility::ResetTrackSections(
    UMovieSceneTrack* Track) {
    if (!Track) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] Track is null"));
        return nullptr;
    }

    // 1. 删除所有现有Sections
    TArray<UMovieSceneSection*> ExistingSections = Track->GetAllSections();

    if (ExistingSections.Num() > 0) {
        UE_LOG(
            LogTemp, Log,
            TEXT(
                "[InstrumentAnimationUtility] Removing %d sections from track"),
            ExistingSections.Num());

        for (UMovieSceneSection* Section : ExistingSections) {
            if (Section) {
                Track->RemoveSection(*Section);
            }
        }
    }

    // 2. 创建新的空Section
    UMovieSceneSection* NewSection = Track->CreateNewSection();
    if (!NewSection) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentAnimationUtility] Failed to create new section"));
        return nullptr;
    }

    Track->AddSection(*NewSection);

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentAnimationUtility] Created new empty section"));

    return NewSection;
}

bool UInstrumentAnimationUtility::CleanupInstrumentAnimationTracks(
    ASkeletalMeshActor* SkeletalMeshActor) {
#if WITH_EDITOR
    if (!SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] SkeletalMeshActor is null in "
                    "CleanupInstrumentAnimationTracks"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentAnimationUtility] ========== Cleanup Animation Tracks Started =========="));

    // 获取当前Level Sequence和Sequencer
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!GetActiveLevelSequenceAndSequencer(LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentAnimationUtility] No active Level Sequence found, skipping cleanup"));
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] MovieScene is null"));
        return false;
    }

    int32 TotalRemovedSections = 0;

    // 1. 清理Control Rig轨道
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
            SkeletalMeshActor, ControlRigInstance, ControlRigBlueprint)) {
        if (ControlRigInstance) {
            UMovieSceneControlRigParameterTrack* ControlRigTrack =
                FControlRigSequencerHelpers::FindControlRigTrack(
                    LevelSequence, ControlRigInstance);

            if (ControlRigTrack) {
                TArray<UMovieSceneSection*> Sections =
                    ControlRigTrack->GetAllSections();
                int32 RemovedCount = 0;

                for (UMovieSceneSection* Section : Sections) {
                    if (Section) {
                        ControlRigTrack->RemoveSection(*Section);
                        RemovedCount++;
                    }
                }

                TotalRemovedSections += RemovedCount;
                UE_LOG(LogTemp, Warning,
                       TEXT("[InstrumentAnimationUtility] Removed %d sections from Control Rig track"),
                       RemovedCount);
            }
        }
    }

    // 2. 清理材质参数轨道
    USkeletalMeshComponent* SkeletalMeshComp =
        SkeletalMeshActor->GetSkeletalMeshComponent();
    if (SkeletalMeshComp) {
        FGuid SkeletalMeshCompBindingID =
            GetOrCreateComponentBinding(Sequencer, SkeletalMeshComp, true);

        if (SkeletalMeshCompBindingID.IsValid()) {
            TArray<UMovieSceneTrack*> MaterialTracks = MovieScene->FindTracks(
                UMovieSceneComponentMaterialTrack::StaticClass(),
                SkeletalMeshCompBindingID);

            int32 RemovedMaterialSections = 0;
            for (UMovieSceneTrack* Track : MaterialTracks) {
                UMovieSceneComponentMaterialTrack* MaterialTrack =
                    Cast<UMovieSceneComponentMaterialTrack>(Track);
                if (MaterialTrack) {
                    TArray<UMovieSceneSection*> Sections =
                        MaterialTrack->GetAllSections();
                    for (UMovieSceneSection* Section : Sections) {
                        if (Section) {
                            MaterialTrack->RemoveSection(*Section);
                            RemovedMaterialSections++;
                        }
                    }
                }
            }

            TotalRemovedSections += RemovedMaterialSections;
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentAnimationUtility] Removed %d sections from material tracks"),
                   RemovedMaterialSections);
        }
    }

    // 3. 标记为已修改
    if (TotalRemovedSections > 0) {
        MovieScene->Modify();
        LevelSequence->MarkPackageDirty();
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentAnimationUtility] ========== Cleanup Completed (Total %d sections removed) =========="),
           TotalRemovedSections);

    return true;
#else
    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentAnimationUtility] Cleanup requires editor support"));
    return false;
#endif
}

// ========== 旋转处理 ==========

void UInstrumentAnimationUtility::UnwrapRotationSequence(
    TArray<FMovieSceneFloatValue>& RotationValues) {
    if (RotationValues.Num() < 2) {
        return;
    }

    for (int32 i = 1; i < RotationValues.Num(); ++i) {
        float PrevAngle = RotationValues[i - 1].Value;
        float CurrAngle = RotationValues[i].Value;

        // 计算最短角度差 (使用Unreal的FindDeltaAngleDegrees)
        float Delta = FMath::FindDeltaAngleDegrees(PrevAngle, CurrAngle);

        // 应用累积的角度，确保连续性
        RotationValues[i].Value = PrevAngle + Delta;
    }
}

void UInstrumentAnimationUtility::ProcessRotationChannelsUnwrap(
    TArray<FMovieSceneFloatValue>& RotationXValues,
    TArray<FMovieSceneFloatValue>& RotationYValues,
    TArray<FMovieSceneFloatValue>& RotationZValues) {
    // 分别处理每个旋转轴
    UnwrapRotationSequence(RotationXValues);
    UnwrapRotationSequence(RotationYValues);
    UnwrapRotationSequence(RotationZValues);
}

// ========== 通道管理 ==========

FMovieSceneFloatChannel* UInstrumentAnimationUtility::FindFloatChannel(
    UMovieSceneSection* Section, const FString& ChannelName) {
    if (!Section) {
        UE_LOG(LogTemp, Warning,
               TEXT("UInstrumentAnimationUtility::FindFloatChannel: Section is "
                    "null"));
        return nullptr;
    }

    FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
    FName ChannelNameAsFName(*ChannelName);

    TMovieSceneChannelHandle<FMovieSceneFloatChannel> ChannelHandle =
        ChannelProxy.GetChannelByName<FMovieSceneFloatChannel>(
            ChannelNameAsFName);

    if (ChannelHandle.Get()) {
        return ChannelHandle.Get();
    }

    UE_LOG(LogTemp, Error,
           TEXT("UInstrumentAnimationUtility::FindFloatChannel: ✗ Failed to "
                "find channel '%s'"),
           *ChannelName);

    // LogAvailableChannels(Section);

    return nullptr;
}

void UInstrumentAnimationUtility::LogAvailableChannels(
    UMovieSceneSection* Section) {
    if (!Section) return;

    FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
    TArrayView<const FMovieSceneChannelEntry> AllEntries =
        ChannelProxy.GetAllEntries();

    UE_LOG(LogTemp, Warning, TEXT("=== Available Channels Debug ==="));
    for (const FMovieSceneChannelEntry& Entry : AllEntries) {
#if WITH_EDITOR
        TArrayView<const FMovieSceneChannelMetaData> MetaDataArray =
            Entry.GetMetaData();
        for (int32 i = 0; i < MetaDataArray.Num(); ++i) {
            const FMovieSceneChannelMetaData& MetaData = MetaDataArray[i];
            UE_LOG(LogTemp, Warning, TEXT("Channel: %s"),
                   *MetaData.Name.ToString());
        }
#endif
    }
}

// ========== 轨道验证 ==========

bool UInstrumentAnimationUtility::ValidateNoExistingTracks(
    ULevelSequence* LevelSequence, UControlRig* ControlRigInstance,
    bool bAutoFix) {
    if (!LevelSequence || !ControlRigInstance) {
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        return false;
    }

    TArray<UMovieSceneTrack*> AllTracks = MovieScene->GetTracks();
    int32 ControlRigTrackCount = 0;

    for (UMovieSceneTrack* Track : AllTracks) {
        if (Track && Track->IsA<UMovieSceneControlRigParameterTrack>()) {
            ControlRigTrackCount++;
        }
    }

    if (ControlRigTrackCount > 1) {
        UE_LOG(LogTemp, Error,
               TEXT("WARNING: Found %d Control Rig Parameter Tracks in the "
                    "sequence. This may cause duplicate corrupted controls. "
                    "Expected only 1."),
               ControlRigTrackCount);

        if (bAutoFix) {
            bool bSkipFirst = true;
            int32 RemovedCount = 0;

            for (UMovieSceneTrack* Track : AllTracks) {
                if (Track &&
                    Track->IsA<UMovieSceneControlRigParameterTrack>()) {
                    if (bSkipFirst) {
                        bSkipFirst = false;
                        continue;
                    }
                    MovieScene->RemoveTrack(*Track);
                    RemovedCount++;
                }
            }

            UE_LOG(LogTemp, Warning,
                   TEXT("Auto-fixed: Removed %d duplicate Control Rig tracks"),
                   RemovedCount);
        }

        return ControlRigTrackCount > 1;
    }

    return false;
}

void UInstrumentAnimationUtility::BatchInsertControlRigKeys(
    ULevelSequence* LevelSequence, UControlRig* ControlRigInstance,
    const TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
    const FBatchInsertKeyframesSettings& Settings) {
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence is null"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error, TEXT("ControlRigInstance is null"));
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error, TEXT("MovieScene is null"));
        return;
    }

    UMovieSceneControlRigParameterTrack* TargetControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!TargetControlRigTrack) {
        UE_LOG(
            LogTemp, Error,
            TEXT("Failed to find ControlRigParameterTrack for ControlRig: %s"),
            *ControlRigInstance->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("Found ControlRigParameterTrack for ControlRig: %s"),
           *ControlRigInstance->GetName());

    TArray<UMovieSceneSection*> Sections =
        TargetControlRigTrack->GetAllSections();
    while (Sections.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("ControlRig Track has no sections"));

        UMovieSceneSection* NewSection =
            TargetControlRigTrack->CreateNewSection();
        if (NewSection) {
            TargetControlRigTrack->AddSection(*NewSection);
            Sections = TargetControlRigTrack->GetAllSections();
        }

        if (Sections.Num() == 0) {
            UE_LOG(LogTemp, Error, TEXT("Failed to create section"));
            return;
        }
    }

    UMovieSceneSection* Section = Sections[0];
    if (!Section) {
        UE_LOG(LogTemp, Error, TEXT("Section is null"));
        return;
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    FFrameNumber MinFrame(MAX_int32);
    FFrameNumber MaxFrame(MIN_int32);

    UE_LOG(LogTemp, Warning, TEXT("[COMMON] ===== FRAME RATE INFO ====="));
    UE_LOG(LogTemp, Warning, TEXT("[COMMON] Tick Resolution: %d/%d = %.4f"),
           TickResolution.Numerator, TickResolution.Denominator,
           (float)TickResolution.Numerator / TickResolution.Denominator);
    UE_LOG(LogTemp, Warning, TEXT("[COMMON] Display Rate: %d/%d = %.4f"),
           DisplayRate.Numerator, DisplayRate.Denominator,
           (float)DisplayRate.Numerator / DisplayRate.Denominator);

    UE_LOG(LogTemp, Warning, TEXT("[COMMON] Total controls to process: %d"),
           ControlKeyframeData.Num());

    for (const auto& ControlPair : ControlKeyframeData) {
        const FString& ControlName = ControlPair.Key;
        const TArray<FAnimationKeyframe>& Keyframes = ControlPair.Value;
        FString Prefix = ControlName + TEXT(".");

        UE_LOG(LogTemp, Warning,
               TEXT("[COMMON] Processing control '%s' with %d keyframes"),
               *ControlName, Keyframes.Num());

        FMovieSceneFloatChannel* LocationX = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sLocation.X"), *Prefix));
        FMovieSceneFloatChannel* LocationY = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sLocation.Y"), *Prefix));
        FMovieSceneFloatChannel* LocationZ = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sLocation.Z"), *Prefix));

        FMovieSceneFloatChannel* RotationX = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sRotation.X"), *Prefix));
        FMovieSceneFloatChannel* RotationY = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sRotation.Y"), *Prefix));
        FMovieSceneFloatChannel* RotationZ = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sRotation.Z"), *Prefix));

        if (!LocationX || !LocationY || !LocationZ || !RotationX ||
            !RotationY || !RotationZ) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Missing channel in control '%s', skipping keyframes"),
                   *ControlName);
            continue;
        }

        TArray<FFrameNumber> Times;
        TArray<FMovieSceneFloatValue> LocationXValues, LocationYValues,
            LocationZValues;
        TArray<FMovieSceneFloatValue> RotationXValues, RotationYValues,
            RotationZValues;

        for (int32 KeyIdx = 0; KeyIdx < Keyframes.Num(); ++KeyIdx) {
            const FAnimationKeyframe& Keyframe = Keyframes[KeyIdx];

            int32 ScaledFrameNumber =
                Keyframe.FrameNumber * TickResolution.Numerator *
                DisplayRate.Denominator /
                (TickResolution.Denominator * DisplayRate.Numerator);

            FFrameNumber FrameNum(ScaledFrameNumber);
            Times.Add(FrameNum);

            if (FrameNum < MinFrame) {
                MinFrame = FrameNum;
            }
            if (FrameNum > MaxFrame) {
                MaxFrame = FrameNum;
            }

            LocationXValues.Add(FMovieSceneFloatValue(Keyframe.Translation.X));
            LocationYValues.Add(FMovieSceneFloatValue(Keyframe.Translation.Y));
            LocationZValues.Add(FMovieSceneFloatValue(Keyframe.Translation.Z));

            FRotator EulerRotation = Keyframe.Rotation.Rotator();
            RotationXValues.Add(FMovieSceneFloatValue(EulerRotation.Roll));
            RotationYValues.Add(FMovieSceneFloatValue(EulerRotation.Pitch));
            RotationZValues.Add(FMovieSceneFloatValue(EulerRotation.Yaw));
        }

        if (Settings.bUnwrapRotationInterpolation) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[COMMON] Control '%s': Starting rotation unwrap "
                        "processing"),
                   *ControlName);
            ProcessRotationChannelsUnwrap(RotationXValues, RotationYValues,
                                          RotationZValues);
            UE_LOG(LogTemp, Warning,
                   TEXT("[COMMON] Control '%s': Rotation unwrap processing "
                        "completed"),
                   *ControlName);
        }

        // Check for special controller handling
        bool bIsSpecialControl = false;
        bool bOnlyInsertXAxis = false;

        for (const auto& SpecialRule : Settings.SpecialControllerRules) {
            if (ControlName.Contains(SpecialRule.Key,
                                     ESearchCase::IgnoreCase)) {
                bIsSpecialControl = true;
                bOnlyInsertXAxis = SpecialRule.Value;
                break;
            }
        }

        if (bIsSpecialControl && bOnlyInsertXAxis) {
            LocationX->AddKeys(Times, LocationXValues);
            UE_LOG(
                LogTemp, Warning,
                TEXT("[COMMON] Special control '%s': Only X-axis keys added"),
                *ControlName);
        } else {
            LocationX->AddKeys(Times, LocationXValues);
            LocationY->AddKeys(Times, LocationYValues);
            LocationZ->AddKeys(Times, LocationZValues);

            RotationX->AddKeys(Times, RotationXValues);
            RotationY->AddKeys(Times, RotationYValues);
            RotationZ->AddKeys(Times, RotationZValues);
        }

        UE_LOG(LogTemp, Warning,
               TEXT("[COMMON] Control '%s': Keys added successfully"),
               *ControlName);
    }

    if (MinFrame != MAX_int32 && MaxFrame != MIN_int32 &&
        MinFrame <= MaxFrame) {
        Section->SetRange(
            TRange<FFrameNumber>(MinFrame, MaxFrame + Settings.FramePadding));
        UE_LOG(LogTemp, Warning, TEXT("[COMMON] Set section range to %d - %d"),
               MinFrame.Value, (MaxFrame + Settings.FramePadding).Value);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("[COMMON] Warning: Invalid frame range. MinFrame=%d, "
                    "MaxFrame=%d"),
               MinFrame.Value, MaxFrame.Value);
    }

    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();
#if WITH_EDITOR
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif
    UE_LOG(LogTemp, Warning,
           TEXT("[COMMON] Batch keyframe insertion finished."));
}

// ========== 关键帧清理 ==========

void UInstrumentAnimationUtility::ClearControlRigKeyframes(
    ULevelSequence* LevelSequence, UControlRig* ControlRigInstance,
    const TSet<FString>& ControlNamesToClean) {
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence is null"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error, TEXT("ControlRigInstance is null"));
        return;
    }

    UMovieSceneControlRigParameterTrack* TargetTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!TargetTrack) {
        UE_LOG(LogTemp, Warning,
               TEXT("ControlRig %s is not bound to any track in the sequence"),
               *ControlRigInstance->GetName());
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[COMMON] Identified %d control names to clean from animation "
                "tracks"),
           ControlNamesToClean.Num());

    TArray<UMovieSceneSection*> AllSections = TargetTrack->GetAllSections();

    if (AllSections.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[COMMON] ControlRig Track has no sections"));
        return;
    }

    int32 ClearedChannelsCount = 0;

    for (UMovieSceneSection* Section : AllSections) {
        if (!Section) {
            continue;
        }

        for (const FString& ControlName : ControlNamesToClean) {
            FString Prefix = ControlName + TEXT(".");

            FMovieSceneFloatChannel* LocationX = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sLocation.X"), *Prefix));
            FMovieSceneFloatChannel* LocationY = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sLocation.Y"), *Prefix));
            FMovieSceneFloatChannel* LocationZ = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sLocation.Z"), *Prefix));

            FMovieSceneFloatChannel* RotationX = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sRotation.X"), *Prefix));
            FMovieSceneFloatChannel* RotationY = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sRotation.Y"), *Prefix));
            FMovieSceneFloatChannel* RotationZ = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sRotation.Z"), *Prefix));

            if (LocationX) {
                LocationX->Reset();
                ClearedChannelsCount++;
            }
            if (LocationY) {
                LocationY->Reset();
                ClearedChannelsCount++;
            }
            if (LocationZ) {
                LocationZ->Reset();
                ClearedChannelsCount++;
            }

            if (RotationX) {
                RotationX->Reset();
                ClearedChannelsCount++;
            }
            if (RotationY) {
                RotationY->Reset();
                ClearedChannelsCount++;
            }
            if (RotationZ) {
                RotationZ->Reset();
                ClearedChannelsCount++;
            }
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[COMMON] Cleared %d channels from Control Rig track"),
           ClearedChannelsCount);

    LevelSequence->MarkPackageDirty();

    UE_LOG(
        LogTemp, Warning,
        TEXT("[COMMON] Control Rig keyframes cleared for specified controls"));
}

// ========== 控制器验证 ==========

FString UInstrumentAnimationUtility::ValidateControllerName(
    const FString& ControlName, const TSet<FString>& ValidNames,
    const FString& ErrorLogPrefix) {
    if (ValidNames.Contains(ControlName)) {
        return ControlName;
    }

    if (!ErrorLogPrefix.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("[%s] INVALID CONTROLLER: '%s'"),
               *ErrorLogPrefix, *ControlName);
    } else {
        UE_LOG(LogTemp, Error, TEXT("INVALID CONTROLLER: '%s'"), *ControlName);
    }

    return FString();
}

// ========== JSON 控件容器处理 ==========

void UInstrumentAnimationUtility::ExtractRotationData(
    TSharedPtr<FJsonObject> ControlsContainer,
    TMap<FString, FRotationData>& OutRotations) {
    if (!ControlsContainer.IsValid()) {
        return;
    }

    // 检查 H_rotation_L
    if (ControlsContainer->HasField(TEXT("H_rotation_L"))) {
        TSharedPtr<FJsonValue> RotationDataValue =
            ControlsContainer->TryGetField(TEXT("H_rotation_L"));
        if (RotationDataValue.IsValid()) {
            TArray<TSharedPtr<FJsonValue>> DataArray =
                RotationDataValue->AsArray();
            if (DataArray.Num() == 4) {
                FQuat Rotation;
                Rotation.W = DataArray[0]->AsNumber();
                Rotation.X = DataArray[1]->AsNumber();
                Rotation.Y = DataArray[2]->AsNumber();
                Rotation.Z = DataArray[3]->AsNumber();
                OutRotations.Add(TEXT("H_L"), FRotationData(Rotation, true));
            }
        }
    }

    // 检查 H_rotation_R
    if (ControlsContainer->HasField(TEXT("H_rotation_R"))) {
        TSharedPtr<FJsonValue> RotationDataValue =
            ControlsContainer->TryGetField(TEXT("H_rotation_R"));
        if (RotationDataValue.IsValid()) {
            TArray<TSharedPtr<FJsonValue>> DataArray =
                RotationDataValue->AsArray();
            if (DataArray.Num() == 4) {
                FQuat Rotation;
                Rotation.W = DataArray[0]->AsNumber();
                Rotation.X = DataArray[1]->AsNumber();
                Rotation.Y = DataArray[2]->AsNumber();
                Rotation.Z = DataArray[3]->AsNumber();
                OutRotations.Add(TEXT("H_R"), FRotationData(Rotation, true));
            }
        }
    }
}

void UInstrumentAnimationUtility::ProcessControlsContainer(
    TSharedPtr<FJsonObject> ControlsContainer, int32 FrameNumber,
    TMap<FString, TArray<FAnimationKeyframe>>& ControlKeyframeData,
    const TSet<FString>& ValidControllerNames, int32& OutKeyframesAdded) {
    if (!ControlsContainer.IsValid()) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("ProcessControlsContainer: Controls container is not valid"));
        return;
    }

    // 第一步：提前提取旋转数据
    TMap<FString, FRotationData> RotationDataMap;
    ExtractRotationData(ControlsContainer, RotationDataMap);

    // 第二步：遍历每个控制器的数据
    for (const auto& Pair : ControlsContainer->Values) {
        FString RawControlName = Pair.Key;

        // 跳过旋转控制器（已在上面提取）
        if (RawControlName == TEXT("H_rotation_L") ||
            RawControlName == TEXT("H_rotation_R")) {
            continue;
        }

        // 验证控制器名称
        FString ControlName = ValidateControllerName(
            RawControlName, ValidControllerNames, TEXT("Common"));

        if (ControlName.IsEmpty()) {
            continue;
        }

        TSharedPtr<FJsonValue> ControlDataValue = Pair.Value;

        if (!ControlDataValue.IsValid()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Frame %d control %s has invalid data"), FrameNumber,
                   *ControlName);
            continue;
        }

        TArray<TSharedPtr<FJsonValue>> DataArray = ControlDataValue->AsArray();

        if (DataArray.Num() == 0) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Frame %d control %s has empty data array"),
                   FrameNumber, *ControlName);
            continue;
        }

        if (DataArray.Num() == 3) {
            // 3维数据 - 位置
            FVector Location;
            Location.X = DataArray[0]->AsNumber();
            Location.Y = DataArray[1]->AsNumber();
            Location.Z = DataArray[2]->AsNumber();

            FAnimationKeyframe Keyframe;
            Keyframe.FrameNumber = FrameNumber;
            Keyframe.Translation = Location;

            // 尝试使用提前提取的旋转数据
            if (FRotationData* RotationData =
                    RotationDataMap.Find(ControlName)) {
                if (RotationData->bIsValid) {
                    Keyframe.Rotation = RotationData->Rotation;
                } else {
                    Keyframe.Rotation = FQuat::Identity;
                }
            } else {
                Keyframe.Rotation = FQuat::Identity;
            }

            ControlKeyframeData.FindOrAdd(ControlName).Add(Keyframe);
        } else if (DataArray.Num() == 4) {
            // 4维数据 - 旋转
            FQuat Rotation;
            Rotation.W = DataArray[0]->AsNumber();
            Rotation.X = DataArray[1]->AsNumber();
            Rotation.Y = DataArray[2]->AsNumber();
            Rotation.Z = DataArray[3]->AsNumber();

            FAnimationKeyframe Keyframe;
            Keyframe.FrameNumber = FrameNumber;
            Keyframe.Translation = FVector::ZeroVector;
            Keyframe.Rotation = Rotation;

            ControlKeyframeData.FindOrAdd(ControlName).Add(Keyframe);
        } else {
            UE_LOG(
                LogTemp, Warning,
                TEXT("Frame %d control %s has unexpected data dimension: %d"),
                FrameNumber, *ControlName, DataArray.Num());
            continue;
        }

        OutKeyframesAdded++;
    }
}
