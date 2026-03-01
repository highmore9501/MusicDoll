#include "ControlRigCacheSubsystem.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ISequencer.h"
#include "InstrumentControlRigUtility.h"
#include "LevelEditor.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Sequencer/ControlRigSequencerHelpers.h"

void UControlRigCacheSubsystem::Initialize(
    FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log,
           TEXT("UControlRigCacheSubsystem initialized with runtime cache"));
}

void UControlRigCacheSubsystem::Deinitialize() {
    ClearAllCaches();
    UE_LOG(LogTemp, Log, TEXT("UControlRigCacheSubsystem deinitialized"));
    Super::Deinitialize();
}

UControlRig* UControlRigCacheSubsystem::GetControlRig(
    ASkeletalMeshActor* Actor, ULevelSequence* Sequence) {
    if (!Actor || !Sequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetControlRig: Invalid input parameters"));
        return nullptr;
    }

    FString ActorName = Actor->GetName();
    FControlRigCacheKey CacheKey(ActorName, Sequence);

    // 检查运行时缓存中是否存在
    FControlRigRuntimeCacheEntry* CacheEntry =
        RuntimeControlRigCache.Find(CacheKey);
    if (CacheEntry) {
        // 第一层：检查缓存条目是否有效
        if (CacheEntry->IsValid()) {
            CacheEntry->UpdateAccessTime();
            // 再次验证返回的ControlRig指针是否真的可用
            UControlRig* ResultControlRig = CacheEntry->GetControlRig();
            return ResultControlRig;
        }
    }

    // 缓存中没有找到或需要完全重建，触发注册
    UE_LOG(LogTemp, Warning,
           TEXT("GetControlRig: No valid cache entry found for Actor %s, "
                "triggering registration"),
           *ActorName);
    TriggerRegistrationIfNeeded(Actor, Sequence);
    return nullptr;
}

UControlRigBlueprint* UControlRigCacheSubsystem::GetControlRigBlueprint(
    ASkeletalMeshActor* Actor, ULevelSequence* Sequence) {
    UControlRig* ControlRig = GetControlRig(Actor, Sequence);
    if (!ControlRig) {
        return nullptr;
    }

    // 从ControlRig实例获取Blueprint
    UClass* ControlRigClass = ControlRig->GetClass();
    if (ControlRigClass) {
        UObject* ClassGeneratedBy = ControlRigClass->ClassGeneratedBy;
        return Cast<UControlRigBlueprint>(ClassGeneratedBy);
    }

    return nullptr;
}

void UControlRigCacheSubsystem::RegisterControlRig(
    ASkeletalMeshActor* Actor, ULevelSequence* Sequence,
    UControlRig* ControlRig, UControlRigBlueprint* Blueprint) {
    if (!Actor || !Sequence || !ControlRig) {
        UE_LOG(LogTemp, Warning,
               TEXT("RegisterControlRig: Invalid input parameters"));
        return;
    }

    FString ActorName = Actor->GetName();
    FControlRigCacheKey CacheKey(ActorName, Sequence);

    // 查找ControlRig绑定代理
    FControlRigSequencerBindingProxy ControlRigProxy =
        FindControlRigProxy(ActorName, Sequence);
    if (ControlRigProxy.ControlRig == nullptr ||
        ControlRigProxy.Track == nullptr) {
        UE_LOG(LogTemp, Warning,
               TEXT("RegisterControlRig: Failed to find valid ControlRig proxy "
                    "for Actor %s"),
               *ActorName);
        return;
    }

    FControlRigRuntimeCacheEntry CacheEntry(ControlRigProxy);
    RuntimeControlRigCache.FindOrAdd(CacheKey) = CacheEntry;

    UE_LOG(LogTemp, Log,
           TEXT("Registered/Updated runtime ControlRig for Actor %s with "
                "Sequence %s using ControlRigSequencerBindingProxy"),
           *ActorName, *Sequence->GetName());
}

void UControlRigCacheSubsystem::ClearAllCaches() {
    int32 ClearedCount = RuntimeControlRigCache.Num();
    RuntimeControlRigCache.Empty();
    UE_LOG(LogTemp, Log, TEXT("Cleared all %d runtime cache entries"),
           ClearedCount);
}

bool UControlRigCacheSubsystem::FindControlRigFromActorAndSequence(
    ASkeletalMeshActor* Actor, ULevelSequence* Sequence,
    UControlRig*& OutControlRig, UControlRigBlueprint*& OutBlueprint) {
    OutControlRig = nullptr;
    OutBlueprint = nullptr;

    if (!Actor || !Sequence) {
        return false;
    }

    // 使用Subsystem内部的方法查找ControlRig
    return GetControlRigFromSkeletalMeshActor(Actor, Sequence, OutControlRig,
                                              OutBlueprint);
}

bool UControlRigCacheSubsystem::GetControlRigFromSkeletalMeshActor(
    ASkeletalMeshActor* Actor, ULevelSequence* Sequence,
    UControlRig*& OutControlRigInstance,
    UControlRigBlueprint*& OutControlRigBlueprint) {
    OutControlRigInstance = nullptr;
    OutControlRigBlueprint = nullptr;

    if (!Actor) {
        UE_LOG(LogTemp, Error,
               TEXT("UControlRigCacheSubsystem::"
                    "GetControlRigFromSkeletalMeshActor: "
                    "Actor is null"));
        return false;
    }

    if (!Sequence) {
        UE_LOG(LogTemp, Error,
               TEXT("UControlRigCacheSubsystem::"
                    "GetControlRigFromSkeletalMeshActor: "
                    "Sequence is null"));
        return false;
    }

    // 获取当前打开的Sequencer
    TArray<TWeakPtr<ISequencer>> WeakSequencers =
        FLevelEditorSequencerIntegration::Get().GetSequencers();

    if (WeakSequencers.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("UControlRigCacheSubsystem::"
                    "GetControlRigFromSkeletalMeshActor: "
                    "No sequencers found"));
        return false;
    }

    // 获取LevelSequence中的所有ControlRig绑定
    TArray<FControlRigSequencerBindingProxy> RigBindings =
        UControlRigSequencerEditorLibrary::GetControlRigs(Sequence);

    if (RigBindings.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("UControlRigCacheSubsystem::"
                    "GetControlRigFromSkeletalMeshActor: "
                    "No ControlRig bindings found in the sequence"));
        return false;
    }

    // 遍历所有ControlRig绑定，查找绑定到指定SkeletalMeshActor的ControlRig
    for (int32 RigIndex = 0; RigIndex < RigBindings.Num(); ++RigIndex) {
        const FControlRigSequencerBindingProxy& Proxy = RigBindings[RigIndex];

        UControlRig* CurrentControlRigInstance = Proxy.ControlRig;

        if (!CurrentControlRigInstance) {
            continue;
        }

        // 遍历打开的Sequencer，检查该ControlRig是否绑定到目标SkeletalMeshActor
        for (int32 SeqIndex = 0; SeqIndex < WeakSequencers.Num(); ++SeqIndex) {
            const TWeakPtr<ISequencer>& WeakSequencer =
                WeakSequencers[SeqIndex];

            if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin()) {
                // 获取该ControlRig绑定的GUID
                FGuid BindingID = Proxy.Proxy.BindingID;

                if (!BindingID.IsValid()) {
                    continue;
                }

                // 查找该绑定ID关联的所有对象
                TArrayView<TWeakObjectPtr<UObject>> WeakBoundObjects =
                    Sequencer->FindBoundObjects(
                        BindingID, Sequencer->GetFocusedTemplateID());

                // 检查目标SkeletalMeshActor是否在绑定的对象中
                for (int32 ObjIndex = 0; ObjIndex < WeakBoundObjects.Num();
                     ++ObjIndex) {
                    const TWeakObjectPtr<UObject>& WeakObj =
                        WeakBoundObjects[ObjIndex];

                    if (WeakObj.IsValid() && WeakObj.Get() == Actor) {
                        // 找到了！设置输出参数
                        OutControlRigInstance = CurrentControlRigInstance;

                        // 获取ControlRig的蓝图
                        UObject* GeneratedBy =
                            OutControlRigInstance->GetClass()->ClassGeneratedBy;
                        OutControlRigBlueprint =
                            Cast<UControlRigBlueprint>(GeneratedBy);

                        if (OutControlRigBlueprint) {
                            UE_LOG(LogTemp, Verbose,
                                   TEXT("UControlRigCacheSubsystem::"
                                        "GetControlRigFromSkeletalMeshActor: "
                                        "Successfully found ControlRig for "
                                        "Actor %s"),
                                   *Actor->GetName());
                            return true;
                        } else {
                            UE_LOG(LogTemp, Warning,
                                   TEXT("UControlRigCacheSubsystem::"
                                        "GetControlRigFromSkeletalMeshActor: "
                                        "Found ControlRig instance but failed "
                                        "to get blueprint for Actor %s"),
                                   *Actor->GetName());
                            return false;
                        }
                    }
                }
            }
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT("UControlRigCacheSubsystem::GetControlRigFromSkeletalMeshActor: "
             "Failed to find ControlRig bound to SkeletalMeshActor '%s'"),
        *Actor->GetName());
    return false;
}

FControlRigSequencerBindingProxy UControlRigCacheSubsystem::FindControlRigProxy(
    const FString& ActorName, ULevelSequence* Sequence) {
    if (ActorName.IsEmpty() || !Sequence) {
        return FControlRigSequencerBindingProxy();
    }

    // 使用ControlRigSequencerEditorLibrary获取所有ControlRig代理
    TArray<FControlRigSequencerBindingProxy> RigProxies =
        UControlRigSequencerEditorLibrary::GetControlRigs(Sequence);

    // 获取当前活跃的Sequencer
    TSharedPtr<ISequencer> Sequencer = nullptr;
    TArray<TWeakPtr<ISequencer>> WeakSequencers =
        FLevelEditorSequencerIntegration::Get().GetSequencers();

    for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
        if (TSharedPtr<ISequencer> CurrentSequencer = WeakSequencer.Pin()) {
            UMovieSceneSequence* RootSequence =
                CurrentSequencer->GetRootMovieSceneSequence();
            if (RootSequence && RootSequence == Sequence) {
                Sequencer = CurrentSequencer;
                break;
            }
        }
    }

    if (!Sequencer.IsValid()) {
        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "FindControlRigProxy: No active sequencer found for sequence"));
        return FControlRigSequencerBindingProxy();
    }

    // 遍历代理查找绑定到目标Actor的ControlRig
    for (const FControlRigSequencerBindingProxy& ProxyInfo : RigProxies) {
        // 通过Sequencer和BindingID获取绑定的对象
        FGuid BindingID = ProxyInfo.Proxy.BindingID;
        if (!BindingID.IsValid()) {
            continue;
        }

        TArrayView<TWeakObjectPtr<UObject>> WeakBoundObjects =
            Sequencer->FindBoundObjects(BindingID,
                                        Sequencer->GetFocusedTemplateID());

        // 检查目标Actor是否在绑定的对象中
        for (const TWeakObjectPtr<UObject>& WeakObj : WeakBoundObjects) {
            if (WeakObj.IsValid()) {
                ASkeletalMeshActor* SkeletalActor =
                    Cast<ASkeletalMeshActor>(WeakObj.Get());
                if (SkeletalActor && SkeletalActor->GetName() == ActorName) {
                    UE_LOG(LogTemp, Log,
                           TEXT("FindControlRigProxy: Found ControlRig proxy "
                                "for Actor %s"),
                           *ActorName);
                    return ProxyInfo;
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("FindControlRigProxy: No ControlRig proxy found for Actor %s"),
           *ActorName);
    return FControlRigSequencerBindingProxy();
}

// 移除旧的GetControlRigFromBindingID和ReconstructControlRigFromPersistentData方法，现在使用ControlRigSequencerBindingProxy

void UControlRigCacheSubsystem::TriggerRegistrationIfNeeded(
    ASkeletalMeshActor* Actor, ULevelSequence* Sequence) {
    if (!Actor || !Sequence) {
        return;
    }

    FString ActorName = Actor->GetName();

    // 检查是否正在进行注册（避免递归）
    static TSet<FString> RegistrationInProgress;
    if (RegistrationInProgress.Contains(ActorName)) {
        UE_LOG(LogTemp, Verbose,
               TEXT("TriggerRegistrationIfNeeded: Registration already in "
                    "progress for Actor %s"),
               *ActorName);
        return;
    }

    RegistrationInProgress.Add(ActorName);

    // 直接尝试查找并注册ControlRig
    UControlRig* ControlRig = nullptr;
    UControlRigBlueprint* Blueprint = nullptr;

    if (FindControlRigFromActorAndSequence(Actor, Sequence, ControlRig,
                                           Blueprint)) {
        RegisterControlRig(Actor, Sequence, ControlRig, Blueprint);
        UE_LOG(LogTemp, Log,
               TEXT("TriggerRegistrationIfNeeded: Successfully registered "
                    "ControlRig for Actor %s"),
               *ActorName);
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("TriggerRegistrationIfNeeded: Failed to find ControlRig "
                    "for Actor %s"),
               *ActorName);
    }

    RegistrationInProgress.Remove(ActorName);
}
