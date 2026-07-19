#pragma once

#include "CoreMinimal.h"
#include "LipSyncTypes.generated.h"

/**
 * 口型字母到 Morph Target 的映射对
 * 参照 FBoneControlPair 的设计，存储在 ControlRigBlueprint 变量中
 */
USTRUCT(BlueprintType, Blueprintable)
struct MUSICDOLLCOMMON_API FLipSyncMappingPair {
    GENERATED_BODY()

    /** 口型字母：A ~ K, X（兼容 Lisa 8 符号 + Cherry 12 符号） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync Mapping")
    FString Phoneme;

    /** 对应的 Morph Target 名称（如 "mouth_open", "lips_smile"） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lip Sync Mapping")
    FString MorphTargetName;

    FLipSyncMappingPair() : Phoneme(TEXT("")), MorphTargetName(TEXT("")) {}

    FLipSyncMappingPair(const FString& InPhoneme,
                        const FString& InMorphTargetName)
        : Phoneme(InPhoneme), MorphTargetName(InMorphTargetName) {}

    bool operator==(const FLipSyncMappingPair& Other) const {
        return Phoneme == Other.Phoneme &&
               MorphTargetName == Other.MorphTargetName;
    }
};

/**
 * JSON 口型文件中的一个 cue 片段
 * 纯 C++ 结构，不需要暴露给蓝图
 */
USTRUCT()
struct MUSICDOLLCOMMON_API FLipSyncMouthCue {
    GENERATED_BODY()

    /** 开始时间（秒） */
    UPROPERTY()
    float Start = 0.0f;

    /** 结束时间（秒） */
    UPROPERTY()
    float End = 0.0f;

    /** 口型字母：A ~ K, X（兼容 Lisa 8 符号 + Cherry 12 符号） */
    UPROPERTY()
    FString Value;

    FLipSyncMouthCue() : Start(0.0f), End(0.0f), Value(TEXT("")) {}

    FLipSyncMouthCue(float InStart, float InEnd, const FString& InValue)
        : Start(InStart), End(InEnd), Value(InValue) {}

    /** 获取 cue 的持续时间 */
    float GetDuration() const { return End - Start; }
};
