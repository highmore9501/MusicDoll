// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "LevelSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "ControlRigCacheTypes.h"
#include "ControlRigCacheSubsystem.generated.h"

/**
 * 通用ControlRig缓存子系统
 * 提供基于SkeletalMeshActor和LevelSequence的ControlRig查询服务
 */
UCLASS()
class MUSICDOLLCOMMON_API UControlRigCacheSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()

public:
    //~ Begin USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    //~ End USubsystem interface

    /**
     * 获取ControlRig实例
     * @param Actor SkeletalMeshActor
     * @param Sequence LevelSequence
     * @return ControlRig实例，如果未找到或无效则返回nullptr
     */
    UControlRig* GetControlRig(ASkeletalMeshActor* Actor, ULevelSequence* Sequence);

    /**
     * 获取ControlRig实例（支持RootControlName筛选）
     * @param Actor SkeletalMeshActor
     * @param Sequence LevelSequence
     * @param RootControlName 根控制器名称，用于在多个ControlRig中筛选
     * @return ControlRig实例，如果未找到或无效则返回nullptr
     */
    UControlRig* GetControlRig(ASkeletalMeshActor* Actor, ULevelSequence* Sequence, const FString& RootControlName);

    /**
     * 获取ControlRig Blueprint
     * @param Actor SkeletalMeshActor
     * @param Sequence LevelSequence
     * @return ControlRig Blueprint，如果未找到或无效则返回nullptr
     */
    UControlRigBlueprint* GetControlRigBlueprint(ASkeletalMeshActor* Actor, ULevelSequence* Sequence);

    /**
     * 获取ControlRig Blueprint（支持RootControlName筛选）
     * @param Actor SkeletalMeshActor
     * @param Sequence LevelSequence
     * @param RootControlName 根控制器名称，用于在多个ControlRig中筛选
     * @return ControlRig Blueprint，如果未找到或无效则返回nullptr
     */
    UControlRigBlueprint* GetControlRigBlueprint(ASkeletalMeshActor* Actor, ULevelSequence* Sequence, const FString& RootControlName);

    /**
     * 注册ControlRig信息
     * @param Actor SkeletalMeshActor
     * @param Sequence LevelSequence
     * @param ControlRig ControlRig实例
     * @param Blueprint ControlRig Blueprint
     */
    void RegisterControlRig(ASkeletalMeshActor* Actor, ULevelSequence* Sequence, 
                           UControlRig* ControlRig, UControlRigBlueprint* Blueprint);

    /**
     * 清除所有缓存
     */
    void ClearAllCaches();

    /**
     * 触发注册机制（当缓存失效时调用）
     * @param Actor SkeletalMeshActor
     * @param Sequence LevelSequence
     * @param RootControlName 可选的根控制器名称，用于在多个ControlRig中筛选
     */
    void TriggerRegistrationIfNeeded(ASkeletalMeshActor* Actor, ULevelSequence* Sequence,
                                    const FString& RootControlName = TEXT(""));

private:
    /** 运行时缓存映射表 */
    UPROPERTY()
    TMap<FControlRigCacheKey, FControlRigRuntimeCacheEntry> RuntimeControlRigCache;

    /**
     * 从Actor和Sequence动态查找ControlRig
     * @param Actor SkeletalMeshActor
     * @param Sequence LevelSequence
     * @param OutControlRig 输出ControlRig实例
     * @param OutBlueprint 输出ControlRig Blueprint
     * @param RootControlName 可选的根控制器名称，用于在多个ControlRig中筛选
     * @return 是否成功找到
     */
    bool FindControlRigFromActorAndSequence(ASkeletalMeshActor* Actor, ULevelSequence* Sequence,
                                          UControlRig*& OutControlRig, UControlRigBlueprint*& OutBlueprint,
                                          const FString& RootControlName = TEXT(""));
    
    
    
    /**
     * 通过Actor名称和Sequence查找ControlRig绑定代理
     * @param ActorName Actor名称
     * @param Sequence LevelSequence
     * @return ControlRig的绑定代理
     */
    FControlRigSequencerBindingProxy FindControlRigProxy(const FString& ActorName, ULevelSequence* Sequence);

    /**
     * 从SkeletalMeshActor获取绑定的ControlRig实例和蓝图
     * 这是原始FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor的迁移版本
     * 
     * @param Actor 要查询的SkeletalMeshActor
     * @param Sequence 当前LevelSequence（已由调用方提供）
     * @param OutControlRigInstance 输出参数：ControlRig实例指针
     * @param OutControlRigBlueprint 输出参数：ControlRig蓝图指针
     * @param RootControlName 可选的根控制器名称，用于在多个ControlRig中筛选
     *                        如果为空，则返回第一个找到的ControlRig
     *                        如果不为空，则优先返回包含该根控制器的ControlRig
     * @return 是否成功获取ControlRig
     */
    bool GetControlRigFromSkeletalMeshActor(
        ASkeletalMeshActor* Actor,
        ULevelSequence* Sequence,
        UControlRig*& OutControlRigInstance,
        UControlRigBlueprint*& OutControlRigBlueprint,
        const FString& RootControlName = TEXT(""));
};