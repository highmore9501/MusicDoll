#pragma once

#include "CoreMinimal.h"
#include "LipSyncTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class AInstrumentBase;
class UControlRigBlueprint;

/**
 * Lip Sync 面板
 * 整合映射编辑和口型生成两个功能区域
 *
 * 使用模式：由乐器模块主面板（如 FretDanceModuleMainPanel）作为 Tab 注册，
 * 通过 SetActor() 接收演奏者 Actor。
 */
class MUSICDOLLCOMMON_API SLipSyncPanel : public SCompoundWidget {
   public:
    SLATE_BEGIN_ARGS(SLipSyncPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** 设置当前 Actor（由模块主面板调用） */
    void SetActor(AActor* InActor);

    /** 检查是否能处理该 Actor 类型 */
    bool CanHandleActor(const AActor* InActor) const;

   private:
    // ===== Actor 引用 =====
    TWeakObjectPtr<AInstrumentBase> InstrumentActor;
    TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;

    // ===== Mapping 区域 =====

    /** 映射列表项（12 行固定：A~K, X，兼容 Lisa 8 符号 + Cherry 12 符号） */
    TArray<TSharedPtr<FLipSyncMappingPair>> MappingPairs;

    /** 映射列表视图 */
    TSharedPtr<SListView<TSharedPtr<FLipSyncMappingPair>>> MappingListView;

    /** Morph Target 可选名称列表 */
    TArray<TSharedPtr<FString>> MorphTargetOptions;

    /** 刷新映射列表 */
    void RefreshMappingList();

    /** 刷新 Morph Target 可选列表（从演奏者 SkeletalMesh 获取） */
    void RefreshMorphTargetOptions();

    /** 生成映射行 */
    TSharedRef<ITableRow> GenerateMappingRow(
        TSharedPtr<FLipSyncMappingPair> InPair,
        const TSharedRef<STableViewBase>& OwnerTable);

    /** Morph Target 下拉选择变更 */
    void OnMorphTargetSelectionChanged(TSharedPtr<FString> InSelection,
                                       ESelectInfo::Type SelectInfo,
                                       TSharedPtr<FLipSyncMappingPair> InPair);

    /** 生成下拉选项 */
    TSharedRef<SWidget> GenerateMorphTargetComboItem(
        TSharedPtr<FString> InOption);

    /** 获取当前选中的 Morph Target 文本 */
    FText GetSelectedMorphTargetText(
        TSharedPtr<FLipSyncMappingPair> InPair) const;

    /** 保存映射 */
    FReply OnSaveMappingClicked();

    /** 初始化 Lip Sync Control */
    FReply OnInitLipSyncControlClicked();

    /** 将 Mapping 应用到 Rig */
    FReply OnApplyMappingToRigClicked();

    // ===== Generation 区域 =====

    /** JSON 文件路径 */
    FString JsonFilePath;

    /** 解析结果摘要文本 */
    FString ParseResultText;

    /** 用于显示解析结果的 TextBlock */
    TSharedPtr<STextBlock> ParseResultTextBlock;

    /** 浏览 JSON 文件 */
    FReply OnBrowseJsonClicked();

    /** 清除关键帧 */
    FReply OnClearKeyFrameClicked();

    /** 生成 Lip Sync 动画 */
    FReply OnGenerateLipSyncClicked();

    // ===== 内部工具 =====

    /** 确保 ControlRigBlueprint 有效 */
    bool EnsureControlRigBlueprintValid();

    /** 从 InstrumentActor 获取 ControlRigBlueprint */
    bool RetrieveControlRigBlueprint();

    /** 获取演奏者 SkeletalMesh 的 Morph Target 名称 */
    TArray<FString> GetPerformerMorphTargetNames() const;

    /** 显示通知消息 */
    void ShowNotification(const FText& Message, bool bIsSuccess = true) const;
};
