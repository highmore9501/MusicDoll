#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ZhengDriftTransformSyncProcessor.generated.h"

class AZhengDriftUnreal;

/**
 * UZhengDriftTransformSyncProcessor
 *
 * 古筝位置同步处理器（预留接口，实现暂为空）
 *
 * 古筝目前不需要实时同步，但保留完整接口以备将来扩展。
 * 如需实现，参考 FretDanceTransformSyncProcessor 的模式：
 *   - 使用 FInstrumentControlRigUtility::CalculateRelativeTransform() 初始化
 *   - 使用 FInstrumentControlRigUtility::UpdateChildControlFromParent() 同步
 */
UCLASS()
class ZHENGDRIFTUNREAL_API UZhengDriftTransformSyncProcessor : public UObject {
    GENERATED_BODY()
public:
    /**
     * 同步所有乐器变换（在 Tick 中调用）
     * 当前实现：直接返回 true（预留）
     */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Transform Sync")
    static bool SyncAllInstrumentTransforms(AZhengDriftUnreal* ZhengDriftActor);

    /**
     * 同步古筝变换（zheng_root 跟随 controller_root）
     * 当前实现：直接返回 true（预留）
     */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Transform Sync")
    static bool SyncZhengTransform(AZhengDriftUnreal* ZhengDriftActor,
                                    bool bIsRenderingEnvironment = false);

    /**
     * 初始化古筝同步（计算并缓存相对变换）
     * 当前实现：直接返回 true（预留）
     */
    UFUNCTION(BlueprintCallable, Category = "ZhengDrift Transform Sync Init")
    static bool InitializeZhengSync(AZhengDriftUnreal* ZhengDriftActor);
};
