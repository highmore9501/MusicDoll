#include "StringFlowControlRigProcessor.h"

#include "Animation/SkeletalMeshActor.h"
#include "BoneControlMappingUtility.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentControlRigUtility.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelEditor.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "MovieSceneSequence.h"
#include "Rigs/RigHierarchyController.h"
#include "StringFlowControlRigHelper.h"

#define LOCTEXT_NAMESPACE "StringFlowControlRigProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UStringFlowControlRigProcessor implementations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UStringFlowControlRigProcessor::GetControlRigFromStringInstrument(
    ASkeletalMeshActor* StringInstrumentActor,
    UControlRig*& OutControlRigInstance,
    UControlRigBlueprint*& OutControlRigBlueprint) {
    // 通过ControlRig缓存子系统获取ControlRig，而不是直接查询
    if (!StringInstrumentActor) {
        UE_LOG(LogTemp, Error,
               TEXT("GetControlRigFromStringInstrument: StringInstrumentActor "
                    "is null"));
        return false;
    }

    // 获取当前LevelSequence
    ULevelSequence* CurrentSequence = nullptr;
    if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
        TArray<TWeakPtr<ISequencer>> WeakSequencers =
            FLevelEditorSequencerIntegration::Get().GetSequencers();

        for (const TWeakPtr<ISequencer>& WeakSequencer : WeakSequencers) {
            if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin()) {
                UMovieSceneSequence* RootSequence =
                    Sequencer->GetRootMovieSceneSequence();
                CurrentSequence = Cast<ULevelSequence>(RootSequence);
                if (CurrentSequence) {
                    break;
                }
            }
        }
    }

    if (!CurrentSequence) {
        UE_LOG(
            LogTemp, Warning,
            TEXT("GetControlRigFromStringInstrument: No LevelSequence found"));
        return false;
    }

    // 通过Subsystem获取ControlRig
    if (!GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("GetControlRigFromStringInstrument: GEngine is NULL"));
        return false;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();

    if (!CacheSubsystem) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "GetControlRigFromStringInstrument: CacheSubsystem not found"));
        return false;
    }

    // 使用Subsystem获取ControlRig
    OutControlRigInstance =
        CacheSubsystem->GetControlRig(StringInstrumentActor, CurrentSequence, TEXT("violin_root"));
    OutControlRigBlueprint = CacheSubsystem->GetControlRigBlueprint(
        StringInstrumentActor, CurrentSequence, TEXT("violin_root"));

    // 如果获取失败，Subsystem会自动处理注册和更新逻辑，我们只需记录日志
    if (!OutControlRigInstance || !OutControlRigBlueprint) {
        UE_LOG(LogTemp, Warning,
               TEXT("GetControlRigFromStringInstrument: Failed to get "
                    "ControlRig from subsystem for Actor %s"),
               *StringInstrumentActor->GetName());
    }

    return OutControlRigInstance != nullptr &&
           OutControlRigBlueprint != nullptr;
}

void UStringFlowControlRigProcessor::CheckObjectsStatus(
    AStringFlowUnreal* StringFlowActor) {
    if (!FStringFlowControlRigHelper::ValidateStringFlowActor(
            StringFlowActor, TEXT("CheckObjectsStatus"))) {
        return;
    }

    UControlRig* ControlRigInstance =
        StringFlowActor->GetCachedControlRig(TEXT("Performer"));
    UControlRigBlueprint* ControlRigBlueprint =
        StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "StringFlowActor"));
        return;
    }

    if (!ControlRigBlueprint) {
        UE_LOG(LogTemp, Error, TEXT("ControlRigBlueprint is null"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigBlueprint"));
        return;
    }

    TSet<FString> ExpectedObjects;

    // 收集所有预期的控制器名称（真实的控制器）
    for (const auto& Pair : StringFlowActor->LeftFingerControllers) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : StringFlowActor->RightFingerControllers) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : StringFlowActor->LeftHandControllers) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : StringFlowActor->RightHandControllers) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : StringFlowActor->GuideLines) {
        ExpectedObjects.Add(Pair.Value);
    }

    // 添加特殊的实际控制器
    ExpectedObjects.Add(TEXT("String_Touch_Point"));
    ExpectedObjects.Add(TEXT("Bow_Controller"));
    ExpectedObjects.Add(TEXT("Right_Hand_Tar")); 

    // 添加参考点控制器（这些是真实的控制器，不是记录器）
    // 注：mid_s* 和 f9_s* 由蓝图自动生成，不需要在这里验证
    const FStringFlowStringArray* OtherArray =
        StringFlowActor->OtherRecorders.Find(TEXT("other_recorders"));
    if (OtherArray) {
        for (int32 i = 0; i < OtherArray->Num(); ++i) {
            FString RecorderName = OtherArray->Get(i);
            // 只添加不是状态相关记录器、不是蓝图生成参考点的控制器名称
            if (!RecorderName.StartsWith(TEXT("stp_")) &&
                !RecorderName.StartsWith(TEXT("bow_position_")) &&
                !RecorderName.StartsWith(TEXT("mid_s")) &&
                !RecorderName.StartsWith(TEXT("f9_s"))) {
                ExpectedObjects.Add(RecorderName);
            }
        }
    }

    // 收集所有极点控制器
    for (const auto& FingerPair : StringFlowActor->LeftFingerControllers) {
        FString PoleControlName =
            FString::Printf(TEXT("pole_%s"), *FingerPair.Value);
        ExpectedObjects.Add(PoleControlName);
    }

    for (const auto& FingerPair : StringFlowActor->RightFingerControllers) {
        FString PoleControlName =
            FString::Printf(TEXT("pole_%s"), *FingerPair.Value);
        ExpectedObjects.Add(PoleControlName);
    }

    // 验证层次结构中的所有控制器
    TArray<FString> ExistingObjects;
    TArray<FString> MissingObjects;

    for (const FString& ObjectName : ExpectedObjects) {
        bool bFound = false;

        FRigElementKey ElementKey(*ObjectName, ERigElementType::Control);

        if (RigHierarchy->Contains(ElementKey)) {
            ExistingObjects.Add(ObjectName);
            bFound = true;
        } else {
            ElementKey.Type = ERigElementType::Bone;
            if (RigHierarchy->Contains(ElementKey)) {
                ExistingObjects.Add(ObjectName);
                bFound = true;
            }
        }

        if (!bFound) {
            MissingObjects.Add(ObjectName);
        }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("StringFlow 对象状态报告 (Control Rig 版本)"));
    UE_LOG(LogTemp, Warning, TEXT("========================"));
    UE_LOG(LogTemp, Warning, TEXT("预期对象总数: %d"), ExpectedObjects.Num());
    UE_LOG(LogTemp, Warning, TEXT("存在的对象数量: %d"), ExistingObjects.Num());
    UE_LOG(LogTemp, Warning, TEXT("缺失的对象数量: %d"), MissingObjects.Num());

    if (ExistingObjects.Num() > 0) {
        UE_LOG(LogTemp, Warning, TEXT("存在的对象:"));
        for (const FString& ObjName : ExistingObjects) {
            UE_LOG(LogTemp, Warning, TEXT("  ✓ %s"), *ObjName);
        }
    }

    if (MissingObjects.Num() > 0) {
        UE_LOG(LogTemp, Warning, TEXT("缺失的对象:"));
        for (const FString& ObjName : MissingObjects) {
            UE_LOG(LogTemp, Warning, TEXT("  ✗ %s"), *ObjName);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("========================"));
}

void UStringFlowControlRigProcessor::SetupAllObjects(
    AStringFlowUnreal* StringFlowActor) {
    if (!FStringFlowControlRigHelper::ValidateStringFlowActor(
            StringFlowActor, TEXT("SetupAllObjects"))) {
        return;
    }

    if (StringFlowActor->StringInstrument && GEngine) {
        UControlRigCacheSubsystem* CacheSubsystem =
            GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
        if (CacheSubsystem) {
            ULevelSequence* LevelSequence =
                UInstrumentAnimationUtility::GetCurrentLevelSequence();
            if (LevelSequence) {
                // 触发注册，确保 ControlRig 已注册到缓存
                CacheSubsystem->TriggerRegistrationIfNeeded(
                    StringFlowActor->StringInstrument, LevelSequence);
            }
        }
    }

    UControlRig* ControlRigInstance =
        StringFlowActor->GetCachedControlRig(TEXT("Performer"));
    UControlRigBlueprint* ControlRigBlueprint =
        StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "StringFlowActor"));
        return;
    }

    SetupControllers(StringFlowActor);

    // 添加Bone Control Mapping变量
    if (ControlRigBlueprint) {
        FBoneControlMappingUtility::AddBoneControlMappingVariable(
            ControlRigBlueprint, StringFlowActor);
    }

    FStringFlowControlRigHelper::InitializeRecorderTransforms(StringFlowActor);

    UE_LOG(LogTemp, Warning, TEXT("All StringFlow objects have been set up"));
}

void UStringFlowControlRigProcessor::SetupControllers(
    AStringFlowUnreal* StringFlowActor) {
    if (!FStringFlowControlRigHelper::ValidateStringFlowActor(
            StringFlowActor, TEXT("SetupControllers"))) {
        return;
    }

    UControlRig* ControlRigInstance =
        StringFlowActor->GetCachedControlRig(TEXT("Performer"));
    UControlRigBlueprint* ControlRigBlueprint =
        StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "StringFlowActor"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigBlueprint->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigBlueprint"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== SetupControllers Started =========="));

    TSet<FString> AllControllerNames =
        FStringFlowControlRigHelper::GetAllControllerNames(StringFlowActor);
    FControlRigCreationUtility::CleanupDuplicateControls(
        RigHierarchy, AllControllerNames, true);

    // 使用新的 CreateControl 接口
    if (!FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, TEXT("base_root"), TEXT(""))) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create base_root"));
        return;
    }

    if (!FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, TEXT("controller_root"), TEXT("base_root"))) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create controller_root"));
        return;
    }

    TArray<FString> SortedControllerNames = AllControllerNames.Array();

    // 预先收集右手控制器名称集合，用于判断父级
    TSet<FString> RightFingerControllerNames;
    for (const auto& Pair : StringFlowActor->RightFingerControllers)
    {
        RightFingerControllerNames.Add(Pair.Value);
    }
    TSet<FString> RightHandControllerNames;
    for (const auto& Pair : StringFlowActor->RightHandControllers)
    {
        RightHandControllerNames.Add(Pair.Value);
    }

    for (const FString& ControllerName : SortedControllerNames) {
        FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);
        if (RigHierarchy->Contains(ElementKey)) {
            UE_LOG(LogTemp, Warning, TEXT("✓ Controller '%s' already exists"),
                   *ControllerName);
            continue;
        }

        // 右手手指/手掌控制器（除 HP_R 外）全部挂在 Bow_Controller 下
        bool bIsRightHandCtrl = RightFingerControllerNames.Contains(ControllerName)
                             || RightHandControllerNames.Contains(ControllerName);
        FString ParentName = (bIsRightHandCtrl && ControllerName != TEXT("HP_R"))
                             ? TEXT("Bow_Controller")
                             : TEXT("controller_root");

        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, ControllerName, ParentName);
    }

    UE_LOG(LogTemp, Warning, TEXT("Creating special controllers..."));

    // 创建 String_Touch_Point 控制器
    FRigElementKey STPElementKey(TEXT("String_Touch_Point"), ERigElementType::Control);
    if (!RigHierarchy->Contains(STPElementKey)) {
        FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                  TEXT("String_Touch_Point"),
                                                  TEXT("controller_root"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("✓ Controller 'String_Touch_Point' already exists"));
    }

    // 创建 Bow_Controller 控制器
    FRigElementKey BowElementKey(TEXT("Bow_Controller"), ERigElementType::Control);
    if (!RigHierarchy->Contains(BowElementKey)) {
        FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                  TEXT("Bow_Controller"),
                                                  TEXT("controller_root"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("✓ Controller 'Bow_Controller' already exists"));
    }

    // 创建 Right_Hand_Tar 控制器（作为 Bow_Controller 的子级）
    FRigElementKey RHTElementKey(TEXT("Right_Hand_Tar"), ERigElementType::Control);
    if (!RigHierarchy->Contains(RHTElementKey)) {
        FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                  TEXT("Right_Hand_Tar"),
                                                  TEXT("Bow_Controller"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("✓ Controller 'Right_Hand_Tar' already exists"));
    }

    // 设置 TP 控制器为 H 的子级
    UE_LOG(LogTemp, Warning, TEXT("Setting TP controllers as children of H controllers..."));
    
    // 左手 TP_L -> H_L
    FRigElementKey TPLKey(TEXT("TP_L"), ERigElementType::Control);
    if (RigHierarchy->Contains(TPLKey)) {
        FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                  TEXT("TP_L"),
                                                  TEXT("H_L"));
        UE_LOG(LogTemp, Warning, TEXT("✓ Set TP_L as child of H_L"));
    }
    
    // 右手 TP_R -> H_R
    FRigElementKey TPRKey(TEXT("TP_R"), ERigElementType::Control);
    if (RigHierarchy->Contains(TPRKey)) {
        FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                  TEXT("TP_R"),
                                                  TEXT("H_R"));
        UE_LOG(LogTemp, Warning, TEXT("✓ Set TP_R as child of H_R"));
    }

    UE_LOG(LogTemp, Warning, TEXT("Creating pole controls for fingers..."));

    for (const auto& FingerPair : StringFlowActor->LeftFingerControllers) {
        FString FingerControlName = FingerPair.Value;
        FString PoleControlName =
            FString::Printf(TEXT("pole_%s"), *FingerControlName);
        FString HandControlName = TEXT("H_L");

        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, PoleControlName, HandControlName);
    }

    for (const auto& FingerPair : StringFlowActor->RightFingerControllers) {
        FString FingerControlName = FingerPair.Value;
        FString PoleControlName =
            FString::Printf(TEXT("pole_%s"), *FingerControlName);
        // 右手 pole 控制器挂在 Bow_Controller 下
        FString HandControlName = TEXT("Bow_Controller");

        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, PoleControlName, HandControlName);
    }

    UE_LOG(LogTemp, Warning, TEXT("Pole controls creation completed"));

    UE_LOG(LogTemp, Warning,
           TEXT("Creating string reference position controllers..."));

    for (int32 StringIndex = 0; StringIndex < StringFlowActor->StringNumber;
         ++StringIndex) {
        FString StringStartName =
            FString::Printf(TEXT("position_s%d_f0"), StringIndex);
        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, StringStartName, TEXT("controller_root"));

        FString StringEndName =
            FString::Printf(TEXT("position_s%d_f12"), StringIndex);
        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, StringEndName, TEXT("controller_root"));

        FString StringMidName = FString::Printf(TEXT("mid_s%d"), StringIndex);
        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, StringMidName, TEXT("controller_root"));

        FString StringF9Name = FString::Printf(TEXT("f9_s%d"), StringIndex);
        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, StringF9Name, TEXT("controller_root"));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== SetupControllers Fully Completed =========="));
}

void UStringFlowControlRigProcessor::SaveState(
    AStringFlowUnreal* StringFlowActor) {
    if (!FStringFlowControlRigHelper::ValidateStringFlowActorBasic(
            StringFlowActor, TEXT("SaveState"))) {
        return;
    }

    UControlRig* ControlRigInstance =
        StringFlowActor->GetCachedControlRig(TEXT("Performer"));
    UControlRigBlueprint* ControlRigBlueprint =
        StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "StringFlowActor"));
        return;
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get Control Rig Instance"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigInstance"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow SaveState Started =========="));

    UE_LOG(LogTemp, Warning, TEXT("Current Playing State:"));
    UE_LOG(LogTemp, Warning, TEXT("  Left Hand Position: %d (Position Type)"),
           (int32)StringFlowActor->LeftHandPositionType);
    UE_LOG(LogTemp, Warning, TEXT("  Right Hand Position: %d (Position Type)"),
           (int32)StringFlowActor->RightHandPositionType);
    UE_LOG(LogTemp, Warning,
           TEXT("  Left Hand Fret Index: %d (FretIndex enum)"),
           (int32)StringFlowActor->LeftHandFretIndex);
    UE_LOG(LogTemp, Warning,
           TEXT("  Right Hand String Index: %d (StringIndex enum)"),
           (int32)StringFlowActor->RightHandStringIndex);

    int32 CurrentStringNum = (int32)StringFlowActor->RightHandStringIndex;
    int32 CurrentFretNum = 0;

    switch (StringFlowActor->LeftHandFretIndex) {
        case EStringFlowLeftHandFretIndex::FRET_1:
            CurrentFretNum = 1;
            break;
        case EStringFlowLeftHandFretIndex::FRET_9:
            CurrentFretNum = 9;
            break;
        case EStringFlowLeftHandFretIndex::FRET_12:
            CurrentFretNum = 12;
            break;
    }

    UE_LOG(LogTemp, Warning, TEXT("Current Playing String: %d, Fret: %d"),
           CurrentStringNum, CurrentFretNum);

    int32 SavedCount = 0;
    int32 FailedCount = 0;

    ControlRigInstance->Evaluate_AnyThread();

    FString LeftPositionStr = StringFlowActor->GetLeftHandPositionTypeString(
        StringFlowActor->LeftHandPositionType);
    FString RightPositionStr = StringFlowActor->GetRightHandPositionTypeString(
        StringFlowActor->RightHandPositionType);

    UE_LOG(LogTemp, Warning, TEXT("Position strings: Left=%s, Right=%s"),
           *LeftPositionStr, *RightPositionStr);

    // 保存左手手指控制器
    FStringFlowControlRigHelper::SaveStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT, SavedCount,
        FailedCount);

    // 保存右手手指控制器
    FStringFlowControlRigHelper::SaveStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::RIGHT,
        SavedCount, FailedCount);

    // 保存左手掌控制器
    FStringFlowControlRigHelper::SaveStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT, SavedCount,
        FailedCount);

    // 保存右手掌控制器
    FStringFlowControlRigHelper::SaveStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::RIGHT,
        SavedCount, FailedCount);

    // 保存状态相关的其他控制器 (stp, bow_position)
    FStringFlowControlRigHelper::SaveStateDependentOtherControllers(
        StringFlowActor, RigHierarchy, SavedCount, FailedCount);

    // 保存状态无关的其他控制器 (mid_s*, f9_s*, position_s*_f*, etc.)
    FStringFlowControlRigHelper::SaveStatelessOtherControllers(
        StringFlowActor, RigHierarchy, SavedCount, FailedCount);

    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow SaveState Completed =========="));
    UE_LOG(LogTemp, Warning, TEXT("成功保存控制器数量: %d"), SavedCount);
    UE_LOG(LogTemp, Warning, TEXT("失败保存控制器数量: %d"), FailedCount);
}

void UStringFlowControlRigProcessor::SaveLeft(
    AStringFlowUnreal* StringFlowActor) {
    if (!FStringFlowControlRigHelper::ValidateStringFlowActorBasic(
            StringFlowActor, TEXT("SaveLeft"))) {
        return;
    }

    UControlRig* ControlRigInstance =
        StringFlowActor->GetCachedControlRig(TEXT("Performer"));
    UControlRigBlueprint* ControlRigBlueprint =
        StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "StringFlowActor"));
        // 尝试重新注册 ControlRig
        UE_LOG(LogTemp, Warning,
               TEXT("SaveLeft: Attempting to re-register ControlRig..."));
        StringFlowActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveLeft"));
        
        // 重新尝试获取 ControlRig
        ControlRigInstance =
            StringFlowActor->GetCachedControlRig(TEXT("Performer"));
        ControlRigBlueprint =
            StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));
        
        if (!ControlRigInstance || !ControlRigBlueprint) {
            UE_LOG(LogTemp, Error,
                   TEXT("Still failed to get ControlRig after re-registration"));
            return;
        }
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get Control Rig Instance"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigInstance"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow SaveLeft Started =========="));

    UE_LOG(LogTemp, Warning, TEXT("Current Playing State:"));
    UE_LOG(LogTemp, Warning, TEXT("  Left Hand Position: %d (Position Type)"),
           (int32)StringFlowActor->LeftHandPositionType);
    UE_LOG(LogTemp, Warning,
           TEXT("  Left Hand Fret Index: %d (FretIndex enum)"),
           (int32)StringFlowActor->LeftHandFretIndex);

    int32 CurrentStringNum = (int32)StringFlowActor->RightHandStringIndex;
    int32 CurrentFretNum = 0;

    switch (StringFlowActor->LeftHandFretIndex) {
        case EStringFlowLeftHandFretIndex::FRET_1:
            CurrentFretNum = 1;
            break;
        case EStringFlowLeftHandFretIndex::FRET_9:
            CurrentFretNum = 9;
            break;
        case EStringFlowLeftHandFretIndex::FRET_12:
            CurrentFretNum = 12;
            break;
    }

    UE_LOG(LogTemp, Warning, TEXT("Current Playing String: %d, Fret: %d"),
           CurrentStringNum, CurrentFretNum);

    int32 SavedCount = 0;
    int32 FailedCount = 0;

    ControlRigInstance->Evaluate_AnyThread();

    FString LeftPositionStr = StringFlowActor->GetLeftHandPositionTypeString(
        StringFlowActor->LeftHandPositionType);

    UE_LOG(LogTemp, Warning, TEXT("Position strings: Left=%s"),
           *LeftPositionStr);

    // 保存左手手指控制器
    FStringFlowControlRigHelper::SaveStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT, SavedCount,
        FailedCount);

    // 保存左手掌控制器
    FStringFlowControlRigHelper::SaveStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT, SavedCount,
        FailedCount);

    // 保存状态无关的其他控制器 (mid_s*, f9_s*, position_s*_f*, etc.)
    FStringFlowControlRigHelper::SaveStatelessOtherControllers(
        StringFlowActor, RigHierarchy, SavedCount, FailedCount);

    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow SaveLeft Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Playing State -> String: %d, Fret: %d"),
           CurrentStringNum, CurrentFretNum);
    UE_LOG(LogTemp, Warning, TEXT("Successfully updated: %d transforms"),
           SavedCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed: %d transforms"), FailedCount);
    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow SaveLeft Completed =========="));

    if (StringFlowActor) {
        StringFlowActor->MarkPackageDirty();
    }
}

void UStringFlowControlRigProcessor::SaveRight(
    AStringFlowUnreal* StringFlowActor) {
    if (!FStringFlowControlRigHelper::ValidateStringFlowActorBasic(
            StringFlowActor, TEXT("SaveRight"))) {
        return;
    }

    UControlRig* ControlRigInstance =
        StringFlowActor->GetCachedControlRig(TEXT("Performer"));
    UControlRigBlueprint* ControlRigBlueprint =
        StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "StringFlowActor"));
        // 尝试重新注册 ControlRig
        UE_LOG(LogTemp, Warning,
               TEXT("SaveRight: Attempting to re-register ControlRig..."));
        StringFlowActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during SaveRight"));
        
        // 重新尝试获取 ControlRig
        ControlRigInstance =
            StringFlowActor->GetCachedControlRig(TEXT("Performer"));
        ControlRigBlueprint =
            StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));
        
        if (!ControlRigInstance || !ControlRigBlueprint) {
            UE_LOG(LogTemp, Error,
                   TEXT("Still failed to get ControlRig after re-registration"));
            return;
        }
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get Control Rig Instance"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigInstance"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow SaveRight Started =========="));

    UE_LOG(LogTemp, Warning, TEXT("Current Playing State:"));
    UE_LOG(LogTemp, Warning, TEXT("  Right Hand Position: %d (Position Type)"),
           (int32)StringFlowActor->RightHandPositionType);
    UE_LOG(LogTemp, Warning,
           TEXT("  Right Hand String Index: %d (StringIndex enum)"),
           (int32)StringFlowActor->RightHandStringIndex);

    int32 CurrentStringNum = (int32)StringFlowActor->RightHandStringIndex;

    UE_LOG(LogTemp, Warning, TEXT("Current Playing String: %d"),
           CurrentStringNum);

    int32 SavedCount = 0;
    int32 FailedCount = 0;

    ControlRigInstance->Evaluate_AnyThread();

    FString RightPositionStr = StringFlowActor->GetRightHandPositionTypeString(
        StringFlowActor->RightHandPositionType);

    UE_LOG(LogTemp, Warning, TEXT("Position strings: Right=%s"),
           *RightPositionStr);

    // 保存右手手指控制器
    FStringFlowControlRigHelper::SaveStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightFingerControllers,
        CurrentStringNum, 0, EStringFlowHandType::RIGHT, SavedCount,
        FailedCount);

    // 保存右手掌控制器
    FStringFlowControlRigHelper::SaveStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightHandControllers,
        CurrentStringNum, 0, EStringFlowHandType::RIGHT, SavedCount,
        FailedCount);

    // 保存状态相关的其他控制器 (stp, bow_position)
    FStringFlowControlRigHelper::SaveStateDependentOtherControllers(
        StringFlowActor, RigHierarchy, SavedCount, FailedCount);

    // 保存状态无关的其他控制器 (mid_s*, f9_s*, position_s*_f*, etc.)
    FStringFlowControlRigHelper::SaveStatelessOtherControllers(
        StringFlowActor, RigHierarchy, SavedCount, FailedCount);

    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow SaveRight Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Playing State -> String: %d"),
           CurrentStringNum);
    UE_LOG(LogTemp, Warning, TEXT("Successfully updated: %d transforms"),
           SavedCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed: %d transforms"), FailedCount);
    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow SaveRight Completed =========="));

    if (StringFlowActor) {
        StringFlowActor->MarkPackageDirty();
    }
}

void UStringFlowControlRigProcessor::LoadState(
    AStringFlowUnreal* StringFlowActor) {
    if (!FStringFlowControlRigHelper::ValidateStringFlowActorBasic(
            StringFlowActor, TEXT("LoadState"))) {
        return;
    }

    UControlRig* ControlRigInstance =
        StringFlowActor->GetCachedControlRig(TEXT("Performer"));
    UControlRigBlueprint* ControlRigBlueprint =
        StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));

    if (!ControlRigInstance || !ControlRigBlueprint) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint from "
                    "StringFlowActor"));
        // 尝试重新注册 ControlRig
        UE_LOG(LogTemp, Warning,
               TEXT("LoadState: Attempting to re-register ControlRig..."));
        StringFlowActor->TriggerControlRigReregistration(
            TEXT("ControlRig not found during LoadState"));
        
        // 重新尝试获取 ControlRig
        ControlRigInstance =
            StringFlowActor->GetCachedControlRig(TEXT("Performer"));
        ControlRigBlueprint =
            StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));
        
        if (!ControlRigInstance || !ControlRigBlueprint) {
            UE_LOG(LogTemp, Error,
                   TEXT("Still failed to get ControlRig after re-registration"));
            return;
        }
    }

    if (!ControlRigInstance) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get Control Rig Instance"));
        return;
    }

    URigHierarchy* RigHierarchy = ControlRigInstance->GetHierarchy();
    if (!RigHierarchy) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get hierarchy from ControlRigInstance"));
        return;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow LoadState Started =========="));

    UE_LOG(LogTemp, Warning, TEXT("Current Playing State:"));
    UE_LOG(LogTemp, Warning, TEXT("  Left Hand Position: %d (Position Type)"),
           (int32)StringFlowActor->LeftHandPositionType);
    UE_LOG(LogTemp, Warning, TEXT("  Right Hand Position: %d (Position Type)"),
           (int32)StringFlowActor->RightHandPositionType);
    UE_LOG(LogTemp, Warning,
           TEXT("  Left Hand Fret Index: %d (FretIndex enum)"),
           (int32)StringFlowActor->LeftHandFretIndex);
    UE_LOG(LogTemp, Warning,
           TEXT("  Right Hand String Index: %d (StringIndex enum)"),
           (int32)StringFlowActor->RightHandStringIndex);

    int32 CurrentStringNum = (int32)StringFlowActor->RightHandStringIndex;
    int32 CurrentFretNum = 0;

    switch (StringFlowActor->LeftHandFretIndex) {
        case EStringFlowLeftHandFretIndex::FRET_1:
            CurrentFretNum = 1;
            break;
        case EStringFlowLeftHandFretIndex::FRET_9:
            CurrentFretNum = 9;
            break;
        case EStringFlowLeftHandFretIndex::FRET_12:
            CurrentFretNum = 12;
            break;
    }

    UE_LOG(LogTemp, Warning, TEXT("Current Playing String: %d, Fret: %d"),
           CurrentStringNum, CurrentFretNum);

    int32 LoadedCount = 0;
    int32 FailedCount = 0;

    FString LeftPositionStr = StringFlowActor->GetLeftHandPositionTypeString(
        StringFlowActor->LeftHandPositionType);
    FString RightPositionStr = StringFlowActor->GetRightHandPositionTypeString(
        StringFlowActor->RightHandPositionType);

    UE_LOG(LogTemp, Warning, TEXT("Position strings: Left=%s, Right=%s"),
           *LeftPositionStr, *RightPositionStr);

    // 加载左手手指控制器
    FStringFlowControlRigHelper::LoadStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT,
        LoadedCount, FailedCount);

    // 加载右手手指控制器
    FStringFlowControlRigHelper::LoadStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::RIGHT,
        LoadedCount, FailedCount);

    // 加载左手掌控制器
    FStringFlowControlRigHelper::LoadStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT,
        LoadedCount, FailedCount);

    // 加载右手掌控制器
    FStringFlowControlRigHelper::LoadStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::RIGHT,
        LoadedCount, FailedCount);

    // 加载状态相关的其他控制器 (stp, bow_position)
    FStringFlowControlRigHelper::LoadStateDependentOtherControllers(
        StringFlowActor, RigHierarchy, LoadedCount, FailedCount);

    // 加载状态无关的其他控制器 (mid_s*, f9_s*, position_s*_f*, etc.)
    FStringFlowControlRigHelper::LoadStatelessOtherControllers(
        StringFlowActor, RigHierarchy, LoadedCount, FailedCount);

// 重新评估 Control Rig 以传播变更（约束、IK 等）
// 注意：不能调用 ForceEvaluate / RefreshCurrentLevelSequence，
// 否则 Sequencer 会重新从轨道读取关键帧数据，覆盖刚写入的值
if (ControlRigInstance) {
    ControlRigInstance->Evaluate_AnyThread();
}

UE_LOG(LogTemp, Warning,
       TEXT("========== StringFlow LoadState Summary =========="));
    UE_LOG(LogTemp, Warning, TEXT("Playing State -> String: %d, Fret: %d"),
           CurrentStringNum, CurrentFretNum);
    UE_LOG(LogTemp, Warning, TEXT("Successfully loaded: %d transforms"),
           LoadedCount);
    UE_LOG(LogTemp, Warning, TEXT("Failed: %d transforms"), FailedCount);
    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow LoadState Completed =========="));
}
#undef LOCTEXT_NAMESPACE