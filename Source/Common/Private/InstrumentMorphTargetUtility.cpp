#include "InstrumentMorphTargetUtility.h"

#include "Animation/MorphTarget.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRigBlueprintLegacy.h"
#include "Engine/SkeletalMesh.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Misc/FileHelper.h"
#include "MovieSceneSection.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Channels/MovieSceneChannelProxy.h"

bool UInstrumentMorphTargetUtility::GetMorphTargetNames(
    USkeletalMeshComponent* SkeletalMeshComp,
    TArray<FString>& OutNames)
{
    OutNames.Empty();

    if (!SkeletalMeshComp)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] SkeletalMeshComp is null"));
        return false;
    }

    USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMeshAsset();
    if (!SkeletalMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] SkeletalMesh is null"));
        return false;
    }

    const TArray<UMorphTarget*>& MorphTargets = SkeletalMesh->GetMorphTargets();
    if (MorphTargets.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InstrumentMorphTargetUtility] SkeletalMesh has no morph targets"));
        return false;
    }

    for (const UMorphTarget* MorphTarget : MorphTargets)
    {
        if (MorphTarget)
        {
            OutNames.Add(MorphTarget->GetName());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[InstrumentMorphTargetUtility] Found %d morph targets"), OutNames.Num());

    return OutNames.Num() > 0;
}

bool UInstrumentMorphTargetUtility::EnsureRootControlExists(
    UControlRigBlueprint* ControlRigBlueprint,
    const FString& RootControlName,
    ERigControlType ControlType)
{
    if (!ControlRigBlueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] ControlRigBlueprint is null"));
        return false;
    }

    if (RootControlName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] RootControlName is empty"));
        return false;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] Failed to get RigHierarchy"));
        return false;
    }

    FRigElementKey RootControlKey(*RootControlName, ERigElementType::Control);

    // 检查Root Control是否已存在
    if (RigHierarchy->Contains(RootControlKey))
    {
        UE_LOG(LogTemp, Log, TEXT("[InstrumentMorphTargetUtility] Root control '%s' already exists"), *RootControlName);
        return true;
    }

    // 创建Root Control
    URigHierarchyController* HierarchyController = RigHierarchy->GetController();
    if (!HierarchyController)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] Failed to get HierarchyController"));
        return false;
    }

    FRigControlSettings RootControlSettings;
    RootControlSettings.ControlType = ControlType;
    RootControlSettings.DisplayName = FName(*RootControlName);
    RootControlSettings.ShapeName = FName(TEXT("Cube"));

    FTransform InitialTransform = FTransform::Identity;
    FRigControlValue InitialValue;
    InitialValue.SetFromTransform(InitialTransform, ControlType, ERigControlAxis::X);

    FRigElementKey NewRootControlKey = HierarchyController->AddControl(
        FName(*RootControlName),
        FRigElementKey(),
        RootControlSettings,
        InitialValue,
        FTransform::Identity,
        FTransform::Identity,
        true,
        false);

    if (NewRootControlKey.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("[InstrumentMorphTargetUtility] Successfully created root control '%s'"), *RootControlName);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] Failed to create root control '%s'"), *RootControlName);
        return false;
    }
}

int32 UInstrumentMorphTargetUtility::AddAnimationChannels(
    UControlRigBlueprint* ControlRigBlueprint,
    const FRigElementKey& ParentControl,
    const TArray<FString>& ChannelNames,
    ERigControlType ChannelType)
{
    if (!ControlRigBlueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] ControlRigBlueprint is null"));
        return 0;
    }

    if (ChannelNames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InstrumentMorphTargetUtility] ChannelNames is empty"));
        return 0;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] Failed to get RigHierarchy"));
        return 0;
    }

    URigHierarchyController* HierarchyController = RigHierarchy->GetController();
    if (!HierarchyController)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] Failed to get HierarchyController"));
        return 0;
    }

    // 检查父Control是否存在
    if (!RigHierarchy->Contains(ParentControl))
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] Parent control '%s' does not exist"),
               *ParentControl.Name.ToString());
        return 0;
    }

    int32 SuccessCount = 0;
    int32 FailureCount = 0;

    // 获取父Control下所有现有的Animation Channel
    TArray<FRigElementKey> ExistingChannels = RigHierarchy->GetAnimationChannels(ParentControl, true);

    UE_LOG(LogTemp, Log, TEXT("[InstrumentMorphTargetUtility] Adding %d animation channels to '%s'"),
           ChannelNames.Num(), *ParentControl.Name.ToString());

    for (const FString& ChannelName : ChannelNames)
    {
        FName ChannelFName(*ChannelName);

        // 检查通道是否已存在
        bool bChannelExists = false;
        for (const FRigElementKey& ExistingKey : ExistingChannels)
        {
            if (ExistingKey.Name == ChannelFName)
            {
                const FRigControlElement* ControlElement = RigHierarchy->Find<FRigControlElement>(ExistingKey);
                if (ControlElement && ControlElement->IsAnimationChannel())
                {
                    bChannelExists = true;
                    break;
                }
            }
        }

        if (bChannelExists)
        {
            SuccessCount++;
            continue;
        }

        // 创建Animation Channel
        FRigControlSettings ChannelSettings;
        ChannelSettings.ControlType = ChannelType;
        ChannelSettings.DisplayName = ChannelFName;

        FRigElementKey NewChannelKey = HierarchyController->AddAnimationChannel(
            ChannelFName,
            ParentControl,
            ChannelSettings,
            true,
            false);

        if (NewChannelKey.IsValid())
        {
            SuccessCount++;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[InstrumentMorphTargetUtility] Failed to create animation channel '%s'"), *ChannelName);
            FailureCount++;
        }
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Animation channels: %d succeeded, %d failed"),
           SuccessCount, FailureCount);

    return SuccessCount;
}

bool UInstrumentMorphTargetUtility::ParseMorphTargetJSON(
    const FString& JsonFilePath,
    TArray<FMorphTargetKeyframeData>& OutKeyframeData,
    FFrameRate TickResolution,
    FFrameRate DisplayRate)
{
    OutKeyframeData.Empty();

    if (JsonFilePath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] JsonFilePath is empty"));
        return false;
    }

    // 1. 读取JSON文件
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *JsonFilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] Failed to load JSON file: %s"), *JsonFilePath);
        return false;
    }

    // 2. 解析JSON
    TArray<TSharedPtr<FJsonValue>> KeyDataArray;
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        // 尝试作为数组解析
        TArray<TSharedPtr<FJsonValue>> RootArray;
        TSharedRef<TJsonReader<>> ArrayReader = TJsonReaderFactory<>::Create(JsonContent);
        if (!FJsonSerializer::Deserialize(ArrayReader, RootArray))
        {
            UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] Failed to parse JSON"));
            return false;
        }
        KeyDataArray = RootArray;
    }
    else if (RootObject->HasField(TEXT("keys")))

    {
        KeyDataArray = RootObject->GetArrayField(TEXT("keys"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] No keys found in JSON"));
        return false;
    }

    if (KeyDataArray.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] No key data found"));
        return false;
    }

    // 3. 使用Map来聚合同一Morph Target的多个关键帧数据
    // 🔥 修复：直接使用FMorphTargetKeyframeData并完善追加逻辑
    TMap<FString, FMorphTargetKeyframeData> MorphTargetKeyframeData;
    int32 TotalSuccess = 0;
    int32 TotalFailure = 0;

    for (const TSharedPtr<FJsonValue>& KeyValue : KeyDataArray)
    {
        TSharedPtr<FJsonObject> KeyObject = KeyValue->AsObject();
        if (!KeyObject.IsValid())
        {
            TotalFailure++;
            continue;
        }

        FString MorphTargetName = KeyObject->GetStringField(TEXT("shape_key_name"));
        if (MorphTargetName.IsEmpty())
        {
            TotalFailure++;
            continue;
        }

        TArray<TSharedPtr<FJsonValue>> Keyframes = KeyObject->GetArrayField(TEXT("keyframes"));
        if (Keyframes.Num() == 0)
        {
            TotalFailure++;
            continue;
        }

        // 解析关键帧数据
        TArray<FFrameNumber> NewFrameNumbers;
        TArray<float> NewValues;
        
        for (const TSharedPtr<FJsonValue>& KeyframeValue : Keyframes)
        {
            TSharedPtr<FJsonObject> KeyframeObj = KeyframeValue->AsObject();
            if (KeyframeObj.IsValid())
            {
                float Frame = KeyframeObj->GetNumberField(TEXT("frame"));
                float Value = KeyframeObj->GetNumberField(TEXT("shape_key_value"));

                // 转换帧数
                float ScaledFrameNumberFloat =
                    Frame * TickResolution.Numerator * DisplayRate.Denominator /
                    (TickResolution.Denominator * DisplayRate.Numerator);
                int32 ScaledFrameNumber = static_cast<int32>(ScaledFrameNumberFloat);

                FFrameNumber FrameNumber(ScaledFrameNumber);

                NewFrameNumbers.Add(FrameNumber);
                NewValues.Add(Value);
            }
        }

        // 🔥 核心修复：完善FMorphTargetKeyframeData的追加逻辑
        if (MorphTargetKeyframeData.Contains(MorphTargetName))
        {
            // 追加数据而不是替换 - 这是关键修复
            FMorphTargetKeyframeData& ExistingData = MorphTargetKeyframeData[MorphTargetName];
            
            // 追加新的帧号和值
            ExistingData.FrameNumbers.Append(NewFrameNumbers);
            ExistingData.Values.Append(NewValues);
        }
        else
        {
            // 第一次遇到此 MorphTargetName，创建新条目
            FMorphTargetKeyframeData NewData(MorphTargetName);
            NewData.FrameNumbers = NewFrameNumbers;
            NewData.Values = NewValues;
            
            MorphTargetKeyframeData.Add(MorphTargetName, NewData);
            TotalSuccess++;
        }
    }

    // 4. 将Map转换为数组
    for (const auto& Pair : MorphTargetKeyframeData)
    {
        OutKeyframeData.Add(Pair.Value);
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Collected %d unique morph targets, %d failed"),
           TotalSuccess, TotalFailure);

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Parsed %d morph targets from JSON"),
           OutKeyframeData.Num());

    return OutKeyframeData.Num() > 0;
}

int32 UInstrumentMorphTargetUtility::WriteMorphTargetKeyframes(
    UMovieSceneSection* Section,
    const TArray<FMorphTargetKeyframeData>& KeyframeData)
{
    if (!Section)
    {
        UE_LOG(LogTemp, Error, TEXT("[InstrumentMorphTargetUtility] Section is null"));
        return 0;
    }

    if (KeyframeData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InstrumentMorphTargetUtility] KeyframeData is empty"));
        return 0;
    }

    FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
    int32 SuccessCount = 0;

    for (const FMorphTargetKeyframeData& Data : KeyframeData)
    {
        if (Data.MorphTargetName.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("[InstrumentMorphTargetUtility] Skipping keyframe data with empty morph target name"));
            continue;
        }

        if (Data.FrameNumbers.Num() != Data.Values.Num())
        {
            UE_LOG(LogTemp, Error,
                   TEXT("[InstrumentMorphTargetUtility] FrameNumbers and Values count mismatch for '%s': %d vs %d"),
                   *Data.MorphTargetName, Data.FrameNumbers.Num(), Data.Values.Num());
            continue;
        }

        if (Data.FrameNumbers.Num() == 0)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] No keyframes to write for '%s'"),
                   *Data.MorphTargetName);
            continue;
        }

        // 查找对应的Float Channel
        FName ChannelName(*Data.MorphTargetName);
        TMovieSceneChannelHandle<FMovieSceneFloatChannel> ChannelHandle =
            ChannelProxy.GetChannelByName<FMovieSceneFloatChannel>(ChannelName);

        FMovieSceneFloatChannel* FloatChannel = ChannelHandle.Get();

        // 🔥 关键修复：如果通过名称找不到，尝试通过元数据查找
        if (!FloatChannel)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] Channel '%s' not found by name, trying metadata search"),
                   *Data.MorphTargetName);

            // 遍历所有通道条目，寻找匹配的FloatChannel
            TArrayView<const FMovieSceneChannelEntry> AllEntries = ChannelProxy.GetAllEntries();
            
            for (const FMovieSceneChannelEntry& Entry : AllEntries)
            {
                if (Entry.GetChannelTypeName() == FMovieSceneFloatChannel::StaticStruct()->GetFName())
                {
                    TArrayView<FMovieSceneChannel* const> ChannelsInEntry = Entry.GetChannels();
                    
#if WITH_EDITOR
                    TArrayView<const FMovieSceneChannelMetaData> MetaDataArray = Entry.GetMetaData();
                    
                    for (int32 i = 0; i < ChannelsInEntry.Num() && i < MetaDataArray.Num(); ++i)
                    {
                        const FMovieSceneChannelMetaData& MetaData = MetaDataArray[i];
                        
                        // 🔥 关键：查找名为 "Pressed" 的通道
                        if (MetaData.Name.ToString() == TEXT("Pressed"))
                        {
                            FloatChannel = static_cast<FMovieSceneFloatChannel*>(ChannelsInEntry[i]);
                            UE_LOG(LogTemp, Warning,
                                   TEXT("[InstrumentMorphTargetUtility] ✓ Found 'Pressed' channel via metadata"));
                            break;
                        }
                    }
#endif
                }
                
                if (FloatChannel)
                {
                    break;
                }
            }
        }

        if (!FloatChannel)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[InstrumentMorphTargetUtility] Channel '%s' not found after all search methods"),
                   *Data.MorphTargetName);
            continue;
        }

        // 转换Values为FMovieSceneFloatValue数组
        TArray<FMovieSceneFloatValue> FloatValues;
        FloatValues.Reserve(Data.Values.Num());
        for (float Value : Data.Values)
        {
            FloatValues.Add(FMovieSceneFloatValue(Value));
        }

        // 批量写入关键帧
        FloatChannel->AddKeys(Data.FrameNumbers, FloatValues);

        SuccessCount++;

        UE_LOG(LogTemp, Log,
               TEXT("[InstrumentMorphTargetUtility] Wrote %d keyframes for '%s'"),
               Data.FrameNumbers.Num(), *Data.MorphTargetName);
    }

    UE_LOG(LogTemp, Log,
           TEXT("[InstrumentMorphTargetUtility] Wrote keyframes for %d morph targets"),
           SuccessCount);

    return SuccessCount;
}
