#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StringFlowUnreal.h"
#include "StringFlowTransformSyncProcessor.generated.h"

// 前置声明
class AStringFlowUnreal;
class ASkeletalMeshActor;
class UControlRig;

/**
 * StringFlow Transform Sync 处理器
 * 负责同步弦乐器（小提琴）和琴弓与人物Control Rig的位置和旋转
 *
 * ============================================================
 * 功能说明：
 * ============================================================
 *
 * 1. 小提琴同步：
 *    - 将小提琴的 violin_root 骨骼位置同步到 controller_root control
 *    - 跟随 controller_root 的位置和旋转
 *
 * 2. 琴弓同步：
 *    - 位置同步：bow_root 跟随 bow_controller 的位置
 *    - 旋转同步：bow_root 的指定轴指向 string_touch_point
 *    - 轴配置：通过 BowAxisTowardString 属性配置（如 X、Y、Z 轴）
 *
 * ============================================================
 * Control 对应关系：
 * ============================================================
 *
 * 小提琴：
 *   源：人物 Control Rig -> controller_root
 *   目标：StringInstrument -> violin_root 骨骼
 *
 * 琴弓：
 *   位置源：人物 Control Rig -> bow_controller
 *   朝向源：人物 Control Rig -> string_touch_point
 *   目标：Bow -> bow_root 骨骼
 *
 * ============================================================
 */
UCLASS()
class STRINGFLOWUNREAL_API UStringFlowTransformSyncProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 同步所有乐器变换（小提琴和琴弓）
     * 这是主要的调用入口，通常在 AStringFlowUnreal::Tick() 中调用
     *
     * @param StringFlowActor StringFlowUnreal实例
     * @return 同步是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Transform Sync")
    static bool SyncAllInstrumentTransforms(AStringFlowUnreal* StringFlowActor);

    /**
     * 同步小提琴（violin_root 跟随 controller_root）
     * 带有缓存优化：仅在 transform 发生变化时才进行更新
     *
     * @param StringFlowActor StringFlowUnreal实例
     * @return 同步是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Transform Sync")
    static bool SyncStringInstrumentTransform(AStringFlowUnreal* StringFlowActor);

    /**
     * 同步琴弓变换（位置和旋转）
     *
     * @param StringFlowActor StringFlowUnreal实例
     * @return 同步是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "StringFlow Transform Sync")
    static bool SyncBowTransform(AStringFlowUnreal* StringFlowActor);

   private:
    /**
     * 获取指定骨骼的世界位置和旋转
     *
     * @param SkeletalActor 骨骼网格Actor
     * @param BoneName 骨骼名称
     * @param OutLocation [out] 输出的世界位置
     * @param OutRotation [out] 输出的世界旋转
     * @return 是否成功获取
     */
    static bool GetBoneTransform(ASkeletalMeshActor* SkeletalActor,
                                  const FString& BoneName, FVector& OutLocation,
                                  FQuat& OutRotation);

    /**
     * 根据轴向量计算旋转，使指定轴指向目标方向
     *
     * @param CurrentRotation 当前旋转
     * @param AxisToRotate 要旋转的轴（如 FVector(1,0,0) 表示 X 轴）
     * @param TargetDirection 目标方向
     * @return 新的旋转四元数
     */
    static FQuat RotateTowardDirection(const FQuat& CurrentRotation,
                                       const FVector& AxisToRotate,
                                       const FVector& TargetDirection);

    /**
     * 根据两个约束轴计算旋转，使一个轴指向目标方向，另一个轴指向上方向
     * 这用于计算需要同时满足两个方向约束的对象（如琴弓）的旋转
     *
     * @param ForwardAxis 前向轴（要指向目标的轴，如 X 轴）
     * @param UpAxis 向上轴（要指向上方的轴，如 Z 轴）
     * @param TargetForwardDirection 目标前向方向（如指向弦触点）
     * @param TargetUpDirection 目标上向方向（如世界向上）
     * @return 满足两个约束的旋转四元数
     */
    static FQuat RotateWithTwoConstraints(const FVector& ForwardAxis,
                                          const FVector& UpAxis,
                                          const FVector& TargetForwardDirection,
                                          const FVector& TargetUpDirection);
};
