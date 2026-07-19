#include "InstrumentMorphTargetUtility.h"

#include "Animation/MorphTarget.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Engine/SkeletalMesh.h"
#include "ISequencer.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Misc/FileHelper.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"

bool UInstrumentMorphTargetUtility::GetMorphTargetNames(
    USkeletalMeshComponent* SkeletalMeshComp, TArray<FString>& OutNames) {
    OutNames.Empty();

    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] SkeletalMeshComp is null"));
        return false;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMeshAsset();
    if (!SkeletalMesh) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] SkeletalMesh is null"));
        return false;
    }

    const TArray<UMorphTarget*>& MorphTargets = SkeletalMesh->GetMorphTargets();
    if (MorphTargets.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentMorphTargetUtility] SkeletalMesh has no morph "
                    "targets"));
        return false;
    }

    for (const UMorphTarget* MorphTarget : MorphTargets) {
        if (MorphTarget) {
            OutNames.Add(MorphTarget->GetName());
        }
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Found %d morph targets"),
           OutNames.Num());

    return OutNames.Num() > 0;
}

bool UInstrumentMorphTargetUtility::EnsureRootControlExists(
    UControlRigBlueprint* ControlRigBlueprint, const FString& RootControlName,
    ERigControlType ControlType) {
    if (!ControlRigBlueprint) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] ControlRigBlueprint is null"));
        return false;
    }

    if (RootControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] RootControlName is empty"));
        return false;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] Failed to get RigHierarchy"));
        return false;
    }

    FRigElementKey RootControlKey(*RootControlName, ERigElementType::Control);

    // 检查Root Control是否存在
    if (RigHierarchy->Contains(RootControlKey)) {
        const FRigControlElement* ControlElement =
            RigHierarchy->Find<FRigControlElement>(RootControlKey);

        if (ControlElement) {
            UE_LOG(
                LogTemp, Log,
                TEXT("[InstrumentMorphTargetUtility] Root control '%s' exists"),
                *RootControlName);
            return true;
        }
    }

    // 不存在则尝试创建
    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentMorphTargetUtility] Root control '%s' does not "
                "exist, attempting to create it"),
           *RootControlName);

    if (!FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                   RootControlName, TEXT(""))) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Root control '%s' does not "
                    "exist in hierarchy"),
               *RootControlName);
        return false;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentMorphTargetUtility] Root control '%s' created "
                "successfully"),
           *RootControlName);
    return true;
}

int32 UInstrumentMorphTargetUtility::AddAnimationChannels(
    UControlRigBlueprint* ControlRigBlueprint,
    const FRigElementKey& ParentControl, const TArray<FString>& ChannelNames,
    ERigControlType ChannelType) {
    if (!ControlRigBlueprint) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] ControlRigBlueprint is null"));
        return 0;
    }

    if (ChannelNames.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentMorphTargetUtility] ChannelNames is empty"));
        return 0;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] Failed to get RigHierarchy"));
        return 0;
    }

    URigHierarchyController* HierarchyController =
        RigHierarchy->GetController();
    if (!HierarchyController) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Failed to get "
                    "HierarchyController"));
        return 0;
    }

    // 检查父Control是否存在
    if (!RigHierarchy->Contains(ParentControl)) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Parent control '%s' does "
                    "not exist"),
               *ParentControl.Name.ToString());
        return 0;
    }

    int32 SuccessCount = 0;
    int32 FailureCount = 0;

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Adding %d animation channels "
                "to '%s'"),
           ChannelNames.Num(), *ParentControl.Name.ToString());

    for (const FString& ChannelName : ChannelNames) {
        if (ChannelName.IsEmpty()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] Skipping empty channel "
                        "name"));
            FailureCount++;
            continue;
        }

        FName ChannelFName(*ChannelName);

        // 创建通道Key并检查是否已存在
        FRigElementKey ChannelKey(ChannelFName, ERigElementType::Control);

        if (RigHierarchy->Contains(ChannelKey)) {
            // 通道已存在，验证它是否是Animation Channel
            const FRigControlElement* ExistingElement =
                RigHierarchy->Find<FRigControlElement>(ChannelKey);

            if (ExistingElement && ExistingElement->IsAnimationChannel()) {
                SuccessCount++;
                UE_LOG(LogTemp, Verbose,
                       TEXT("[InstrumentMorphTargetUtility] Animation channel "
                            "'%s' already exists"),
                       *ChannelName);
                continue;
            }
        }

        // 创建Animation Channel
        FRigControlSettings ChannelSettings;
        ChannelSettings.ControlType = ChannelType;
        ChannelSettings.DisplayName = ChannelFName;

        FRigElementKey NewChannelKey = HierarchyController->AddAnimationChannel(
            ChannelFName, ParentControl, ChannelSettings, true, false);

        if (NewChannelKey.IsValid()) {
            SuccessCount++;
            UE_LOG(LogTemp, Verbose,
                   TEXT("[InstrumentMorphTargetUtility] Created animation "
                        "channel '%s'"),
                   *ChannelName);
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] Failed to create "
                        "animation channel '%s'"),
                   *ChannelName);
            FailureCount++;
        }
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Animation channels: %d "
                "succeeded, %d failed"),
           SuccessCount, FailureCount);

    return SuccessCount;
}

bool UInstrumentMorphTargetUtility::ProcessMorphTargetKeyframeData(
    const TArray<TSharedPtr<FJsonValue>>& KeyDataArray,
    TArray<FMorphTargetKeyframeData>& OutKeyframeData,
    FFrameRate TickResolution, FFrameRate DisplayRate) {
    OutKeyframeData.Empty();

    if (KeyDataArray.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] KeyDataArray is empty"));
        return false;
    }

    // 使用Map来聚合同一Morph Target的多个关键帧数据
    TMap<FString, FMorphTargetKeyframeData> MorphTargetKeyframeData;
    int32 TotalSuccess = 0;
    int32 TotalFailure = 0;

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
            continue;
        }

        // 解析关键帧数据
        TArray<FFrameNumber> NewFrameNumbers;
        TArray<float> NewValues;

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

                NewFrameNumbers.Add(FrameNumber);
                NewValues.Add(Value);
            }
        }

        // 完善FMorphTargetKeyframeData的追加逻辑
        if (MorphTargetKeyframeData.Contains(MorphTargetName)) {
            FMorphTargetKeyframeData& ExistingData =
                MorphTargetKeyframeData[MorphTargetName];

            // 追加新的帧号和值
            ExistingData.FrameNumbers.Append(NewFrameNumbers);
            ExistingData.Values.Append(NewValues);
        } else {
            // 第一次遇到此 MorphTargetName，创建新条目
            FMorphTargetKeyframeData NewData(MorphTargetName);
            NewData.FrameNumbers = NewFrameNumbers;
            NewData.Values = NewValues;

            MorphTargetKeyframeData.Add(MorphTargetName, NewData);
            TotalSuccess++;
        }
    }

    // 4. 将Map转换为数组
    for (const auto& Pair : MorphTargetKeyframeData) {
        OutKeyframeData.Add(Pair.Value);
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Collected %d unique morph "
                "targets, %d failed"),
           TotalSuccess, TotalFailure);

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Processed %d morph targets "
                "from JSON data"),
           OutKeyframeData.Num());

    return OutKeyframeData.Num() > 0;
}

int32 UInstrumentMorphTargetUtility::WriteMorphTargetKeyframes(
    UMovieSceneSection* Section,
    const TArray<FMorphTargetKeyframeData>& KeyframeData) {
    if (!Section) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Section is null"));
        return 0;
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentMorphTargetUtility] KeyframeData is empty"));
        return 0;
    }

    UMovieSceneControlRigParameterSection* ParameterSection =
        Cast<UMovieSceneControlRigParameterSection>(Section);
    if (!ParameterSection) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Section is not a "
                    "UMovieSceneControlRigParameterSection"));
        return 0;
    }

    int32 SuccessCount = 0;

    for (const FMorphTargetKeyframeData& Data : KeyframeData) {
        if (Data.MorphTargetName.IsEmpty()) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] Skipping keyframe data "
                        "with empty morph target name"));
            continue;
        }

        if (Data.FrameNumbers.Num() != Data.Values.Num()) {
            UE_LOG(LogTemp, Error,
                   TEXT("[InstrumentMorphTargetUtility] FrameNumbers and "
                        "Values count mismatch for '%s': %d vs %d"),
                   *Data.MorphTargetName, Data.FrameNumbers.Num(),
                   Data.Values.Num());
            continue;
        }

        if (Data.FrameNumbers.Num() == 0) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] No keyframes to write "
                        "for '%s'"),
                   *Data.MorphTargetName);
            continue;
        }

        FName ChannelFName(*Data.MorphTargetName);

        // 确保 scalar parameter 存在（若已存在则 bReconstructChannel=false
        // 跳过重建）
        if (!ParameterSection->HasScalarParameter(ChannelFName)) {
            ParameterSection->AddScalarParameter(ChannelFName,
                                                 TOptional<float>(0.0f), true);
        }

        // 逐帧写入，走 GetInterpolationMode + AddKeyToChannel
        // 路径，与材质动画对齐
        for (int32 i = 0; i < Data.FrameNumbers.Num(); ++i) {
            ParameterSection->AddScalarParameterKey(
                ChannelFName, Data.FrameNumbers[i], Data.Values[i],
                EMovieSceneKeyInterpolation::Linear);
        }

        SuccessCount++;

        UE_LOG(
            LogTemp, Log,
            TEXT("[InstrumentMorphTargetUtility] Wrote %d keyframes for '%s'"),
            Data.FrameNumbers.Num(), *Data.MorphTargetName);
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Wrote keyframes for %d morph "
                "targets"),
           SuccessCount);

    return SuccessCount;
}

int32 UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig(
    class ASkeletalMeshActor* Instrument,
    const TArray<FMorphTargetKeyframeData>& KeyframeData,
    class ULevelSequence* LevelSequence, const FString& RootControlName,
    int32 FramePadding) {
    if (!Instrument) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Instrument is null"));
        return 0;
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] LevelSequence is null"));
        return 0;
    }

    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] KeyframeData is empty"));
        return 0;
    }

    // 获取MovieScene
    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] MovieScene is null"));
        return 0;
    }

    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] GEngine is not available"));
        return 0;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] ControlRig Cache Subsystem "
                    "is not available"));
        return 0;
    }

    UControlRig* ControlRigInstance =
        CacheSubsystem->GetControlRig(Instrument, LevelSequence);
    UControlRigBlueprint* ControlRigBlueprint =
        CacheSubsystem->GetControlRigBlueprint(Instrument, LevelSequence);

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Failed to get ControlRig "
                    "from Subsystem"));
        return 0;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Failed to get RigHierarchy "
                    "from ControlRig"));
        return 0;
    }

    // 检查Root Control是否存在
    FRigElementKey RootControlKey(*RootControlName, ERigElementType::Control);
    if (!RigHierarchy->Contains(RootControlKey)) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] Root control '%s' not found "
                 "in RigHierarchy"),
            *RootControlName);
        return 0;
    }

    // 查找或创建 Control Rig 轨道
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Failed to find Control Rig "
                    "track"));
        return 0;
    }

    // 删除所有现有 Sections 并创建新的（与材质动画处理保持一致）
    TArray<UMovieSceneSection*> AllExistingSections =
        ControlRigTrack->GetAllSections();

    if (AllExistingSections.Num() > 0) {
        // 删除所有现有 Section 以确保完全清理
        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "[InstrumentMorphTargetUtility] Removing %d existing sections"),
            AllExistingSections.Num());

        for (UMovieSceneSection* Section : AllExistingSections) {
            if (Section) {
                ControlRigTrack->RemoveSection(*Section);
            }
        }
    }

    // 创建新的 Section
    UMovieSceneSection* Section = ControlRigTrack->CreateNewSection();
    if (!Section) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] Failed to create new Section "
                 "for Morph Target animation"));
        return 0;
    }

    ControlRigTrack->AddSection(*Section);

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentMorphTargetUtility] Created new section for morph "
                "target animation"));

    // 计算帧数范围
    FFrameNumber MinFrame(MAX_int32);
    FFrameNumber MaxFrame(MIN_int32);
    bool bHasFrames = false;

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

    // 写入关键帧
    int32 WrittenTargets = WriteMorphTargetKeyframes(Section, KeyframeData);

    // 扩展 Section 范围（而非覆盖），始终从零帧开始
    if (bHasFrames) {
        // 将 FramePadding 从显示帧转换为内部帧空间
        FFrameRate TickResolution = MovieScene->GetTickResolution();
        FFrameRate DisplayRate = MovieScene->GetDisplayRate();
        int32 PaddingInInternalFrames =
            FramePadding * TickResolution.Numerator * DisplayRate.Denominator /
            (TickResolution.Denominator * DisplayRate.Numerator);

        FFrameNumber NewEnd = MaxFrame + PaddingInInternalFrames;
        if (!Section->GetRange().IsEmpty()) {
            NewEnd =
                FMath::Max(Section->GetRange().GetUpperBoundValue(), NewEnd);
        }
        Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0), NewEnd));
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentMorphTargetUtility] Expanded section range to "
                    "[0, %d) (Padding: %d display frames -> %d internal "
                    "frames)"),
               NewEnd.Value, FramePadding, PaddingInInternalFrames);
    }

    // 对 Section 和 Track 调用 Modify 确保更改被追踪
    Section->Modify();
    ControlRigTrack->Modify();
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    // 先刷新序列，再通知数据变更以触发完整的评估模板重建
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();

    {
        TSharedPtr<ISequencer> ActiveSequencer = nullptr;
        ULevelSequence* ActiveLevelSequence = nullptr;
        if (UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
                ActiveLevelSequence, ActiveSequencer)) {
            if (ActiveSequencer.IsValid() &&
                ActiveLevelSequence == LevelSequence) {
                // MovieSceneStructureItemsChanged 会触发完整的评估模板重建
                ActiveSequencer->NotifyMovieSceneDataChanged(
                    EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
                // 强制在当前时间重新评估，确保视口立即反映变更
                ActiveSequencer->ForceEvaluate();
                UE_LOG(
                    LogTemp, Warning,
                    TEXT("[InstrumentMorphTargetUtility] Notified sequencer of "
                         "data change to trigger template recompilation"));
            }
        }
    }

    // 刷新 UI 以显示更新
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentMorphTargetUtility] Successfully wrote %d morph "
                "target animations"),
           WrittenTargets);

    return WrittenTargets;
}

bool UInstrumentMorphTargetUtility::GetCurveNamesFromBlueprint(
    UControlRigBlueprint* ControlRigBlueprint, TArray<FString>& OutNames) {
    OutNames.Empty();

    if (!ControlRigBlueprint) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] ControlRigBlueprint is null"));
        return false;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Failed to get RigHierarchy "
                    "from Blueprint"));
        return false;
    }

    TArray<FRigElementKey> CurveKeys = RigHierarchy->GetAllKeys();
    for (const FRigElementKey& Key : CurveKeys) {
        if (Key.Type == ERigElementType::Curve) {
            OutNames.Add(Key.Name.ToString());
        }
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Found %d curves in ControlRig "
                "Blueprint"),
           OutNames.Num());

    return OutNames.Num() > 0;
}

int32 UInstrumentMorphTargetUtility::InitializeMorphTargetChannels(
    UControlRigBlueprint* ControlRigBlueprint, const FString& RootControlName,
    TArray<FString>* OutChannelNames) {
    // 参数验证
    if (!ControlRigBlueprint) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] ControlRigBlueprint is null"));
        return 0;
    }

    if (RootControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] RootControlName is empty"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeMorphTargetChannels Started =========="));
    UE_LOG(LogTemp, Warning, TEXT("Root Control: %s"), *RootControlName);

    // 步骤 1: 从 ControlRig Blueprint 的 Curve Container 读取所有曲线名称
    TArray<FString> MorphTargetNames;
    if (!GetCurveNamesFromBlueprint(ControlRigBlueprint, MorphTargetNames)) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Failed to get curve names "
                    "from ControlRig Blueprint"));
        return 0;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentMorphTargetUtility] Found %d curves in "
                "ControlRig Blueprint"),
           MorphTargetNames.Num());

    if (MorphTargetNames.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] No Morph Targets found"));
        return 0;
    }

    // 步骤 2: 确保 Root Control 存在
    if (!EnsureRootControlExists(ControlRigBlueprint, RootControlName)) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Failed to ensure Root "
                    "Control '%s' exists"),
               *RootControlName);
        return 0;
    }

    // 步骤 3: 批量添加 Animation Channels
    FRigElementKey RootControlKey(*RootControlName, ERigElementType::Control);

    int32 ChannelsAdded = AddAnimationChannels(
        ControlRigBlueprint, RootControlKey, MorphTargetNames);

    // 输出结果
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeMorphTargetChannels Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully created/verified: %d channels"),
           ChannelsAdded);
    UE_LOG(LogTemp, Warning, TEXT("Expected total: %d Morph Targets"),
           MorphTargetNames.Num());

    if (ChannelsAdded == MorphTargetNames.Num()) {
        UE_LOG(LogTemp, Warning,
               TEXT("✓ All Morph Target channels initialized successfully"));
    } else if (ChannelsAdded > 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("⚠ Partially initialized: %d/%d channels"), ChannelsAdded,
               MorphTargetNames.Num());
    } else {
        UE_LOG(LogTemp, Error, TEXT("✗ Failed to initialize any channels"));
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("========== InitializeMorphTargetChannels Completed =========="));

    // 如果需要，输出通道名称列表
    if (OutChannelNames) {
        *OutChannelNames = MorphTargetNames;
    }

    // 修改 Blueprint Hierarchy 后，必须触发 Blueprint 重编译。
    // 否则运行时 ControlRig 实例的 Hierarchy 仍然是旧的结构，
    // Sequencer 的 ControlRig Parameter Section 也不知道新增了哪些 Channel。
    // 保存、关闭、重新打开 Sequence 后，Section 会尝试用旧数据匹配新
    // Hierarchy， 导致 "Array index out of bounds" 崩溃。
    //
    // MarkBlueprintAsStructurallyModified 会：
    //   1. 重编译 ControlRig VM，同步 Blueprint Hierarchy 到所有运行时实例
    //   2. 触发依赖此 Blueprint 的 Sequencer 轨道重建其 Channel Proxy
    //
    // 重要：Blueprint 重编译后，所有基于该 Blueprint 的运行时 ControlRig 实例
    // 都会被销毁并重建。ControlRigCacheSubsystem 中缓存的指针全部失效。
    // 同时，Sequencer 发出 MovieSceneStructureItemsChanged 通知后，
    // 会对所有轨道（包括其他 Actor 如演奏者的 ControlRig 轨道）进行结构重建。
    // 如果 Anim Outliner 中仍然持有旧的 ControlRig 内部指针（如 FRigElementKey
    // 对应的 Element 指针），访问时就会崩溃。
    //
    // 修复策略：
    //   1. 清空 Sequencer 选择（防止 Anim Outliner 持有过时引用）
    //   2. 重编译 Blueprint（使运行时实例同步）
    //   3. 清除所有 ControlRig 缓存（因为指针已失效）
    //   4. 通知 Sequencer 结构变化并强制重新评估
    //   5. 刷新 UI
    if (ChannelsAdded > 0) {
        ULevelSequence* LevelSequence = nullptr;
        TSharedPtr<ISequencer> Sequencer = nullptr;
        bool bHasSequencer =
            UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
                LevelSequence, Sequencer);

        // 步骤 1: 在重编译前先清空 Sequencer 选择，
        // 防止 Anim Outliner 在重编译过程中访问正在被销毁的 ControlRig 元素
        if (bHasSequencer && Sequencer.IsValid()) {
            Sequencer->EmptySelection();
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] Cleared Sequencer "
                        "selection before Blueprint recompilation"));
        }

        // 步骤 2: 重编译 Blueprint
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
            ControlRigBlueprint);
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentMorphTargetUtility] Marked Blueprint as "
                    "structurally modified to trigger VM recompilation"));

        // 步骤 3: 清除所有 ControlRig 缓存
        // Blueprint 重编译后，基于该 Blueprint 的运行时 ControlRig
        // 实例已被重建。
        // 缓存中的旧指针不再有效，必须清除以防止后续操作使用过时的指针。
        if (GEngine) {
            UControlRigCacheSubsystem* CacheSubsystem =
                GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
            if (CacheSubsystem) {
                CacheSubsystem->ClearAllCaches();
                UE_LOG(LogTemp, Warning,
                       TEXT("[InstrumentMorphTargetUtility] Cleared all "
                            "ControlRig caches after Blueprint recompilation"));
            }
        }

        // 步骤 4: 通知 Sequencer 数据结构已变化并强制重新评估
        // MovieSceneStructureItemsChanged 会触发所有轨道的评估模板重建，
        // 包括其他 Actor（如演奏者）的 ControlRig 轨道。
        // ForceEvaluate 确保 Sequencer 完成一次完整评估循环，
        // 使所有内部状态（包括 Anim Outliner 的树形结构）完全同步。
        if (bHasSequencer && Sequencer.IsValid() && LevelSequence) {
            Sequencer->NotifyMovieSceneDataChanged(
                EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
            Sequencer->ForceEvaluate();
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] Notified Sequencer of "
                        "structural change and forced re-evaluation"));
        }

        // 步骤 5: 刷新 Sequencer UI
        ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
    }

    return ChannelsAdded;
}
