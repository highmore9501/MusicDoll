#pragma once

#include "ControlRig/Public/Units/Highlevel/RigUnit_HighlevelBase.h"
#include "CoreMinimal.h"
#include "RigVM/Public/RigVMCore/RigVMStruct.h"
#include "AccordionBellowsIK.generated.h"

/**
 * Accordion Bellows IK（手风琴风箱 IK）
 *
 * 用来复刻 Blender 里"直骨链 + 末端 IK(stretch) + effector 旋转逐级带动"那套风箱骨骼结构，
 * 但完全不依赖迭代求解：给定「链根位置、effector 位置、effector 旋转」三个输入，
 * 链条的形状在数学上就是唯一确定的，一次算完，没有 FABRIK、没有雅可比、没有迭代次数。
 *
 * 几何模型：
 * 1. 链根、effector、pivot 三点确定一个平面，所有计算都在这个平面里做。
 * 2. effector 相对它自己初始旋转的偏移，投影到该平面上得到一个带符号的角度 a。
 *    a = 0 时链条是一条直线；a != 0 时链条是一段圆弧，
 *    圆弧在 effector 处的切线与「链根 -> effector」这条弦的夹角正好是 a。
 * 3. 由「弦长 c + 切线夹角 a」可以唯一确定圆弧：
 *    圆心角 = 2a，半径 = c / (2·sin a)，弧长 = c·a / sin a。
 *    注意 a 只是半角：effector 拧 a 度，整条链总共弯 2a 度，每节平均弯 2a / 节数。
 *    弧长永远 >= 弦长，这正是"弯起来必须伸长"的物理含义，也天然和 stretch 对得上。
 * 4. 把每节骨骼的原始（rest）长度按比例铺到弧长上，得到每根骨骼的新位置；
 *    整条链的伸缩比 = 弧长 / 链的总 rest 长度，这个比值对每节都相同
 *    （rest 长度不等长也没关系：每节占的弧长按自己的 rest 长度比例分配）。
 * 5. 每根骨骼的朝向 = 它自己那一节的弦方向（径向轴），roll 用"平面内垂直于弦、指向 pivot 那一侧"
 *    的轴来锁定。因为曲线在平面内，这个 roll 约束等价于 Blender 里"每节 roll 跟着一起转"。
 * 6. 伸缩比不写在链骨骼上，而是写在链骨骼的第一个子骨骼（做形变的那根）上，
 *    只放大它本地坐标里 ChildScaleAxis 指定的那一根轴，其余两根轴保持它自己的初始 local scale。
 *
 * 注意事项：
 * - EffectorControl / PivotControl 都是 control。它们的位置和旋转都从层级实时读取，
 *   静止姿态从 Initial 变换读取，所以不需要额外接引脚。
 * - 这两个 control 不要挂在链骨骼下面，否则链骨骼被本节点写回后会把它们一起带走，
 *   形成"自己驱动自己"的反馈。
 * - effector 绕非弯曲轴的旋转（比如拧手腕）会被忽略：只取绕平面法线的那部分角度。
 * - 三点定平面的固有行为：effector 一旦穿过「链根 -> pivot」这条线，平面会翻面，roll 会翻 180°。
 */
USTRUCT(meta = (DisplayName = "Accordion Bellows IK", Category = "Hierarchy",
                Keywords = "Accordion,Bellows,IK,Arc,Stretch", Version = "5.7"))
struct MUSICDOLLCOMMON_API FRigUnit_AccordionBellowsIK
    : public FRigUnit_HighlevelBaseMutable {
    GENERATED_BODY()

    /** 链末端骨骼（关节）：从它沿父级向上取 ChainLength 根骨骼，构成「根 -> 末端」整条链 */
    UPROPERTY(meta = (Input))
    FRigElementKey EndBone = FRigElementKey(NAME_None, ERigElementType::Bone);

    /** 链上骨骼数量（含末端骨骼，不含挂在链骨骼下做形变的子骨骼） */
    UPROPERTY(meta = (Input, DisplayName = "Chain Length"))
    int32 ChainLength = 6;

    /**
     * 末端目标 control。它的位置 = 链末端的骨尖（Unreal 只有关节没有骨尖，
     * 所以最后一节长度用手头这个 control 的静止位置补上）；
     * 它的旋转相对自身初始旋转的偏移 = 整条链的弯曲角。
     */
    UPROPERTY(meta = (Input))
    FRigElementKey EffectorControl =
        FRigElementKey(NAME_None, ERigElementType::Control);

    /** 朝向 control：与链根、effector 三点确定弯曲平面，并决定骨骼 roll 朝向哪一侧 */
    UPROPERTY(meta = (Input))
    FRigElementKey PivotControl =
        FRigElementKey(NAME_None, ERigElementType::Control);

    /** 链骨骼本地坐标里「沿链方向（径向）」的那根轴，对应骨骼的切线方向 */
    UPROPERTY(meta = (Input))
    FVector RadialAxis = FVector(0.0, 1.0, 0.0);

    /** 链骨骼本地坐标里「指向 pivot」的那根轴，用来锁定 roll */
    UPROPERTY(meta = (Input))
    FVector PivotAxis = FVector(0.0, 0.0, 1.0);

    /** 子骨骼本地坐标里「沿链方向、要继承伸缩比」的那根轴，其余两轴保持初始 local scale */
    UPROPERTY(meta = (Input))
    FVector ChildScaleAxis = FVector(1.0, 0.0, 0.0);

    /** 弯曲角 |a| 的上限（度）。夹住它是为了避免弧长在 a 接近 180° 时发散 */
    UPROPERTY(meta = (Input, Constant))
    float MaxBendAngle = 150.0f;

    /** 求解权重：0 = 完全不改动（保持 FK 姿态），1 = 完全按解算结果写回 */
    UPROPERTY(meta = (Input))
    float Weight = 1.0f;

    /** 打开后在输出窗口打印每帧的解算参数，用于对轴参数 */
    UPROPERTY(meta = (Input))
    bool bUseDebug = false;

    RIGVM_METHOD()
    virtual void Execute() override;
};
