#include "WindRiseControlRigProcessor.h"

#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCreationUtility.h"
#include "Engine/Engine.h"
#include "InstrumentControlRigUtility.h"
#include "InstrumentMorphTargetUtility.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "WindRiseUnreal.h"

// ============================================================
// 控制器映射初始化
// ============================================================

void UWindRiseControlRigProcessor::InitializeControllers(
    AWindRiseUnreal* WindRiseActor) {
    if (!WindRiseActor) return;

    // ========== 手部控制器（14 个） ==========
    WindRiseActor->HandControllers.Empty();
    WindRiseActor->HandControllers.Add(TEXT("left_palm"), TEXT("H_L"));
    WindRiseActor->HandControllers.Add(TEXT("left_palm_ik_pivot"),
                                       TEXT("HP_L"));
    WindRiseActor->HandControllers.Add(TEXT("left_thumb"), TEXT("T_L"));
    WindRiseActor->HandControllers.Add(TEXT("left_index"), TEXT("I_L"));
    WindRiseActor->HandControllers.Add(TEXT("left_middle"), TEXT("M_L"));
    WindRiseActor->HandControllers.Add(TEXT("left_ring"), TEXT("R_L"));
    WindRiseActor->HandControllers.Add(TEXT("left_little"), TEXT("P_L"));
    WindRiseActor->HandControllers.Add(TEXT("right_palm"), TEXT("H_R"));
    WindRiseActor->HandControllers.Add(TEXT("right_palm_ik_pivot"),
                                       TEXT("HP_R"));
    WindRiseActor->HandControllers.Add(TEXT("right_thumb"), TEXT("T_R"));
    WindRiseActor->HandControllers.Add(TEXT("right_index"), TEXT("I_R"));
    WindRiseActor->HandControllers.Add(TEXT("right_middle"), TEXT("M_R"));
    WindRiseActor->HandControllers.Add(TEXT("right_ring"), TEXT("R_R"));
    WindRiseActor->HandControllers.Add(TEXT("right_little"), TEXT("P_R"));

    // ========== Pole Target 控制器（10 个） ==========
    WindRiseActor->PoleControllers.Empty();
    WindRiseActor->PoleControllers.Add(TEXT("left_thumb_pole"),
                                       TEXT("T_L_pole"));
    WindRiseActor->PoleControllers.Add(TEXT("left_index_pole"),
                                       TEXT("I_L_pole"));
    WindRiseActor->PoleControllers.Add(TEXT("left_middle_pole"),
                                       TEXT("M_L_pole"));
    WindRiseActor->PoleControllers.Add(TEXT("left_ring_pole"),
                                       TEXT("R_L_pole"));
    WindRiseActor->PoleControllers.Add(TEXT("left_little_pole"),
                                       TEXT("P_L_pole"));
    WindRiseActor->PoleControllers.Add(TEXT("right_thumb_pole"),
                                       TEXT("T_R_pole"));
    WindRiseActor->PoleControllers.Add(TEXT("right_index_pole"),
                                       TEXT("I_R_pole"));
    WindRiseActor->PoleControllers.Add(TEXT("right_middle_pole"),
                                       TEXT("M_R_pole"));
    WindRiseActor->PoleControllers.Add(TEXT("right_ring_pole"),
                                       TEXT("R_R_pole"));
    WindRiseActor->PoleControllers.Add(TEXT("right_little_pole"),
                                       TEXT("P_R_pole"));

    // ========== 脚部控制器（4 个） ==========
    WindRiseActor->FootControllers.Empty();
    WindRiseActor->FootControllers.Add(TEXT("left_foot"), TEXT("F_L"));
    WindRiseActor->FootControllers.Add(TEXT("left_foot_ik_pivot"),
                                       TEXT("FP_L"));
    WindRiseActor->FootControllers.Add(TEXT("right_foot"), TEXT("F_R"));
    WindRiseActor->FootControllers.Add(TEXT("right_foot_ik_pivot"),
                                       TEXT("FP_R"));

    // ========== 头部控制器（1 个） ==========
    WindRiseActor->HeadControl.Empty();
    WindRiseActor->HeadControl.Add(TEXT("head_control"), TEXT("Head_Control"));

    // ========== Breath Control（1 个） ==========
    WindRiseActor->BreathControl.Empty();
    WindRiseActor->BreathControl.Add(TEXT("breath"), TEXT("Breath_Control"));

    UE_LOG(LogTemp, Log, TEXT("UWindRiseCRProcessor: Controllers initialized"));
}

// ============================================================
// ControlRig 初始化与检查
// ============================================================

void UWindRiseControlRigProcessor::InitializePerformerControlRig(
    AWindRiseUnreal* WindRiseActor) {
    if (!WindRiseActor) {
        UE_LOG(LogTemp, Error,
               TEXT("UWindRiseCRProcessor: WindRiseActor is null"));
        return;
    }

    if (!WindRiseActor->SkeletalMeshActor) {
        UE_LOG(
            LogTemp, Error,
            TEXT("UWindRiseCRProcessor: No Performer SkeletalMeshActor set"));
        return;
    }

    UControlRigBlueprint* CRBlueprint =
        WindRiseActor->GetCachedControlRigBlueprint(TEXT("Performer"));
    if (!CRBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("UWindRiseCRProcessor: Cannot get ControlRigBlueprint for "
                    "Performer"));
        WindRiseActor->TriggerControlRigReregistration(
            TEXT("Performer ControlRig not found. Please ensure it is set up "
                 "in the SkeletalMeshActor."));
        return;
    }

    UE_LOG(LogTemp, Log,
           TEXT("========== InitializePerformerControlRig Started =========="));

    // ── 1. 创建基础骨架：controller_root → controller_root_offset ──
    static const FString BaseRootName = TEXT("base_root");
    static const FString RootName = TEXT("controller_root");
    static const FString OffsetName = TEXT("controller_root_offset");

    if (!EnsureControl(CRBlueprint, BaseRootName, TEXT(""))) return;
    if (!EnsureControl(CRBlueprint, RootName, BaseRootName)) return;
    if (!EnsureControl(CRBlueprint, OffsetName, RootName)) return;

    // ── 2. 创建手部控制器（全部平级，挂在 controller_root_offset 下） ──
    // H_L / H_R / HP_L / HP_R / T_L / I_L / M_L / R_L / P_L / T_R / I_R / M_R /
    // R_R / P_R 全部 → controller_root_offset
    for (const auto& Pair : WindRiseActor->HandControllers) {
        EnsureControl(CRBlueprint, Pair.Value, OffsetName);
    }

    // ── 2.5 创建辅助控件（ext_）— 每个手指一个，与手指控件同级 ──
    // WindRise 手指全部扁平挂在 controller_root_offset 下，故 ext 也挂该处
    //     极向量控件（pole）将重挂到对应的 ext_ 控件下面（见第 3 步）
    for (const auto& Pair : WindRiseActor->HandControllers) {
        const FString& CtrlName = Pair.Value;  // 如 "T_L"
        // 跳过手掌控制器（H_L / HP_L / H_R / HP_R），仅手指需要 ext
        if (CtrlName.StartsWith(TEXT("H"))) {
            continue;
        }
        FString ExtName = FString::Printf(TEXT("ext_%s"), *CtrlName);
        FControlRigCreationUtility::EnsureControl(CRBlueprint, ExtName,
                                                  OffsetName);
    }

    // ── 3. 创建/重挂 Pole Target 控制器（重挂到对应 ext_ 控件下） ──
    for (const auto& Pair : WindRiseActor->PoleControllers) {
        const FString& CtrlName = Pair.Value;  // 如 "T_L_pole"
        // 从 pole 名推导手指控件名: "T_L_pole" → "T_L" → ext_T_L
        FString FingerName = CtrlName;
        if (FingerName.EndsWith(TEXT("_pole"))) {
            FingerName = FingerName.LeftChop(5);  // 去掉 "_pole"
        }
        FString ExtName = FString::Printf(TEXT("ext_%s"), *FingerName);

        FControlRigCreationUtility::EnsureControl(CRBlueprint, CtrlName,
                                                  ExtName);
    }

    // ── 4. 创建脚部控制器（挂在 base root 下，与 controller root 同级） ──
    for (const auto& Pair : WindRiseActor->FootControllers) {
        EnsureControl(CRBlueprint, Pair.Value, BaseRootName);
    }

    // ── 5. 创建头部控制器（挂在 controller_root 下） ──
    for (const auto& Pair : WindRiseActor->HeadControl) {
        EnsureControl(CRBlueprint, Pair.Value, RootName);
    }

    // ── 6. 创建 Breath Control（挂在 base root 下） ──
    FString BreathCtrlName;
    for (const auto& Pair : WindRiseActor->BreathControl) {
        BreathCtrlName = Pair.Value;
        EnsureControl(CRBlueprint, BreathCtrlName, BaseRootName);
    }

    // ── 7. 在 Breath_Control 下创建 Morph Target Float Channels ──
    if (!BreathCtrlName.IsEmpty() &&
        WindRiseActor->CharacterMorphTargets.Num() > 0) {
        FRigElementKey BreathKey(*BreathCtrlName, ERigElementType::Control);
        int32 ChannelsAdded =
            UInstrumentMorphTargetUtility::AddAnimationChannels(
                CRBlueprint, BreathKey, WindRiseActor->CharacterMorphTargets);
        UE_LOG(LogTemp, Log,
               TEXT("UWindRiseCRProcessor: %d morph target channels added "
                    "under %s"),
               ChannelsAdded, *BreathCtrlName);
    }

    UE_LOG(
        LogTemp, Log,
        TEXT("========== InitializePerformerControlRig Completed =========="));
}

// ============================================================
// 层级辅助
// ============================================================

bool UWindRiseControlRigProcessor::EnsureControl(
    UControlRigBlueprint* CRBlueprint, const FString& ControlName,
    const FString& ExpectedParentName) {
    if (!CRBlueprint || ControlName.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("EnsureControl: Invalid parameters"));
        return false;
    }

    URigHierarchy* Hierarchy = CRBlueprint->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(LogTemp, Error, TEXT("EnsureControl: Failed to get hierarchy"));
        return false;
    }

    URigHierarchyController* Controller = Hierarchy->GetController();
    if (!Controller) {
        UE_LOG(LogTemp, Error,
               TEXT("EnsureControl: Failed to get hierarchy controller"));
        return false;
    }

    FRigElementKey Key(*ControlName, ERigElementType::Control);

    // 检查 control 是否存在
    if (Hierarchy->Contains(Key)) {
        // 存在 → 检查 parent 是否匹配
        if (!ExpectedParentName.IsEmpty()) {
            FRigElementKey CurrentParent = Hierarchy->GetFirstParent(Key);
            FRigElementKey ExpectedKey(*ExpectedParentName,
                                       ERigElementType::Control);

            if (CurrentParent != ExpectedKey) {
                // parent 不匹配 → reparent
                UE_LOG(LogTemp, Warning,
                       TEXT("EnsureControl: '%s' parent is '%s', "
                            "reparenting to '%s'"),
                       *ControlName, *CurrentParent.Name.ToString(),
                       *ExpectedParentName);

                if (!Controller->SetParent(Key, ExpectedKey, true, false)) {
                    UE_LOG(LogTemp, Error,
                           TEXT("EnsureControl: Failed to reparent '%s'"),
                           *ControlName);
                    return false;
                }
                CRBlueprint->MarkPackageDirty();
            }
        }
        return true;
    }

    // 不存在 → 创建
    bool bCreated = FControlRigCreationUtility::CreateControl(
        CRBlueprint, ControlName, ExpectedParentName);
    if (!bCreated) {
        UE_LOG(LogTemp, Error, TEXT("EnsureControl: Failed to create '%s'"),
               *ControlName);
        return false;
    }

    return true;
}

void UWindRiseControlRigProcessor::CheckControlRigStatus(
    AWindRiseUnreal* WindRiseActor) {
    if (!WindRiseActor) {
        UE_LOG(LogTemp, Warning,
               TEXT("UWindRiseCRProcessor: WindRiseActor is null"));
        return;
    }

    if (!WindRiseActor->SkeletalMeshActor) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("UWindRiseCRProcessor: No Performer SkeletalMeshActor set"));
        return;
    }

    UControlRigBlueprint* CRBlueprint =
        WindRiseActor->GetCachedControlRigBlueprint(TEXT("Performer"));
    if (!CRBlueprint) {
        UE_LOG(LogTemp, Warning,
               TEXT("UWindRiseCRProcessor: Performer ControlRigBlueprint "
                    "not found"));
        return;
    }

    URigHierarchy* Hierarchy = CRBlueprint->GetHierarchy();
    if (!Hierarchy) {
        UE_LOG(LogTemp, Warning,
               TEXT("UWindRiseCRProcessor: Performer ControlRig has no "
                    "hierarchy"));
        return;
    }

    // 检查各控制器是否存在
    int32 FoundCount = 0;
    int32 MissingCount = 0;
    auto CheckControl = [&](const FString& CtrlName) {
        FRigElementKey Key(*CtrlName, ERigElementType::Control);
        if (Hierarchy->Contains(Key)) {
            FoundCount++;
        } else {
            MissingCount++;
            UE_LOG(LogTemp, Warning,
                   TEXT("UWindRiseCRProcessor: Missing control '%s'"),
                   *CtrlName);
        }
    };

    for (const auto& Pair : WindRiseActor->HandControllers) {
        CheckControl(Pair.Value);
        // ext 辅助控件（仅手指有，手掌 H_L/HP_L/H_R/HP_R 除外）
        if (!Pair.Value.StartsWith(TEXT("H"))) {
            CheckControl(FString::Printf(TEXT("ext_%s"), *Pair.Value));
        }
    }
    for (const auto& Pair : WindRiseActor->PoleControllers)
        CheckControl(Pair.Value);
    for (const auto& Pair : WindRiseActor->FootControllers)
        CheckControl(Pair.Value);
    for (const auto& Pair : WindRiseActor->HeadControl)
        CheckControl(Pair.Value);
    for (const auto& Pair : WindRiseActor->BreathControl)
        CheckControl(Pair.Value);

    UE_LOG(LogTemp, Log,
           TEXT("UWindRiseCRProcessor: ControlRig status check: %d found, "
                "%d missing"),
           FoundCount, MissingCount);
}

// ============================================================
// 状态捕获与恢复
// ============================================================

void UWindRiseControlRigProcessor::CaptureControllers(
    AWindRiseUnreal* WindRiseActor, UControlRig* CR,
    FWindRiseNoteState& OutState) {
    if (!CR || !WindRiseActor) return;

    URigHierarchy* RigHierarchy = CR->GetHierarchy();
    if (!RigHierarchy) return;

    auto CaptureCtrlMap = [&](const TMap<FString, FString>& CtrlMap) {
        for (const auto& Pair : CtrlMap) {
            FTransform CtrlTransform;
            if (FInstrumentControlRigUtility::GetControlLocalTransform(
                    RigHierarchy, Pair.Value, CtrlTransform)) {
                OutState.Controllers.Add(Pair.Value, CtrlTransform);
            }
        }
    };

    CaptureCtrlMap(WindRiseActor->HandControllers);
}

void UWindRiseControlRigProcessor::RestoreControllers(
    AWindRiseUnreal* WindRiseActor, UControlRig* CR,
    const FWindRiseNoteState& State) {
    if (!CR || !WindRiseActor) return;

    URigHierarchy* RigHierarchy = CR->GetHierarchy();
    if (!RigHierarchy) return;

    for (const auto& Pair : State.Controllers) {
        FInstrumentControlRigUtility::SetControlLocalTransform(
            RigHierarchy, Pair.Key, Pair.Value);
    }
}

void UWindRiseControlRigProcessor::RestoreCharacterMorphTargets(
    AWindRiseUnreal* WindRiseActor, const FWindRiseNoteState& State) {
    if (!WindRiseActor || !WindRiseActor->SkeletalMeshActor ||
        !WindRiseActor->SkeletalMeshActor->GetSkeletalMeshComponent())
        return;

    USkeletalMeshComponent* SkelComp =
        WindRiseActor->SkeletalMeshActor->GetSkeletalMeshComponent();

    // 先全部归零
    for (const FString& Name : WindRiseActor->CharacterMorphTargets) {
        SkelComp->SetMorphTarget(FName(*Name), 0.0f);
    }
    // 再恢复非零值
    for (const FWindRiseMorphTargetValue& MT : State.CharacterMT) {
        SkelComp->SetMorphTarget(FName(*MT.MorphTargetName), MT.Value);
    }
}

// ============================================================
// 实时 Morph Target 辅助
// ============================================================

void UWindRiseControlRigProcessor::SetCharacterMTValue(
    AWindRiseUnreal* WindRiseActor, int32 Index, float Value) {
    if (!WindRiseActor || !WindRiseActor->SkeletalMeshActor ||
        !WindRiseActor->SkeletalMeshActor->GetSkeletalMeshComponent())
        return;
    if (!WindRiseActor->CharacterMorphTargets.IsValidIndex(Index)) return;

    WindRiseActor->SkeletalMeshActor->GetSkeletalMeshComponent()
        ->SetMorphTarget(FName(*WindRiseActor->CharacterMorphTargets[Index]),
                         Value);
}

void UWindRiseControlRigProcessor::ResetAllCharacterMT(
    AWindRiseUnreal* WindRiseActor) {
    if (!WindRiseActor || !WindRiseActor->SkeletalMeshActor ||
        !WindRiseActor->SkeletalMeshActor->GetSkeletalMeshComponent())
        return;

    USkeletalMeshComponent* SkelComp =
        WindRiseActor->SkeletalMeshActor->GetSkeletalMeshComponent();
    for (const FString& Name : WindRiseActor->CharacterMorphTargets) {
        SkelComp->SetMorphTarget(FName(*Name), 0.0f);
    }
}
