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

bool FInstrumentControlRigUtility::GetControlRigControlTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    FTransform& OutTransform) {
    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigControlTransform: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    // 获取 Control Rig 实例
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigFromSkeletalMeshActor(InSkeletalMeshActor,
                                            ControlRigInstance,
                                            ControlRigBlueprint)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigControlTransform: "
                    "Failed to get ControlRig from SkeletalMeshActor"));
        return false;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigControlTransform: "
                    "ControlRigInstance is null"));
        return false;
    }

    // 获取 Control 在 Control Rig 中的索引
    int32 ControlIndex = ControlRigInstance->GetHierarchy()->GetIndex(
        FRigElementKey(*ControlName, ERigElementType::Control));

    if (ControlIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigControlTransform: "
                    "Control '%s' not found in ControlRig"),
               *ControlName);
        return false;
    }

    // ========== 坐标系转换：Control Rig内部全局 → 真实世界 ==========
    // 获取 Control 在 Hierarchy 中的全局变换（Control Rig 内部坐标系）
    FTransform ControlGlobalTransform = ControlRigInstance->GetHierarchy()->GetGlobalTransform(ControlIndex);
    
    // SkeletalMeshActor 的世界变换
    FTransform ActorWorldTransform = InSkeletalMeshActor->GetActorTransform();
    
    // 真实世界坐标 = Control Rig 内部全局变换 × Actor 的世界变换
    OutTransform = ControlGlobalTransform * ActorWorldTransform;

    return true;
}

bool FInstrumentControlRigUtility::SetControllerTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    const FVector& NewLocation, const FQuat& NewRotation) {
    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::SetControllerTransform: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    // 获取 Control Rig 实例
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigFromSkeletalMeshActor(InSkeletalMeshActor,
                                            ControlRigInstance,
                                            ControlRigBlueprint)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::SetControllerTransform: "
                    "Failed to get ControlRig from SkeletalMeshActor"));
        return false;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::SetControllerTransform: "
                    "ControlRigInstance is null"));
        return false;
    }

    URigHierarchy* Hierarchy = ControlRigInstance->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::SetControllerTransform: "
                    "Hierarchy is null"));
        return false;
    }

    // 获取 Control 在 Control Rig 中的索引
    int32 ControlIndex = Hierarchy->GetIndex(
        FRigElementKey(*ControlName, ERigElementType::Control));

    if (ControlIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::SetControllerTransform: "
                    "Control '%s' not found in ControlRig"),
               *ControlName);
        return false;
    }

    // 构建期望的世界变换
    FTransform DesiredWorldTransform(NewRotation, NewLocation,
                                     FVector(1.0f, 1.0f, 1.0f));

    // ========== 核心逻辑：将世界变换转换为局部变换 ==========
    // 根元素的世界变换 = SkeletalMeshActor 的世界变换
    FTransform RootWorldTransform = InSkeletalMeshActor->GetActorTransform();

    // 局部变换 = 根元素的世界变换的逆 × 世界变换
    FTransform NewLocalTransform = DesiredWorldTransform.GetRelativeTransform(RootWorldTransform);

    // 设置 Control 的局部变换（而不是世界变换）
    Hierarchy->SetLocalTransform(ControlIndex, NewLocalTransform);

    // ========== 通知 Control Rig 系统更新 ========= =
    
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

bool FInstrumentControlRigUtility::SyncControlTransform(
    ASkeletalMeshActor* SourceSkeletalMeshActor,
    const FString& SourceControlName,
    ASkeletalMeshActor* TargetSkeletalMeshActor,
    const FString& TargetControlName) {
    if (!SourceSkeletalMeshActor || !TargetSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::SyncControlTransform: "
                    "Source or Target SkeletalMeshActor is null"));
        return false;
    }

    // 1. 获取源 Control 的世界变换
    FTransform SourceWorldTransform;
    if (!GetControlRigControlTransform(SourceSkeletalMeshActor,
                                       SourceControlName,
                                       SourceWorldTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::SyncControlTransform: "
                    "Failed to get source control '%s' transform"),
               *SourceControlName);
        return false;
    }

    // 2. 获取目标 Control 的世界变换（用于计算父变换）
    FTransform TargetCurrentWorldTransform;
    if (!GetControlRigControlTransform(TargetSkeletalMeshActor,
                                       TargetControlName,
                                       TargetCurrentWorldTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::SyncControlTransform: "
                    "Failed to get target control '%s' current transform"),
               *TargetControlName);
        return false;
    }

    // 3. 计算目标 Control 相对于其父的变换
    // 需要获取目标 Control 的父信息以计算相对变换
    // 目前简化处理：直接设置世界变换
    // 完整实现需要访问 Control Rig Hierarchy 来获取父子关系
    
    if (!SetControllerTransform(TargetSkeletalMeshActor,
                                TargetControlName,
                                SourceWorldTransform.GetLocation(),
                                SourceWorldTransform.Rotator().Quaternion())) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::SyncControlTransform: "
                    "Failed to set target control '%s' transform"),
               *TargetControlName);
        return false;
    }

    UE_LOG(LogTemp, Log,
           TEXT("FInstrumentControlRigUtility::SyncControlTransform: "
                "Successfully synced '%s' from '%s' to '%s'"),
           *SourceControlName, *SourceSkeletalMeshActor->GetName(),
           *TargetSkeletalMeshActor->GetName());

    return true;
}

bool FInstrumentControlRigUtility::AreTransformsEqual(
    const FTransform& TransformA, const FTransform& TransformB,
    float LocationTolerance, float RotationTolerance) {
    // ========== 位置比较 ==========
    FVector LocationDiff = TransformA.GetLocation() - TransformB.GetLocation();
    float LocationDistance = LocationDiff.Length();
    
    if (LocationDistance > LocationTolerance) {
        return false;
    }

    // ========== 旋转比较 ==========
    FQuat RotationA = TransformA.GetRotation();
    FQuat RotationB = TransformB.GetRotation();
    
    // 四元数的点积：值越接近1，表示两个旋转越接近
    // 注意：q 和 -q 表示相同的旋转，所以取绝对值
    float RotationDotProduct = FMath::Abs(RotationA | RotationB);
    
    // 当点积接近1时，表示旋转相同
    // 1.0f - DotProduct 近似等于旋转角度（弧度），在小角度时
    float RotationAngleDiff = FMath::Acos(FMath::Clamp(RotationDotProduct, -1.0f, 1.0f));
    
    if (RotationAngleDiff > RotationTolerance) {
        return false;
    }

    return true;
}

bool FInstrumentControlRigUtility::GetControlRigControlInitTransform(
    ASkeletalMeshActor* InSkeletalMeshActor, const FString& ControlName,
    FTransform& OutInitTransform) {
    if (!InSkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigControlInitTransform: "
                    "InSkeletalMeshActor is null"));
        return false;
    }

    // 获取 Control Rig 实例和蓝图
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!GetControlRigFromSkeletalMeshActor(InSkeletalMeshActor,
                                            ControlRigInstance,
                                            ControlRigBlueprint)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigControlInitTransform: "
                    "Failed to get ControlRig from SkeletalMeshActor"));
        return false;
    }

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::GetControlRigControlInitTransform: "
                    "ControlRigBlueprint is null"));
        return false;
    }

    // 获取蓝图的 Hierarchy
    URigHierarchy* BlueprintHierarchy = ControlRigBlueprint->Hierarchy;
    if (!BlueprintHierarchy) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigControlInitTransform: "
                    "Blueprint Hierarchy is null"));
        return false;
    }

    // 在蓝图的 Hierarchy 中查找同名的 Control
    int32 BlueprintControlIndex = BlueprintHierarchy->GetIndex(
        FRigElementKey(*ControlName, ERigElementType::Control));

    if (BlueprintControlIndex == INDEX_NONE) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::GetControlRigControlInitTransform: "
                    "Control '%s' not found in Blueprint Hierarchy"),
               *ControlName);
        return false;
    }

    // 获取蓝图中定义的初始化变换
    // 使用 GetInitialLocalTransform 获取相对于父级的初始化变换
    OutInitTransform = BlueprintHierarchy->GetInitialLocalTransform(BlueprintControlIndex);

    return true;
}

bool FInstrumentControlRigUtility::ParentBetweenControlRig(
    ASkeletalMeshActor* ParentControlRig,
    const FString& ParentControlName,
    ASkeletalMeshActor* ChildControlRig,
    const FString& ChildControlName) {
    
    // ========== 参数验证 =========
    if (!ParentControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::ParentBetweenControlRig: "
                    "ParentControlRig is null"));
        return false;
    }

    if (!ChildControlRig) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::ParentBetweenControlRig: "
                    "ChildControlRig is null"));
        return false;
    }

    if (ParentControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::ParentBetweenControlRig: "
                    "ParentControlName is empty"));
        return false;
    }

    if (ChildControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("FInstrumentControlRigUtility::ParentBetweenControlRig: "
                    "ChildControlName is empty"));
        return false;
    }

    // ========== 获取父 Control 的初始化变换 =========
    FTransform ParentInitTransform;
    if (!GetControlRigControlInitTransform(ParentControlRig, ParentControlName,
                                           ParentInitTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::ParentBetweenControlRig: "
                    "Failed to get parent '%s' init transform"),
               *ParentControlName);
        return false;
    }

    // ========== 获取父 Control 的当前变换 =========
    FTransform ParentCurrentTransform;
    if (!GetControlRigControlTransform(ParentControlRig, ParentControlName,
                                       ParentCurrentTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::ParentBetweenControlRig: "
                    "Failed to get parent '%s' current transform"),
               *ParentControlName);
        return false;
    }

    // ========== 计算父 Control 的 offset 矩阵 =========
    // offset = ParentInitTransform^(-1) × ParentCurrentTransform
    // 这个 offset 表示父 Control 相对于其初始化位置的偏移
    FTransform ParentOffsetTransform = ParentInitTransform.Inverse() * ParentCurrentTransform;

    // ========== 获取子 Control 的初始化变换 =========
    FTransform ChildInitTransform;
    if (!GetControlRigControlInitTransform(ChildControlRig, ChildControlName,
                                           ChildInitTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::ParentBetweenControlRig: "
                    "Failed to get child '%s' init transform"),
               *ChildControlName);
        return false;
    }

    // ========== 计算子 Control 的新变换 =========
    // 新变换 = 子初始化变换 × 父offset矩阵
    // 这样子 Control 就会跟随父 Control 的偏移而运动
    FTransform ChildTargetTransform = ChildInitTransform * ParentOffsetTransform;

    // ========== 应用到子 Control =========
    if (!SetControllerTransform(ChildControlRig, ChildControlName,
                               ChildTargetTransform.GetLocation(),
                               ChildTargetTransform.Rotator().Quaternion())) {
        UE_LOG(LogTemp, Warning,
               TEXT("FInstrumentControlRigUtility::ParentBetweenControlRig: "
                    "Failed to set child '%s' transform"),
               *ChildControlName);
        return false;
    }

    return true;
}
