// ============================================================================
// Control Rig / Sequencer 写入后"立即生效"排查用控制台命令（仅编辑器，纯诊断）
//
// 背景（本次会话的结论）：
//   在 Sequencer 轨道上写完 float channel 关键帧后，节点（Apply Float Channels
//   To Curves）有时读到的 control 值仍然是 0 —— section 里明明有值，值却没进 rig。
//   现象特征：
//     - 关闭 Sequencer 再重新打开 → 生效；
//     - 或者进 Control Rig 重新编译 → 生效。
//   这说明"失效"只存在于 Sequencer 的内存评估状态里，序列数据本身是对的。
//
//   Sequencer 判断"编译出来的评估数据（模板/实体）要不要重建"依据的是
//   UMovieSceneSignedObject 的 Signature（见 MovieSceneSignedObject.cpp：
//   MarkAsChanged() 会换掉自己以及所有父级的 GUID；而 UMovieSceneSignedObject::Modify()
//   默认带 bAlwaysMarkDirty=true，会顺带调用 MarkAsChanged()）。
//   所以"写完数据后有没有真正让它失效"是关键。
//
// 本文件只提供诊断/试验命令，不参与任何烘焙流程，删除本文件不影响其他功能。
// 排查顺序建议（每步之后都重跑 1，看 AGG 里的 rigNonZero 是否恢复）：
//   1 MusicDoll.DumpControlRigChannelDiff      确认失效（secNonZero>0 而 rigNonZero=0）
//   2 MusicDoll.SequencerRefreshAll            只重建/重评估（已测：无效）
//   3 MusicDoll.MarkSectionsChangedAndRefresh  只换 signature（已测：无效）
//   4 MusicDoll.RebuildChannelProxies          重建 section 的通道代理 + ControlChannelMap
//                                              （引擎自己改完通道会调这一步，而你们的
//                                               写入流程从没调过它）
//   5 MusicDoll.ReopenSequencer                程序化"关掉再打开 Sequencer"（保底，
//                                              等价于你手动验证有效的那个操作）
//   6 MusicDoll.RequestInvalidateCachedData    引擎自己的"强制重新链接"机制：只置一个
//                                              标志，Sequencer 下一帧走
//                                              IMovieScenePlayer::InvalidateCachedData()
//                                              → FSequenceInstance::InvalidateCachedData()
//                                              → 重建 SequenceUpdater、清掉实体链接缓存。
//                                              引擎在"绑定对象换人了"时就是这么做的
//                                              （LevelSequenceEditorSpawnRegister.cpp）。
//   7 MusicDoll.InvalidateCachedDataNow        同一件事但立刻执行（不等下一帧）。
//
//   为什么怀疑 6/7：重开 Sequencer 之所以有效，实质有两层——
//     (a) 旧 FSequencer 被销毁 → RootTemplateInstance.TearDown() → 根序列实例连同它
//         已经 import 的实体一起被销毁；ConstructEntitySystemLinker() 每次都会新建一个
//         独立的 EntitySystemLinker（Sequencer.cpp:4525），系统缓存也一并换新；
//     (b) 新 FSequencer 重新 Initialize → 所有轨道实体重新 import。
//   而"就地刷新"那几步（2/3/4）都只刷新了通道数据，没有让**已经 import 的实体**
//   重新链接，所以 2/3/4 无效而 5 有效。6/7 就是只做 (a) 里"实体重新链接"这一件事，
//   代价远小于重开。
// ============================================================================
#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Channels/MovieSceneFloatChannel.h"
#include "ControlRig.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "InstrumentAnimationUtility.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Rigs/RigHierarchy.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectBaseUtility.h"

namespace MusicDollControlRigRefreshTests {

/** 只有 Float 类型的 control 才能和 section 的 float 通道直接比较 */
static bool IsFloatControl(const UControlRig* Rig, const FName& InName) {
    if (Rig == nullptr) {
        return false;
    }
    const FRigControlElement* Control = Rig->FindControl(InName);
    return Control != nullptr &&
           (Control->Settings.ControlType == ERigControlType::Float ||
            Control->Settings.ControlType == ERigControlType::ScaleFloat);
}

/** 取出当前 Sequencer 与 LevelSequence */
static bool GetSequencerContext(ULevelSequence*& OutSequence,
                                TSharedPtr<ISequencer>& OutSequencer) {
    if (!UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            OutSequence, OutSequencer) ||
        OutSequence == nullptr || !OutSequencer.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("[CRTEST] no active level sequence / sequencer"));
        return false;
    }
    return true;
}

/** 收集当前序列里所有 Control Rig 轨（同时用于统计） */
static TArray<FControlRigSequencerBindingProxy> GatherProxies(ULevelSequence* Sequence) {
    if (Sequence == nullptr) {
        return TArray<FControlRigSequencerBindingProxy>();
    }
    return UControlRigSequencerEditorLibrary::GetControlRigs(Sequence);
}

// ---------------------------------------------------------------------------
// 1) MusicDoll.DumpControlRigChannelDiff [MaxNames]
//    只读：对每条 Control Rig 轨的所有 Float control，比较"section 求值"与
//    "rig 实例上的当前值"，输出 AGG 汇总与少量样例。
//      AGG floatControls=N secNonZero=X rigNonZero=Y mismatch=Z
//      X>0 且 Y==0  → 值没进 rig（就是本次排查的问题）
// ---------------------------------------------------------------------------
static void DumpControlRigChannelDiff(const TArray<FString>& Args, UWorld* /*World*/) {
    const int32 MaxNames =
        Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 64) : 6;

    ULevelSequence* Sequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!GetSequencerContext(Sequence, Sequencer)) {
        return;
    }

    const FFrameTime Time = Sequencer->GetLocalTime().Time;
    const TArray<FControlRigSequencerBindingProxy> Proxies = GatherProxies(Sequence);

    UE_LOG(LogTemp, Warning, TEXT("[CRTEST] dump sequence=%s proxies=%d time=%.3f"),
           *Sequence->GetName(), Proxies.Num(), Time.AsDecimal());

    for (const FControlRigSequencerBindingProxy& ProxyInfo : Proxies) {
        if (ProxyInfo.Track == nullptr) {
            continue;
        }
        UControlRig* Rig = ProxyInfo.Track->GetControlRig();

        for (UMovieSceneSection* SectionBase : ProxyInfo.Track->GetAllSections()) {
            UMovieSceneControlRigParameterSection* Section =
                Cast<UMovieSceneControlRigParameterSection>(SectionBase);
            if (Section == nullptr) {
                continue;
            }

            FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
            UControlRig* SectionRig = Section->GetControlRig();
            const URigHierarchy* Hierarchy =
                SectionRig ? SectionRig->GetHierarchy() : nullptr;

            int32 FloatControls = 0;
            int32 SecNonZero = 0;
            int32 RigNonZero = 0;
            int32 Printed = 0;

            for (const TPair<FName, FChannelMapInfo>& Pair : Section->ControlChannelMap) {
                const FName ControlName = Pair.Key;
                if (!IsFloatControl(SectionRig, ControlName)) {
                    continue;
                }
                ++FloatControls;

                const float SecValue =
                    Section->EvaluateScalarParameter(Time, ControlName).Get(0.f);
                float RigValue = 0.f;
                if (Hierarchy) {
                    RigValue = Hierarchy
                                   ->GetControlValue(
                                       FRigElementKey(ControlName, ERigElementType::Control),
                                       ERigControlValueType::Current)
                                   .Get<float>();
                }

                const bool bSec = !FMath::IsNearlyZero(SecValue);
                const bool bRig = !FMath::IsNearlyZero(RigValue);
                SecNonZero += bSec ? 1 : 0;
                RigNonZero += bRig ? 1 : 0;

                if (bSec || bRig) {
                    if (Printed < MaxNames) {
                        ++Printed;
                        UE_LOG(LogTemp, Warning,
                               TEXT("[CRTEST]   %s keyCount=%d secVal=%.4f rigVal=%.4f%s"),
                               *ControlName.ToString(),
                               ChannelProxy.GetChannelByName<FMovieSceneFloatChannel>(ControlName)
                                   .Get()
                                   ? ChannelProxy
                                         .GetChannelByName<FMovieSceneFloatChannel>(ControlName)
                                         .Get()
                                         ->GetNumKeys()
                                   : -1,
                               SecValue, RigValue,
                               (bSec != bRig) ? TEXT("  <== MISMATCH") : TEXT(""));
                    }
                }
            }

            UE_LOG(LogTemp, Warning,
                   TEXT("[CRTEST] AGG rig=%s section=%p floatControls=%d secNonZero=%d "
                        "rigNonZero=%d (time=%.3f)"),
                   *GetNameSafe(SectionRig), Section, FloatControls, SecNonZero,
                   RigNonZero, Time.AsDecimal());
        }
    }
}

static FAutoConsoleCommandWithWorldAndArgs GDumpDiffCmd(
    TEXT("MusicDoll.DumpControlRigChannelDiff"),
    TEXT("Read-only: compares section channel values against the values present on "
         "the rig instance for every Control Rig track (AGG + samples + MISMATCH)."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DumpControlRigChannelDiff));

// ---------------------------------------------------------------------------
// 2) MusicDoll.SequencerRefreshAll
//    让 Sequencer 彻底重建 + 立即评估（相当于"关掉再打开"的那部分内存重建），
//    不写任何数据。用来验证"缺的是不是 Sequencer 的完全失效/重建通知"。
// ---------------------------------------------------------------------------
static void SequencerRefreshAll(const TArray<FString>& Args, UWorld* /*World*/) {
    ULevelSequence* Sequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!GetSequencerContext(Sequence, Sequencer)) {
        return;
    }

    Sequencer->NotifyMovieSceneDataChanged(
        EMovieSceneDataChangeType::RefreshAllImmediately);
    Sequencer->ForceEvaluate();

    UE_LOG(LogTemp, Warning,
           TEXT("[CRTEST] SequencerRefreshAll done (RefreshAllImmediately + ForceEvaluate)"));
}

static FAutoConsoleCommandWithWorldAndArgs GRefreshAllCmd(
    TEXT("MusicDoll.SequencerRefreshAll"),
    TEXT("Rebuilds and re-evaluates the whole Sequencer immediately (equivalent to "
         "the in-memory part of closing/reopening it). Writes no data."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SequencerRefreshAll));

// ---------------------------------------------------------------------------
// 3) MusicDoll.MarkSectionsChangedAndRefresh
//    对每条 Control Rig 轨：打印 section / movie scene / sequence 的 signature，
//    调用 Section->Modify()（= MarkAsChanged，换 signature）后再打印一次，
//    最后用 TrackValueChangedRefreshImmediately 通知并立即评估。
//    用来验证"签名是否真的变了 / 该用哪一种通知类型"。
// ---------------------------------------------------------------------------
static void MarkSectionsChangedAndRefresh(const TArray<FString>& Args, UWorld* /*World*/) {
    ULevelSequence* Sequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!GetSequencerContext(Sequence, Sequencer)) {
        return;
    }

    const TArray<FControlRigSequencerBindingProxy> Proxies = GatherProxies(Sequence);

    for (const FControlRigSequencerBindingProxy& ProxyInfo : Proxies) {
        if (ProxyInfo.Track == nullptr) {
            continue;
        }
        UMovieScene* MovieScene = Sequence->GetMovieScene();

        for (UMovieSceneSection* SectionBase : ProxyInfo.Track->GetAllSections()) {
            UMovieSceneControlRigParameterSection* Section =
                Cast<UMovieSceneControlRigParameterSection>(SectionBase);
            if (Section == nullptr) {
                continue;
            }

            const FGuid SectionBefore = Section->GetSignature();
            const FGuid MovieSceneBefore =
                MovieScene ? MovieScene->GetSignature() : FGuid();
            const FGuid SequenceBefore = Sequence->GetSignature();

            Section->Modify();
            ProxyInfo.Track->Modify();
            MovieScene->Modify();

            UE_LOG(LogTemp, Warning,
                   TEXT("[CRTEST] %s signatureChanged: section=%d movieScene=%d sequence=%d"),
                   *GetNameSafe(ProxyInfo.Track),
                   (Section->GetSignature() != SectionBefore) ? 1 : 0,
                   (MovieScene && MovieScene->GetSignature() != MovieSceneBefore) ? 1 : 0,
                   (Sequence->GetSignature() != SequenceBefore) ? 1 : 0);
        }
    }

    Sequencer->NotifyMovieSceneDataChanged(
        EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
    Sequencer->ForceEvaluate();

    UE_LOG(LogTemp, Warning,
           TEXT("[CRTEST] MarkSectionsChangedAndRefresh done "
                "(Modify + TrackValueChangedRefreshImmediately + ForceEvaluate)"));
}

static FAutoConsoleCommandWithWorldAndArgs GMarkChangedCmd(
    TEXT("MusicDoll.MarkSectionsChangedAndRefresh"),
    TEXT("Marks every Control Rig section as changed (prints whether the signature "
         "actually changed) and refreshes with TrackValueChangedRefreshImmediately."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MarkSectionsChangedAndRefresh));

// ---------------------------------------------------------------------------
// 4) MusicDoll.RebuildChannelProxies
//    对每条 Control Rig 轨的每个 section 调 Section->ReconstructChannelProxy()：
//    它会 ChannelProxy.Reset() + BroadcastChanged()，下次访问时重新执行
//    CacheChannelProxy()（重建 ControlChannelMap 与通道），这是引擎自己在
//    "通道集合/顺序变了"之后必做的一步。不换 rig 实例、不动序列数据。
// ---------------------------------------------------------------------------
static void RebuildChannelProxies(const TArray<FString>& Args, UWorld* /*World*/) {
    ULevelSequence* Sequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!GetSequencerContext(Sequence, Sequencer)) {
        return;
    }

    const TArray<FControlRigSequencerBindingProxy> Proxies = GatherProxies(Sequence);
    int32 Count = 0;

    for (const FControlRigSequencerBindingProxy& ProxyInfo : Proxies) {
        if (ProxyInfo.Track == nullptr) {
            continue;
        }
        for (UMovieSceneSection* SectionBase : ProxyInfo.Track->GetAllSections()) {
            UMovieSceneControlRigParameterSection* Section =
                Cast<UMovieSceneControlRigParameterSection>(SectionBase);
            if (Section == nullptr) {
                continue;
            }
            Section->ReconstructChannelProxy();
            ++Count;
            UE_LOG(LogTemp, Warning,
                   TEXT("[CRTEST] rebuilt channel proxy: track=%s section=%p "
                        "mapEntries(after)=%d"),
                   *GetNameSafe(ProxyInfo.Track), Section,
                   Section->ControlChannelMap.Num());
        }
    }

    Sequencer->NotifyMovieSceneDataChanged(
        EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
    Sequencer->ForceEvaluate();

    UE_LOG(LogTemp, Warning,
           TEXT("[CRTEST] RebuildChannelProxies done (%d section(s))"), Count);
}

static FAutoConsoleCommandWithWorldAndArgs GRebuildProxiesCmd(
    TEXT("MusicDoll.RebuildChannelProxies"),
    TEXT("Calls UMovieSceneControlRigParameterSection::ReconstructChannelProxy() on "
         "every Control Rig section (rebuilds ControlChannelMap + channel proxy, the "
         "step the engine performs after channels change) and refreshes immediately."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RebuildChannelProxies));

// ---------------------------------------------------------------------------
// 5) MusicDoll.ReopenSequencer
//    程序化执行"关掉 Sequencer 再打开"（关闭并重新打开 LevelSequence 资产编辑器），
//    并尽量恢复原来的播放头位置。用于确认"重开 Sequencer"这条路可以被代码复现。
// ---------------------------------------------------------------------------
static void ReopenSequencer(const TArray<FString>& Args, UWorld* /*World*/) {
    ULevelSequence* Sequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!GetSequencerContext(Sequence, Sequencer)) {
        return;
    }

    const FFrameTime SavedTime = Sequencer->GetLocalTime().Time;

    if (GEditor == nullptr) {
        UE_LOG(LogTemp, Warning, TEXT("[CRTEST] GEditor is null"));
        return;
    }
    UAssetEditorSubsystem* AssetEditorSubsystem =
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (AssetEditorSubsystem == nullptr) {
        UE_LOG(LogTemp, Warning, TEXT("[CRTEST] AssetEditorSubsystem is null"));
        return;
    }

    const int32 ClosedCount = AssetEditorSubsystem->CloseAllEditorsForAsset(Sequence);
    AssetEditorSubsystem->OpenEditorForAsset(Sequence);

    // 重新取一次 Sequencer 并恢复播放头（重开后会是一个新的 Sequencer 实例）
    ULevelSequence* NewSequence = nullptr;
    TSharedPtr<ISequencer> NewSequencer = nullptr;
    if (UInstrumentAnimationUtility::GetActiveLevelSequenceAndSequencer(
            NewSequence, NewSequencer) &&
        NewSequencer.IsValid()) {
        NewSequencer->SetLocalTime(SavedTime, ESnapTimeMode::STM_None, true);
        NewSequencer->ForceEvaluate();
        UE_LOG(LogTemp, Warning,
               TEXT("[CRTEST] ReopenSequencer done (closed=%d, restored time=%.3f)"),
               ClosedCount, SavedTime.AsDecimal());
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("[CRTEST] ReopenSequencer: closed=%d but the new sequencer was "
                    "not found yet (try running the dump again)"),
               ClosedCount);
    }
}

static FAutoConsoleCommandWithWorldAndArgs GReopenSequencerCmd(
    TEXT("MusicDoll.ReopenSequencer"),
    TEXT("Closes and reopens the LevelSequence asset editor (programmatic equivalent "
         "of closing/reopening the Sequencer) and restores the playhead position."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ReopenSequencer));

// ---------------------------------------------------------------------------
// 6) MusicDoll.RequestInvalidateCachedData
//    只调用 ISequencer::RequestInvalidateCachedData()（置标志，下一帧由 Sequencer 自己
//    执行 IMovieScenePlayer::InvalidateCachedData()）。这是引擎在绑定对象变化时用的官方
//    路径，不做任何手工刷新，用来验证"缺的就是这一次强制重新链接"。
// ---------------------------------------------------------------------------
static void RequestInvalidateCachedData(const TArray<FString>& Args, UWorld* /*World*/) {
    ULevelSequence* Sequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!GetSequencerContext(Sequence, Sequencer)) {
        return;
    }

    Sequencer->RequestInvalidateCachedData();

    UE_LOG(LogTemp, Warning,
           TEXT("[CRTEST] RequestInvalidateCachedData done (the Sequencer will "
                "invalidate + re-link on its next tick; now run the dump again)"));
}

static FAutoConsoleCommandWithWorldAndArgs GRequestInvalidateCmd(
    TEXT("MusicDoll.RequestInvalidateCachedData"),
    TEXT("Calls ISequencer::RequestInvalidateCachedData(): the Sequencer invalidates "
         "its cached evaluation data on the next tick, forcing every entity to be "
         "re-linked. This is the engine's own path for changed object bindings."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RequestInvalidateCachedData));

// ---------------------------------------------------------------------------
// 7) MusicDoll.InvalidateCachedDataNow
//    立刻执行 IMovieScenePlayer::InvalidateCachedData()（不等下一帧）再 ForceEvaluate()。
//    用于脚本化/无人值守场景；如果它比 6 更容易崩，说明"必须等下一帧"是硬约束。
// ---------------------------------------------------------------------------
static void InvalidateCachedDataNow(const TArray<FString>& Args, UWorld* /*World*/) {
    ULevelSequence* Sequence = nullptr;
    TSharedPtr<ISequencer> Sequencer = nullptr;
    if (!GetSequencerContext(Sequence, Sequencer)) {
        return;
    }

    Sequencer->InvalidateCachedData();
    Sequencer->ForceEvaluate();

    UE_LOG(LogTemp, Warning,
           TEXT("[CRTEST] InvalidateCachedDataNow done "
                "(InvalidateCachedData + ForceEvaluate)"));
}

static FAutoConsoleCommandWithWorldAndArgs GInvalidateNowCmd(
    TEXT("MusicDoll.InvalidateCachedDataNow"),
    TEXT("Immediately calls IMovieScenePlayer::InvalidateCachedData() (rebuilds the "
         "sequence updater and clears the entity linking cache) and then ForceEvaluate()."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&InvalidateCachedDataNow));

}  // namespace MusicDollControlRigRefreshTests

#endif  // WITH_EDITOR
