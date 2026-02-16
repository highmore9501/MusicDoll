#pragma once

#include "CoreMinimal.h"

class URigHierarchy;
class URigHierarchyController;
struct FRigElementKey;

/**
 * Control Rig 控制器创建工具类
 * 提供通用的 Control 创建和管理功能，用于 StringFlow 和 KeyRipple 等模块
 * 
 * 支持三层结构设计：

 * - base_root: 顶层根控制器，所有Control的总根
 * - controller_root: 乐器演奏相关Control的根，作为base_root的子级
 * - others: 具体的乐器演奏相关Control
 * 
 * 这样可以灵活地在base_root下直接添加非乐器相关的Control，

 * 同时保持乐器演奏相关Control的独立性。
 */
class COMMON_API FControlRigCreationUtility
{
public:
    /**
     * 创建顶层根控制器
     * 这是所有Control的总根，通常命名为 "base_root"
     * 
     * @param HierarchyController Rig 层级结构控制器
     * @param RigHierarchy Rig 层级结构
     * @param RootName 根控制器的名称 (默认: "base_root")
     * @param ShapeName 根控制器的形状 (默认: "Cube")
     * @return 创建是否成功
     */
    static bool CreateRootController(
        URigHierarchyController* HierarchyController,
        URigHierarchy* RigHierarchy,
        const FString& RootName = TEXT("base_root"),
        const FString& ShapeName = TEXT("Cube"));

    /**
     * 创建乐器演奏根控制器
     * 这是所有乐器演奏相关Control的根，作为base_root的子级
     * 通常命名为 "controller_root"
     * 
     * @param HierarchyController Rig 层级结构控制器
     * @param RigHierarchy Rig 层级结构
     * @param ControllerRootName 乐器根控制器的名称 (默认: "controller_root")
     * @param ParentName 父控制器的名称 (默认: "base_root")
     * @param ShapeName 乐器根控制器的形状 (默认: "Cube")
     * @return 创建是否成功
     */
    static bool CreateInstrumentRootController(
        URigHierarchyController* HierarchyController,
        URigHierarchy* RigHierarchy,
        const FString& ControllerRootName = TEXT("controller_root"),
        const FString& ParentName = TEXT("base_root"),
        const FString& ShapeName = TEXT("Cube"));

    /**
     * 创建 Control 的通用辅助方法
     * 用于精简重复的 Control 创建代码
     *
     * @param HierarchyController Rig 层级结构控制器
     * @param RigHierarchy Rig 层级结构
     * @param ControlName 控制器名称
     * @param ParentKey 父控制器的 RigElementKey（可为空）
     * @param ShapeName 控制器形状名称（默认为智能确定）
     * @return 创建是否成功
     *
     * @note 该方法会进行严格的存在性检查，防止重复创建
     * @note ParentKey 如果为空，则创建的 Control 没有父级
     * @note 如果ShapeName为空，将使用DetermineShapeName自动确定
     */
    static bool CreateControl(
        URigHierarchyController* HierarchyController,
        URigHierarchy* RigHierarchy,
        const FString& ControlName,
        const FRigElementKey& ParentKey,
        const FString& ShapeName = TEXT(""));

    /**
     * 检查 Control 是否严格存在（包括完整性检查）
     * 这是比 RigHierarchy->Contains 更严格的检查
     *
     * @param RigHierarchy Rig 层级结构
     * @param ControllerName 控制器名称
     * @return 控制器是否存在且完整
     */
    static bool StrictControlExistenceCheck(
        URigHierarchy* RigHierarchy,
        const FString& ControllerName);

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
        const TSet<FString>& ExpectedControllerNames,
        bool bLogVerbose = true);
};
