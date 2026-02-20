#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "BoneControlPair.h"

class AInstrumentBase;
class UControlRigBlueprint;

/**
 * Bone Control Mapping 编辑面板
 * 用于编辑 ControlRigBlueprint 中的 BoneControlMapping 变量
 * 支持添加、删除、编辑映射对
 */
class COMMON_API SBoneControlMappingEditPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBoneControlMappingEditPanel)
	{
	}
	SLATE_END_ARGS()

	/**
	 * 构造函数
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * 获取 Widget
	 */
	TSharedPtr<SWidget> GetWidget();

	/**
	 * 设置要编辑的 Actor
	 * @param InActor 乐器 Actor（包含 SkeletalMeshActor）
	 */
	void SetActor(AActor* InActor);

	/**
	 * 检查是否可以处理该 Actor
	 */
	bool CanHandleActor(const AActor* InActor) const;

private:
	/**
	 * 刷新映射列表
	 */
	void RefreshMappingList();

	/**
	 * 生成行 Widget
	 */
	TSharedRef<ITableRow> GenerateMappingRow(
		TSharedPtr<FBoneControlPair> InPair,
		const TSharedRef<STableViewBase>& OwnerTable);

	/**
	 * 生成 ComboBox 项
	 */
	TSharedRef<SWidget> GenerateComboBoxItem(
		TSharedPtr<FString> InOption,
		const TSharedRef<SComboBox<TSharedPtr<FString>>>& InComboBox);

	/**
	 * 获取所有 Bone 名称
	 */
	TArray<FString> GetAllBoneNames() const;

	/**
	 * 获取所有 Control 名称
	 */
	TArray<FString> GetAllControlNames() const;

	/**
	 * 应用搜索过滤
	 */
	void ApplyBoneFilter(const FText& InFilterText);
	void ApplyControlFilter(const FText& InFilterText);

	/**
	 * 添加新行
	 */
	FReply OnAddRowClicked();

	/**
	 * 删除指定行
	 */
	FReply OnDeleteRowClicked(TSharedPtr<FBoneControlPair> InPair);

	/**
	 * 保存映射
	 */
	FReply OnSaveClicked();

	/**
	 * 同步Bone与Control的位置变换
	 */
	FReply OnSyncBoneControlPairsClicked();

	/**
	 * 当 Bone 下拉菜单选择改变时
	 */
	void OnBoneSelectionChanged(
		TSharedPtr<FString> InBone,
		ESelectInfo::Type SelectInfo,
		TSharedPtr<FBoneControlPair> InPair);

	/**
	 * 当 Control 下拉菜单选择改变时
	 */
	void OnControlSelectionChanged(
		TSharedPtr<FString> InControl,
		ESelectInfo::Type SelectInfo,
		TSharedPtr<FBoneControlPair> InPair);

	// Data members
	TWeakObjectPtr<AInstrumentBase> InstrumentActor;
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;

	TArray<TSharedPtr<FBoneControlPair>> MappingPairs;
	TSharedPtr<SListView<TSharedPtr<FBoneControlPair>>> MappingListView;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> BoneComboBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ControlComboBox;

	// Cached lists for combo boxes
	TArray<TSharedPtr<FString>> BoneNames;
	TArray<TSharedPtr<FString>> ControlNames;

	// Filtered lists for searching
	TArray<TSharedPtr<FString>> FilteredBoneNames;
	TArray<TSharedPtr<FString>> FilteredControlNames;

	// Search filter text
	FString BoneFilterText;
	FString ControlFilterText;
};
