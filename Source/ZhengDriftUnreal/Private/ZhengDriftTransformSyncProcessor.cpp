#include "ZhengDriftTransformSyncProcessor.h"

#include "ZhengDriftUnreal.h"

bool UZhengDriftTransformSyncProcessor::SyncAllInstrumentTransforms(
    AZhengDriftUnreal* ZhengDriftActor) {
    if (!ZhengDriftActor) return false;
    if (!ZhengDriftActor->bEnableRealtimeSync) return true;

    // 预留：将来可调用 SyncZhengTransform()
    return true;
}

bool UZhengDriftTransformSyncProcessor::SyncZhengTransform(
    AZhengDriftUnreal* ZhengDriftActor, bool bIsRenderingEnvironment) {
    // 预留：将来参考 FretDanceTransformSyncProcessor::SyncGuitarTransform
    return true;
}

bool UZhengDriftTransformSyncProcessor::InitializeZhengSync(
    AZhengDriftUnreal* ZhengDriftActor) {
    // 预留：将来参考 FretDanceTransformSyncProcessor::InitializeGuitarSync
    return true;
}
