#pragma once

#include "CoreMinimal.h"
#include "StringFlowUnreal.h"
#include "UObject/NoExportTypes.h"
#include "StringFlowControlRigProcessor.generated.h"

// 前置声明
class AStringFlowUnreal;
class UControlRigBlueprint;
class UControlRig;

/**
 * StringFlow Control Rig 处理器
 * 管理弦乐器的Control Rig相关操作，包括控制器创建、同步、状态保存/加载等
 */
UCLASS()
class STRINGFLOWUNREAL_API UStringFlowControlRigProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 从骨骼网格Actor获取Control Rig实例和蓝图
     * @param StringInstrumentActor 弦乐器骨骼网格Actor
     * @param OutControlRigInstance 输出的Control Rig实例
     * @param OutControlRigBlueprint 输出的Control Rig蓝图
     * @return 是否成功获取
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Control Rig Processor")
    static bool GetControlRigFromStringInstrument(
        ASkeletalMeshActor* StringInstrumentActor,
        UControlRig*& OutControlRigInstance,
        UControlRigBlueprint*& OutControlRigBlueprint);

    /**
     * 检查弦乐器对象状态
     * @param StringFlowActor StringFlowUnreal实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Control Rig Processor")
    static void CheckObjectsStatus(AStringFlowUnreal* StringFlowActor);

    /**
     * 初始化所有对象（控制器和记录器）
     * @param StringFlowActor StringFlowUnreal实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Control Rig Processor")
    static void SetupAllObjects(AStringFlowUnreal* StringFlowActor);

    /**
     * 保存当前状态到记录器变换数据
     * @param StringFlowActor StringFlowUnreal实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Control Rig Processor")
    static void SaveState(AStringFlowUnreal* StringFlowActor);

    /**
     * 仅保存左手状态（不保存右手相关的状态信息）
     * 保存所有与左右手无关的状态无关信息
     * @param StringFlowActor StringFlowUnreal实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Control Rig Processor")
    static void SaveLeft(AStringFlowUnreal* StringFlowActor);

    /**
     * 仅保存右手状态（不保存左手相关的状态信息）
     * 保存所有与左右手无关的状态无关信息
     * @param StringFlowActor StringFlowUnreal实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Control Rig Processor")
    static void SaveRight(AStringFlowUnreal* StringFlowActor);

    /**
     * 从记录器变换数据加载状态
     * @param StringFlowActor StringFlowUnreal实例
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Control Rig Processor")
    static void LoadState(AStringFlowUnreal* StringFlowActor);

    /**
     * 设置控制器
     * @param StringFlowActor StringFlowUnreal实例
     */
    static void SetupControllers(AStringFlowUnreal* StringFlowActor);

    /**
     * 获取指定控制器对应的记录器名称（根据当前状态）
     * @param StringFlowActor StringFlowUnreal实例
     * @param ControlName 控制器名称
     * @param bIsFingerControl 是否为手指控制器
     * @return 记录器名称
     */
    static FString GetRecorderNameForControl(AStringFlowUnreal* StringFlowActor,
                                             const FString& ControlName,
                                             bool bIsFingerControl = true);

    /**
     * 创建控制器
     * @param StringFlowActor StringFlowUnreal实例
     * @param ControllerName 控制器名称
     * @param bIsRotation 是否为旋转控制器
     * @param ParentControllerName 父控制器名称
     * @return 创建的Actor指针（Control Rig中为nullptr）
     */
    static AActor* CreateController(
        AStringFlowUnreal* StringFlowActor, const FString& ControllerName,
        bool bIsRotation = false,
        const FString& ParentControllerName = TEXT(""));
};
