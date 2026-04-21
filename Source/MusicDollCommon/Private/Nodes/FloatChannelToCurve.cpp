#include "Nodes/FloatChannelToCurve.h"

#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FloatChannelToCurve)

FRigUnit_FloatChannelsToCurves_Execute() {
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy) {
        return;
    }

    WrittenCount = 0;

    for (const FRigElementKey& ChannelKey : ChannelKeys) {
        if (!ChannelKey.IsValid()) {
            continue;
        }

        // 直接从 hierarchy 读取 float channel 的当前值
        const float Value = Hierarchy->GetControlValue(
            ChannelKey, ERigControlValueType::Current).Get<float>();

        if (bSkipZeroValues && FMath::IsNearlyZero(Value)) {
            continue;
        }

        // 以 float channel 的名称在 curve 容器中查找同名 curve
        const FRigElementKey CurveKey(ChannelKey.Name, ERigElementType::Curve);
        if (!Hierarchy->Contains(CurveKey)) {
            continue;
        }

        Hierarchy->SetCurveValue(CurveKey, Value);
        ++WrittenCount;
    }
}
