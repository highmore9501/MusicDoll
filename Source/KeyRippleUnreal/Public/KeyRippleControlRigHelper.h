#pragma once

#include "CoreMinimal.h"
#include "KeyRippleUnreal.h"

class UControlRig;
class UControlRigBlueprint;
class URigHierarchy;

/**
 * KeyRipple Control Rig 辅助工具类
 * 提供 KeyRipple 特有的 Control Rig 辅助方法
 * 
 * 注意：这些方法是 KeyRipple 模块特有的，不应该放入 Common 通用模块
 */
class KEYRIPPLEUNREAL_API FKeyRippleControlRigHelper {
   public:
    // ========================================
    // 控制器存在性检查
    // ========================================

    /**
     * 严格检查 Control 是否存在（包括完整性检查）
     * 这是比 RigHierarchy->Contains 更严格的检查
     */
    static bool StrictControlExistenceCheck(URigHierarchy* RigHierarchy,
                                            const FString& ControllerName);

    // ========================================
    // 验证方法
    // ========================================

    /**
     * 验证 KeyRippleActor 是否有效
     */
    static bool ValidateKeyRippleActor(AKeyRippleUnreal* KeyRippleActor,
                                       const FString& FunctionName);

    // ========================================
    // 日志辅助方法
    // ========================================

    /**
     * 记录标准操作开始日志
     */
    static void LogStandardStart(const FString& OperationName);

    /**
     * 记录标准操作结束日志
     */
    static void LogStandardEnd(const FString& OperationName, int32 SuccessCount,
                               int32 FailCount, int32 TotalCount);

    // ========================================
    // 控制器名称收集
    // ========================================

    /**
     * 收集所有控制器名称
     */
    static TSet<FString> GetAllControllerNames(
        AKeyRippleUnreal* KeyRippleActor);

    // ========================================
    // 状态相关的记录器生成
    // ========================================

    /**
     * 生成状态相关的记录器名称列表
     */
    static TArray<FString> GenerateStateDependentRecorders(
        AKeyRippleUnreal* KeyRippleActor, const FString& ControllerName);

    // ========================================
    // 记录器初始化
    // ========================================

    /**
     * 初始化单个控制器的记录器项
     */
    static void InitializeControllerRecorderItem(
        AKeyRippleUnreal* KeyRippleActor, const FString& RecorderName);

    /**
     * 将控制器记录器添加到 Transforms 映射中
     */
    static void AddControllerRecordersToTransforms(
        AKeyRippleUnreal* KeyRippleActor,
        const TMap<FString, FString>& Controllers, bool bIsStateDependent);

    /**
     * 初始化所有记录器变换
     */
    static void InitializeRecorderTransforms(AKeyRippleUnreal* KeyRippleActor);

    // ========================================
    // 单个控制器保存/加载
    // ========================================

    /**
     * 保存单个控制器的变换
     */
    static void SaveControllerTransform(AKeyRippleUnreal* KeyRippleActor,
                                        URigHierarchy* RigHierarchy,
                                        const FString& ControlName,
                                        const FString& RecorderName,
                                        int32& SavedCount, int32& FailedCount);

    /**
     * 加载单个控制器的变换
     */
    static void LoadControllerTransform(AKeyRippleUnreal* KeyRippleActor,
                                        URigHierarchy* RigHierarchy,
                                        const FString& ControlName,
                                        const FString& ExpectedRecorderName,
                                        int32& LoadedCount, int32& FailedCount);

    // ========================================
    // 批量控制器处理
    // ========================================

    /**
     * 保存控制器（通用方法）
     */
    static void SaveControllers(
        AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32& SavedCount,
        int32& FailedCount, bool bIsFingerControl = true,
        bool isStateDependent = true);

    /**
     * 加载控制器（通用方法）
     */
    static void LoadControllers(
        AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32& LoadedCount,
        int32& FailedCount, bool bIsFingerControl = true,
        bool isStateDependent = true);

    // ========================================
    // 重复控制器清理
    // ========================================

    /**
     * 清理重复或损坏的控制器
     */
    static void CleanupDuplicateControls(
        AKeyRippleUnreal* KeyRippleActor, URigHierarchy* RigHierarchy,
        const TSet<FString>& ExpectedControllerNames);
};
