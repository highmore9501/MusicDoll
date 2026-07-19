#include "StringFlowControlRigHelper.h"

#include "ControlRigCreationUtility.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#define LOCTEXT_NAMESPACE "StringFlowControlRigHelper"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FStringFlowControlRigHelper 实现
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FStringFlowControlRigHelper::ValidateStringFlowActorBasic(
    AStringFlowUnreal* StringFlowActor, const FString& FunctionName) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error, TEXT("%s: StringFlowActor is null"),
               *FunctionName);
        return false;
    }
    return true;
}

bool FStringFlowControlRigHelper::ValidateStringFlowActor(
    AStringFlowUnreal* StringFlowActor, const FString& FunctionName) {
    if (!StringFlowActor) {
        UE_LOG(LogTemp, Error, TEXT("%s: StringFlowActor is null"),
               *FunctionName);
        return false;
    }
    if (!StringFlowActor->StringInstrument) {
        UE_LOG(LogTemp, Error,
               TEXT("%s: StringInstrument is not assigned in StringFlowActor"),
               *FunctionName);
        return false;
    }
    return true;
}

TSet<FString> FStringFlowControlRigHelper::GetAllControllerNames(
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

    for (const auto& Pair : StringFlowActor->GuideLines) {
        AllControllerNames.Add(Pair.Value);
    }

    return AllControllerNames;
}

void FStringFlowControlRigHelper::InitializeRecorderTransforms(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        return;
    }

    UE_LOG(
        LogTemp, Verbose,
        TEXT("Ensuring all recorder keys exist in RecorderTransforms map..."));

    int32 KeyCount = 0;
    FStringFlowRecorderTransform DefaultTransform;
    DefaultTransform.Location = FVector::ZeroVector;
    DefaultTransform.Rotation = FQuat::Identity;

    auto EnsureKey = [&](const FString& Key) {
        StringFlowActor->RecorderTransforms.FindOrAdd(Key, DefaultTransform);
        KeyCount++;
    };

    const FStringFlowStringArray* LeftFingerArray =
        StringFlowActor->LeftFingerRecorders.Find(
            TEXT("left_finger_recorders"));
    if (LeftFingerArray) {
        for (int32 i = 0; i < LeftFingerArray->Num(); ++i) {
            EnsureKey(LeftFingerArray->Get(i));
        }
    }

    const FStringFlowStringArray* LeftHandPositionArray =
        StringFlowActor->LeftHandPositionRecorders.Find(
            TEXT("left_hand_position_recorders"));
    if (LeftHandPositionArray) {
        for (int32 i = 0; i < LeftHandPositionArray->Num(); ++i) {
            EnsureKey(LeftHandPositionArray->Get(i));
        }
    }

    const FStringFlowStringArray* LeftThumbArray =
        StringFlowActor->LeftThumbRecorders.Find(
            TEXT("left_thumb_position_recorders"));
    if (LeftThumbArray) {
        for (int32 i = 0; i < LeftThumbArray->Num(); ++i) {
            EnsureKey(LeftThumbArray->Get(i));
        }
    }

    const FStringFlowStringArray* RightFingerArray =
        StringFlowActor->RightFingerRecorders.Find(
            TEXT("right_finger_recorders"));
    if (RightFingerArray) {
        for (int32 i = 0; i < RightFingerArray->Num(); ++i) {
            EnsureKey(RightFingerArray->Get(i));
        }
    }

    const FStringFlowStringArray* RightHandPositionArray =
        StringFlowActor->RightHandPositionRecorders.Find(
            TEXT("right_hand_position_recorders"));
    if (RightHandPositionArray) {
        for (int32 i = 0; i < RightHandPositionArray->Num(); ++i) {
            EnsureKey(RightHandPositionArray->Get(i));
        }
    }

    const FStringFlowStringArray* RightThumbArray =
        StringFlowActor->RightThumbRecorders.Find(
            TEXT("right_thumb_position_recorders"));
    if (RightThumbArray) {
        for (int32 i = 0; i < RightThumbArray->Num(); ++i) {
            EnsureKey(RightThumbArray->Get(i));
        }
    }

    const FStringFlowStringArray* OtherArray =
        StringFlowActor->OtherRecorders.Find(TEXT("other_recorders"));
    if (OtherArray) {
        for (int32 i = 0; i < OtherArray->Num(); ++i) {
            EnsureKey(OtherArray->Get(i));
        }
    }

    for (const auto& GuidePair : StringFlowActor->GuideLines) {
        EnsureKey(GuidePair.Value);
    }

    UE_LOG(LogTemp, Verbose,
           TEXT("Ensured %d recorder keys exist in RecorderTransforms map"),
           KeyCount);
}

FString FStringFlowControlRigHelper::GenerateStateDependentSTPRecorderName(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        return FString();
    }

    int32 StringIndex = (int32)StringFlowActor->RightHandStringIndex;
    FString RightPositionStr = StringFlowActor->GetRightHandPositionTypeString(
        StringFlowActor->RightHandPositionType);
    return FString::Printf(TEXT("stp_%d_%s"), StringIndex, *RightPositionStr);
}

FString FStringFlowControlRigHelper::GenerateStateDependentBowRecorderName(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        return FString();
    }

    int32 StringIndex = (int32)StringFlowActor->RightHandStringIndex;
    FString RightPositionStr = StringFlowActor->GetRightHandPositionTypeString(
        StringFlowActor->RightHandPositionType);
    return FString::Printf(TEXT("bow_position_s%d_%s"), StringIndex,
                           *RightPositionStr);
}

FString FStringFlowControlRigHelper::GenerateStateDependentRHTRecorderName(
    AStringFlowUnreal* StringFlowActor) {
    if (!StringFlowActor) {
        return FString();
    }

    int32 StringIndex = (int32)StringFlowActor->RightHandStringIndex;
    FString RightPositionStr = StringFlowActor->GetRightHandPositionTypeString(
        StringFlowActor->RightHandPositionType);
    return FString::Printf(TEXT("right_hand_tar_%s_s%d"), *RightPositionStr,
                           StringIndex);
}

void FStringFlowControlRigHelper::SaveSingleController(
    AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
    const FString& ControlName, const FString& RecorderName, int32& SavedCount,
    int32& FailedCount) {
    if (!StringFlowActor || !RigHierarchy) {
        FailedCount++;
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("  Processing: %s -> %s"), *ControlName,
           *RecorderName);

    FStringFlowRecorderTransform* ExistingTransform =
        StringFlowActor->RecorderTransforms.Find(RecorderName);
    if (!ExistingTransform) {
        UE_LOG(LogTemp, Warning,
               TEXT("    ⚠ RecorderKey '%s' NOT FOUND in RecorderTransforms, "
                    "adding it"),
               *RecorderName);
        FStringFlowRecorderTransform DefaultTransform;
        DefaultTransform.Location = FVector::ZeroVector;
        DefaultTransform.Rotation = FQuat::Identity;
        ExistingTransform = &StringFlowActor->RecorderTransforms.Add(
            RecorderName, DefaultTransform);
    }

    FTransform CurrentTransform;
    if (!FInstrumentControlRigUtility::GetControlLocalTransform(
            RigHierarchy, ControlName, CurrentTransform)) {
        UE_LOG(LogTemp, Warning,
               TEXT("    ⚠ Failed to get control '%s' from RigHierarchy"),
               *ControlName);
        FailedCount++;
        return;
    }

    FStringFlowRecorderTransform RecorderTransform;
    RecorderTransform.FromTransform(CurrentTransform);

    UE_LOG(LogTemp, Warning, TEXT("    ✓ Saved: %s -> Loc(%.2f, %.2f, %.2f)"),
           *RecorderName, RecorderTransform.Location.X,
           RecorderTransform.Location.Y, RecorderTransform.Location.Z);

    StringFlowActor->RecorderTransforms[RecorderName] = RecorderTransform;
    SavedCount++;
}

void FStringFlowControlRigHelper::LoadSingleController(
    AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
    const FString& ControlName, const FString& RecorderName, int32& LoadedCount,
    int32& FailedCount) {
    if (!StringFlowActor || !RigHierarchy) {
        FailedCount++;
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("  Processing: %s <- %s"), *ControlName,
           *RecorderName);

    const FStringFlowRecorderTransform* FoundTransform =
        StringFlowActor->RecorderTransforms.Find(RecorderName);
    if (!FoundTransform) {
        UE_LOG(LogTemp, Warning,
               TEXT("    ⚠ RecorderKey '%s' NOT FOUND in RecorderTransforms"),
               *RecorderName);
        FailedCount++;
        return;
    }

    if (!FInstrumentControlRigUtility::SetControlLocalTransform(
            RigHierarchy, ControlName, FoundTransform->ToTransform())) {
        UE_LOG(LogTemp, Warning,
               TEXT("    ⚠ Failed to set control '%s' in RigHierarchy"),
               *ControlName);
        FailedCount++;
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("    ✓ Loaded: %s <- Loc(%.2f, %.2f, %.2f)"),
           *RecorderName, FoundTransform->Location.X,
           FoundTransform->Location.Y, FoundTransform->Location.Z);

    LoadedCount++;
}

void FStringFlowControlRigHelper::SaveStateDependentFingerControllers(
    AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
    const TMap<FString, FString>& Controllers, int32 StringIndex,
    int32 FretIndex, EStringFlowHandType HandType, int32& SavedCount,
    int32& FailedCount) {
    if (!StringFlowActor) {
        return;
    }

    FString PositionStr = (HandType == EStringFlowHandType::LEFT)
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

void FStringFlowControlRigHelper::LoadStateDependentFingerControllers(
    AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
    const TMap<FString, FString>& Controllers, int32 StringIndex,
    int32 FretIndex, EStringFlowHandType HandType, int32& LoadedCount,
    int32& FailedCount) {
    if (!StringFlowActor) {
        return;
    }

    FString PositionStr = (HandType == EStringFlowHandType::LEFT)
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

void FStringFlowControlRigHelper::SaveStateDependentHandControllers(
    AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
    const TMap<FString, FString>& Controllers, int32 StringIndex,
    int32 FretIndex, EStringFlowHandType HandType, int32& SavedCount,
    int32& FailedCount) {
    if (!StringFlowActor) {
        return;
    }

    FString PositionStr = (HandType == EStringFlowHandType::LEFT)
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

void FStringFlowControlRigHelper::LoadStateDependentHandControllers(
    AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
    const TMap<FString, FString>& Controllers, int32 StringIndex,
    int32 FretIndex, EStringFlowHandType HandType, int32& LoadedCount,
    int32& FailedCount) {
    if (!StringFlowActor) {
        return;
    }

    FString PositionStr = (HandType == EStringFlowHandType::LEFT)
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

void FStringFlowControlRigHelper::SaveStateDependentOtherControllers(
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
        FStringFlowControlRigHelper::GenerateStateDependentSTPRecorderName(
            StringFlowActor);

    SaveSingleController(StringFlowActor, RigHierarchy,
                         TEXT("String_Touch_Point"), STPRecorderName,
                         SavedCount, FailedCount);

    // 保存 Bow_Controller 到当前弦对应的 bow_position 记录器
    FString BowRecorderName =
        FStringFlowControlRigHelper::GenerateStateDependentBowRecorderName(
            StringFlowActor);

    SaveSingleController(StringFlowActor, RigHierarchy, TEXT("Bow_Controller"),
                         BowRecorderName, SavedCount, FailedCount);

    // 保存 Right_Hand_Tar 到当前弦对应的 right_hand_tar 记录器
    FString RHTRecorderName =
        FStringFlowControlRigHelper::GenerateStateDependentRHTRecorderName(
            StringFlowActor);

    SaveSingleController(StringFlowActor, RigHierarchy, TEXT("Right_Hand_Tar"),
                         RHTRecorderName, SavedCount, FailedCount);
}

void FStringFlowControlRigHelper::LoadStateDependentOtherControllers(
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
        FStringFlowControlRigHelper::GenerateStateDependentSTPRecorderName(
            StringFlowActor);

    LoadSingleController(StringFlowActor, RigHierarchy,
                         TEXT("String_Touch_Point"), STPRecorderName,
                         LoadedCount, FailedCount);

    // 从当前弦对应的 bow_position 记录器加载到 Bow_Controller
    FString BowRecorderName =
        FStringFlowControlRigHelper::GenerateStateDependentBowRecorderName(
            StringFlowActor);

    LoadSingleController(StringFlowActor, RigHierarchy, TEXT("Bow_Controller"),
                         BowRecorderName, LoadedCount, FailedCount);

    // 从当前弦对应的 right_hand_tar 记录器加载到 Right_Hand_Tar
    FString RHTRecorderName =
        FStringFlowControlRigHelper::GenerateStateDependentRHTRecorderName(
            StringFlowActor);

    LoadSingleController(StringFlowActor, RigHierarchy, TEXT("Right_Hand_Tar"),
                         RHTRecorderName, LoadedCount, FailedCount);
}

void FStringFlowControlRigHelper::SaveStatelessOtherControllers(
    AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
    int32& SavedCount, int32& FailedCount) {
    if (!StringFlowActor) {
        return;
    }

    UE_LOG(LogTemp, Warning,
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

        // 跳过状态相关的 stp、bow_position 和 right_hand_tar 记录器
        if (RecorderName.StartsWith(TEXT("stp_")) ||
            RecorderName.StartsWith(TEXT("bow_position_")) ||
            RecorderName.StartsWith(TEXT("right_hand_tar_"))) {
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

void FStringFlowControlRigHelper::LoadStatelessOtherControllers(
    AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
    int32& LoadedCount, int32& FailedCount) {
    if (!StringFlowActor) {
        return;
    }

    UE_LOG(LogTemp, Warning,
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

        // 跳过状态相关的 stp、bow_position 和 right_hand_tar 记录器
        if (RecorderName.StartsWith(TEXT("stp_")) ||
            RecorderName.StartsWith(TEXT("bow_position_")) ||
            RecorderName.StartsWith(TEXT("right_hand_tar_"))) {
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
            UE_LOG(LogTemp, Warning,
                   TEXT("  ⚠ RecorderKey '%s' NOT FOUND in RecorderTransforms"),
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
            UE_LOG(LogTemp, Warning, TEXT("    ⚠ ControlElement '%s' is NULL"),
                   *ControlName);
            FailedCount++;
            continue;
        }

        FTransform NewTransform = FoundTransform->ToTransform();
        FInstrumentControlRigUtility::SetControlLocalTransform(
            RigHierarchy, ControlName, NewTransform);

        UE_LOG(LogTemp, Warning,
               TEXT("    ✓ Loaded: %s <- Loc(%.2f, %.2f, %.2f)"), *RecorderName,
               FoundTransform->Location.X, FoundTransform->Location.Y,
               FoundTransform->Location.Z);

        LoadedCount++;
    }
}

#undef LOCTEXT_NAMESPACE
