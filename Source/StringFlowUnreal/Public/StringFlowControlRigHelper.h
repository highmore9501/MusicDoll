#pragma once

#include "CoreMinimal.h"
#include "StringFlowUnreal.h"

class UControlRig;
class UControlRigBlueprint;
class URigHierarchy;

/**
 * StringFlow Control Rig 辅助工具类
 * 提供 StringFlow 特有的 Control Rig 辅助方法
 * 
 * 注意：这些方法是 StringFlow 模块特有的，不应该放入 Common 通用模块
 */
class STRINGFLOWUNREAL_API FStringFlowControlRigHelper {
   public:
    // ========================================
    // 验证方法
    // ========================================

    /**
     * 基础验证：检查 StringFlowActor 是否为空
     */
    static bool ValidateStringFlowActorBasic(AStringFlowUnreal* StringFlowActor,
                                             const FString& FunctionName);

    /**
     * 完整验证：检查 StringFlowActor 及其 StringInstrument 是否有效
     */
    static bool ValidateStringFlowActor(AStringFlowUnreal* StringFlowActor,
                                        const FString& FunctionName);

    // ========================================
    // 控制器名称收集
    // ========================================

    /**
     * 收集所有控制器名称（包括手指、手掌、脚部、其他控制器和辅助线）
     */
    static TSet<FString> GetAllControllerNames(
        AStringFlowUnreal* StringFlowActor);

    // ========================================
    // 记录器初始化
    // ========================================

    /**
     * 初始化 RecorderTransforms 映射表
     * 从现有的各种 Recorder 数组中提取所有记录器键
     */
    static void InitializeRecorderTransforms(
        AStringFlowUnreal* StringFlowActor);

    // ========================================
    // 状态相关的记录器名称生成
    // ========================================

    /**
     * 生成状态相关的 STP 记录器名称
     * 格式：stp_{弦索引}_{位置类型}
     */
    static FString GenerateStateDependentSTPRecorderName(
        AStringFlowUnreal* StringFlowActor);

    /**
     * 生成状态相关的 Bow 记录器名称
     * 格式：bow_position_s{弦索引}_{位置类型}
     */
    static FString GenerateStateDependentBowRecorderName(
        AStringFlowUnreal* StringFlowActor);

    // ========================================
    // 单个控制器保存/加载
    // ========================================

    /**
     * 保存单个控制器的变换到 RecorderTransforms
     */
    static void SaveSingleController(AStringFlowUnreal* StringFlowActor,
                                     URigHierarchy* RigHierarchy,
                                     const FString& ControlName,
                                     const FString& RecorderName,
                                     int32& SavedCount, int32& FailedCount);

    /**
     * 从 RecorderTransforms 加载单个控制器的变换
     */
    static void LoadSingleController(AStringFlowUnreal* StringFlowActor,
                                     URigHierarchy* RigHierarchy,
                                     const FString& ControlName,
                                     const FString& RecorderName,
                                     int32& LoadedCount, int32& FailedCount);

    // ========================================
    // 批量控制器处理 - 手指控制器
    // ========================================

    /**
     * 保存状态相关的手指控制器
     */
    static void SaveStateDependentFingerControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32 StringIndex,
        int32 FretIndex, EStringFlowHandType HandType, int32& SavedCount,
        int32& FailedCount);

    /**
     * 加载状态相关的手指控制器
     */
    static void LoadStateDependentFingerControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32 StringIndex,
        int32 FretIndex, EStringFlowHandType HandType, int32& LoadedCount,
        int32& FailedCount);

    // ========================================
    // 批量控制器处理 - 手掌控制器
    // ========================================

    /**
     * 保存状态相关的手掌控制器
     */
    static void SaveStateDependentHandControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32 StringIndex,
        int32 FretIndex, EStringFlowHandType HandType, int32& SavedCount,
        int32& FailedCount);

    /**
     * 加载状态相关的手掌控制器
     */
    static void LoadStateDependentHandControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        const TMap<FString, FString>& Controllers, int32 StringIndex,
        int32 FretIndex, EStringFlowHandType HandType, int32& LoadedCount,
        int32& FailedCount);

    // ========================================
    // 批量控制器处理 - 其他控制器（状态相关）
    // ========================================

    /**
     * 保存状态相关的其他控制器（stp, bow_position）
     */
    static void SaveStateDependentOtherControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        int32& SavedCount, int32& FailedCount);

    /**
     * 加载状态相关的其他控制器（stp, bow_position）
     */
    static void LoadStateDependentOtherControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        int32& LoadedCount, int32& FailedCount);

    // ========================================
    // 批量控制器处理 - 其他控制器（状态无关）
    // ========================================

    /**
     * 保存状态无关的其他控制器（position_s*_f*, GuideLines 等）
     * 跳过 stp, bow_position, mid_s*, f9_s*
     */
    static void SaveStatelessOtherControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        int32& SavedCount, int32& FailedCount);

    /**
     * 加载状态无关的其他控制器（position_s*_f*, GuideLines 等）
     * 跳过 stp, bow_position, mid_s*, f9_s*
     */
    static void LoadStatelessOtherControllers(
        AStringFlowUnreal* StringFlowActor, URigHierarchy* RigHierarchy,
        int32& LoadedCount, int32& FailedCount);
};
