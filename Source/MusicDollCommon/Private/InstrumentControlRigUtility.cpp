#include "InstrumentControlRigUtility.h"

#include "Rigs/RigHierarchy.h"

// ============================================================
// 通用局部 Transform 读写
// ============================================================

bool FInstrumentControlRigUtility::GetControlLocalTransform(
    URigHierarchy* InHierarchy, const FString& ControlName,
    FTransform& OutTransform) {
    if (!InHierarchy || ControlName.IsEmpty()) {
        return false;
    }

    FRigElementKey ControlKey(*ControlName, ERigElementType::Control);
    if (!InHierarchy->Contains(ControlKey)) {
        return false;
    }

    FRigControlElement* ControlElement =
        InHierarchy->Find<FRigControlElement>(ControlKey);
    if (!ControlElement) {
        return false;
    }

    FRigControlValue CurrentValue = InHierarchy->GetControlValue(
        ControlElement, ERigControlValueType::Current);
    OutTransform =
        CurrentValue.GetAsTransform(ControlElement->Settings.ControlType,
                                    ControlElement->Settings.PrimaryAxis);
    return true;
}

bool FInstrumentControlRigUtility::SetControlLocalTransform(
    URigHierarchy* InHierarchy, const FString& ControlName,
    const FTransform& InTransform) {
    if (!InHierarchy || ControlName.IsEmpty()) {
        return false;
    }

    FRigElementKey ControlKey(*ControlName, ERigElementType::Control);
    if (!InHierarchy->Contains(ControlKey)) {
        return false;
    }

    FRigControlElement* ControlElement =
        InHierarchy->Find<FRigControlElement>(ControlKey);
    if (!ControlElement) {
        return false;
    }

    FRigControlValue NewValue;
    NewValue.SetFromTransform(InTransform, ControlElement->Settings.ControlType,
                              ControlElement->Settings.PrimaryAxis);
    InHierarchy->SetControlValue(ControlElement, NewValue,
                                 ERigControlValueType::Current);
    return true;
}