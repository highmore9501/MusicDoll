#pragma once

#include "CoreMinimal.h"

class URigHierarchy;

/**
 * Control Rig 工具类
 * 提供与 Control Rig 相关的通用功能
 */
class MUSICDOLLCOMMON_API FInstrumentControlRigUtility {
   public:
    /**
     * 从 RigHierarchy 中读取指定 Control 的局部 Transform
     *
     * 封装了 FRigElementKey → Contains → Find<FRigControlElement>
     * → GetControlValue → GetAsTransform 的重复模式。
     * 不包含 Evaluate_AnyThread()，由调用方自行控制时机。
     *
     * @param InHierarchy 目标 RigHierarchy（来自
     * ControlRigInstance->GetHierarchy()）
     * @param ControlName Control 的名称
     * @param OutTransform 输出的局部 Transform
     * @return 是否成功读取
     */
    static bool GetControlLocalTransform(URigHierarchy* InHierarchy,
                                         const FString& ControlName,
                                         FTransform& OutTransform);

    /**
     * 向 RigHierarchy 中设置指定 Control 的局部 Transform
     *
     * 封装了 FRigElementKey → Contains → Find<FRigControlElement>
     * → SetFromTransform → SetControlValue 的重复模式。
     * 不包含 Evaluate_AnyThread()，由调用方自行控制时机。
     *
     * @param InHierarchy 目标 RigHierarchy（来自
     * ControlRigInstance->GetHierarchy()）
     * @param ControlName Control 的名称
     * @param InTransform 要设置的局部 Transform
     * @return 是否成功设置
     */
    static bool SetControlLocalTransform(URigHierarchy* InHierarchy,
                                         const FString& ControlName,
                                         const FTransform& InTransform);
};
