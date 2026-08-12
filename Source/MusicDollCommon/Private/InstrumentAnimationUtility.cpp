#include "InstrumentAnimationUtility.h"

#include "Animation/SkeletalMeshActor.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EngineUtils.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "InstrumentControlRigUtility.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Misc/FileHelper.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Sections/MovieSceneComponentMaterialParameterSection.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tracks/MovieSceneMaterialTrack.h"

// ========== Sequencer 集成 ==========

bool UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
    ULevelSequence*& OutLevelSequence, TSharedPtr<ISequencer>& OutSequencer) {
    OutLevelSequence = nullptr;
    OutSequencer = nullptr;

    // ------------------ 编辑器模式 -------------------
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

                ULevelSequence* LevelSequence =
                    Cast<ULevelSequence>(RootSequence);
                if (LevelSequence) {
                    OutLevelSequence = LevelSequence;
                    OutSequencer = CurrentSequencer;
                    return true;
                }
            }
        }
    }

    // ------------------ 渲染模式 -------------------
    if (GEditor) {
        UMoviePipelineQueueSubsystem* PipelineSubsystem =
            GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
        if (PipelineSubsystem) {
            UMoviePipelineQueue* Queue = PipelineSubsystem->GetQueue();
            if (Queue) {
                const TArray<UMoviePipelineExecutorJob*>& Jobs =
                    Queue->GetJobs();
                if (Jobs.Num() > 0) {
                    UMoviePipelineExecutorJob* Job = Jobs[0];
                    if (Job) {
                        // 通过软引用获取LevelSequence
                        const FSoftObjectPath& SequencePath = Job->Sequence;
                        UObject* SequenceObj = SequencePath.ResolveObject();
                        if (!SequenceObj) {
                            SequenceObj = SequencePath.TryLoad();
                        }
                        ULevelSequence* LevelSequence =
                            Cast<ULevelSequence>(SequenceObj);
                        if (LevelSequence) {
                            OutLevelSequence = LevelSequence;
                            OutSequencer = nullptr;  // 运行时没有Sequencer实例
                            return true;
                        }
                    }
                }
            }
        }
    }

    // 没找到活动序列
    UE_LOG(LogTemp, Warning,
           TEXT("No active Level Sequence found in editor or render queue."));
    return false;
}

ULevelSequence* UInstrumentAnimationUtility::GetCurrentLevelSequence() {
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (GetActiveLevelSequenceAndSequencer(LevelSequence, Sequencer)) {
        return LevelSequence;
    }

    return nullptr;
}

ASkeletalMeshActor*
UInstrumentAnimationUtility::FindSkeletalMeshActorFromComponent(
    USkeletalMeshComponent* SkeletalMeshComponent) {
    if (!SkeletalMeshComponent) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentAnimationUtility] SkeletalMeshComponent is null"));
        return nullptr;
    }

    // 获取组件所属的Actor
    AActor* OwnerActor = SkeletalMeshComponent->GetOwner();
    if (!OwnerActor) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("[InstrumentAnimationUtility] Component has no owner actor"));
        return nullptr;
    }

    // 检查Owner是否为SkeletalMeshActor
    ASkeletalMeshActor* SkeletalMeshActor =
        Cast<ASkeletalMeshActor>(OwnerActor);
    if (SkeletalMeshActor) {
        // 验证组件确实是该Actor的SkeletalMeshComponent
        if (SkeletalMeshActor->GetSkeletalMeshComponent() ==
            SkeletalMeshComponent) {
            UE_LOG(LogTemp, Log,
                   TEXT("[InstrumentAnimationUtility] Found SkeletalMeshActor: "
                        "%s for component: %s"),
                   *SkeletalMeshActor->GetName(),
                   *SkeletalMeshComponent->GetName());
            return SkeletalMeshActor;
        }
    }

    // 如果直接Cast失败，遍历场景中所有SkeletalMeshActor进行查找
    UWorld* World = SkeletalMeshComponent->GetWorld();
    if (World) {
        for (TActorIterator<ASkeletalMeshActor> ActorItr(World); ActorItr;
             ++ActorItr) {
            ASkeletalMeshActor* TestActor = *ActorItr;
            if (TestActor && TestActor->GetSkeletalMeshComponent() ==
                                 SkeletalMeshComponent) {
                UE_LOG(LogTemp, Log,
                       TEXT("[InstrumentAnimationUtility] Found "
                            "SkeletalMeshActor by scene search: %s"),
                       *TestActor->GetName());
                return TestActor;
            }
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentAnimationUtility] No SkeletalMeshActor found for "
                "component: %s"),
           *SkeletalMeshComponent->GetName());
    return nullptr;
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
            EMovieSceneKeyInterpolation::Constant);

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
            Section->AddScalarParameterKey(
                ParameterInfo, Data.FrameNumbers[i], Data.Values[i], TEXT(""),
                TEXT(""), EMovieSceneKeyInterpolation::Constant);
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

// ========== 辅助方法：处理材质参数关键帧写入后的同步操作 ==========

void UInstrumentAnimationUtility::SyncMaterialParameterKeyframesAfterWrite(
    UMovieSceneComponentMaterialParameterSection* Section,
    UMovieSceneComponentMaterialTrack* Track, ULevelSequence* LevelSequence) {
#if WITH_EDITOR
    if (!Section || !Track || !LevelSequence) {
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        return;
    }

    // 对 Section、Track 和 MovieScene 都调用 Modify 确保更改被追踪
    Section->Modify();
    Track->Modify();
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

    // 先刷新序列，再通知数据变更，最后再刷新 UI
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();

    {
        TSharedPtr<ISequencer> ActiveSequencer = nullptr;
        ULevelSequence* ActiveLevelSequence = nullptr;
        if (GetActiveLevelSequenceAndSequencer(ActiveLevelSequence,
                                               ActiveSequencer)) {
            if (ActiveSequencer.IsValid() &&
                ActiveLevelSequence == LevelSequence) {
                // MovieSceneStructureItemsChanged 会触发完整的评估模板重建
                ActiveSequencer->NotifyMovieSceneDataChanged(
                    EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
                // 强制在当前时间重新评估，确保视口立即反映变更
                ActiveSequencer->ForceEvaluate();
                UE_LOG(
                    LogTemp, Warning,
                    TEXT("[InstrumentAnimationUtility] Notified sequencer of "
                         "data change to trigger template recompilation"));
            }
        }
    }

    // 刷新 UI 以显示更新
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif
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
    ASkeletalMeshActor* SkeletalMeshActor,
    const TArray<FString>& ControlSectionNamesToKeep) {
#if WITH_EDITOR
    if (!SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] SkeletalMeshActor is null in "
                    "CleanupInstrumentAnimationTracks"));
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentAnimationUtility] ========== Cleanup Animation "
                "Tracks Started =========="));

    // 获取当前Level Sequence和Sequencer
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;

    if (!GetActiveLevelSequenceAndSequencer(LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentAnimationUtility] No active Level Sequence "
                    "found, skipping cleanup"));
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
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] GEngine is not available"));
        return false;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] ControlRig Cache Subsystem "
                    "is not available"));
        return false;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(SkeletalMeshActor, LevelSequence);
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(SkeletalMeshActor,
                                               LevelSequence);

    if (ControlRigInstance) {
        UMovieSceneControlRigParameterTrack* ControlRigTrack =
            FControlRigSequencerHelpers::FindControlRigTrack(
                LevelSequence, ControlRigInstance);

        if (ControlRigTrack) {
            TArray<UMovieSceneSection*> Sections =
                ControlRigTrack->GetAllSections();
            int32 RemovedCount = 0;

            for (UMovieSceneSection* Section : Sections) {
                if (!Section) {
                    continue;
                }

                const FString SectionName = Section->GetName();
                if (ControlSectionNamesToKeep.Contains(SectionName)) {
                    continue;
                }

                ControlRigTrack->RemoveSection(*Section);
                RemovedCount++;
            }

            TotalRemovedSections += RemovedCount;
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentAnimationUtility] Removed %d sections from "
                        "Control Rig track"),
                   RemovedCount);

            // After removing all sections, create a new empty section to keep
            // the track in a valid state. A ControlRig track with zero sections
            // causes crashes in Anim Outliner when Sequencer tries to read the
            // ChannelProxy (e.g. after save/reload).
            if (RemovedCount > 0 &&
                ControlRigTrack->GetAllSections().Num() == 0) {
                UMovieSceneSection* NewSection =
                    ControlRigTrack->CreateNewSection();
                if (NewSection) {
                    ControlRigTrack->AddSection(*NewSection);
                    UE_LOG(
                        LogTemp, Warning,
                        TEXT("[InstrumentAnimationUtility] Created new empty "
                             "section for Control Rig track to prevent "
                             "invalid state"));
                }
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
                   TEXT("[InstrumentAnimationUtility] Removed %d sections from "
                        "material tracks"),
                   RemovedMaterialSections);
        }
    }

    // 3. 标记为已修改
    if (TotalRemovedSections > 0) {
        MovieScene->Modify();
        LevelSequence->MarkPackageDirty();
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentAnimationUtility] ========== Cleanup Completed "
                "(Total %d sections removed) =========="),
           TotalRemovedSections);

    return true;
#else
    UE_LOG(
        LogTemp, Warning,
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
        UE_LOG(LogTemp, Warning,
               TEXT("ControlRigParameterTrack not found for ControlRig: %s, "
                    "attempting to create one"),
               *ControlRigInstance->GetName());

        // 尝试创建新的 Control Rig Parameter Track
        TargetControlRigTrack =
            MovieScene->AddTrack<UMovieSceneControlRigParameterTrack>();
        if (TargetControlRigTrack) {
            TargetControlRigTrack->Modify();
            // 使用 CreateControlRigSection 关联 ControlRig 并创建 Section
            TargetControlRigTrack->CreateControlRigSection(
                FFrameNumber(0), ControlRigInstance, false);
            UE_LOG(LogTemp, Warning,
                   TEXT("Successfully created new ControlRigParameterTrack for "
                        "ControlRig: %s"),
                   *ControlRigInstance->GetName());
        } else {
            UE_LOG(LogTemp, Error,
                   TEXT("Failed to create ControlRigParameterTrack for "
                        "ControlRig: %s"),
                   *ControlRigInstance->GetName());
            return;
        }
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

    // 自动清理：在写入前清除本次要写入的控制器通道的旧关键帧
    // 这样调用方无需再单独调用 ClearControlRigKeyframes
    // 清除后会将通道默认值设为 Control 的初始（rest pose）值，而非 0
    {
        int32 AutoClearedCount = 0;

        // 获取 ControlRig Hierarchy 以读取各 control 的初始值
        URigHierarchy* RigHierarchy =
            ControlRigInstance ? ControlRigInstance->GetHierarchy() : nullptr;

        for (const auto& ControlPair : ControlKeyframeData) {
            const FString& ControlName = ControlPair.Key;
            FString Prefix = ControlName + TEXT(".");

            FMovieSceneFloatChannel* LocX = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sLocation.X"), *Prefix));
            FMovieSceneFloatChannel* LocY = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sLocation.Y"), *Prefix));
            FMovieSceneFloatChannel* LocZ = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sLocation.Z"), *Prefix));
            FMovieSceneFloatChannel* RotX = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sRotation.X"), *Prefix));
            FMovieSceneFloatChannel* RotY = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sRotation.Y"), *Prefix));
            FMovieSceneFloatChannel* RotZ = FindFloatChannel(
                Section, *FString::Printf(TEXT("%sRotation.Z"), *Prefix));

            // 读取该 control 的初始（rest pose）transform
            FVector InitialLoc = FVector::ZeroVector;
            FRotator InitialRot = FRotator::ZeroRotator;
            if (RigHierarchy) {
                FRigElementKey ControlKey(*ControlName,
                                          ERigElementType::Control);
                if (RigHierarchy->Contains(ControlKey)) {
                    FRigControlElement* ControlElement =
                        RigHierarchy->Find<FRigControlElement>(ControlKey);
                    if (ControlElement) {
                        FRigControlValue InitialValue =
                            RigHierarchy->GetControlValue(
                                ControlElement, ERigControlValueType::Initial);
                        FTransform InitialTransform =
                            InitialValue.GetAsTransform(
                                ControlElement->Settings.ControlType,
                                ControlElement->Settings.PrimaryAxis);
                        InitialLoc = InitialTransform.GetLocation();
                        InitialRot = InitialTransform.GetRotation().Rotator();
                    }
                }
            }

            // Reset 后设置默认值为 control 的初始值，而非 0
            if (LocX) {
                LocX->Reset();
                LocX->SetDefault(InitialLoc.X);
                AutoClearedCount++;
            }
            if (LocY) {
                LocY->Reset();
                LocY->SetDefault(InitialLoc.Y);
                AutoClearedCount++;
            }
            if (LocZ) {
                LocZ->Reset();
                LocZ->SetDefault(InitialLoc.Z);
                AutoClearedCount++;
            }
            if (RotX) {
                RotX->Reset();
                RotX->SetDefault(InitialRot.Roll);
                AutoClearedCount++;
            }
            if (RotY) {
                RotY->Reset();
                RotY->SetDefault(InitialRot.Pitch);
                AutoClearedCount++;
            }
            if (RotZ) {
                RotZ->Reset();
                RotZ->SetDefault(InitialRot.Yaw);
                AutoClearedCount++;
            }
        }
        UE_LOG(LogTemp, Log,
               TEXT("[COMMON] BatchInsert: Auto-cleared %d channels for %d "
                    "controllers before writing, defaults set to rest pose"),
               AutoClearedCount, ControlKeyframeData.Num());
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

        TArray<FFrameNumber> LocationTimes;
        TArray<FFrameNumber> RotationTimes;
        TArray<FMovieSceneFloatValue> LocationXValues, LocationYValues,
            LocationZValues;
        TArray<FMovieSceneFloatValue> RotationXValues, RotationYValues,
            RotationZValues;

        // 四元数符号一致性处理（hemisphere alignment）：
        // 确保相邻帧的四元数在同一半球上，避免 q/-q 歧义导致欧拉角跳变。
        // q 和 -q 表示相同旋转，但转换为欧拉角后可能差异巨大。
        // 构建已对齐的四元数序列副本，不修改原始数据。
        TArray<FQuat> AlignedQuaternions;
        AlignedQuaternions.Reserve(Keyframes.Num());
        for (int32 AlignIdx = 0; AlignIdx < Keyframes.Num(); ++AlignIdx) {
            FQuat CurrentQuat = Keyframes[AlignIdx].Rotation;
            if (AlignIdx > 0) {
                const FQuat& PrevQuat = AlignedQuaternions[AlignIdx - 1];
                if ((PrevQuat | CurrentQuat) < 0.0f) {
                    CurrentQuat = FQuat(-CurrentQuat.X, -CurrentQuat.Y,
                                        -CurrentQuat.Z, -CurrentQuat.W);
                }
            }
            AlignedQuaternions.Add(CurrentQuat);
        }

        for (int32 KeyIdx = 0; KeyIdx < Keyframes.Num(); ++KeyIdx) {
            const FAnimationKeyframe& Keyframe = Keyframes[KeyIdx];

            int32 ScaledFrameNumber =
                Keyframe.FrameNumber * TickResolution.Numerator *
                DisplayRate.Denominator /
                (TickResolution.Denominator * DisplayRate.Numerator);

            FFrameNumber FrameNum(ScaledFrameNumber);

            // ✅ 只收集有位置数据的关键帧时间点和值
            if (Keyframe.bHasLocation) {
                LocationTimes.Add(FrameNum);

                if (FrameNum < MinFrame) {
                    MinFrame = FrameNum;
                }
                if (FrameNum > MaxFrame) {
                    MaxFrame = FrameNum;
                }

                LocationXValues.Add(
                    FMovieSceneFloatValue(Keyframe.Translation.X));
                LocationYValues.Add(
                    FMovieSceneFloatValue(Keyframe.Translation.Y));
                LocationZValues.Add(
                    FMovieSceneFloatValue(Keyframe.Translation.Z));
            }

            // ✅ 只收集有旋转数据的关键帧时间点和值
            if (Keyframe.bHasRotation) {
                RotationTimes.Add(FrameNum);

                if (FrameNum < MinFrame) {
                    MinFrame = FrameNum;
                }
                if (FrameNum > MaxFrame) {
                    MaxFrame = FrameNum;
                }

                FRotator EulerRotation = AlignedQuaternions[KeyIdx].Rotator();
                RotationXValues.Add(FMovieSceneFloatValue(EulerRotation.Roll));
                RotationYValues.Add(FMovieSceneFloatValue(EulerRotation.Pitch));
                RotationZValues.Add(FMovieSceneFloatValue(EulerRotation.Yaw));
            }
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
        ESpecialAxisMode SpecialAxisMode = ESpecialAxisMode::X;

        for (const auto& SpecialRule : Settings.SpecialControllerRules) {
            if (ControlName.Contains(SpecialRule.Key,
                                     ESearchCase::IgnoreCase)) {
                bIsSpecialControl = true;
                SpecialAxisMode = SpecialRule.Value;
                break;
            }
        }

        if (bIsSpecialControl) {
            switch (SpecialAxisMode) {
                case ESpecialAxisMode::X:
                    LocationX->AddKeys(LocationTimes, LocationXValues);
                    UE_LOG(LogTemp, Warning,
                           TEXT("[COMMON] Special control '%s': Only X-axis "
                                "keys added"),
                           *ControlName);
                    break;
                case ESpecialAxisMode::Y:
                    LocationY->AddKeys(LocationTimes, LocationYValues);
                    UE_LOG(LogTemp, Warning,
                           TEXT("[COMMON] Special control '%s': Only Y-axis "
                                "keys added"),
                           *ControlName);
                    break;
                case ESpecialAxisMode::Z:
                    LocationZ->AddKeys(LocationTimes, LocationZValues);
                    UE_LOG(LogTemp, Warning,
                           TEXT("[COMMON] Special control '%s': Only Z-axis "
                                "keys added"),
                           *ControlName);
                    break;
            }
        } else {
            LocationX->AddKeys(LocationTimes, LocationXValues);
            LocationY->AddKeys(LocationTimes, LocationYValues);
            LocationZ->AddKeys(LocationTimes, LocationZValues);

            RotationX->AddKeys(RotationTimes, RotationXValues);
            RotationY->AddKeys(RotationTimes, RotationYValues);
            RotationZ->AddKeys(RotationTimes, RotationZValues);
        }

        UE_LOG(LogTemp, Warning,
               TEXT("[COMMON] Control '%s': Keys added successfully"),
               *ControlName);
    }

    if (MinFrame != MAX_int32 && MaxFrame != MIN_int32 &&
        MinFrame <= MaxFrame) {
        // 将 FramePadding 从显示帧转换为内部帧空间
        int32 PaddingInInternalFrames =
            Settings.FramePadding * TickResolution.Numerator *
            DisplayRate.Denominator /
            (TickResolution.Denominator * DisplayRate.Numerator);

        // 扩展 Section 范围（而非覆盖），始终从零帧开始
        FFrameNumber NewEnd = MaxFrame + PaddingInInternalFrames;
        if (!Section->GetRange().IsEmpty() &&
            Section->GetRange().HasUpperBound()) {
            NewEnd =
                FMath::Max(Section->GetRange().GetUpperBoundValue(), NewEnd);
        }
        Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0), NewEnd));
        UE_LOG(LogTemp, Warning,
               TEXT("[COMMON] Set section range to 0 - %d (Padding: %d "
                    "display frames -> %d internal frames)"),
               NewEnd.Value, Settings.FramePadding, PaddingInInternalFrames);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("[COMMON] Warning: Invalid frame range. MinFrame=%d, "
                    "MaxFrame=%d"),
               MinFrame.Value, MaxFrame.Value);
    }

    // 对 Section、Track 和 MovieScene 都调用 Modify 确保更改被追踪
    Section->Modify();
    TargetControlRigTrack->Modify();
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    // 先刷新序列，再通知数据变更，最后再刷新 UI
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();

    {
        TSharedPtr<ISequencer> ActiveSequencer = nullptr;
        ULevelSequence* ActiveLevelSequence = nullptr;
        if (GetActiveLevelSequenceAndSequencer(ActiveLevelSequence,
                                               ActiveSequencer)) {
            if (ActiveSequencer.IsValid() &&
                ActiveLevelSequence == LevelSequence) {
                // MovieSceneStructureItemsChanged 会触发完整的评估模板重建
                ActiveSequencer->NotifyMovieSceneDataChanged(
                    EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
                // 强制在当前时间重新评估，确保视口立即反映变更
                ActiveSequencer->ForceEvaluate();
                UE_LOG(LogTemp, Warning,
                       TEXT("[COMMON] Notified sequencer of data change to "
                            "trigger template recompilation"));
            }
        }
    }

    // 刷新 UI 以显示更新
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

    // 对 Section 和 Track 调用 Modify 确保更改被追踪
    for (UMovieSceneSection* Section : AllSections) {
        if (Section) {
            Section->Modify();
        }
    }
    TargetTrack->Modify();

    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    // 先刷新序列，再通知数据变更，最后再刷新 UI
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();

    {
        TSharedPtr<ISequencer> ActiveSequencer = nullptr;
        ULevelSequence* ActiveLevelSequence = nullptr;
        if (GetActiveLevelSequenceAndSequencer(ActiveLevelSequence,
                                               ActiveSequencer)) {
            if (ActiveSequencer.IsValid() &&
                ActiveLevelSequence == LevelSequence) {
                // MovieSceneStructureItemsChanged 会触发完整的评估模板重建
                ActiveSequencer->NotifyMovieSceneDataChanged(
                    EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
                // 强制在当前时间重新评估，确保视口立即反映变更
                ActiveSequencer->ForceEvaluate();
                UE_LOG(LogTemp, Warning,
                       TEXT("[COMMON] Notified sequencer of data change to "
                            "trigger template recompilation"));
            }
        }
    }

    // 刷新 UI 以显示更新
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif

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

bool UInstrumentAnimationUtility::WriteActiveCurveFromFile(
    ASkeletalMeshActor* PerformerActor, const FString& ActivityCurveFilePath,
    ULevelSequence* LevelSequence) {
    if (!PerformerActor) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "PerformerActor is null"));
        return false;
    }
    if (ActivityCurveFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "ActivityCurveFilePath is empty"));
        return false;
    }
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "LevelSequence is null"));
        return false;
    }

    // 1. 读取 JSON 文件
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *ActivityCurveFilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "Failed to load file '%s'"),
               *ActivityCurveFilePath);
        return false;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);
    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "Failed to parse JSON from '%s'"),
               *ActivityCurveFilePath);
        return false;
    }

    if (JsonArray.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "JSON array is empty in '%s'"),
               *ActivityCurveFilePath);
        return false;
    }

    // 2. 获取 MovieScene 帧率信息
    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "MovieScene is null"));
        return false;
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    // 3. 获取 ControlRig
    if (!GEngine) {
        return false;
    }
    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        return false;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(PerformerActor, LevelSequence);
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(PerformerActor, LevelSequence);

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "Failed to get ControlRig for PerformerActor '%s'"),
               *PerformerActor->GetName());
        return false;
    }

    // 4. 确保 controller_root 下存在 active_curve 动画通道
    {
        URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
        if (!RigHierarchy) {
            return false;
        }

        // 确保 controller_root 存在
        FRigElementKey RootKey(TEXT("controller_root"),
                               ERigElementType::Control);
        if (!RigHierarchy->Contains(RootKey)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentAnimationUtility] "
                        "WriteActiveCurveFromFile: "
                        "controller_root not found, attempting to create"));
            if (!FControlRigCreationUtility::CreateControl(
                    ControlRigBlueprint, TEXT("controller_root"), TEXT(""))) {
                UE_LOG(LogTemp, Error,
                       TEXT("[InstrumentAnimationUtility] "
                            "WriteActiveCurveFromFile: "
                            "Failed to create controller_root"));
                return false;
            }
        }

        // 确保 active_curve 动画通道存在
        FRigElementKey ChannelKey(TEXT("active_curve"),
                                  ERigElementType::Control);
        if (!RigHierarchy->Contains(ChannelKey)) {
            URigHierarchyController* HierarchyController =
                RigHierarchy->GetController();
            if (HierarchyController) {
                FRigControlSettings ChannelSettings;
                ChannelSettings.ControlType = ERigControlType::Float;
                ChannelSettings.DisplayName = TEXT("active_curve");

                FRigElementKey NewKey =
                    HierarchyController->AddAnimationChannel(
                        TEXT("active_curve"), RootKey, ChannelSettings, true,
                        false);

                if (NewKey.IsValid()) {
                    UE_LOG(LogTemp, Warning,
                           TEXT("[InstrumentAnimationUtility] "
                                "WriteActiveCurveFromFile: "
                                "Created active_curve channel under "
                                "controller_root"));
                } else {
                    UE_LOG(LogTemp, Error,
                           TEXT("[InstrumentAnimationUtility] "
                                "WriteActiveCurveFromFile: "
                                "Failed to create active_curve channel"));
                    return false;
                }
            }
        }
    }

    // 5. 找到 ControlRig 轨道上的 Section
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);
    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "ControlRig track not found"));
        return false;
    }

    TArray<UMovieSceneSection*> Sections = ControlRigTrack->GetAllSections();
    if (Sections.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "ControlRig track has no sections"));
        return false;
    }

    UMovieSceneSection* Section = Sections[0];

    // 6. 找到 active_curve 浮点通道
    FMovieSceneFloatChannel* ActiveCurveChannel =
        FindFloatChannel(Section, TEXT("controller_root.active_curve"));
    if (!ActiveCurveChannel) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "active_curve channel not found in section"));
        return false;
    }

    // 7. 解析关键帧并写入
    TArray<FFrameNumber> FrameNumbers;
    TArray<FMovieSceneFloatValue> FloatValues;

    for (const TSharedPtr<FJsonValue>& EntryVal : JsonArray) {
        TSharedPtr<FJsonObject> Entry = EntryVal->AsObject();
        if (!Entry.IsValid()) continue;

        double FrameDouble = 0.0;
        double Value = 0.0;
        Entry->TryGetNumberField(TEXT("frame"), FrameDouble);
        Entry->TryGetNumberField(TEXT("value"), Value);

        int32 ScaledFrameNumber = static_cast<int32>(FMath::RoundToInt(
            FrameDouble * TickResolution.Numerator * DisplayRate.Denominator /
            (TickResolution.Denominator * DisplayRate.Numerator)));

        FFrameNumber FrameNumber(ScaledFrameNumber);
        FMovieSceneFloatValue FloatValue(static_cast<float>(Value));
        FloatValue.InterpMode = ERichCurveInterpMode::RCIM_Linear;

        FrameNumbers.Add(FrameNumber);
        FloatValues.Add(FloatValue);
    }

    if (FrameNumbers.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                    "No keyframes parsed"));
        return false;
    }

    ActiveCurveChannel->AddKeys(FrameNumbers, FloatValues);

    // 8. 标记修改
    Section->Modify();
    ControlRigTrack->Modify();
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
    {
        TSharedPtr<ISequencer> ActiveSequencer = nullptr;
        ULevelSequence* ActiveLevelSequence = nullptr;
        if (GetActiveLevelSequenceAndSequencer(ActiveLevelSequence,
                                               ActiveSequencer)) {
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

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentAnimationUtility] WriteActiveCurveFromFile: "
                "Successfully wrote %d keyframes for active_curve"),
           FrameNumbers.Num());
    return true;
}

bool UInstrumentAnimationUtility::SetControlRigFloatChannelValue(
    UControlRig* ControlRigInstance, const FString& ControlName, float Value) {
    if (!ControlRigInstance) {
        return false;
    }

    // 1. 获取当前 Level Sequence
    ULevelSequence* LevelSequence = GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("SetControlRigFloatChannelValue: No active Level Sequence"));
        return false;
    }

    // 2. 查找 Control Rig Track
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);
    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Warning,
               TEXT("SetControlRigFloatChannelValue: ControlRig track not "
                    "found for ControlName='%s'"),
               *ControlName);
        return false;
    }

    // 3. 获取 Section
    TArray<UMovieSceneSection*> Sections = ControlRigTrack->GetAllSections();
    if (Sections.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("SetControlRigFloatChannelValue: No sections in ControlRig "
                    "track"));
        return false;
    }

    UMovieSceneSection* Section = Sections[0];
    if (!Section) {
        return false;
    }

    // 4. 查找 Float Channel（尝试多种命名模式）
    FMovieSceneFloatChannel* Channel = FindFloatChannel(Section, ControlName);
    if (!Channel) {
        // 回退：尝试 ControlName.Float 模式
        FString AltName = ControlName + TEXT(".Float");
        Channel = FindFloatChannel(Section, AltName);
    }

    if (!Channel) {
        UE_LOG(LogTemp, Warning,
               TEXT("SetControlRigFloatChannelValue: Float channel not found "
                    "for '%s' in section. Dumping available channels:"),
               *ControlName);
        LogAvailableChannels(Section);
        return false;
    }

    // 5. 设置通道默认值（等效于用户在 Sequencer 中调整 Control 的当前值）
    Channel->SetDefault(Value);

    // 6. 标记修改并通知 Sequencer 刷新
    Section->Modify();
    ControlRigTrack->Modify();
    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (MovieScene) {
        MovieScene->Modify();
    }
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    {
        TSharedPtr<ISequencer> ActiveSequencer = nullptr;
        ULevelSequence* ActiveLevelSequence = nullptr;
        if (GetActiveLevelSequenceAndSequencer(ActiveLevelSequence,
                                               ActiveSequencer)) {
            if (ActiveSequencer.IsValid() &&
                ActiveLevelSequence == LevelSequence) {
                ActiveSequencer->NotifyMovieSceneDataChanged(
                    EMovieSceneDataChangeType::TrackValueChanged);
                ActiveSequencer->ForceEvaluate();
            }
        }
    }
#endif

    return true;
}

bool UInstrumentAnimationUtility::IsInRenderingScenario() {
    if (GEditor) {
        UMoviePipelineQueueSubsystem* PipelineSubsystem =
            GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
        if (PipelineSubsystem) {
            UMoviePipelineExecutorBase* Executor =
                PipelineSubsystem->GetActiveExecutor();
            if (Executor && Executor->IsRendering()) {
                // 当前有正在运行的渲染作业
                return true;
            }
        }
    }
    return false;
}

// ===== 当前姿态关键帧写入 =====

/** 清除 Float Channel 上指定帧的所有关键帧（避免同帧堆叠） */
static void RemoveKeysAtFrame(FMovieSceneFloatChannel* Channel,
                              FFrameNumber Frame) {
    if (!Channel) return;

    auto Data = Channel->GetData();
    const TArrayView<const FFrameNumber> Times = Data.GetTimes();

    TArray<FKeyHandle> KeysToRemove;
    for (int32 i = 0; i < Times.Num(); ++i) {
        if (Times[i] == Frame) {
            KeysToRemove.Add(Data.GetHandle(i));
        }
    }

    if (KeysToRemove.Num() > 0) {
        Channel->DeleteKeys(KeysToRemove);
    }
}

int32 UInstrumentAnimationUtility::InsertCurrentPoseKeyframes(
    UControlRig* ControlRig, const TArray<FString>& ControlNames) {
    if (!ControlRig) {
        UE_LOG(LogTemp, Warning,
               TEXT("InsertCurrentPoseKeyframes: ControlRig is null"));
        return 0;
    }

    if (ControlNames.Num() == 0) {
        return 0;
    }

    // 1. 获取 LevelSequence 和 Sequencer
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!GetActiveLevelSequenceAndSequencer(LevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Warning,
               TEXT("InsertCurrentPoseKeyframes: No active Level Sequence"));
        return 0;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("InsertCurrentPoseKeyframes: MovieScene is null"));
        return 0;
    }

    // 2. 获取当前播放头帧号
    // 注意：必须用 GetLocalTime()（序列本地时间），不能用 GetGlobalTime()。
    // GetGlobalTime() 返回的是全局时间，包含序列在 master timeline 上的偏移，
    // 用它作为关键帧时间戳会把关键帧写到播放范围外的错误位置（日志中曾出现
    // frame 987600），导致加载的状态无法呈现，并会异常扩展 Section 范围，
    // 使控件在某个关键帧之后被“锁死”在保存状态。
    FFrameNumber CurrentFrame(0);
    if (Sequencer.IsValid()) {
        CurrentFrame = Sequencer->GetLocalTime().Time.FrameNumber;
    }

    // 3. 查找 Control Rig Parameter Track
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRig);
    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Warning,
               TEXT("InsertCurrentPoseKeyframes: ControlRig track not found "
                    "for '%s'"),
               *ControlRig->GetName());
        return 0;
    }

    TArray<UMovieSceneSection*> Sections = ControlRigTrack->GetAllSections();
    if (Sections.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("InsertCurrentPoseKeyframes: No sections in ControlRig "
                    "track"));
        return 0;
    }

    // 优先选择与当前播放头相交的 Section，避免在存在多个 Section 时把关键帧
    // 写进求值不会读取的 Section，导致加载的状态无法呈现。
    UMovieSceneSection* Section = nullptr;
    for (UMovieSceneSection* S : Sections) {
        if (S && S->GetRange().Contains(CurrentFrame)) {
            Section = S;
            break;
        }
    }
    if (!Section) {
        Section = Sections[0];
    }
    if (!Section) {
        return 0;
    }

    // 4. 读取 CR Hierarchy
    URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Warning,
               TEXT("InsertCurrentPoseKeyframes: No RigHierarchy"));
        return 0;
    }

    // 注意：这里不能调用 Evaluate_AnyThread()。
    // 在 Sequencer 环境中，Evaluate 会触发 Sequencer 用当前帧的旧关键帧
    // 覆盖 hierarchy 中刚由 SaveState/LoadState 写入的值，导致写入的关键帧
    // 变成 sequence 当前帧的状态而不是目标状态。直接读取当前 hierarchy 值即可。
    int32 SuccessCount = 0;

    for (const FString& ControlName : ControlNames) {
        // 4a. 读取当前 Transform
        FTransform CurrentTransform;
        if (!FInstrumentControlRigUtility::GetControlLocalTransform(
                RigHierarchy, ControlName, CurrentTransform)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("InsertCurrentPoseKeyframes: Control '%s' not found in "
                        "hierarchy"),
                   *ControlName);
            continue;
        }

        FString Prefix = ControlName + TEXT(".");

        // 4b. 查找 Location / Rotation 通道
        FMovieSceneFloatChannel* LocX = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sLocation.X"), *Prefix));
        FMovieSceneFloatChannel* LocY = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sLocation.Y"), *Prefix));
        FMovieSceneFloatChannel* LocZ = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sLocation.Z"), *Prefix));
        FMovieSceneFloatChannel* RotX = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sRotation.X"), *Prefix));
        FMovieSceneFloatChannel* RotY = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sRotation.Y"), *Prefix));
        FMovieSceneFloatChannel* RotZ = FindFloatChannel(
            Section, *FString::Printf(TEXT("%sRotation.Z"), *Prefix));

        if (!LocX || !LocY || !LocZ || !RotX || !RotY || !RotZ) {
            UE_LOG(LogTemp, Warning,
                   TEXT("InsertCurrentPoseKeyframes: Missing channels for "
                        "control '%s', skipping"),
                   *ControlName);
            continue;
        }

        // 4c. 读取当前 Transform 的 Location / Rotation（用于写入）
        FVector Loc = CurrentTransform.GetLocation();
        FRotator Rot = CurrentTransform.GetRotation().Rotator();

        // 4d. 先清除当前帧旧键（避免反复调用导致同帧堆叠）
        RemoveKeysAtFrame(LocX, CurrentFrame);
        RemoveKeysAtFrame(LocY, CurrentFrame);
        RemoveKeysAtFrame(LocZ, CurrentFrame);
        RemoveKeysAtFrame(RotX, CurrentFrame);
        RemoveKeysAtFrame(RotY, CurrentFrame);
        RemoveKeysAtFrame(RotZ, CurrentFrame);

        // 4e. 写入新关键帧
        // 必须使用标准 Sequencer API AddKeyToChannel 逐个写入，不能用
        // AddKeys（批量）。引擎源码证明：FMovieSceneFloatChannel::AddKeys
        // 的实现是直接把新帧 Append 到关键帧列表末尾（不排序），头文件注释
        // 明确要求 "times must be greater than last time and increasing"，
        // 违反时通道排序被破坏，随后的 Evaluate 用二分查找会读错值
        // （曾观察到新帧被追加到列表末尾：last3=6411600,6483600,617600）。
        // AddKeyToChannel 内部走 InsertKeyInternal（Algo::UpperBound 排序
        // 插入）→ AddCubicKey/AddLinearKey/AddConstantKey，始终维持通道
        // 有序，与 Sequencer 手动加键行为完全一致。
        // 注意：此重载（FMovieSceneFloatChannel* 版本）声明在全局命名空间，
        // 需用 :: 前缀限定，避免与 UE::MovieScene 命名空间内的默认模板实现
        // 混淆。
        ::AddKeyToChannel(LocX, CurrentFrame, Loc.X,
                          EMovieSceneKeyInterpolation::Auto);
        ::AddKeyToChannel(LocY, CurrentFrame, Loc.Y,
                          EMovieSceneKeyInterpolation::Auto);
        ::AddKeyToChannel(LocZ, CurrentFrame, Loc.Z,
                          EMovieSceneKeyInterpolation::Auto);
        ::AddKeyToChannel(RotX, CurrentFrame, Rot.Roll,
                          EMovieSceneKeyInterpolation::Auto);
        ::AddKeyToChannel(RotY, CurrentFrame, Rot.Pitch,
                          EMovieSceneKeyInterpolation::Auto);
        ::AddKeyToChannel(RotZ, CurrentFrame, Rot.Yaw,
                          EMovieSceneKeyInterpolation::Auto);

        SuccessCount++;
    }

    if (SuccessCount == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("InsertCurrentPoseKeyframes: No controls were keyframed"));
        return 0;
    }

    // 5. 扩展 Section 范围以包含当前帧
    if (!Section->GetRange().IsEmpty() && Section->GetRange().HasUpperBound()) {
        FFrameNumber CurrentEnd = Section->GetRange().GetUpperBoundValue();
        if (CurrentFrame > CurrentEnd) {
            Section->SetRange(TRange<FFrameNumber>(
                Section->GetRange().GetLowerBoundValue(), CurrentFrame));
        }
    }

    // 6. 标记修改
    Section->Modify();
    ControlRigTrack->Modify();
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    // 7. 通知 Sequencer 刷新
    // 注意：必须使用 MovieSceneStructureItemsChanged 而非 TrackValueChanged。
    // TrackValueChanged 只做局部失效，Sequencer 求值模板可能仍使用旧的通道
    // 数据缓存，导致“写入了关键帧但求值仍读到旧值”。而
    // MovieSceneStructureItemsChanged 会触发完整的评估模板重建，
    // 确保 ForceEvaluate 使用新写入的关键帧（参考 BatchInsertControlRigKeys）。
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();

    {
        TSharedPtr<ISequencer> ActiveSequencer = nullptr;
        ULevelSequence* ActiveLevelSequence = nullptr;
        if (GetActiveLevelSequenceAndSequencer(ActiveLevelSequence,
                                               ActiveSequencer)) {
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
           TEXT("InsertCurrentPoseKeyframes: Keyframed %d controls at frame "
                "%d"),
           SuccessCount, CurrentFrame.Value);

    return SuccessCount;
}
