#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "FretDanceUnreal.h"
#include "FretDanceTransformSyncProcessor.generated.h"

// 前置声明
class AFretDanceUnreal;
class ASkeletalMeshActor;
class UControlRig;

/**
 * FretDance Transform Sync 处理器
 * 负责同步吉他与人物 Control Rig 的位置和旋转
 *
 * ============================================================
 * 功能说明：
 * ============================================================
 *
 * 1. 吉他同步：
 *    - 将吉他的 root 骨骼位置同步到 controller_root control
 *    - 跟随 controller_root 的位置和旋转
 *
 * ============================================================
 * Control 对应关系：
 * ============================================================
 *
 * 吉他：
 *   源：人物 Control Rig -> controller_root
 *   目标：Guitar -> root 骨骼
 *
 * ============================================================
 */
UCLASS()
class FRETDANCEUNREAL_API UFretDanceTransformSyncProcessor : public UObject {
    GENERATED_BODY()

   public:
    /**
     * 同步所有乐器变换（吉他）
     * 这是主要的调用入口，通常在 AFretDanceUnreal::Tick() 中调用
     *
     * @param FretDanceActor FretDanceUnreal 实例
     * @return 同步是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Transform Sync")
    static bool SyncAllInstrumentTransforms(AFretDanceUnreal* FretDanceActor);

    /**
     * 同步吉他（root 跟随 controller_root）
     * 纯读取 - 计算 - 应用方法，需要先调用 InitializeGuitarSync 初始化
     *
     * @param FretDanceActor FretDanceUnreal 实例
     * @param bIsRenderingEnvironment 是否为渲染环境
     * @return 同步是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Transform Sync")
    static bool SyncGuitarTransform(AFretDanceUnreal* FretDanceActor, bool bIsRenderingEnvironment = false);

    /**
     * 初始化吉他同步（计算并缓存相对变换）
     * 需要在开始同步前调用一次
     *
     * @param FretDanceActor FretDanceUnreal 实例
     * @return 初始化是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "FretDance Transform Sync Init")
    static bool InitializeGuitarSync(AFretDanceUnreal* FretDanceActor);
};
