#include "InstrumentMorphTargetUtility.h"

#include "Animation/MorphTarget.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "Engine/SkeletalMesh.h"
#include "InstrumentControlRigUtility.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Misc/FileHelper.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"

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
        // 验证它确实是一个有效的Control
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

    UE_LOG(LogTemp, Error,
           TEXT("[InstrumentMorphTargetUtility] Root control '%s' does not "
                "exist in hierarchy"),
           *RootControlName);
    return false;
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
            // 追加数据而不是替换 - 这是关键修复
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

    FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
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

        // 查找对应的Float Channel
        FName ChannelName(*Data.MorphTargetName);
        TMovieSceneChannelHandle<FMovieSceneFloatChannel> ChannelHandle =
            ChannelProxy.GetChannelByName<FMovieSceneFloatChannel>(ChannelName);

        FMovieSceneFloatChannel* FloatChannel = ChannelHandle.Get();

        if (!FloatChannel) {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] Channel '%s' not found "
                        "after all search methods"),
                   *Data.MorphTargetName);
            continue;
        }

        // 转换Values为FMovieSceneFloatValue数组
        TArray<FMovieSceneFloatValue> FloatValues;
        FloatValues.Reserve(Data.Values.Num());
        for (float Value : Data.Values) {
            FloatValues.Add(FMovieSceneFloatValue(Value));
        }

        // 批量写入关键帧
        FloatChannel->AddKeys(Data.FrameNumbers, FloatValues);

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
    class ULevelSequence* LevelSequence, const FString& RootControlName) {
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

    // 使用通用工具方法获取ControlRig
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
            Instrument, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] ControlRigInstance is null"));
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

    // 查找或创建Control Rig轨道
    UMovieSceneControlRigParameterTrack* ControlRigTrack =
        FControlRigSequencerHelpers::FindControlRigTrack(LevelSequence,
                                                         ControlRigInstance);

    if (!ControlRigTrack) {
        UE_LOG(LogTemp, Error,
               TEXT("[InstrumentMorphTargetUtility] Failed to find Control Rig "
                    "track"));
        return 0;
    }

    // 清理所有现有Section
    TArray<UMovieSceneSection*> AllExistingSections =
        ControlRigTrack->GetAllSections();

    for (UMovieSceneSection* ExistingSection : AllExistingSections) {
        if (ExistingSection) {
            ControlRigTrack->RemoveSection(*ExistingSection);
        }
    }

    // 创建新的Section
    UMovieSceneSection* Section = ControlRigTrack->CreateNewSection();
    if (!Section) {
        UE_LOG(
            LogTemp, Error,
            TEXT("[InstrumentMorphTargetUtility] Failed to create new Section "
                 "for Morph Target animation"));
        return 0;
    }

    ControlRigTrack->AddSection(*Section);

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

    // 更新Section范围
    if (bHasFrames) {
        Section->SetRange(TRange<FFrameNumber>(MinFrame, MaxFrame + 1));
        UE_LOG(LogTemp, Warning,
               TEXT("[InstrumentMorphTargetUtility] Set section range to [%d, "
                    "%d)"),
               MinFrame.Value, (MaxFrame + 1).Value);
    }

    // 标记为修改并刷新
    MovieScene->Modify();
    LevelSequence->MarkPackageDirty();

#if WITH_EDITOR
    ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
#endif

    UE_LOG(LogTemp, Warning,
           TEXT("[InstrumentMorphTargetUtility] Successfully wrote %d morph "
                "target animations"),
           WrittenTargets);

    return WrittenTargets;
}
