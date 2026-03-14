#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BeatBloomTransformSyncProcessor.generated.h"

class ABeatBloomUnreal;

/**
 * BeatBloom Transform Sync 处理器
 * 负责在运行时将鼓组的位置/旋转同步到演奏者的位移上
 *
 * ============================================================
 * 对标参考：
 * ============================================================
 *
 * - UFretDanceTransformSyncProcessor（吉他跟随演奏者）
 * - UStringFlowTransformSyncProcessor（弦乐器+琴弓跟随演奏者）
 *
 * ============================================================
 * 同步原理：
 * ============================================================
 *
 * 1. 初始化时计算鼓组相对于演奏者的变换矩阵
 * 2. 每帧读取演奏者 ControlRig 中 controller_root 的世界变换
 * 3. 将相对变换矩阵应用到鼓组的 drumkit_root 控制器上
 *
 * ============================================================
 * ControlRig 控制器命名约定：
 * ============================================================
 *
 * | 角色      | 根控制器名称     |
 * |-----------|------------------|
 * | Performer | controller_root  |
 * | DrumKit   | drumkit_root     |
 *
 * ============================================================
 */
UCLASS()
class BEATBLOOMUNREAL_API UBeatBloomTransformSyncProcessor : public UObject {
    GENERATED_BODY()

public:
    /**
     * 同步所有乐器变换（鼓组）
     * 这是主要的调用入口，通常在 ABeatBloomUnreal::Tick() 中调用
     *
     * 流程：
     * 1. 读取 Performer ControlRig 中 controller_root 的当前世界变换
     * 2. 将缓存的相对变换矩阵应用
     * 3. 将结果写入 DrumKit ControlRig 中 drumkit_root 的世界变换
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     * @return 同步是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom Transform Sync")
    static bool SyncAllInstrumentTransforms(ABeatBloomUnreal* BeatBloomActor);

    /**
     * 初始化鼓组同步（计算并缓存相对变换矩阵）
     * 需要在开始同步前调用一次
     *
     * 流程：
     * 1. 验证 Performer 和 DrumKit 存在
     * 2. 获取 Performer 的 controller_root 和 DrumKit 的 drumkit_root
     * 3. 计算相对变换矩阵
     * 4. 缓存到 BeatBloomActor->CachedDrumKitRelativeTransform
     *
     * @param BeatBloomActor BeatBloom Actor 实例
     * @return 初始化是否成功
     */
    UFUNCTION(BlueprintCallable, Category = "BeatBloom Transform Sync")
    static bool InitializeDrumKitSync(ABeatBloomUnreal* BeatBloomActor);
};
