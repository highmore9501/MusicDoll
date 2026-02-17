#include "InstrumentControlRigUtility.h"

#include "Animation/SkeletalMeshActor.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "ISequencer.h"
#include "LevelEditor.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneSequence.h"

bool FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
    ASkeletalMeshActor* InSkeletalMeshActor,
    UControlRig*& OutControlRigInstance,
    UControlRigBlueprint*& OutControlRigBlueprint) {
    OutControlRigInstance = nullptr;
    OutControlRigBlueprint = nullptr;

    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigFromSkeletalMeshActor: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    // 第一步：获取当前打开的 Level Sequence
    ULevelSequence* LevelSequence = nullptr;

    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
        TArray<TWeakPtr<ISequencer>> WeakSequencers =
            FLevelEditorSequencerIntegration::Get().GetSequencers();

        TArray<TSharedPtr<ISequencer>> OpenSequencers;
        for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
            if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin()) {
                OpenSequencers.Add(Sequencer);
            }
        }

        for (const TSharedPtr<ISequencer>& Sequencer : OpenSequencers) {
            UMovieSceneSequence* RootSequence =
                Sequencer->GetRootMovieSceneSequence();

            if (!RootSequence) {
                continue;
            }

            LevelSequence = Cast<ULevelSequence>(RootSequence);
            if (LevelSequence) {
                break;
            }
        }
    }

    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigFromSkeletalMeshActor: "
                    "No Level Sequence is currently open"));
        return false;
    }

    // 第二步：获取 Level Sequence 中的所有 Control Rig 绑定
    TArray<FControlRigSequencerBindingProxy> RigBindings =
        UControlRigSequencerEditorLibrary::GetControlRigs(LevelSequence);

    if (RigBindings.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigFromSkeletalMeshActor: "
                    "No Control Rig bindings found in the sequence"));
        return false;
    }

    // 第三步：遍历所有 Control Rig 绑定，查找绑定到指定 SkeletalMeshActor 的
    // Control Rig
    for (int32 RigIndex = 0; RigIndex < RigBindings.Num(); ++RigIndex) {
        const FControlRigSequencerBindingProxy& Proxy = RigBindings[RigIndex];

        UControlRig* CurrentControlRigInstance = Proxy.ControlRig;

        if (!CurrentControlRigInstance) {
            continue;
        }

        // 遍历打开的 Sequencer，检查该 Control Rig 是否绑定到目标
        // SkeletalMeshActor
        if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
            TArray<TWeakPtr<ISequencer>> WeakSequencers =
                FLevelEditorSequencerIntegration::Get().GetSequencers();

            for (int32 SeqIndex = 0; SeqIndex < WeakSequencers.Num();
                 ++SeqIndex) {
                const TWeakPtr<ISequencer>& WeakSequencer =
                    WeakSequencers[SeqIndex];

                if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin()) {
                    // 获取该 Control Rig 绑定的 GUID
                    FGuid BindingID = Proxy.Proxy.BindingID;

                    if (!BindingID.IsValid()) {
                        continue;
                    }

                    // 查找该绑定 ID 关联的所有对象
                    TArrayView<TWeakObjectPtr<UObject>> WeakBoundObjects =
                        Sequencer->FindBoundObjects(
                            BindingID, Sequencer->GetFocusedTemplateID());

                    // 检查目标 SkeletalMeshActor 是否在绑定的对象中
                    for (int32 ObjIndex = 0; ObjIndex < WeakBoundObjects.Num();
                         ++ObjIndex) {
                        const TWeakObjectPtr<UObject>& WeakObj =
                            WeakBoundObjects[ObjIndex];

                        if (WeakObj.IsValid() &&
                            WeakObj.Get() == InSkeletalMeshActor) {
                            // 找到了！设置输出参数
                            OutControlRigInstance = CurrentControlRigInstance;

                            // 获取 Control Rig 的蓝图
                            UObject* GeneratedBy =
                                OutControlRigInstance->GetClass()
                                    ->ClassGeneratedBy;

                            OutControlRigBlueprint =
                                Cast<UControlRigBlueprint>(GeneratedBy);

                            if (OutControlRigBlueprint) {
                                return true;
                            } else {
                                UE_LOG(
                                    LogTemp, Warning,
                                    TEXT("FInstrumentControlRigUtility::"
                                         "GetControlRigFromSkeletalMeshActor: "
                                         "Found Control Rig instance but "
                                         "failed to get blueprint"));
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }

    UE_LOG(
        LogTemp, Warning,
        TEXT(
            "FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor: "
            "Failed to find Control Rig bound to SkeletalMeshActor '%s'"),
        *InSkeletalMeshActor->GetName());
    return false;
}

bool FInstrumentControlRigUtility::GetControlRigControlWorldTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    FTransform& OutTransform) {
    // 获取 Control Rig 实例和 Control 索引
    UControlRig* ControlRigInstance = nullptr;
    int32 ControlIndex = INDEX_NONE;

    if (!GetControlRigAndIndex(InSkeletalMeshActor, ControlName,
                               ControlRigInstance, ControlIndex)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlWorldTransform: "
                    "Failed to get Control Rig instance or Control index"));
        return false;
    }

    // 获取 Control 在 Hierarchy 中的全局变换（Control Rig 内部坐标系）
    FTransform ControlGlobalTransform =
        ControlRigInstance->GetHierarchy()->GetGlobalTransform(ControlIndex);

    // SkeletalMeshActor 的世界变换
    FTransform ActorWorldTransform = InSkeletalMeshActor->GetActorTransform();

    // 真实世界坐标 = Control Rig 内部全局变换 × Actor 的世界变换
    OutTransform = ControlGlobalTransform * ActorWorldTransform;

    return true;
}

bool FInstrumentControlRigUtility::SetControlRigLocalTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    const FVector& NewLocation, const FQuat& NewRotation) {
    // 获取 Control Rig 实例和 Control 索引
    UControlRig* ControlRigInstance = nullptr;
    int32 ControlIndex = INDEX_NONE;

    if (!GetControlRigAndIndex(InSkeletalMeshActor, ControlName,
                               ControlRigInstance, ControlIndex)) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("FInstrumentControlRigUtility::SetControlRigLocalTransform: "
                 "Failed to get Control Rig instance or Control index"));
        return false;
    }

    URigHierarchy* Hierarchy = ControlRigInstance->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(
            LogTemp, Error,
            TEXT("FInstrumentControlRigUtility::SetControlRigLocalTransform: "
                 "Hierarchy is null"));
        return false;
    }

    // 直接构建局部变换，无需任何坐标系转换
    FTransform NewLocalTransform(NewRotation, NewLocation,
                                 FVector(1.0f, 1.0f, 1.0f));

    // 设置 Control 的局部变换
    Hierarchy->SetLocalTransform(ControlIndex, NewLocalTransform);

    // 通知 Control Rig 系统更新
    // 1. 重新评估 Control Rig 以应用变换更改
    ControlRigInstance->Evaluate_AnyThread();

    // 2. 强制更新骨骼网格组件
    if (USkeletalMeshComponent* SkelMeshComp =
            InSkeletalMeshActor->GetSkeletalMeshComponent()) {
        SkelMeshComp->RefreshBoneTransforms();
        SkelMeshComp->MarkRenderTransformDirty();
        SkelMeshComp->MarkRenderStateDirty();
    }

    return true;
}

bool FInstrumentControlRigUtility::SetControlRigWorldTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    const FVector& NewWorldLocation, const FQuat& NewWorldRotation) {
    // 构建期望的世界变换
    FTransform DesiredWorldTransform(NewWorldRotation, NewWorldLocation,
                                     FVector(1.0f, 1.0f, 1.0f));

    if (!InSkeletalMeshActor) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("FInstrumentControlRigUtility::SetControlRigWorldTransform: "
                 "InSkeletalMeshActor is null"));
        return false;
    }

    // 获取 SkeletalMeshActor 的世界变换
    FTransform RootWorldTransform = InSkeletalMeshActor->GetActorTransform();

    // 计算相对于 Actor 的局部变换
    // 局部变换 = 根元素的世界变换的逆 × 世界变换
    FTransform LocalTransform =
        DesiredWorldTransform.GetRelativeTransform(RootWorldTransform);

    // 调用低级的局部变换应用方法
    return SetControlRigLocalTransform(InSkeletalMeshActor, ControlName,
                                       LocalTransform.GetLocation(),
                                       LocalTransform.Rotator().Quaternion());
}

// ========== 私有辅助方法实现 =========

bool FInstrumentControlRigUtility::GetControlRigControlGlobalInitTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    FTransform& OutGlobalInitTransform) {
    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    // 获取 Control Rig 蓝图
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigFromSkeletalMeshActor(
            InSkeletalMeshActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "Failed to get ControlRig from SkeletalMeshActor"));
        return false;
    }

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "ControlRigBlueprint is null"));
        return false;
    }

    URigHierarchy* BlueprintHierarchy = ControlRigBlueprint->Hierarchy;
    if (!BlueprintHierarchy) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "Blueprint Hierarchy is null"));
        return false;
    }

    // 获取 Control 的索引
    int32 ControlIndex = BlueprintHierarchy->GetIndex(
        FRigElementKey(*ControlName, ERigElementType::Control));

    if (ControlIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlGlobalInitTransform: "
                    "Control '%s' not found in Blueprint Hierarchy"),
               *ControlName);
        return false;
    }

    // 使用 URigHierarchy::GetInitialGlobalTransform 获取相对于 Control Rig
    // 根的全局初始化变换 这个方法已经在 URigHierarchy
    // 中实现，能高效地计算完整的初始化全局变换
    OutGlobalInitTransform =
        BlueprintHierarchy->GetInitialGlobalTransform(ControlIndex);

    return true;
}

bool FInstrumentControlRigUtility::GetControlRigControlCurrentGlobalTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    FTransform& OutGlobalTransform) {
    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlCurrentGlobalTransform: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    // 获取 Control Rig 实例和索引
    UControlRig* ControlRigInstance = nullptr;
    int32 ControlIndex = INDEX_NONE;

    if (!GetControlRigAndIndex(InSkeletalMeshActor, ControlName,
                               ControlRigInstance, ControlIndex)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::"
                    "GetControlRigControlCurrentGlobalTransform: "
                    "Failed to get Control Rig instance or Control index"));
        return false;
    }

    // 直接使用 GetGlobalTransform 获取当前全局变换（Control Rig 内部坐标系）
    OutGlobalTransform =
        ControlRigInstance->GetHierarchy()->GetGlobalTransform(ControlIndex);

    return true;
}

bool FInstrumentControlRigUtility::GetControlRigAndIndex(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    UControlRig*& OutControlRigInstance, int32& OutControlIndex) {
    OutControlRigInstance = nullptr;
    OutControlIndex = INDEX_NONE;

    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    if (ControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "ControlName is empty"));
        return false;
    }

    // 获取 Control Rig 实例
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigFromSkeletalMeshActor(
            InSkeletalMeshActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "Failed to get ControlRig from SkeletalMeshActor"));
        return false;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "ControlRigInstance is null"));
        return false;
    }

    // 获取 Control 索引
    URigHierarchy* Hierarchy = ControlRigInstance->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "Hierarchy is null"));
        return false;
    }

    OutControlIndex = Hierarchy->GetIndex(
        FRigElementKey(*ControlName, ERigElementType::Control));

    if (OutControlIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigAndIndex: "
                    "Control '%s' not found in Hierarchy"),
               *ControlName);
        return false;
    }

    OutControlRigInstance = ControlRigInstance;
    return true;
}

bool FInstrumentControlRigUtility::InitializeControlRelationship(
    ASkeletalMeshActor* ParentControlRig, const FString& ParentControlName,
    ASkeletalMeshActor* ChildControlRig, const FString& ChildControlName,
    FTransform& OutRelativeTransform) {
    OutRelativeTransform = FTransform::Identity;

    if (!ParentControlRig || !ChildControlRig || ParentControlName.IsEmpty() ||
        ChildControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("InitializeControlRelationship: Invalid parameters"));
        return false;
    }

    // ========== 步骤1：获取父 Control 的初始化全局变换 =========
    FTransform ParentInitGlobalTransform;
    if (!GetControlRigControlGlobalInitTransform(
            ParentControlRig, ParentControlName, ParentInitGlobalTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("InitializeControlRelationship: Failed to get parent '%s' "
                    "init transform"),
               *ParentControlName);
        return false;
    }

    // ========== 步骤2：获取子 Control 的初始化全局变换 =========
    FTransform ChildInitGlobalTransform;
    if (!GetControlRigControlGlobalInitTransform(
            ChildControlRig, ChildControlName, ChildInitGlobalTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("InitializeControlRelationship: Failed to get child '%s' "
                    "init transform"),
               *ChildControlName);
        return false;
    }

    // ========== 步骤3：获取 Actor 的世界变换 =========
    FTransform ParentActorWorldTransform =
        ParentControlRig->GetActorTransform();
    FTransform ChildActorWorldTransform = ChildControlRig->GetActorTransform();

    // ========== 步骤4：计算初始的世界坐标下的变换 =========
    // 父 Control 的初始世界变换
    FTransform ParentInitWorldTransform =
        ParentInitGlobalTransform * ParentActorWorldTransform;
    // 子 Control 的初始世界变换
    FTransform ChildInitWorldTransform =
        ChildInitGlobalTransform * ChildActorWorldTransform;

    // ========== 步骤5：计算相对变换矩阵 =========
    // 相对变换 = Child 初始世界变换 × (Parent 初始世界变换 的逆)
    // 含义：从 Parent 的初始位置和旋转到 Child 的初始位置和旋转的变换
    // 这个变换在整个生命周期中保持不变
    OutRelativeTransform =
        ChildInitWorldTransform.GetRelativeTransform(ParentInitWorldTransform);

    UE_LOG(LogTemp, Warning,
           TEXT("InitializeControlRelationship: Successfully initialized "
                "relative transform for '%s' relative to '%s'"),
           *ChildControlName, *ParentControlName);

    return true;
}

bool FInstrumentControlRigUtility::UpdateChildControlFromParent(
    ASkeletalMeshActor* ParentControlRig, const FString& ParentControlName,
    ASkeletalMeshActor* ChildControlRig, const FString& ChildControlName,
    const FTransform& RelativeTransform) {
    if (!ParentControlRig || !ChildControlRig || ParentControlName.IsEmpty() ||
        ChildControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("UpdateChildControlFromParent: Invalid parameters"));
        return false;
    }

    // ========== 步骤1：获取父 Control 的初始化全局变换 =========
    FTransform ParentInitGlobalTransform;
    if (!GetControlRigControlGlobalInitTransform(
            ParentControlRig, ParentControlName, ParentInitGlobalTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("UpdateChildControlFromParent: Failed to get parent '%s' "
                    "init transform"),
               *ParentControlName);
        return false;
    }

    // ========== 步骤2：获取父 Control 的当前全局变换 =========
    FTransform ParentCurrentGlobalTransform;
    if (!GetControlRigControlCurrentGlobalTransform(
            ParentControlRig, ParentControlName,
            ParentCurrentGlobalTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("UpdateChildControlFromParent: Failed to get parent '%s' "
                    "current transform"),
               *ParentControlName);
        return false;
    }

    // ========== 步骤3：获取 Actor 的世界变换 =========
    FTransform ParentActorWorldTransform =
        ParentControlRig->GetActorTransform();
    FTransform ChildActorWorldTransform = ChildControlRig->GetActorTransform();

    // ========== 步骤4：计算父 Control 的当前世界变换 =========
    FTransform ParentInitWorldTransform =
        ParentInitGlobalTransform * ParentActorWorldTransform;
    FTransform ParentCurrentWorldTransform =
        ParentCurrentGlobalTransform * ParentActorWorldTransform;

    // ========== 步骤5：计算子 Control 的新世界变换 =========
    // ChildNewWorldTransform = RelativeTransform × ParentCurrentWorldTransform
    // 这样 Child 就会保持相对于 Parent 初始位置的相对关系，
    // 但会跟随 Parent 当前位置的变化
    FTransform ChildNewWorldTransform =
        RelativeTransform * ParentCurrentWorldTransform;

    // ========== 步骤6：将世界坐标转换回 Child 的 Control Rig 坐标系 =========
    // 子 Control 的新全局变换（相对于 Child Control Rig 根）
    FTransform ChildNewGlobalTransform =
        ChildNewWorldTransform.GetRelativeTransform(ChildActorWorldTransform);

    // ========== 步骤7：应用变换 =========
    // 获取子 Control Rig 实例
    UControlRig* ChildControlRigInstance = nullptr;
    int32 ChildControlIndex = INDEX_NONE;

    if (!GetControlRigAndIndex(ChildControlRig, ChildControlName,
                               ChildControlRigInstance, ChildControlIndex)) {
        UE_LOG(LogTemp, Warning,
               TEXT("UpdateChildControlFromParent: Failed to get child ControlRig "
                    "or Control index"));
        return false;
    }

    URigHierarchy* ChildHierarchy = ChildControlRigInstance->GetHierarchy();
    if (!ChildHierarchy) {
        UE_LOG(LogTemp, Warning,
               TEXT("UpdateChildControlFromParent: Child Hierarchy is null"));
        return false;
    }

    // 设置 Child Control 的全局变换
    ChildHierarchy->SetGlobalTransform(
        FRigElementKey(*ChildControlName, ERigElementType::Control),
        ChildNewGlobalTransform);

    // 重新评估 Control Rig 以应用变换更改
    ChildControlRigInstance->Evaluate_AnyThread();

    // 强制更新骨骼网格组件
    if (USkeletalMeshComponent* SkelMeshComp =
            ChildControlRig->GetSkeletalMeshComponent()) {
        SkelMeshComp->RefreshBoneTransforms();
        SkelMeshComp->MarkRenderTransformDirty();
        SkelMeshComp->MarkRenderStateDirty();
    }

    return true;
}

bool FInstrumentControlRigUtility::HasInitializationValuesChanged(
    ASkeletalMeshActor* ParentControlRig,
    const FString& ParentControlName,
    ASkeletalMeshActor* ChildControlRig,
    const FString& ChildControlName,
    const TArray<FTransform>& CachedValues,
    TArray<FTransform>& OutNewValues) {
    OutNewValues.SetNum(4);

    if (!ParentControlRig || !ChildControlRig || ParentControlName.IsEmpty() ||
        ChildControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("HasInitializationValuesChanged: Invalid parameters"));
        return false;
    }

    if (CachedValues.Num() != 4) {
        UE_LOG(LogTemp, Warning,
               TEXT("HasInitializationValuesChanged: CachedValues array size is "
                    "not 4"));
        return true;  // 如果缓存大小不对，认为值已改变
    }

    // ========== 获取当前的四个初始化值 =========
    // [0] ParentInitGlobalTransform
    if (!GetControlRigControlGlobalInitTransform(
            ParentControlRig, ParentControlName, OutNewValues[0])) {
        UE_LOG(LogTemp, Warning,
               TEXT("HasInitializationValuesChanged: Failed to get parent init "
                    "transform"));
        return true;
    }

    // [1] ChildInitGlobalTransform
    if (!GetControlRigControlGlobalInitTransform(
            ChildControlRig, ChildControlName, OutNewValues[1])) {
        UE_LOG(LogTemp, Warning,
               TEXT("HasInitializationValuesChanged: Failed to get child init "
                    "transform"));
        return true;
    }

    // [2] ParentActorWorldTransform
    OutNewValues[2] = ParentControlRig->GetActorTransform();

    // [3] ChildActorWorldTransform
    OutNewValues[3] = ChildControlRig->GetActorTransform();

    // ========== 检测是否有任何值发生变化 =========
    bool bChanged = false;

    for (int32 i = 0; i < 4; ++i) {
        // 比较位置
        if (!OutNewValues[i].GetLocation().Equals(CachedValues[i].GetLocation(),
                                                   1.0f)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("HasInitializationValuesChanged: Value [%d] location "
                        "changed"),
                   i);
            bChanged = true;
            break;
        }

        // 比较旋转
        if (!OutNewValues[i].GetRotation().Equals(
                CachedValues[i].GetRotation(), 0.01f)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("HasInitializationValuesChanged: Value [%d] rotation "
                        "changed"),
                   i);
            bChanged = true;
            break;
        }

        // 比较缩放
        if (!OutNewValues[i].GetScale3D().Equals(CachedValues[i].GetScale3D(),
                                                  0.01f)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("HasInitializationValuesChanged: Value [%d] scale "
                        "changed"),
                   i);
            bChanged = true;
            break;
        }
    }

    return bChanged;
}
