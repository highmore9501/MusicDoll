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
#include "MovieSceneSequence.h"
#include "Rigs/RigHierarchyController.h"

#define LOCTEXT_NAMESPACE "StringFlowControlRigProcessor"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Local helper structure - contains all static helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct FStringFlowControlRigHelpers {
    // ========================================
    // Validation methods
    // ========================================

    static bool ValidateStringFlowActorBasic(AStringFlowUnreal* StringFlowActor,
                                             const FString& FunctionName) {
        if (!StringFlowActor) {
            UE_LOG(LogTemp, Error, TEXT("%s: StringFlowActor is null"),
                   *FunctionName);
            return false;
        }
        return true;
    }

    static bool ValidateStringFlowActor(AStringFlowUnreal* StringFlowActor,
                                        const FString& FunctionName) {
        if (!StringFlowActor) {
            UE_LOG(LogTemp, Error, TEXT("%s: StringFlowActor is null"),
                   *FunctionName);
            return false;
        }
        if (!StringFlowActor->StringInstrument) {
            UE_LOG(
                LogTemp, Error,
                TEXT("%s: StringInstrument is not assigned in StringFlowActor"),
                *FunctionName);
            return false;
        }
        return true;
    }

    static bool StrictControlExistenceCheck(URigHierarchy* RigHierarchy,
                                            const FString& ControllerName) {
        if (!RigHierarchy) {
            return false;
        }

        FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);

        if (!RigHierarchy->Contains(ElementKey)) {
            return false;
        }

        FRigControlElement* ControlElement =
            RigHierarchy->Find<FRigControlElement>(ElementKey);
        if (!ControlElement) {
            UE_LOG(LogTemp, Warning,
                   TEXT("Control '%s' exists in hierarchy but element is null"),
                   *ControllerName);
            return false;
        }

        return true;
    }

    // ========================================
    // Controller retrieval
    // ========================================

    static bool GetControlRigInstanceAndBlueprint(
        AStringFlowUnreal* StringFlowActor, UControlRig*& OutControlRigInstance,
        UControlRigBlueprint*& OutControlRigBlueprint) {
        // 直接使用StringFlowActor获取演奏者的ControlRig缓存
        OutControlRigInstance =
            StringFlowActor->GetCachedControlRig(TEXT("Performer"));
        OutControlRigBlueprint =
            StringFlowActor->GetCachedControlRigBlueprint(TEXT("Performer"));

        // 不再提供后备查询，如果缓存未命中则直接失败
        if (!OutControlRigInstance || !OutControlRigBlueprint) {
            UE_LOG(LogTemp, Error,
                   TEXT("StringFlowControlRigHelpers: Failed to get ControlRig "
                        "for Performer - cache miss"));
            return false;
        }

        return OutControlRigInstance && OutControlRigBlueprint;
    }

    // ========================================
    // Controller name collection
    // ========================================

    static TSet<FString> GetAllControllerNames(
        AStringFlowUnreal* StringFlowActor) {
        TSet<FString> AllControllerNames;

        if (!StringFlowActor) {
            return AllControllerNames;
        }

        for (const auto& Pair : StringFlowActor->LeftFingerControllers) {
            AllControllerNames.Add(Pair.Value);
        }

        for (const auto& Pair : StringFlowActor->RightFingerControllers) {
            AllControllerNames.Add(Pair.Value);
        }

        for (const auto& Pair : StringFlowActor->LeftHandControllers) {
            AllControllerNames.Add(Pair.Value);
        }

        for (const auto& Pair : StringFlowActor->RightHandControllers) {
            AllControllerNames.Add(Pair.Value);
        }

        for (const auto& Pair : StringFlowActor->OtherControllers) {
            AllControllerNames.Add(Pair.Value);
        }

        for (const auto& Pair : StringFlowActor->GuideLines) {
            AllControllerNames.Add(Pair.Value);
        }

        return AllControllerNames;
    }

    // ========================================
    // Control cleanup
    // ========================================

    static void CleanupDuplicateControls(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        const TSet<FString>& ExpectedControllerNames) {
        if (!RigHierarchy) {
            return;
        }

        FControlRigCreationUtility::CleanupDuplicateControls(
            RigHierarchy, ExpectedControllerNames, true);
    }

    // ========================================
    // Recorder initialization
    // ========================================

    static void InitializeRecorderTransforms(
        AStringFlowUnreal* StringFlowActor) {
        if (!StringFlowActor) {
            return;
        }

        // 如果已经初始化过，跳过重复初始化
        if (StringFlowActor->IsInitialized()) {
            UE_LOG(LogTemp, Verbose,
                   TEXT("InitializeRecorderTransforms: Already initialized, "
                        "skipping"));
            return;
        }

        StringFlowActor->RecorderTransforms.Empty();

        UE_LOG(
            LogTemp, Warning,
            TEXT(
                "Initializing all recorder keys in RecorderTransforms map from "
                "existing lists..."));

        int32 KeyCount = 0;
        FStringFlowRecorderTransform DefaultTransform;
        DefaultTransform.Location = FVector::ZeroVector;
        DefaultTransform.Rotation = FQuat::Identity;

        const FStringFlowStringArray* LeftFingerArray =
            StringFlowActor->LeftFingerRecorders.Find(
                TEXT("left_finger_recorders"));
        if (LeftFingerArray) {
            for (int32 i = 0; i < LeftFingerArray->Num(); ++i) {
                StringFlowActor->RecorderTransforms.Add(LeftFingerArray->Get(i),
                                                        DefaultTransform);
                KeyCount++;
            }
        }

        const FStringFlowStringArray* LeftHandPositionArray =
            StringFlowActor->LeftHandPositionRecorders.Find(
                TEXT("left_hand_position_recorders"));
        if (LeftHandPositionArray) {
            for (int32 i = 0; i < LeftHandPositionArray->Num(); ++i) {
                StringFlowActor->RecorderTransforms.Add(
                    LeftHandPositionArray->Get(i), DefaultTransform);
                KeyCount++;
            }
        }

        const FStringFlowStringArray* LeftThumbArray =
            StringFlowActor->LeftThumbRecorders.Find(
                TEXT("left_thumb_position_recorders"));
        if (LeftThumbArray) {
            for (int32 i = 0; i < LeftThumbArray->Num(); ++i) {
                StringFlowActor->RecorderTransforms.Add(LeftThumbArray->Get(i),
                                                        DefaultTransform);
                KeyCount++;
            }
        }

        const FStringFlowStringArray* RightFingerArray =
            StringFlowActor->RightFingerRecorders.Find(
                TEXT("right_finger_recorders"));
        if (RightFingerArray) {
            for (int32 i = 0; i < RightFingerArray->Num(); ++i) {
                StringFlowActor->RecorderTransforms.Add(
                    RightFingerArray->Get(i), DefaultTransform);
                KeyCount++;
            }
        }

        const FStringFlowStringArray* RightHandPositionArray =
            StringFlowActor->RightHandPositionRecorders.Find(
                TEXT("right_hand_position_recorders"));
        if (RightHandPositionArray) {
            for (int32 i = 0; i < RightHandPositionArray->Num(); ++i) {
                StringFlowActor->RecorderTransforms.Add(
                    RightHandPositionArray->Get(i), DefaultTransform);
                KeyCount++;
            }
        }

        const FStringFlowStringArray* RightThumbArray =
            StringFlowActor->RightThumbRecorders.Find(
                TEXT("right_thumb_position_recorders"));
        if (RightThumbArray) {
            for (int32 i = 0; i < RightThumbArray->Num(); ++i) {
                StringFlowActor->RecorderTransforms.Add(RightThumbArray->Get(i),
                                                        DefaultTransform);
                KeyCount++;
            }
        }

        const FStringFlowStringArray* OtherArray =
            StringFlowActor->OtherRecorders.Find(TEXT("other_recorders"));
        if (OtherArray) {
            for (int32 i = 0; i < OtherArray->Num(); ++i) {
                StringFlowActor->RecorderTransforms.Add(OtherArray->Get(i),
                                                        DefaultTransform);
                KeyCount++;
            }
        }

        for (const auto& GuidePair : StringFlowActor->GuideLines) {
            StringFlowActor->RecorderTransforms.Add(GuidePair.Value,
                                                    DefaultTransform);
            KeyCount++;
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT("Initialized %d recorder keys in RecorderTransforms map from "
                 "existing lists"),
            KeyCount);
    }

    // ========================================
    // State-dependent recorder name generation for stp and bow
    // ========================================

    static FString GenerateStateDependentSTPRecorderName(
        AStringFlowUnreal* StringFlowActor) {
        if (!StringFlowActor) {
            return FString();
        }

        int32 StringIndex = (int32)StringFlowActor->RightHandStringIndex;
        FString RightPositionStr =
            StringFlowActor->GetRightHandPositionTypeString(
                StringFlowActor->RightHandPositionType);
        return FString::Printf(TEXT("stp_%d_%s"), StringIndex,
                               *RightPositionStr);
    }

    static FString GenerateStateDependentBowRecorderName(
        AStringFlowUnreal* StringFlowActor) {
        if (!StringFlowActor) {
            return FString();
        }

        int32 StringIndex = (int32)StringFlowActor->RightHandStringIndex;
        FString RightPositionStr =
            StringFlowActor->GetRightHandPositionTypeString(
                StringFlowActor->RightHandPositionType);
        return FString::Printf(TEXT("bow_position_s%d_%s"), StringIndex,
                               *RightPositionStr);
    }

    // ========================================
    // Single controller save/load methods
    // ========================================

    static void SaveSingleController(AStringFlowUnreal* StringFlowActor,
                                     URigHierarchy* RigHierarchy,
                                     const FString& ControlName,
                                     const FString& RecorderName,
                                     int32& SavedCount, int32& FailedCount) {
        if (!StringFlowActor || !RigHierarchy) {
            FailedCount++;
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("  Processing: %s -> %s"), *ControlName,
               *RecorderName);

        FStringFlowRecorderTransform* ExistingTransform =
            StringFlowActor->RecorderTransforms.Find(RecorderName);
        if (!ExistingTransform) {
            UE_LOG(
                LogTemp, Warning,
                TEXT("    ⚠ RecorderKey '%s' NOT FOUND in RecorderTransforms"),
                *RecorderName);
            FailedCount++;
            return;
        }

        FRigElementKey ControlKey(*ControlName, ERigElementType::Control);
        if (!RigHierarchy->Contains(ControlKey)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("    ⚠ Control '%s' NOT FOUND in RigHierarchy"),
                   *ControlName);
            FailedCount++;
            return;
        }

        FRigControlElement* ControlElement =
            RigHierarchy->Find<FRigControlElement>(ControlKey);
        if (!ControlElement) {
            UE_LOG(LogTemp, Warning, TEXT("    ⚠ ControlElement '%s' is NULL"),
                   *ControlName);
            FailedCount++;
            return;
        }

        FRigControlValue CurrentValue = RigHierarchy->GetControlValue(
            ControlElement, ERigControlValueType::Current);
        FTransform CurrentTransform =
            CurrentValue.GetAsTransform(ControlElement->Settings.ControlType,
                                        ControlElement->Settings.PrimaryAxis);

        FStringFlowRecorderTransform RecorderTransform;
        RecorderTransform.FromTransform(CurrentTransform);

        UE_LOG(LogTemp, Warning,
               TEXT("    ✓ Saved: %s -> Loc(%.2f, %.2f, %.2f)"), *RecorderName,
               RecorderTransform.Location.X, RecorderTransform.Location.Y,
               RecorderTransform.Location.Z);

        StringFlowActor->RecorderTransforms[RecorderName] = RecorderTransform;
        SavedCount++;
    }

    static void LoadSingleController(AStringFlowUnreal* StringFlowActor,
                                     URigHierarchy* RigHierarchy,
                                     const FString& ControlName,
                                     const FString& RecorderName,
                                     int32& LoadedCount, int32& FailedCount) {
        if (!StringFlowActor || !RigHierarchy) {
            FailedCount++;
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("  Processing: %s <- %s"), *ControlName,
               *RecorderName);

        const FStringFlowRecorderTransform* FoundTransform =
            StringFlowActor->RecorderTransforms.Find(RecorderName);
        if (!FoundTransform) {
            UE_LOG(
                LogTemp, Warning,
                TEXT("    ⚠ RecorderKey '%s' NOT FOUND in RecorderTransforms"),
                *RecorderName);
            FailedCount++;
            return;
        }

        FRigElementKey ControlKey(*ControlName, ERigElementType::Control);
        if (!RigHierarchy->Contains(ControlKey)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("    ⚠ Control '%s' NOT FOUND in RigHierarchy"),
                   *ControlName);
            FailedCount++;
            return;
        }

        FRigControlElement* ControlElement =
            RigHierarchy->Find<FRigControlElement>(ControlKey);
        if (!ControlElement) {
            UE_LOG(LogTemp, Warning, TEXT("    ⚠ ControlElement '%s' is NULL"),
                   *ControlName);
            FailedCount++;
            return;
        }

        FTransform NewTransform = FoundTransform->ToTransform();
        FRigControlValue NewValue;
        NewValue.SetFromTransform(NewTransform,
                                  ControlElement->Settings.ControlType,
                                  ControlElement->Settings.PrimaryAxis);

        RigHierarchy->SetControlValue(ControlElement, NewValue,
                                      ERigControlValueType::Current);

        UE_LOG(LogTemp, Warning,
               TEXT("    ✓ Loaded: %s <- Loc(%.2f, %.2f, %.2f)"), *RecorderName,
               FoundTransform->Location.X, FoundTransform->Location.Y,
               FoundTransform->Location.Z);

        LoadedCount++;
    }

    // ========================================
    // Batch controller processing methods
    // ========================================

    static void SaveStateDependentFingerControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32 StringIndex,
        int32 FretIndex, EStringFlowHandType HandType, int32& SavedCount,
        int32& FailedCount) {
        if (!StringFlowActor) {
            return;
        }

        FString PositionStr =
            (HandType == EStringFlowHandType::LEFT)
                ? StringFlowActor->GetLeftHandPositionTypeString(
                      StringFlowActor->LeftHandPositionType)
                : StringFlowActor->GetRightHandPositionTypeString(
                      StringFlowActor->RightHandPositionType);

        for (const auto& ControllerPair : Controllers) {
            int32 FingerNumber = FCString::Atoi(*ControllerPair.Key);
            FString ControlName = ControllerPair.Value;

            FString RecorderName;
            if (HandType == EStringFlowHandType::LEFT) {
                RecorderName = StringFlowActor->GetLeftFingerRecorderName(
                    StringIndex, FretIndex, FingerNumber, PositionStr);
            } else {
                // 右手不包含品格信息，使用专用方法
                RecorderName = StringFlowActor->GetRightFingerRecorderName(
                    StringIndex, FingerNumber, PositionStr);
            }

            SaveSingleController(StringFlowActor, RigHierarchy, ControlName,
                                 RecorderName, SavedCount, FailedCount);
        }
    }

    static void LoadStateDependentFingerControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32 StringIndex,
        int32 FretIndex, EStringFlowHandType HandType, int32& LoadedCount,
        int32& FailedCount) {
        if (!StringFlowActor) {
            return;
        }

        FString PositionStr =
            (HandType == EStringFlowHandType::LEFT)
                ? StringFlowActor->GetLeftHandPositionTypeString(
                      StringFlowActor->LeftHandPositionType)
                : StringFlowActor->GetRightHandPositionTypeString(
                      StringFlowActor->RightHandPositionType);

        for (const auto& ControllerPair : Controllers) {
            int32 FingerNumber = FCString::Atoi(*ControllerPair.Key);
            FString ControlName = ControllerPair.Value;

            FString RecorderName;
            if (HandType == EStringFlowHandType::LEFT) {
                RecorderName = StringFlowActor->GetLeftFingerRecorderName(
                    StringIndex, FretIndex, FingerNumber, PositionStr);
            } else {
                // 右手不包含品格信息，使用专用方法
                RecorderName = StringFlowActor->GetRightFingerRecorderName(
                    StringIndex, FingerNumber, PositionStr);
            }

            LoadSingleController(StringFlowActor, RigHierarchy, ControlName,
                                 RecorderName, LoadedCount, FailedCount);
        }
    }

    static void SaveStateDependentHandControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32 StringIndex,
        int32 FretIndex, EStringFlowHandType HandType, int32& SavedCount,
        int32& FailedCount) {
        if (!StringFlowActor) {
            return;
        }

        FString PositionStr =
            (HandType == EStringFlowHandType::LEFT)
                ? StringFlowActor->GetLeftHandPositionTypeString(
                      StringFlowActor->LeftHandPositionType)
                : StringFlowActor->GetRightHandPositionTypeString(
                      StringFlowActor->RightHandPositionType);

        for (const auto& ControllerPair : Controllers) {
            FString ControlName = ControllerPair.Value;
            FString HandControllerType = ControllerPair.Key;

            FString RecorderName;
            if (HandType == EStringFlowHandType::LEFT) {
                RecorderName = StringFlowActor->GetLeftHandRecorderName(
                    StringIndex, FretIndex, HandControllerType, PositionStr);
            } else {
                // 右手不包含品格信息，使用专用方法
                RecorderName = StringFlowActor->GetRightHandRecorderName(
                    StringIndex, HandControllerType, PositionStr);
            }

            SaveSingleController(StringFlowActor, RigHierarchy, ControlName,
                                 RecorderName, SavedCount, FailedCount);
        }
    }

    static void LoadStateDependentHandControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32 StringIndex,
        int32 FretIndex, EStringFlowHandType HandType, int32& LoadedCount,
        int32& FailedCount) {
        if (!StringFlowActor) {
            return;
        }

        FString PositionStr =
            (HandType == EStringFlowHandType::LEFT)
                ? StringFlowActor->GetLeftHandPositionTypeString(
                      StringFlowActor->LeftHandPositionType)
                : StringFlowActor->GetRightHandPositionTypeString(
                      StringFlowActor->RightHandPositionType);

        for (const auto& ControllerPair : Controllers) {
            FString ControlName = ControllerPair.Value;
            FString HandControllerType = ControllerPair.Key;

            FString RecorderName;
            if (HandType == EStringFlowHandType::LEFT) {
                RecorderName = StringFlowActor->GetLeftHandRecorderName(
                    StringIndex, FretIndex, HandControllerType, PositionStr);
            } else {
                // 右手不包含品格信息，使用专用方法
                RecorderName = StringFlowActor->GetRightHandRecorderName(
                    StringIndex, HandControllerType, PositionStr);
            }

            LoadSingleController(StringFlowActor, RigHierarchy, ControlName,
                                 RecorderName, LoadedCount, FailedCount);
        }
    }

    // ========================================
    // State-dependent other controllers (stp, bow_position)
    // ========================================

    static void SaveStateDependentOtherControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        int32& SavedCount, int32& FailedCount) {
        if (!StringFlowActor) {
            return;
        }

        UE_LOG(LogTemp, Warning,
               TEXT("Processing state-dependent other controllers (stp, "
                    "bow_position)..."));

        // 保存 String_Touch_Point 到当前弦对应的 stp 记录器
        FString STPRecorderName =
            FStringFlowControlRigHelpers::GenerateStateDependentSTPRecorderName(
                StringFlowActor);

        SaveSingleController(StringFlowActor, RigHierarchy,
                             TEXT("String_Touch_Point"), STPRecorderName,
                             SavedCount, FailedCount);

        // 保存 Bow_Controller 到当前弦对应的 bow_position 记录器
        FString BowRecorderName =
            FStringFlowControlRigHelpers::GenerateStateDependentBowRecorderName(
                StringFlowActor);

        SaveSingleController(StringFlowActor, RigHierarchy,
                             TEXT("Bow_Controller"), BowRecorderName,
                             SavedCount, FailedCount);
    }

    static void LoadStateDependentOtherControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        int32& LoadedCount, int32& FailedCount) {
        if (!StringFlowActor) {
            return;
        }

        UE_LOG(LogTemp, Warning,
               TEXT("Processing state-dependent other controllers (stp, "
                    "bow_position)..."));

        // 从当前弦对应的 stp 记录器加载到 String_Touch_Point
        FString STPRecorderName =
            FStringFlowControlRigHelpers::GenerateStateDependentSTPRecorderName(
                StringFlowActor);

        LoadSingleController(StringFlowActor, RigHierarchy,
                             TEXT("String_Touch_Point"), STPRecorderName,
                             LoadedCount, FailedCount);

        // 从当前弦对应的 bow_position 记录器加载到 Bow_Controller
        FString BowRecorderName =
            FStringFlowControlRigHelpers::GenerateStateDependentBowRecorderName(
                StringFlowActor);

        LoadSingleController(StringFlowActor, RigHierarchy,
                             TEXT("Bow_Controller"), BowRecorderName,
                             LoadedCount, FailedCount);
    }

    // ========================================
    // Stateless other controllers (mid_s*, f9_s*, position_s*_f*, etc.)
    // ========================================

    static void SaveStatelessOtherControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        int32& SavedCount, int32& FailedCount) {
        if (!StringFlowActor) {
            return;
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT("Processing stateless other controllers (position_s*_f*)..."));

        // 从 OtherRecorders 中提取所有记录器，除了 stp、bow_position、mid_s 和
        // f9_s
        const FStringFlowStringArray* OtherArray =
            StringFlowActor->OtherRecorders.Find(TEXT("other_recorders"));
        if (!OtherArray) {
            return;
        }

        for (int32 i = 0; i < OtherArray->Num(); ++i) {
            FString RecorderName = OtherArray->Get(i);

            // 跳过状态相关的 stp 和 bow_position 记录器
            if (RecorderName.StartsWith(TEXT("stp_")) ||
                RecorderName.StartsWith(TEXT("bow_position_"))) {
                continue;
            }

            // 跳过 mid_s 和 f9_s（蓝图生成的参考点，不需要保存）
            if (RecorderName.StartsWith(TEXT("mid_s")) ||
                RecorderName.StartsWith(TEXT("f9_s"))) {
                continue;
            }

            // 从记录器名称提取控制器名称
            FString ControlName = RecorderName;

            // 使用 SaveSingleController 来正确地从 RigHierarchy 读取数据并保存
            SaveSingleController(StringFlowActor, RigHierarchy, ControlName,
                                 RecorderName, SavedCount, FailedCount);
        }

        // 保存 GuideLines 控制器
        for (const auto& GuidePair : StringFlowActor->GuideLines) {
            FString ControlName = GuidePair.Value;
            FString RecorderName = GuidePair.Value;

            SaveSingleController(StringFlowActor, RigHierarchy, ControlName,
                                 RecorderName, SavedCount, FailedCount);
        }
    }

    static void LoadStatelessOtherControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        int32& LoadedCount, int32& FailedCount) {
        if (!StringFlowActor) {
            return;
        }

        UE_LOG(
            LogTemp, Warning,
            TEXT("Processing stateless other controllers (position_s*_f*)..."));

        // 从 OtherRecorders 中提取所有记录器，除了 stp、bow_position、mid_s 和
        // f9_s
        const FStringFlowStringArray* OtherArray =
            StringFlowActor->OtherRecorders.Find(TEXT("other_recorders"));
        if (!OtherArray) {
            return;
        }

        for (int32 i = 0; i < OtherArray->Num(); ++i) {
            FString RecorderName = OtherArray->Get(i);

            // 跳过状态相关的 stp 和 bow_position 记录器
            if (RecorderName.StartsWith(TEXT("stp_")) ||
                RecorderName.StartsWith(TEXT("bow_position_"))) {
                continue;
            }

            // 跳过 mid_s 和 f9_s（蓝图生成的参考点，不需要加载）
            if (RecorderName.StartsWith(TEXT("mid_s")) ||
                RecorderName.StartsWith(TEXT("f9_s"))) {
                continue;
            }

            // 检查记录器是否存在于 RecorderTransforms 中
            const FStringFlowRecorderTransform* FoundTransform =
                StringFlowActor->RecorderTransforms.Find(RecorderName);
            if (!FoundTransform) {
                UE_LOG(
                    LogTemp, Warning,
                    TEXT(
                        "  ⚠ RecorderKey '%s' NOT FOUND in RecorderTransforms"),
                    *RecorderName);
                FailedCount++;
                continue;
            }

            // 从记录器名称提取控制器名称
            FString ControlName = RecorderName;

            FRigElementKey ControlKey(*ControlName, ERigElementType::Control);
            if (!RigHierarchy->Contains(ControlKey)) {
                UE_LOG(LogTemp, Warning,
                       TEXT("    ⚠ Control '%s' NOT FOUND in RigHierarchy"),
                       *ControlName);
                FailedCount++;
                continue;
            }

            FRigControlElement* ControlElement =
                RigHierarchy->Find<FRigControlElement>(ControlKey);
            if (!ControlElement) {
                UE_LOG(LogTemp, Warning,
                       TEXT("    ⚠ ControlElement '%s' is NULL"), *ControlName);
                FailedCount++;
                continue;
            }

            FTransform NewTransform = FoundTransform->ToTransform();
            FRigControlValue NewValue;
            NewValue.SetFromTransform(NewTransform,
                                      ControlElement->Settings.ControlType,
                                      ControlElement->Settings.PrimaryAxis);

            RigHierarchy->SetControlValue(ControlElement, NewValue,
                                          ERigControlValueType::Current);

            UE_LOG(LogTemp, Warning,
                   TEXT("    ✓ Loaded: %s <- Loc(%.2f, %.2f, %.2f)"),
                   *RecorderName, FoundTransform->Location.X,
                   FoundTransform->Location.Y, FoundTransform->Location.Z);

            LoadedCount++;
        }
    }
};

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
        CacheSubsystem->GetControlRig(StringInstrumentActor, CurrentSequence);
    OutControlRigBlueprint = CacheSubsystem->GetControlRigBlueprint(
        StringInstrumentActor, CurrentSequence);

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
    if (!FStringFlowControlRigHelpers::ValidateStringFlowActor(
            StringFlowActor, TEXT("CheckObjectsStatus"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FStringFlowControlRigHelpers::GetControlRigInstanceAndBlueprint(
            StringFlowActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint"));
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

    for (const auto& Pair : StringFlowActor->OtherControllers) {
        ExpectedObjects.Add(Pair.Value);
    }

    for (const auto& Pair : StringFlowActor->GuideLines) {
        ExpectedObjects.Add(Pair.Value);
    }

    // 添加特殊的实际控制器
    ExpectedObjects.Add(TEXT("String_Touch_Point"));
    ExpectedObjects.Add(TEXT("Bow_Controller"));

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
    if (!FStringFlowControlRigHelpers::ValidateStringFlowActor(
            StringFlowActor, TEXT("SetupAllObjects"))) {
        return;
    }

    // 关键修复：先通过 StringInstrument 触发 ControlRig 注册机制
    // 这样可以确保在获取 ControlRig 之前，它已经被正确注册到缓存子系统
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

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FStringFlowControlRigHelpers::GetControlRigInstanceAndBlueprint(
            StringFlowActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint"));
        return;
    }

    SetupControllers(StringFlowActor);

    // 添加Bone Control Mapping变量
    if (ControlRigBlueprint) {
        FBoneControlMappingUtility::AddBoneControlMappingVariable(
            ControlRigBlueprint, StringFlowActor);
    }

    FStringFlowControlRigHelpers::InitializeRecorderTransforms(StringFlowActor);

    UE_LOG(LogTemp, Warning, TEXT("All StringFlow objects have been set up"));
}

void UStringFlowControlRigProcessor::SetupControllers(
    AStringFlowUnreal* StringFlowActor) {
    if (!FStringFlowControlRigHelpers::ValidateStringFlowActor(
            StringFlowActor, TEXT("SetupControllers"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FStringFlowControlRigHelpers::GetControlRigInstanceAndBlueprint(
            StringFlowActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint"));
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
        FStringFlowControlRigHelpers::GetAllControllerNames(StringFlowActor);
    FStringFlowControlRigHelpers::CleanupDuplicateControls(
        StringFlowActor, RigHierarchy, AllControllerNames);

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

    for (const FString& ControllerName : SortedControllerNames) {
        if (FStringFlowControlRigHelpers::StrictControlExistenceCheck(
                RigHierarchy, ControllerName)) {
            UE_LOG(LogTemp, Warning, TEXT("✓ Controller '%s' already exists"),
                   *ControllerName);
            continue;
        }

        FControlRigCreationUtility::CreateControl(
            ControlRigBlueprint, ControllerName, TEXT("controller_root"));
    }

    UE_LOG(LogTemp, Warning, TEXT("Creating special controllers..."));

    // 创建 String_Touch_Point 控制器
    if (!FStringFlowControlRigHelpers::StrictControlExistenceCheck(
            RigHierarchy, TEXT("String_Touch_Point"))) {
        FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                  TEXT("String_Touch_Point"),
                                                  TEXT("controller_root"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("✓ Controller 'String_Touch_Point' already exists"));
    }

    // 创建 Bow_Controller 控制器
    if (!FStringFlowControlRigHelpers::StrictControlExistenceCheck(
            RigHierarchy, TEXT("Bow_Controller"))) {
        FControlRigCreationUtility::CreateControl(ControlRigBlueprint,
                                                  TEXT("Bow_Controller"),
                                                  TEXT("controller_root"));
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("✓ Controller 'Bow_Controller' already exists"));
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
        FString HandControlName = TEXT("H_R");

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
    if (!FStringFlowControlRigHelpers::ValidateStringFlowActorBasic(
            StringFlowActor, TEXT("SaveState"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FStringFlowControlRigHelpers::GetControlRigInstanceAndBlueprint(
            StringFlowActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint"));
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
    FStringFlowControlRigHelpers::SaveStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT, SavedCount,
        FailedCount);

    // 保存右手手指控制器
    FStringFlowControlRigHelpers::SaveStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::RIGHT,
        SavedCount, FailedCount);

    // 保存左手掌控制器
    FStringFlowControlRigHelpers::SaveStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT, SavedCount,
        FailedCount);

    // 保存右手掌控制器
    FStringFlowControlRigHelpers::SaveStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::RIGHT,
        SavedCount, FailedCount);

    // 保存状态相关的其他控制器 (stp, bow_position)
    FStringFlowControlRigHelpers::SaveStateDependentOtherControllers(
        StringFlowActor, RigHierarchy, SavedCount, FailedCount);

    // 保存状态无关的其他控制器 (mid_s*, f9_s*, position_s*_f*, etc.)
    FStringFlowControlRigHelpers::SaveStatelessOtherControllers(
        StringFlowActor, RigHierarchy, SavedCount, FailedCount);

    UE_LOG(LogTemp, Warning,
           TEXT("========== StringFlow SaveState Completed =========="));
    UE_LOG(LogTemp, Warning, TEXT("成功保存控制器数量: %d"), SavedCount);
    UE_LOG(LogTemp, Warning, TEXT("失败保存控制器数量: %d"), FailedCount);
}

void UStringFlowControlRigProcessor::SaveLeft(
    AStringFlowUnreal* StringFlowActor) {
    if (!FStringFlowControlRigHelpers::ValidateStringFlowActorBasic(
            StringFlowActor, TEXT("SaveLeft"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FStringFlowControlRigHelpers::GetControlRigInstanceAndBlueprint(
            StringFlowActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint"));
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
    FStringFlowControlRigHelpers::SaveStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT, SavedCount,
        FailedCount);

    // 保存左手掌控制器
    FStringFlowControlRigHelpers::SaveStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT, SavedCount,
        FailedCount);

    // 保存状态无关的其他控制器 (mid_s*, f9_s*, position_s*_f*, etc.)
    FStringFlowControlRigHelpers::SaveStatelessOtherControllers(
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
    if (!FStringFlowControlRigHelpers::ValidateStringFlowActorBasic(
            StringFlowActor, TEXT("SaveRight"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FStringFlowControlRigHelpers::GetControlRigInstanceAndBlueprint(
            StringFlowActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint"));
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
    FStringFlowControlRigHelpers::SaveStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightFingerControllers,
        CurrentStringNum, 0, EStringFlowHandType::RIGHT, SavedCount,
        FailedCount);

    // 保存右手掌控制器
    FStringFlowControlRigHelpers::SaveStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightHandControllers,
        CurrentStringNum, 0, EStringFlowHandType::RIGHT, SavedCount,
        FailedCount);

    // 保存状态相关的其他控制器 (stp, bow_position)
    FStringFlowControlRigHelpers::SaveStateDependentOtherControllers(
        StringFlowActor, RigHierarchy, SavedCount, FailedCount);

    // 保存状态无关的其他控制器 (mid_s*, f9_s*, position_s*_f*, etc.)
    FStringFlowControlRigHelpers::SaveStatelessOtherControllers(
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
    if (!FStringFlowControlRigHelpers::ValidateStringFlowActorBasic(
            StringFlowActor, TEXT("LoadState"))) {
        return;
    }

    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* ControlRigBlueprint = nullptr;

    if (!FStringFlowControlRigHelpers::GetControlRigInstanceAndBlueprint(
            StringFlowActor, ControlRigInstance, ControlRigBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("Failed to get Control Rig Instance or Blueprint"));
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
    FStringFlowControlRigHelpers::LoadStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT,
        LoadedCount, FailedCount);

    // 加载右手手指控制器
    FStringFlowControlRigHelpers::LoadStateDependentFingerControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightFingerControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::RIGHT,
        LoadedCount, FailedCount);

    // 加载左手掌控制器
    FStringFlowControlRigHelpers::LoadStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->LeftHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::LEFT,
        LoadedCount, FailedCount);

    // 加载右手掌控制器
    FStringFlowControlRigHelpers::LoadStateDependentHandControllers(
        StringFlowActor, RigHierarchy, StringFlowActor->RightHandControllers,
        CurrentStringNum, CurrentFretNum, EStringFlowHandType::RIGHT,
        LoadedCount, FailedCount);

    // 加载状态相关的其他控制器 (stp, bow_position)
    FStringFlowControlRigHelpers::LoadStateDependentOtherControllers(
        StringFlowActor, RigHierarchy, LoadedCount, FailedCount);

    // 加载状态无关的其他控制器 (mid_s*, f9_s*, position_s*_f*, etc.)
    FStringFlowControlRigHelpers::LoadStatelessOtherControllers(
        StringFlowActor, RigHierarchy, LoadedCount, FailedCount);

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