#include "Baking/AnimationBaker.h"

#include "Channels/MovieSceneChannelProxy.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "EngineUtils.h"
#include "ISequencer.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentBase.h"
#include "LevelEditor.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "Rigs/RigHierarchy.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"

bool UAnimationBaker::GetControlRigsForActor(
    ASkeletalMeshActor* Actor, ULevelSequence* LevelSequence,
    UControlRig*& OutControlRig, UControlRigBlueprint*& OutBlueprint) {
    OutControlRig = nullptr;
    OutBlueprint = nullptr;

    if (!Actor || !LevelSequence) {
        return false;
    }

    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("[AnimationBaker] GEngine is not available"));
        return false;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("[AnimationBaker] ControlRig Cache Subsystem is not "
                    "available"));
        return false;
    }

    OutControlRig = CacheSubsystem->GetControlRig(Actor, LevelSequence);
    OutBlueprint = CacheSubsystem->GetControlRigBlueprint(Actor, LevelSequence);

    return OutControlRig != nullptr && OutBlueprint != nullptr;
}

bool UAnimationBaker::EvaluateSequenceAtFrame(TSharedPtr<ISequencer> Sequencer,
                                              ULevelSequence* LevelSequence,
                                              int32 Frame) {
    if (!Sequencer.IsValid() || !LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("[AnimationBaker] Invalid Sequencer or LevelSequence for "
                    "frame evaluation"));
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        return false;
    }

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameRate DisplayRate = MovieScene->GetDisplayRate();

    // 将Display Rate帧号转换为Tick Resolution帧号
    FFrameTime FrameInTicks = FFrameRate::TransformTime(
        FFrameTime(FFrameNumber(Frame)), DisplayRate, TickResolution);

    // 使用SetGlobalTime真正移动播放光标
    // 这等同于人类手动拖动Sequencer播放头到指定帧
    Sequencer->SetGlobalTime(FrameInTicks);

    // SetGlobalTime会触发完整的Sequencer评估流水线，
    // 包括所有Track的评估和Control Rig的更新

    UE_LOG(LogTemp, Verbose,
           TEXT("[AnimationBaker] Moved playhead to frame %d (ticks: %f)"),
           Frame, FrameInTicks.AsDecimal());
    return true;
}

void UAnimationBaker::TriggerInstrumentSync() {
    // 获取编辑器世界
    UWorld* World = nullptr;
    if (GEditor) {
        World = GEditor->GetEditorWorldContext().World();
    }

    if (!World) {
        UE_LOG(
            LogTemp, Verbose,
            TEXT("[AnimationBaker] No editor world found for instrument sync"));
        return;
    }

    // 遍历世界中所有AInstrumentBase子类Actor，调用其Tick/同步方法
    for (TActorIterator<AInstrumentBase> ActorItr(World); ActorItr;
         ++ActorItr) {
        AInstrumentBase* InstrumentActor = *ActorItr;
        if (!InstrumentActor) {
            continue;
        }

        // 调用Tick以触发同步逻辑（如SyncInstrumentTransforms）
        // 使用DeltaTime=0表示这是一个即时评估
        InstrumentActor->Tick(0.0f);

        UE_LOG(LogTemp, Verbose,
               TEXT("[AnimationBaker] Triggered sync for instrument actor: %s"),
               *InstrumentActor->GetName());
    }
}

UMovieSceneSection* UAnimationBaker::EnsureTrackSection(
    UMovieSceneControlRigParameterTrack* Track) {
    if (!Track) {
        return nullptr;
    }

    TArray<UMovieSceneSection*> Sections = Track->GetAllSections();
    if (Sections.Num() > 0) {
        return Sections[0];
    }

    // 创建新的Section
    UMovieSceneSection* NewSection = Track->CreateNewSection();
    if (NewSection) {
        Track->AddSection(*NewSection);
        return NewSection;
    }

    return nullptr;
}

int32 UAnimationBaker::BakeMultiInstanceControlsUnified(
    ULevelSequence* LevelSequence,
    const TArray<TPair<UControlRig*, TArray<FString>>>& ControlGroups,
    const FAnimationBakeSettings& Settings,
    TFunction<void(int32, int32, const FString&)> ProgressCallback) {
    if (!LevelSequence || ControlGroups.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("[AnimationBaker] Invalid parameters for multi-instance "
                    "unified bake"));
        return 0;
    }

    if (!Settings.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("[AnimationBaker] Invalid bake settings"));
        return 0;
    }

    // 获取Sequencer实例（只获取一次，整个烘焙过程复用）
    TSharedPtr<ISequencer> Sequencer = nullptr;
    ULevelSequence* ActiveLevelSequence = nullptr;

    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            ActiveLevelSequence, Sequencer)) {
        UE_LOG(LogTemp, Error,
               TEXT("[AnimationBaker] No active sequencer found"));
        return 0;
    }

    if (ActiveLevelSequence != LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("[AnimationBaker] LevelSequence does not match current "
                    "Sequencer"));
        return 0;
    }

    if (!Sequencer.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("[AnimationBaker] Sequencer is not valid"));
        return 0;
    }

    UE_LOG(LogTemp, Log,
           TEXT("[AnimationBaker] Starting multi-instance unified bake: %d "
                "instances (playhead-based evaluation)"),
           ControlGroups.Num());

    // 第一阶段：预验证所有Control
    TMap<UControlRig*, TMap<FString, int32>> ValidControlsPerInstance;
    int32 TotalControls = 0;

    for (const auto& Group : ControlGroups) {
        UControlRig* ControlRigInstance = Group.Key;
        const TArray<FString>& ControlNames = Group.Value;

        if (!ControlRigInstance || ControlNames.Num() == 0) {
            continue;
        }

        URigHierarchy* Hierarchy = ControlRigInstance->GetHierarchy();
        if (!Hierarchy) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[AnimationBaker] Failed to get hierarchy for "
                        "ControlRig %p"),
                   ControlRigInstance);
            continue;
        }

        TMap<FString, int32>& ValidControls =
            ValidControlsPerInstance.FindOrAdd(ControlRigInstance);
        for (const FString& ControlName : ControlNames) {
            int32 ControlIndex = Hierarchy->GetIndex(
                FRigElementKey(*ControlName, ERigElementType::Control));
            if (ControlIndex != INDEX_NONE) {
                ValidControls.Add(ControlName, ControlIndex);
            } else {
                UE_LOG(LogTemp, Warning,
                       TEXT("[AnimationBaker] Control '%s' not found in "
                            "hierarchy for instance %p"),
                       *ControlName, ControlRigInstance);
            }
        }

        TotalControls += ValidControls.Num();
        UE_LOG(LogTemp, Log,
               TEXT("[AnimationBaker] Instance %p has %d valid controls"),
               ControlRigInstance, ValidControls.Num());
    }

    if (TotalControls == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("[AnimationBaker] No valid controls found across all "
                    "instances"));
        return 0;
    }

    UE_LOG(LogTemp, Log,
           TEXT("[AnimationBaker] Found %d valid controls across %d instances"),
           TotalControls, ValidControlsPerInstance.Num());

    // 记住烘焙前的播放头位置，烘焙结束后恢复
    FFrameTime OriginalGlobalTime = Sequencer->GetGlobalTime().Time;

    // 第二阶段：逐帧移动播放光标，触发同步，然后采集数据
    // 数据结构：Instance -> ControlName -> Frame -> Snapshot
    TMap<UControlRig*, TMap<FString, TMap<int32, FControlValueSnapshot>>>
        UnifiedCollectedData;

    int32 TotalFrames = Settings.GetTotalFrames();
    int32 CompletedFrames = 0;

    UE_LOG(LogTemp, Log,
           TEXT("[AnimationBaker] Starting frame-by-frame collection: %d "
                "frames (range %d-%d, step %d)"),
           TotalFrames, Settings.StartFrame, Settings.EndFrame,
           Settings.FrameStep);

    for (int32 Frame = Settings.StartFrame; Frame <= Settings.EndFrame;
         Frame += Settings.FrameStep) {
        // 步骤1：移动播放光标到当前帧（等同于人类拖动光标）
        if (!EvaluateSequenceAtFrame(Sequencer, LevelSequence, Frame)) {
            UE_LOG(
                LogTemp, Warning,
                TEXT("[AnimationBaker] Failed to evaluate frame %d, skipping"),
                Frame);
            continue;
        }

        // 步骤2：触发所有乐器Actor的同步逻辑
        // 这相当于Tick中SyncInstrumentTransforms()的效果
        TriggerInstrumentSync();

        // 步骤3：采集该帧所有Instance的所有Control数据
        for (const auto& InstancePair : ValidControlsPerInstance) {
            UControlRig* ControlRigInstance = InstancePair.Key;
            const TMap<FString, int32>& ValidControls = InstancePair.Value;

            URigHierarchy* Hierarchy = ControlRigInstance->GetHierarchy();
            if (!Hierarchy) {
                continue;
            }

            for (const auto& ControlPair : ValidControls) {
                const FString& ControlName = ControlPair.Key;
                int32 ControlIndex = ControlPair.Value;

                // 获取当前帧的变换值
                FTransform CurrentTransform =
                    Hierarchy->GetGlobalTransform(ControlIndex);
                FVector CurrentLocation = CurrentTransform.GetLocation();
                FRotator CurrentRotation = CurrentTransform.Rotator();

                // 存储快照
                FControlValueSnapshot Snapshot;
                Snapshot.FrameNumber = Frame;
                Snapshot.Location = CurrentLocation;
                Snapshot.Rotation = CurrentRotation;

                UnifiedCollectedData.FindOrAdd(ControlRigInstance)
                    .FindOrAdd(ControlName)
                    .Add(Frame, Snapshot);
            }
        }

        CompletedFrames++;

        // 进度回调
        if (ProgressCallback) {
            ProgressCallback(CompletedFrames, TotalFrames,
                             FString::Printf(TEXT("Collecting frame %d/%d"),
                                             CompletedFrames, TotalFrames));
        }

        if (CompletedFrames % 1000 == 0) {
            UE_LOG(LogTemp, Log,
                   TEXT("[AnimationBaker] Collected %d/%d frames"),
                   CompletedFrames, TotalFrames);
        }
    }

    UE_LOG(
        LogTemp, Log,
        TEXT(
            "[AnimationBaker] Frame collection completed: %d frames collected"),
        CompletedFrames);

    // 恢复播放头到烘焙前的位置
    Sequencer->SetGlobalTime(OriginalGlobalTime);
    UE_LOG(LogTemp, Log,
           TEXT("[AnimationBaker] Restored playhead to original position"));

    // 第三阶段：批量写入所有Instance的所有Control数据
    UE_LOG(
        LogTemp, Log,
        TEXT("[AnimationBaker] Stage 3: Batch writing data for %d instances"),
        ValidControlsPerInstance.Num());

    int32 SuccessfulBakes = 0;

    for (const auto& InstancePair : ValidControlsPerInstance) {
        UControlRig* ControlRigInstance = InstancePair.Key;
        const TMap<FString, int32>& ValidControls = InstancePair.Value;

        // 重新组织该Instance的数据为批量插入格式
        TMap<FString, TArray<FAnimationKeyframe>> BatchKeyframeData;

        const TMap<FString, TMap<int32, FControlValueSnapshot>>* InstanceData =
            UnifiedCollectedData.Find(ControlRigInstance);
        if (!InstanceData) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[AnimationBaker] No data collected for instance %p"),
                   ControlRigInstance);
            continue;
        }

        for (const auto& ControlPair : ValidControls) {
            const FString& ControlName = ControlPair.Key;

            const TMap<int32, FControlValueSnapshot>* ControlData =
                InstanceData->Find(ControlName);
            if (!ControlData || ControlData->Num() == 0) {
                UE_LOG(
                    LogTemp, Warning,
                    TEXT("[AnimationBaker] No data collected for control '%s' "
                         "in instance %p"),
                    *ControlName, ControlRigInstance);
                continue;
            }

            // 转换为AnimationKeyframe格式
            TArray<FAnimationKeyframe>& Keyframes =
                BatchKeyframeData.FindOrAdd(ControlName);
            Keyframes.Empty(ControlData->Num());

            for (const auto& FramePair : *ControlData) {
                const FControlValueSnapshot& Snapshot = FramePair.Value;
                FAnimationKeyframe Keyframe;
                Keyframe.FrameNumber = Snapshot.FrameNumber;
                Keyframe.Translation = Snapshot.Location;
                Keyframe.Rotation = Snapshot.Rotation.Quaternion();
                Keyframes.Add(Keyframe);
            }
        }

        if (BatchKeyframeData.Num() == 0) {
            UE_LOG(
                LogTemp, Warning,
                TEXT("[AnimationBaker] No valid keyframe data for instance %p"),
                ControlRigInstance);
            continue;
        }

        UE_LOG(LogTemp, Log,
               TEXT("[AnimationBaker] Preparing to insert %d controls for "
                    "instance %p"),
               BatchKeyframeData.Num(), ControlRigInstance);

        // 如果需要覆盖，先清理关键帧
        if (Settings.bOverwriteExistingKeys && BatchKeyframeData.Num() > 0) {
            UE_LOG(LogTemp, Log,
                   TEXT("[AnimationBaker] Cleaning existing keyframes for %d "
                        "controls in instance %p"),
                   BatchKeyframeData.Num(), ControlRigInstance);

            TSet<FString> ControlsToClean;
            for (const auto& Pair : BatchKeyframeData) {
                ControlsToClean.Add(Pair.Key);
            }
            UInstrumentAnimationUtility::ClearControlRigKeyframes(
                LevelSequence, ControlRigInstance, ControlsToClean);
        }

        // 配置批量插入设置
        FBatchInsertKeyframesSettings BatchSettings;

        // 批量插入该Instance的所有Control关键帧
        UInstrumentAnimationUtility::BatchInsertControlRigKeys(
            LevelSequence, ControlRigInstance, BatchKeyframeData,
            BatchSettings);

        SuccessfulBakes += BatchKeyframeData.Num();

        UE_LOG(
            LogTemp, Log,
            TEXT("[AnimationBaker] Batch insert completed for instance %p: %d "
                 "controls successful"),
            ControlRigInstance, BatchKeyframeData.Num());
    }

    UE_LOG(LogTemp, Log,
           TEXT("[AnimationBaker] Multi-instance unified bake completed: %d/%d "
                "controls successful"),
           SuccessfulBakes, TotalControls);

    // 最终进度回调
    if (ProgressCallback) {
        ProgressCallback(SuccessfulBakes, TotalControls,
                         TEXT("Multi-instance bake completed"));
    }

    return SuccessfulBakes;
}

int32 UAnimationBaker::BakeMultipleControlGroups(
    ULevelSequence* LevelSequence,
    const TArray<TPair<UControlRig*, TArray<FString>>>& ControlGroups,
    const FAnimationBakeSettings& Settings,
    TFunction<void(int32, int32, const FString&)> ProgressCallback) {
    if (!LevelSequence) {
        UE_LOG(LogTemp, Error, TEXT("[AnimationBaker] LevelSequence is null"));
        return 0;
    }

    if (ControlGroups.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[AnimationBaker] No control groups provided"));
        return 0;
    }

    if (!Settings.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("[AnimationBaker] Invalid bake settings"));
        return 0;
    }

    // 直接调用统一的多实例烘焙方法
    int32 SuccessfulBakes = BakeMultiInstanceControlsUnified(
        LevelSequence, ControlGroups, Settings, ProgressCallback);

    UE_LOG(LogTemp, Log,
           TEXT("[AnimationBaker] Multi-control groups bake completed: %d "
                "controls successful"),
           SuccessfulBakes);

    return SuccessfulBakes;
}