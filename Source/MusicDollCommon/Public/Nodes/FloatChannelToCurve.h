#pragma once

#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "Rigs/RigHierarchyDefines.h"
#include "Units/RigUnit.h"
#include "FloatChannelToCurve.generated.h"

/**
 * 批量将 Float Channel 数值写入 Control Rig Curve Container。
 *
 * 使用场景：在 morph target 轨道的 float channel 驱动场景中，
 * 将多个 float channel 的值高效地批量同步到 curve 容器，
 * 替代在蓝图中逐一调用 Set Curve Value + Get Float Channel 的低效模式。
 *
 * 输入：
 *   - ChannelKeys：float channel 的 Rig Element Key 数组（类型通常为 Control）
 *   - Values：对应的浮点数值数组
 *
 * 节点会以 ChannelKey 的名称在 hierarchy 的 Curve 容器中查找同名 curve 并写值。
 * 如果 curve 不存在或两个数组长度不匹配，会跳过对应项。
 */
USTRUCT(meta = (DisplayName = "Apply Float Channels To Curves",
                Category = "Curves",
                Keywords = "Float,Channel,Curve,Morph,Apply,Batch",
                NodeColor = "0.2, 0.6, 0.2",
                Version = "5.7"))
struct MUSICDOLLCOMMON_API FRigUnit_FloatChannelsToCurves : public FRigUnitMutable {
    GENERATED_BODY()

    FRigUnit_FloatChannelsToCurves() {}

    RIGVM_METHOD()
    virtual void Execute() override;

    /** Float Channel 的 Rig Element Key 数组 */
    UPROPERTY(meta = (Input))
    TArray<FRigElementKey> ChannelKeys;

    /** 执行时跳过 value 为 0 的通道（可选优化） */
    UPROPERTY(meta = (Input))
    bool bSkipZeroValues = false;

    /** 输出：本次成功写入的 curve 数量 */
    UPROPERTY(meta = (Output))
    int32 WrittenCount = 0;
};

