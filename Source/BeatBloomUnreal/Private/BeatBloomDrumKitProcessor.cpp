#include "BeatBloomDrumKitProcessor.h"

#include "Animation/SkeletalMeshActor.h"
#include "BeatBloomUnreal.h"
#include "Components/SkeletalMeshComponent.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "LevelSequence.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "BeatBloomDrumKitProcessor"

void UBeatBloomDrumKitProcessor::InitializeDrumKit(
    ABeatBloomUnreal* BeatBloomActor) {
    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloomActor is null in InitializeDrumKit"));
        return;
    }
    
    if (!BeatBloomActor->DrumKit) {
        UE_LOG(LogTemp, Error, TEXT("DrumKit is not assigned in BeatBloomActor"));
        return;
    }
    
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeDrumKit Started =========="));
    
#if WITH_EDITOR
    // 触发 ControlRig 注册
    BeatBloomActor->TriggerControlRigReregistration(TEXT("Initialize DrumKit"));
    
    // 获取 Control Rig Instance 和 Blueprint
    UControlRig* ControlRigInstance =
        BeatBloomActor->GetCachedControlRig(TEXT("DrumKit"));
    UControlRigBlueprint* ControlRigBlueprint =
        BeatBloomActor->GetCachedControlRigBlueprint(TEXT("DrumKit"));
    
    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get ControlRig for DrumKit"));
        return;
    }
    
    // 步骤 1: 获取所有 MorphTarget 名称
    TArray<FString> MorphTargetNames;
    USkeletalMeshComponent* SkeletalMeshComp =
        BeatBloomActor->DrumKit->GetSkeletalMeshComponent();
    
    if (!SkeletalMeshComp || !SkeletalMeshComp->GetSkeletalMeshAsset()) {
        UE_LOG(LogTemp, Error, TEXT("Invalid SkeletalMeshComponent"));
        return;
    }
    
    if (!UInstrumentMorphTargetUtility::GetMorphTargetNames(
            SkeletalMeshComp, MorphTargetNames)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get MorphTarget names"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Found %d MorphTargets on DrumKit"),
           MorphTargetNames.Num());
    
    // 步骤 2: 确保 Root Control 存在
    if (!UInstrumentMorphTargetUtility::EnsureRootControlExists(
            ControlRigBlueprint, TEXT("drumkit_control"))) {
        UE_LOG(LogTemp, Error, TEXT("====== INITIALIZATION FAILED ======"));
        UE_LOG(LogTemp, Error,
               TEXT("Root Control 'drumkit_control' does not exist in Control Rig Blueprint"));
        UE_LOG(LogTemp, Error, TEXT(""));
        UE_LOG(LogTemp, Error,
               TEXT("Please manually create the Root Control 'drumkit_control' in your Control Rig Blueprint:"));
        UE_LOG(LogTemp, Error, TEXT("  1. Open the Control Rig Blueprint"));
        UE_LOG(LogTemp, Error, TEXT("  2. Go to the Hierarchy panel"));
        UE_LOG(LogTemp, Error,
               TEXT("  3. Right-click and create a new Control named 'drumkit_control'"));
        UE_LOG(LogTemp, Error, TEXT("  4. Set the Control Type to 'Transform'"));
        UE_LOG(LogTemp, Error, TEXT("  5. Save the Blueprint and try again"));
        UE_LOG(LogTemp, Error, TEXT("====== END OF ERROR REPORT ======"));
        return;
    }
    
    // 步骤 3: 批量添加动画通道
    FRigElementKey RootControlKey(TEXT("drumkit_control"), ERigElementType::Control);
    
    if (MorphTargetNames.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("No MorphTargets found on DrumKit"));
        return;
    }
    
    int32 ChannelsAdded = UInstrumentMorphTargetUtility::AddAnimationChannels(
        ControlRigBlueprint, RootControlKey, MorphTargetNames);
    
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeDrumKit Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Successfully created/verified: %d channels"),
           ChannelsAdded);
    UE_LOG(LogTemp, Warning, TEXT("Expected total: %d MorphTargets"),
           MorphTargetNames.Num());
    UE_LOG(LogTemp, Warning,
           TEXT("========== InitializeDrumKit Completed =========="));
#endif
}

void UBeatBloomDrumKitProcessor::GenerateDrumKitAnimation(
    ABeatBloomUnreal* BeatBloomActor) {
    // 从 .beatbloom 文件中读取 shape_key_animation_path
    FString PerformerAnimationPath;
    FString DrumKitAnimationPath;
    
    if (!ABeatBloomUnreal::ParseBeatBloomFile(
            BeatBloomActor->AnimationFilePath,
            PerformerAnimationPath,
            DrumKitAnimationPath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse .beatbloom file: %s"), *BeatBloomActor->AnimationFilePath);
        return;
    }
    
    // 委托给 GenerateDrumKitAnimationFromPath
    GenerateDrumKitAnimationFromPath(BeatBloomActor, DrumKitAnimationPath);
}

void UBeatBloomDrumKitProcessor::GenerateDrumKitAnimationFromPath(
    ABeatBloomUnreal* BeatBloomActor,
    const FString& ShapeKeyAnimationPath) {
    if (!BeatBloomActor) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloomActor is null"));
        return;
    }
    
    if (ShapeKeyAnimationPath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("ShapeKeyAnimationPath is empty"));
        return;
    }
    
    if (!BeatBloomActor->DrumKit) {
        UE_LOG(LogTemp, Error, TEXT("DrumKit is not assigned in BeatBloomActor"));
        return;
    }
    
    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateDrumKitAnimation Started =========="));
    
#if WITH_EDITOR
    // ========== 读取 JSON 文件 ==========
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *ShapeKeyAnimationPath)) {
        UE_LOG(LogTemp, Error,
               TEXT("[BeatBloomDrumKitProcessor] Failed to load JSON file: %s"),
               *ShapeKeyAnimationPath);
        return;
    }
    
    // 解析 JSON（顶层是数组）
    TArray<TSharedPtr<FJsonValue>> DrumKitDataArray;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
    
    if (!FJsonSerializer::Deserialize(Reader, DrumKitDataArray)) {
        UE_LOG(LogTemp, Error,
               TEXT("[BeatBloomDrumKitProcessor] Failed to parse JSON"));
        return;
    }
    
    if (DrumKitDataArray.Num() == 0) {
        UE_LOG(LogTemp, Error,
               TEXT("[BeatBloomDrumKitProcessor] No drum kit data found"));
        return;
    }
    
    // ========== 获取 LevelSequence 和 Sequencer ==========
    ULevelSequence* LevelSequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    
    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            LevelSequence, Sequencer)) {
        return;
    }
    
    // ========== 处理每个鼓件的数据 ==========
    TArray<FMorphTargetKeyframeData> KeyframeData;
    
    for (const auto& DrumKitValue : DrumKitDataArray) {
        TSharedPtr<FJsonObject> DrumKitObject = DrumKitValue->AsObject();
        if (!DrumKitObject.IsValid()) {
            continue;
        }
        
        // 获取 drum_kit 名称
        FString DrumKitName;
        if (!DrumKitObject->TryGetStringField(TEXT("drum_kit"), DrumKitName)) {
            UE_LOG(LogTemp, Warning, TEXT("Missing drum_kit field"));
            continue;
        }
        
        // 构造 MorphTarget 名称：{drum_kit}_beat
        FString MorphTargetName = FString::Printf(TEXT("%s_beat"), *DrumKitName);
        
        // 获取 animation_data
        if (!DrumKitObject->HasField(TEXT("animation_data"))) {
            UE_LOG(LogTemp, Warning, 
                   TEXT("Drum kit %s has no animation_data, skipping"), *DrumKitName);
            continue;
        }
        
        auto AnimationDataArray = DrumKitObject->GetArrayField(TEXT("animation_data"));
        
        if (AnimationDataArray.Num() == 0) {
            UE_LOG(LogTemp, Warning, 
                   TEXT("Drum kit %s has empty animation_data, skipping"), *DrumKitName);
            continue;
        }
        
        // 处理关键帧数据
        FFrameRate TickResolution = LevelSequence->GetMovieScene()->GetTickResolution();
        FFrameRate DisplayRate = LevelSequence->GetMovieScene()->GetDisplayRate();
        
        // 为当前鼓件创建临时数组
        TArray<FMorphTargetKeyframeData> SingleKeyframeData;
        
        for (const auto& FrameValue : AnimationDataArray) {
            TSharedPtr<FJsonObject> FrameObject = FrameValue->AsObject();
            if (!FrameObject.IsValid()) {
                continue;
            }
            
            double FrameNumberDouble = FrameObject->GetNumberField(TEXT("frame"));
            float Value = static_cast<float>(FrameObject->GetNumberField(TEXT("value")));
            
            // 转换帧编号
            float ScaledFrameNumberFloat =
                FrameNumberDouble * TickResolution.AsDecimal() / DisplayRate.AsDecimal();
            int32 ScaledFrameNumber = static_cast<int32>(ScaledFrameNumberFloat);
            FFrameNumber FrameNumber(ScaledFrameNumber);
            
            // 添加关键帧
            FMorphTargetKeyframeData Data;
            Data.MorphTargetName = MorphTargetName;
            Data.FrameNumbers.Add(FrameNumber);
            Data.Values.Add(Value);
            
            SingleKeyframeData.Add(Data);
        }
        
        if (SingleKeyframeData.Num() > 0) {
            UE_LOG(LogTemp, Warning, TEXT("Processed %d frames for %s"), 
                   SingleKeyframeData[0].FrameNumbers.Num(), *MorphTargetName);
            KeyframeData.Append(SingleKeyframeData);
        }
    }
    
    if (KeyframeData.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("No morph target data found in JSON"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Loaded %d morph target entries from JSON"),
           KeyframeData.Num());
    
    // ========== 写入 MorphTarget 动画 ==========
    int32 WrittenTargets =
        UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig(
            BeatBloomActor->DrumKit, KeyframeData, LevelSequence,
            TEXT("drumkit_control"));
    
    if (WrittenTargets > 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("✓ Successfully wrote %d morph target animations"),
               WrittenTargets);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("✗ Failed to write morph target animations"));
        return;
    }
    
    UE_LOG(LogTemp, Warning,
           TEXT("========== GenerateDrumKitAnimation Completed =========="));
#endif
}

USkeletalMeshComponent* UBeatBloomDrumKitProcessor::FindMorphTargetOwner(
    ABeatBloomUnreal* BeatBloomActor,
    const FString& MorphTargetName) {
    // 策略 A - 直接返回 DrumKit 的主 SkeletalMeshComponent
    if (!BeatBloomActor || !BeatBloomActor->DrumKit) {
        return nullptr;
    }
    
    USkeletalMeshComponent* SkeletalMeshComp = 
        BeatBloomActor->DrumKit->GetSkeletalMeshComponent();
    
    if (!SkeletalMeshComp) {
        return nullptr;
    }
    
    // 验证是否包含该 MorphTarget
    if (SkeletalMeshComp->GetSkeletalMeshAsset()) {
        const TArray<UMorphTarget*>& MorphTargets = SkeletalMeshComp->GetSkeletalMeshAsset()->GetMorphTargets();
        for (const UMorphTarget* MorphTarget : MorphTargets) {
            if (MorphTarget && MorphTarget->GetName() == MorphTargetName) {
                return SkeletalMeshComp;
            }
        }
    }
    
    // 如果不包含，返回 nullptr（可能需要策略 B 遍历查找）
    return nullptr;
}

bool UBeatBloomDrumKitProcessor::WriteDrumKitMorphTargetAnimation(
    ABeatBloomUnreal* BeatBloomActor,
    ULevelSequence* LevelSequence,
    ASkeletalMeshActor* DrumKitActor,
    const FString& MorphTargetName,
    const TArray<TSharedPtr<FJsonValue>>& AnimationDataArray) {
    if (!BeatBloomActor || !DrumKitActor || !LevelSequence) {
        UE_LOG(LogTemp, Error, TEXT("Invalid parameters"));
        return false;
    }
    
    if (AnimationDataArray.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("Empty animation data for %s"), *MorphTargetName);
        return false;
    }
    
    // 查找 MorphTarget 所有者
    USkeletalMeshComponent* SkeletalMeshComp = 
        FindMorphTargetOwner(BeatBloomActor, MorphTargetName);
    
    if (!SkeletalMeshComp) {
        UE_LOG(LogTemp, Error, TEXT("MorphTarget %s not found"), *MorphTargetName);
        return false;
    }
    
    // 准备关键帧数据
    FFrameRate TickResolution = LevelSequence->GetMovieScene()->GetTickResolution();
    FFrameRate DisplayRate = LevelSequence->GetMovieScene()->GetDisplayRate();
    
    FMorphTargetKeyframeData KeyframeData;
    KeyframeData.MorphTargetName = MorphTargetName;
    
    for (const auto& FrameValue : AnimationDataArray) {
        TSharedPtr<FJsonObject> FrameObject = FrameValue->AsObject();
        if (!FrameObject.IsValid()) {
            continue;
        }
        
        double FrameNumberDouble = FrameObject->GetNumberField(TEXT("frame"));
        float Value = static_cast<float>(FrameObject->GetNumberField(TEXT("value")));
        
        // 转换帧编号
        float ScaledFrameNumberFloat =
            FrameNumberDouble * TickResolution.AsDecimal() / DisplayRate.AsDecimal();
        int32 ScaledFrameNumber = static_cast<int32>(ScaledFrameNumberFloat);
        FFrameNumber FrameNumber(ScaledFrameNumber);
        
        KeyframeData.FrameNumbers.Add(FrameNumber);
        KeyframeData.Values.Add(Value);
    }
    
    if (KeyframeData.FrameNumbers.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("No valid frames for %s"), *MorphTargetName);
        return false;
    }
    
    // 写入动画
    TArray<FMorphTargetKeyframeData> SingleKeyframeData;
    SingleKeyframeData.Add(KeyframeData);
    
    int32 WrittenCount = UInstrumentMorphTargetUtility::WriteMorphTargetAnimationToControlRig(
        DrumKitActor, SingleKeyframeData, LevelSequence, TEXT("drumkit_control"));
    
    return WrittenCount > 0;
}

#undef LOCTEXT_NAMESPACE
