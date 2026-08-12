#pragma once

#include "CoreMinimal.h"

class URigHierarchy;
class URigHierarchyController;
class UControlRigBlueprint;
class UControlRig;
struct FRigElementKey;

/**
 * Control Rig 控制器创建工具类
 * 提供通用的 Control 创建和管理功能，用于 StringFlow 和 KeyRipple 等模块
 *
 * 使用简化的公开接口：
 * - 所有创建操作都通过 CreateControl 方法处理
 * - 自动处理 RigHierarchy 和 HierarchyController 的检索
 * - 自动标记脏数据以确保蓝图更新
 */
class MUSICDOLLCOMMON_API FControlRigCreationUtility {
   public:
    /**
     * 创建或获取控制器的统一接口
     * 自动处理所有内部操作，包括参数验证、重复检查、脏数据标记等
     *
     * @param ControlRigBlueprint Control Rig 蓝图对象
     * @param ControlName 要创建的控制器名称
     * @param ParentName 父控制器的名称（可为空，表示无父级）
     * @return 创建是否成功
     *
     * @note 如果控制器已存在，将直接返回 true，不会重复创建
     * @note ShapeName 会根据 ControlName 自动决定
     * @note 控制器创建后会自动标记 ControlRigBlueprint 的脏数据
     */
    static bool CreateControl(UControlRigBlueprint* ControlRigBlueprint,
                              const FString& ControlName,
                              const FString& ParentName = TEXT(""));

    /**
     * 确保控制器存在且挂在指定父级下
     * 若控制器不存在则创建；若已存在但父级不匹配则自动 reparent
     *
     * @param ControlRigBlueprint Control Rig 蓝图对象
     * @param ControlName 控制器名称
     * @param ExpectedParentName 期望的父控制器名称（可为空，表示无父级）
     * @return 是否成功
     *
     * @note 与 CreateControl 的区别：CreateControl 对已存在的控制器直接返回
     *       true（不检查/修正父级）；EnsureControl 会校验并修正父级。
     *       主要用于 ext_ 辅助控件 / pole 极向量控件等需要保证父级正确的情况。
     */
    static bool EnsureControl(UControlRigBlueprint* ControlRigBlueprint,
                              const FString& ControlName,
                              const FString& ExpectedParentName = TEXT(""));

    /**
     * 根据控制器名称确定合适的形状
     * 用于简化形状选择逻辑
     *
     * @param ControlName 控制器名称
     * @return 推荐的形状名称
     *
     * @note 规则如下：
     *       - 包含 "hand" 但不包含 "rotation" -> "Cube"
     *       - 包含 "rotation" -> "Circle"
     *       - 以 "pole_" 开头 -> "Diamond"
     *       - 其他 -> "Sphere"
     */
    static FString DetermineShapeName(const FString& ControlName);

    /**
     * 清理重复或损坏的 Controls
     * 用于在 Setup 过程中清理层级结构
     *
     * @param RigHierarchy Rig 层级结构
     * @param ExpectedControllerNames 期望存在的控制器名称集合
     * @param bLogVerbose 是否输出详细日志
     * @return 移除的重复 Control 数量
     */
    static int32 CleanupDuplicateControls(
        URigHierarchy* RigHierarchy,
        const TSet<FString>& ExpectedControllerNames, bool bLogVerbose = true);

    /**
     * 获取可用的形状名称列表
     * 用于在创建控制器时提供形状选择
     *
     * @param InControlRig 控制器所属的 Control Rig
     * @return 可用的形状名称列表
     */
    static TArray<FName> GetAvailableShapeNames(
        const UControlRig* InControlRig);

    /**
     * 线性分布 Control Rig 中的控制器位置
     * 从当前选中的两个控制器出发，自动识别名称模式（公共后缀 + 数字前缀），
     * 将范围内的所有同类控制器按序号线性插值其位置。
     *
     * @param ControlRig 运行时 Control Rig 对象（需已在 Sequencer 中激活）
     * @return 成功分布的控制器数量，失败返回 -1
     */
    static int32 LinearDistributeControls(UControlRig* ControlRig);

   private:
    /**
     * 检查 Control 是否严格存在（包括完整性检查）
     * 这是比 RigHierarchy->Contains 更严格的检查
     *
     * @param RigHierarchy Rig 层级结构
     * @param ControllerName 控制器名称
     * @return 控制器是否存在且完整
     */
    static bool StrictControlExistenceCheck(URigHierarchy* RigHierarchy,
                                            const FString& ControllerName);

    /**
     * 从 ControlRigBlueprint 检索 RigHierarchy 和 HierarchyController
     *
     * @param ControlRigBlueprint Control Rig 蓝图
     * @param OutRigHierarchy 输出的 RigHierarchy
     * @param OutHierarchyController 输出的 HierarchyController
     * @return 是否成功检索
     */
    static bool GetHierarchyAndController(
        UControlRigBlueprint* ControlRigBlueprint,
        URigHierarchy*& OutRigHierarchy,
        URigHierarchyController*& OutHierarchyController);

    /**
     * 内部的 Control 创建实现
     *
     * @param HierarchyController Rig 层级结构控制器
     * @param RigHierarchy Rig 层级结构
     * @param ControlName 控制器名称
     * @param ParentKey 父控制器的 RigElementKey（可为空）
     * @param ShapeName 控制器形状名称
     * @return 创建是否成功
     */
    static bool CreateControlInternal(
        URigHierarchyController* HierarchyController,
        URigHierarchy* RigHierarchy, const FString& ControlName,
        const FRigElementKey& ParentKey, const FString& ShapeName);

    /** 获取两个字符串的公共后缀 */
    static FString GetCommonSuffix(const FString& NameA, const FString& NameB);

    /** 解析 s{N}{Suffix} 格式，返回数字部分，失败返回 -1 */
    static int32 ParseControlIndex(const FString& Name, const FString& Suffix);
};
