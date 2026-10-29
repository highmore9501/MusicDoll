#pragma once

#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "Units/RigUnit.h"
#include "SetExtControl.generated.h"

/**
 * Set Ext Control
 *
 * 把一个"延长线控制"钉在 原点→中点 的直线上，并把它的朝向完整约束住：
 *
 *     ExtControl 世界位置 = 2 * 中点世界位置 - 原点世界位置
 *     ExtControl 的主轴（PrimaryAxis 指定的本地轴）在世界空间中指向中点位置
 *     ExtControl 的次轴（SecondAxis 指定的本地轴）落在「原点次轴 与 该直线」张成的
 *     平面上，并且和原点的次轴指向同一侧
 *
 * 原点的位置与旋转、中点的位置都在每次执行时从 Hierarchy 实时读取，所以把这个节点
 * 接在原点/中点被驱动之后的执行流里即可，不需要自己接一堆 Get Transform 节点。
 *
 * 次轴约束的作用是消掉主轴约束剩下的那一个自由度（绕直线径向旋转）。当原点的次轴
 * 与直线完全重合时，平面没有定义，此时退化为只约束主轴，ExtControl 原有的滚转保留。
 */
USTRUCT(meta = (DisplayName = "Set Ext Control",
                Category = "Hierarchy",
                Keywords = "SetExtControl,Ext,Extension,Origin,Midpoint,Aim,Line,Constraint",
                NodeColor = "0.2, 0.6, 0.9",
                Version = "1.0"))
struct MUSICDOLLCOMMON_API FRigUnit_SetExtControl : public FRigUnitMutable {
    GENERATED_BODY()

    FRigUnit_SetExtControl()
        : OriginControl(NAME_None, ERigElementType::Control),
          MidpointControl(NAME_None, ERigElementType::Control),
          ExtControl(NAME_None, ERigElementType::Control),
          PrimaryAxis(FVector(1.0f, 0.0f, 0.0f)),
          SecondAxis(FVector(0.0f, 1.0f, 0.0f)),
          bPropagateToChildren(true) {}

    RIGVM_METHOD()
    virtual void Execute() override;

    /** 原点：延长线的起点，实时读取它的世界位置和世界旋转。 */
    UPROPERTY(meta = (Input, ExpandByDefault))
    FRigElementKey OriginControl;

    /** 中点：原点与 ExtControl 之间的中间点，实时读取它的世界位置。 */
    UPROPERTY(meta = (Input, ExpandByDefault))
    FRigElementKey MidpointControl;

    /** 被本节点驱动的延长线控制：位置 = 2*中点-原点，主轴指向中点。 */
    UPROPERTY(meta = (Input, ExpandByDefault))
    FRigElementKey ExtControl;

    /** ExtControl 本地空间中要指向中点的那根轴，默认 X 轴。 */
    UPROPERTY(meta = (Input))
    FVector PrimaryAxis;

    /**
     * 次轴，同样是本地轴向量（默认 Y 轴）。
     * 原点把自己这根轴转到世界空间，与直线一起张成一个平面；ExtControl 的同一根轴
     * 会被约束在该平面内，并与原点次轴指向同一侧，用来固定绕主轴的滚转。
     * 原点次轴与直线重合（或本地次轴与主轴共线）时该约束自动跳过，只保留主轴约束。
     */
    UPROPERTY(meta = (Input))
    FVector SecondAxis;

    /** 是否把这次变换传播给子级（true = 子级保持相对偏移一起移动）。 */
    UPROPERTY(meta = (Input))
    bool bPropagateToChildren;

    // 内部缓存的元素索引
    UPROPERTY(transient)
    FCachedRigElement CachedOriginControl;

    UPROPERTY(transient)
    FCachedRigElement CachedMidpointControl;

    UPROPERTY(transient)
    FCachedRigElement CachedExtControl;
};
