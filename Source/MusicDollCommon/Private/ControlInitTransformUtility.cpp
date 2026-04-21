#include "ControlInitTransformUtility.h"

#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "InstrumentAnimationUtility.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "LevelSequence.h"
#include "Rigs/RigHierarchy.h"

bool FControlInitTransformUtility::ApplySelectedControlsTransformToInitial(
    int32& OutAppliedCount, int32& OutSkippedCount) {
    OutAppliedCount = 0;
    OutSkippedCount = 0;

    // 1. 获取当前打开的 LevelSequence
    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("ApplySelectedControlsTransformToInitial: "
                    "No LevelSequence is currently open"));
        return false;
    }

    // 2. 获取 Sequence 中所有 ControlRig 绑定代理
    TArray<FControlRigSequencerBindingProxy> RigBindings =
        UControlRigSequencerEditorLibrary::GetControlRigs(LevelSequence);

    if (RigBindings.Num() == 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("ApplySelectedControlsTransformToInitial: "
                    "No ControlRig bindings found in the sequence"));
        return false;
    }

    // 3. 遍历所有 ControlRig，找到选中的 Controls
    for (const FControlRigSequencerBindingProxy& Proxy : RigBindings) {
        UControlRig* ControlRigInstance = Proxy.ControlRig;
        if (!ControlRigInstance) {
            continue;
        }

        // 从运行时 ControlRig 实例获取 Blueprint
        UClass* ControlRigClass = ControlRigInstance->GetClass();
        if (!ControlRigClass) {
            continue;
        }

        UControlRigBlueprint* Blueprint =
            Cast<UControlRigBlueprint>(ControlRigClass->ClassGeneratedBy);
        if (!Blueprint) {
            UE_LOG(
                LogTemp, Warning,
                TEXT("ApplySelectedControlsTransformToInitial: "
                     "Failed to get ControlRigBlueprint for ControlRig '%s'"),
                *ControlRigInstance->GetName());
            ++OutSkippedCount;
            continue;
        }

        URigHierarchy* BlueprintHierarchy = Blueprint->Hierarchy;
        if (!BlueprintHierarchy) {
            UE_LOG(LogTemp, Warning,
                   TEXT("ApplySelectedControlsTransformToInitial: "
                        "Blueprint Hierarchy is null for '%s'"),
                   *Blueprint->GetName());
            ++OutSkippedCount;
            continue;
        }

        // 获取运行时 Hierarchy（用于读取当前变换）
        URigHierarchy* RuntimeHierarchy = ControlRigInstance->GetHierarchy();
        if (!RuntimeHierarchy) {
            ++OutSkippedCount;
            continue;
        }

        // 4. 获取该 ControlRig 中所有被选中的 Controls
        TArray<FRigElementKey> SelectedKeys =
            ControlRigInstance->GetHierarchy()->GetSelectedKeys();

        bool bModifiedBlueprint = false;

        for (const FRigElementKey& Key : SelectedKeys) {
            // 只处理 Control 类型
            if (Key.Type != ERigElementType::Control) {
                continue;
            }

            // 在运行时 Hierarchy 中获取 Control 的当前局部变换和 Offset
            int32 RuntimeIndex = RuntimeHierarchy->GetIndex(Key);
            if (RuntimeIndex == INDEX_NONE) {
                UE_LOG(LogTemp, Warning,
                       TEXT("ApplySelectedControlsTransformToInitial: "
                            "Control '%s' not found in runtime Hierarchy"),
                       *Key.Name.ToString());
                ++OutSkippedCount;
                continue;
            }

            // 先确保 ControlRig 运算结果最新
            ControlRigInstance->Evaluate_AnyThread();

            // 获取基础局部变换（不含 Offset）
            FTransform CurrentLocalTransform =
                RuntimeHierarchy->GetLocalTransform(RuntimeIndex);

            // 获取当前的局部 Offset（非全局）
            FRigControlElement* ControlElement =
                RuntimeHierarchy->Find<FRigControlElement>(Key);
            if (!ControlElement) {
                UE_LOG(LogTemp, Warning,
                       TEXT("ApplySelectedControlsTransformToInitial: "
                            "Failed to find ControlElement for '%s'"),
                       *Key.Name.ToString());
                ++OutSkippedCount;
                continue;
            }
            
            FTransform CurrentOffset = RuntimeHierarchy->GetControlOffsetTransform(
                ControlElement, ERigTransformType::CurrentLocal);

            // 计算实际的 Local Transform = CurrentLocalTransform × Offset
            FTransform ActualLocalTransform =
                CurrentLocalTransform * CurrentOffset;

            // 在 Blueprint Hierarchy 中找到对应的 Control
            int32 BlueprintIndex = BlueprintHierarchy->GetIndex(Key);
            if (BlueprintIndex == INDEX_NONE) {
                UE_LOG(LogTemp, Warning,
                       TEXT("ApplySelectedControlsTransformToInitial: "
                            "Control '%s' not found in Blueprint Hierarchy"),
                       *Key.Name.ToString());
                ++OutSkippedCount;
                continue;
            }

            // 5. 将实际的 Local Transform 写入 Blueprint Hierarchy 的 Offset
            constexpr bool bAffectChildren = true;
            constexpr bool bSetupUndo = true;
            constexpr bool bPrintPythonCommands = false;

            // 设置 Initial Offset 为实际变换
            BlueprintHierarchy->SetControlOffsetTransform(
                Key, ActualLocalTransform, true, bAffectChildren, bSetupUndo,
                bPrintPythonCommands);

            // 将 Initial Local Transform 清零（非 Offset）
            BlueprintHierarchy->SetInitialLocalTransform(Key,
                                                         FTransform::Identity);

            ++OutAppliedCount;
            bModifiedBlueprint = true;

            UE_LOG(LogTemp, Log,
                   TEXT("ApplySelectedControlsTransformToInitial: "
                        "Applied initial transform for Control '%s' in "
                        "Blueprint '%s'"),
                   *Key.Name.ToString(), *Blueprint->GetName());
        }

        // 6. 如果修改了蓝图，标记为脏并触发重新编译
        if (bModifiedBlueprint) {
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            Blueprint->MarkPackageDirty();

            UE_LOG(LogTemp, Warning,
                   TEXT("ApplySelectedControlsTransformToInitial: "
                        "Marked Blueprint '%s' as modified and dirty"),
                   *Blueprint->GetName());
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("ApplySelectedControlsTransformToInitial: "
                "Applied=%d, Skipped=%d"),
           OutAppliedCount, OutSkippedCount);

    return OutAppliedCount > 0;
}
