#pragma once

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "CoreMinimal.h"
#include "LevelSequence.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "ControlRigCacheTypes.generated.h"

/**
 * ControlRig缓存键：Actor名称 + LevelSequence组合
 */
USTRUCT()
struct FControlRigCacheKey {
    GENERATED_BODY()

   public:
    /** SkeletalMeshActor名称 */
    UPROPERTY()
    FString ActorName;

    /** LevelSequence指针 */
    UPROPERTY()
    TWeakObjectPtr<ULevelSequence> LevelSequence;

    /** 构造函数 */
    FControlRigCacheKey() {}

    FControlRigCacheKey(const FString& InActorName, ULevelSequence* InSequence)
        : ActorName(InActorName), LevelSequence(InSequence) {}

    /** 比较运算符 */
    bool operator==(const FControlRigCacheKey& Other) const {
        return ActorName == Other.ActorName &&
               LevelSequence == Other.LevelSequence;
    }

    bool operator!=(const FControlRigCacheKey& Other) const {
        return !(*this == Other);
    }

    /** 有效性检查 */
    bool IsValid() const {
        return !ActorName.IsEmpty() && LevelSequence.IsValid();
    }
};

/** 为FControlRigCacheKey生成哈希函数 */
inline uint32 GetTypeHash(const FControlRigCacheKey& Key) {
    return GetTypeHash(Key.ActorName) ^ GetTypeHash(Key.LevelSequence);
}

/**
 * ControlRig运行时缓存条目
 * 使用ControlRigSequencerBindingProxy保存完整绑定信息
 */
USTRUCT()
struct FControlRigRuntimeCacheEntry {
    GENERATED_BODY()

   public:
    /** ControlRig绑定代理 - 包含完整的绑定信息 */
    UPROPERTY()
    FControlRigSequencerBindingProxy ControlRigProxy;

    /** ControlRig实例指针（运行时缓存） */
    UPROPERTY()
    TWeakObjectPtr<UControlRig> CachedControlRig;

    /** ControlRig Blueprint指针（运行时缓存） */
    UPROPERTY()
    TWeakObjectPtr<UControlRigBlueprint> CachedBlueprint;

    /** 缓存创建时间 */
    UPROPERTY()
    double CreationTime;

    /** 上次访问时间 */
    UPROPERTY()
    double LastAccessTime;

    /** 构造函数 */
    FControlRigRuntimeCacheEntry() : CreationTime(0.0), LastAccessTime(0.0) {}

    FControlRigRuntimeCacheEntry(
        const FControlRigSequencerBindingProxy& InProxy)
        : ControlRigProxy(InProxy),
          CreationTime(FPlatformTime::Seconds()),
          LastAccessTime(FPlatformTime::Seconds()) {
        // 初始化时同时缓存实例指针
        UpdateCachedPointers();
    }

    /** 有效性检查 - 更严格的指针验证 */
    bool IsValid() const {
        // 检查代理中的ControlRig是否有效
        if (ControlRigProxy.ControlRig == nullptr ||
            !ControlRigProxy.ControlRig->IsValidLowLevelFast()) {
            return false;
        }

        // 检查轨道是否有效
        if (ControlRigProxy.Track == nullptr ||
            !ControlRigProxy.Track->IsValidLowLevelFast()) {
            return false;
        }

        // 检查缓存的ControlRig指针是否有效
        if (!CachedControlRig.IsValid() ||
            !CachedControlRig->IsValidLowLevelFast()) {
            return false;
        }

        // 检查Blueprint指针是否有效
        if (!CachedBlueprint.IsValid() ||
            !CachedBlueprint->IsValidLowLevelFast()) {
            return false;
        }

        return true;
    }

    /** 更新访问时间 */
    void UpdateAccessTime() { LastAccessTime = FPlatformTime::Seconds(); }

    /** 从代理更新缓存的指针 - 带有效性验证 */
    void UpdateCachedPointers() {
        // 验证代理中的ControlRig是否有效
        if (ControlRigProxy.ControlRig &&
            ControlRigProxy.ControlRig->IsValidLowLevelFast()) {
            CachedControlRig = ControlRigProxy.ControlRig;

            // 获取并验证Blueprint
            UObject* GeneratedBy =
                ControlRigProxy.ControlRig->GetClass()->ClassGeneratedBy;
            UControlRigBlueprint* Blueprint =
                Cast<UControlRigBlueprint>(GeneratedBy);

            if (Blueprint && Blueprint->IsValidLowLevelFast()) {
                CachedBlueprint = Blueprint;
            } else {
                CachedBlueprint = nullptr;
            }
        } else {
            CachedControlRig = nullptr;
            CachedBlueprint = nullptr;
        }
    }

    /** 获取ControlRig实例 */
    UControlRig* GetControlRig() const { return CachedControlRig.Get(); }

    /** 获取ControlRig Blueprint */
    UControlRigBlueprint* GetBlueprint() const { return CachedBlueprint.Get(); }

    /** 获取绑定代理 */
    const FControlRigSequencerBindingProxy& GetProxy() const {
        return ControlRigProxy;
    }
    
    /** 检查ControlRig是否仍然绑定到指定的Actor */
    bool IsBoundToActor(ASkeletalMeshActor* TargetActor, ULevelSequence* TargetSequence) const;
};