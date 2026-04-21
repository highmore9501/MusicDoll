#pragma once

#include "CoreMinimal.h"

class UControlRig;
class UControlRigBlueprint;

/**
 * 将 Sequencer 中选中 Control 的当前变换写回 ControlRigBlueprint 初始值的工具类
 *
 * 使用场景：
 *   在 Sequencer 里把各个 Control 调好以后，执行此功能，
 *   它会自动找到这些 Control 所属的 ControlRigBlueprint，
 *   将当前变换写入对应 Control 的初始变换（Initial Transform），
 *   不需要手动打开 Control Rig Blueprint 去操作。
 */
class MUSICDOLLCOMMON_API FControlInitTransformUtility
{
public:
    /**
     * 将当前 Sequencer 中所有被选中 Control 的变换写入其 Blueprint 的初始值
     *
     * 遍历当前打开的 LevelSequence 里注册的所有 ControlRig，
     * 找出其中处于选中状态（SelectedControls）的 Control，
     * 读取它们在运行时 Hierarchy 中的当前全局变换，
     * 再写入对应 ControlRigBlueprint->Hierarchy 的初始全局变换，
     * 最后标记 Blueprint 为脏并重新编译。
     *
     * @param OutAppliedCount 成功写入的 Control 数量
     * @param OutSkippedCount 因找不到 Blueprint 或 Control 而跳过的数量
     * @return 如果至少写入了一个 Control 则返回 true
     */
    static bool ApplySelectedControlsTransformToInitial(
        int32& OutAppliedCount, int32& OutSkippedCount);
};
