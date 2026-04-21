#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "BoneControlPair.h"

class AInstrumentBase;
class UControlRigBlueprint;

class MUSICDOLLCOMMON_API SBoneControlMappingEditPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBoneControlMappingEditPanel)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	TSharedPtr<SWidget> GetWidget();
	void SetActor(AActor* InActor);
	bool CanHandleActor(const AActor* InActor) const;

private:
	void RefreshMappingList();

	TSharedRef<ITableRow> GenerateMappingRow(
		TSharedPtr<FBoneControlPair> InPair,
		const TSharedRef<STableViewBase>& OwnerTable);

	TSharedRef<SWidget> GenerateComboBoxItem(
		TSharedPtr<FString> InOption,
		const TSharedRef<SComboBox<TSharedPtr<FString>>>& InComboBox);

	TArray<FString> GetAllBoneNames() const;
	TArray<FString> GetAllControlNames() const;

	void ApplyBoneFilter(const FText& InFilterText);
	void ApplyControlFilter(const FText& InFilterText);

	FReply OnAddRowClicked();
	FReply OnDeleteRowClicked(TSharedPtr<FBoneControlPair> InPair);
	FReply OnSaveClicked();
	FReply OnInitBoneControlMappingClicked();
	FReply OnRefreshClicked();
	FReply OnSyncBoneControlPairsClicked();

	FReply OnApplySelectedControlsInitTransformClicked();

	FReply OnExportClicked();
	FReply OnImportClicked();
	FReply OnFilePathBrowse();

	bool BrowseForFile(const FString& FileExtension, FString& OutFilePath);
	bool EnsureControlRigBlueprintValid();
	bool RetrieveControlRigBlueprint(AInstrumentBase* InInstrument);

	void OnBoneSelectionChanged(
		TSharedPtr<FString> InBone,
		ESelectInfo::Type SelectInfo,
		TSharedPtr<FBoneControlPair> InPair);

	void OnControlSelectionChanged(
		TSharedPtr<FString> InControl,
		ESelectInfo::Type SelectInfo,
		TSharedPtr<FBoneControlPair> InPair);

	void DetectDuplicates();
	FText GetDuplicateWarningText() const;

	TWeakObjectPtr<AInstrumentBase> InstrumentActor;
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;

	TArray<TSharedPtr<FBoneControlPair>> MappingPairs;
	TSharedPtr<SListView<TSharedPtr<FBoneControlPair>>> MappingListView;

	TArray<TSharedPtr<FString>> BoneNames;
	TArray<TSharedPtr<FString>> ControlNames;
	TArray<TSharedPtr<FString>> FilteredBoneNames;
	TArray<TSharedPtr<FString>> FilteredControlNames;

	FString BoneFilterText;
	FString ControlFilterText;

	TArray<FString> DuplicateBones;
	TArray<FString> DuplicateControls;

	FString ExportImportFilePath;

	TSharedPtr<SWidget> LeftSidebarWidget;
	TSharedPtr<SBox> RightListContainer;
};
