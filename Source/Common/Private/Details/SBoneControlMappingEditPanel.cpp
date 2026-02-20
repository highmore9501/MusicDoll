#include "Details/SBoneControlMappingEditPanel.h"

#include "InstrumentBase.h"
#include "BoneControlMappingUtility.h"
#include "InstrumentControlRigUtility.h"
#include "ControlRigBlueprintLegacy.h"
#include "Rigs/RigHierarchy.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SBoneControlMappingEditPanel"

void SBoneControlMappingEditPanel::Construct(const FArguments& InArgs)
{
	MappingPairs.Reset();
	BoneNames.Reset();
	ControlNames.Reset();

	UE_LOG(LogTemp, Warning, TEXT("SBoneControlMappingEditPanel::Construct() - Initializing UI"));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BoneControlMappingTitle", "Bone Control Mapping Editor"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SyncBoneControlPairsButton", "SyncBoneControlPairs"))
				.OnClicked(this, &SBoneControlMappingEditPanel::OnSyncBoneControlPairsClicked)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(10.0f)
		[
			SAssignNew(MappingListView, SListView<TSharedPtr<FBoneControlPair>>)
			.ListItemsSource(&MappingPairs)
			.OnGenerateRow(this, &SBoneControlMappingEditPanel::GenerateMappingRow)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column("Bone")
				.DefaultLabel(LOCTEXT("BoneHeader", "Bone"))
				.FillWidth(0.4f)
				+ SHeaderRow::Column("Control")
				.DefaultLabel(LOCTEXT("ControlHeader", "Control"))
				.FillWidth(0.4f)
				+ SHeaderRow::Column("Action")
				.DefaultLabel(LOCTEXT("ActionHeader", "Action"))
				.FillWidth(0.2f)
			)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddButton", "Add"))
				.OnClicked(this, &SBoneControlMappingEditPanel::OnAddRowClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveButton", "Save"))
				.OnClicked(this, &SBoneControlMappingEditPanel::OnSaveClicked)
			]
		]
	];
}

TSharedPtr<SWidget> SBoneControlMappingEditPanel::GetWidget()
{
	return AsShared();
}

void SBoneControlMappingEditPanel::SetActor(AActor* InActor)
{
	UE_LOG(LogTemp, Warning, TEXT("SBoneControlMappingEditPanel::SetActor() - Setting actor to: %s"), InActor ? *InActor->GetName() : TEXT("null"));
	InstrumentActor = Cast<AInstrumentBase>(InActor);
	RefreshMappingList();
}

bool SBoneControlMappingEditPanel::CanHandleActor(const AActor* InActor) const
{
	return InActor && InActor->IsA<AInstrumentBase>();
}

void SBoneControlMappingEditPanel::RefreshMappingList()
{
	UE_LOG(LogTemp, Warning, TEXT("SBoneControlMappingEditPanel::RefreshMappingList() - Starting refresh"));

	MappingPairs.Reset();

	if (!InstrumentActor.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshMappingList: InstrumentActor is not valid"));
		return;
	}

	AInstrumentBase* Instrument = InstrumentActor.Get();
	if (!Instrument || !Instrument->SkeletalMeshActor)
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshMappingList: Instrument or SkeletalMeshActor is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("RefreshMappingList: Instrument found: %s"), *Instrument->GetName());

	// 首先尝试获取 ControlRigBlueprint（在填充列表之前）
	UControlRig* ControlRigInstance = nullptr;
	UControlRigBlueprint* RetrievedBlueprint = nullptr;

	if (!FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
		Instrument->SkeletalMeshActor, ControlRigInstance, RetrievedBlueprint))
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshMappingList: Failed to get ControlRigBlueprint from SkeletalMeshActor"));
		return;
	}

	ControlRigBlueprint = RetrievedBlueprint;
	UE_LOG(LogTemp, Warning, TEXT("RefreshMappingList: Successfully obtained ControlRigBlueprint"));

	// 现在获取 bone 和 control 名称
	BoneNames.Reset();
	ControlNames.Reset();
	FilteredBoneNames.Reset();
	FilteredControlNames.Reset();
	BoneFilterText.Reset();
	ControlFilterText.Reset();

	TArray<FString> BoneNameList = GetAllBoneNames();
	TArray<FString> ControlNameList = GetAllControlNames();

	UE_LOG(LogTemp, Warning, TEXT("RefreshMappingList: Found %d bones and %d controls"), 
		BoneNameList.Num(), ControlNameList.Num());

	for (const FString& BoneName : BoneNameList)
	{
		BoneNames.Add(MakeShareable(new FString(BoneName)));
		UE_LOG(LogTemp, Verbose, TEXT("  Added bone: %s"), *BoneName);
	}

	for (const FString& ControlName : ControlNameList)
	{
		ControlNames.Add(MakeShareable(new FString(ControlName)));
		UE_LOG(LogTemp, Verbose, TEXT("  Added control: %s"), *ControlName);
	}

	// 初始化过滤列表（显示所有项）
	FilteredBoneNames = BoneNames;
	FilteredControlNames = ControlNames;

	// 从 Control Rig Blueprint 中读取现有的 Bone Control Mapping
	if (ControlRigBlueprint.IsValid())
	{
		TArray<FBoneControlPair> ExistingPairs;
		if (FBoneControlMappingUtility::GetBoneControlMapping(
			ControlRigBlueprint.Get(), ExistingPairs))
		{
			UE_LOG(LogTemp, Warning, TEXT("RefreshMappingList: Found %d existing pairs"), ExistingPairs.Num());
			for (const FBoneControlPair& Pair : ExistingPairs)
			{
				MappingPairs.Add(MakeShareable(new FBoneControlPair(Pair)));
				UE_LOG(LogTemp, Verbose, TEXT("  Loaded pair: Bone=%s, Control=%s"), 
					*Pair.BoneName.ToString(), *Pair.ControlName.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RefreshMappingList: GetBoneControlMapping returned false, or no existing pairs"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshMappingList: ControlRigBlueprint is not valid after retrieval"));
	}

	if (MappingListView.IsValid())
	{
		MappingListView->RequestListRefresh();
		UE_LOG(LogTemp, Warning, TEXT("RefreshMappingList: RequestListRefresh called"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshMappingList: MappingListView is not valid"));
	}

	UE_LOG(LogTemp, Warning, TEXT("RefreshMappingList: Refresh completed with %d pairs"), MappingPairs.Num());
}

TSharedRef<ITableRow> SBoneControlMappingEditPanel::GenerateMappingRow(
	TSharedPtr<FBoneControlPair> InPair,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	UE_LOG(LogTemp, Verbose, TEXT("GenerateMappingRow: Creating row for pair Bone=%s, Control=%s"), 
		*InPair->BoneName.ToString(), *InPair->ControlName.ToString());
	UE_LOG(LogTemp, Verbose, TEXT("GenerateMappingRow: BoneNames list size: %d, ControlNames list size: %d"), 
		BoneNames.Num(), ControlNames.Num());

	return SNew(STableRow<TSharedPtr<FBoneControlPair>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.4f)
		.Padding(FMargin(5.0f))
		[
			SAssignNew(BoneComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&FilteredBoneNames)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InOption)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(InOption.IsValid() ? *InOption : FString(TEXT("Invalid"))));
			})
			.OnSelectionChanged(this, &SBoneControlMappingEditPanel::OnBoneSelectionChanged, InPair)
			[
				// 搜索框 + 显示选中值
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(5.0f, 2.0f))
				[
					SNew(SSearchBox)
					.HintText(FText::FromString(TEXT("Search Bones...")))
					.OnTextChanged_Lambda([this, InPair](const FText& InText)
					{
						BoneFilterText = InText.ToString();
						ApplyBoneFilter(InText);
						if (BoneComboBox.IsValid())
						{
							BoneComboBox->RefreshOptions();
						}
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(5.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([InPair]() -> FText
					{
						FString BoneName = InPair->BoneName.ToString();
						if (BoneName == TEXT("None") || BoneName.IsEmpty())
						{
							return FText::FromString(TEXT("Select Bone"));
						}
						return FText::FromString(BoneName);
					})
					.ColorAndOpacity_Lambda([InPair]() -> FSlateColor
					{
						FString BoneName = InPair->BoneName.ToString();
						if (BoneName == TEXT("None") || BoneName.IsEmpty())
						{
							return FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f));
						}
						return FSlateColor(FLinearColor::White);
					})
				]
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.4f)
		.Padding(FMargin(5.0f))
		[
			SAssignNew(ControlComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&FilteredControlNames)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InOption)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(InOption.IsValid() ? *InOption : FString(TEXT("Invalid"))));
			})
			.OnSelectionChanged(this, &SBoneControlMappingEditPanel::OnControlSelectionChanged, InPair)
			[
				// 搜索框 + 显示选中值
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(5.0f, 2.0f))
				[
					SNew(SSearchBox)
					.HintText(FText::FromString(TEXT("Search Controls...")))
					.OnTextChanged_Lambda([this, InPair](const FText& InText)
					{
						ControlFilterText = InText.ToString();
						ApplyControlFilter(InText);
						if (ControlComboBox.IsValid())
						{
							ControlComboBox->RefreshOptions();
						}
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(5.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([InPair]() -> FText
					{
						FString ControlName = InPair->ControlName.ToString();
						if (ControlName == TEXT("None") || ControlName.IsEmpty())
						{
							return FText::FromString(TEXT("Select Control"));
						}
						return FText::FromString(ControlName);
					})
					.ColorAndOpacity_Lambda([InPair]() -> FSlateColor
					{
						FString ControlName = InPair->ControlName.ToString();
						if (ControlName == TEXT("None") || ControlName.IsEmpty())
						{
							return FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f));
						}
						return FSlateColor(FLinearColor::White);
					})
				]
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(5.0f))
		[
			SNew(SButton)
			.Text(LOCTEXT("DeleteButton", "X"))
			.OnClicked(this, &SBoneControlMappingEditPanel::OnDeleteRowClicked, InPair)
		]
	];
}

TSharedRef<SWidget> SBoneControlMappingEditPanel::GenerateComboBoxItem(
	TSharedPtr<FString> InOption,
	const TSharedRef<SComboBox<TSharedPtr<FString>>>&)
{
	if (!InOption.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateComboBoxItem: InOption is null"));
		return SNew(STextBlock).Text(FText::FromString(TEXT("Invalid")));
	}

	UE_LOG(LogTemp, Verbose, TEXT("GenerateComboBoxItem: Creating item for: %s"), **InOption);

	return SNew(STextBlock)
		.Text(FText::FromString(*InOption));
}

TArray<FString> SBoneControlMappingEditPanel::GetAllBoneNames() const
{
	TArray<FString> Result;

	UE_LOG(LogTemp, Warning, TEXT("GetAllBoneNames: Starting..."));

	if (!ControlRigBlueprint.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("GetAllBoneNames: ControlRigBlueprint is not valid"));
		return Result;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetAllBoneNames: ControlRigBlueprint is valid, calling GetAllBoneNamesFromHierarchy"));
	FBoneControlMappingUtility::GetAllBoneNamesFromHierarchy(
		ControlRigBlueprint.Get(), Result);
	UE_LOG(LogTemp, Warning, TEXT("GetAllBoneNames: Found %d bones from hierarchy"), Result.Num());
	for (const FString& BoneName : Result)
	{
		UE_LOG(LogTemp, Verbose, TEXT("  - Bone: %s"), *BoneName);
	}

	return Result;
}

TArray<FString> SBoneControlMappingEditPanel::GetAllControlNames() const
{
	TArray<FString> Result;

	UE_LOG(LogTemp, Warning, TEXT("GetAllControlNames: Starting..."));

	if (!ControlRigBlueprint.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("GetAllControlNames: ControlRigBlueprint is not valid"));
		return Result;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetAllControlNames: ControlRigBlueprint is valid, calling GetAllControlNamesFromHierarchy"));
	FBoneControlMappingUtility::GetAllControlNamesFromHierarchy(
		ControlRigBlueprint.Get(), Result);
	UE_LOG(LogTemp, Warning, TEXT("GetAllControlNames: Found %d controls from hierarchy"), Result.Num());
	for (const FString& ControlName : Result)
	{
		UE_LOG(LogTemp, Verbose, TEXT("  - Control: %s"), *ControlName);
	}

	return Result;
}

FReply SBoneControlMappingEditPanel::OnAddRowClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("OnAddRowClicked: Adding new row"));
	TSharedPtr<FBoneControlPair> NewPair = MakeShareable(new FBoneControlPair());
	MappingPairs.Add(NewPair);

	if (MappingListView.IsValid())
	{
		MappingListView->RequestListRefresh();
		UE_LOG(LogTemp, Warning, TEXT("OnAddRowClicked: ListRefresh requested"));
	}

	return FReply::Handled();
}

FReply SBoneControlMappingEditPanel::OnDeleteRowClicked(TSharedPtr<FBoneControlPair> InPair)
{
	UE_LOG(LogTemp, Warning, TEXT("OnDeleteRowClicked: Deleting row"));
	if (InPair.IsValid())
	{
		MappingPairs.Remove(InPair);

		if (MappingListView.IsValid())
		{
			MappingListView->RequestListRefresh();
		}
	}

	return FReply::Handled();
}

FReply SBoneControlMappingEditPanel::OnSaveClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("OnSaveClicked: Starting save"));

	if (!InstrumentActor.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnSaveClicked: InstrumentActor is not valid"));
		return FReply::Handled();
	}

	AInstrumentBase* Instrument = InstrumentActor.Get();
	if (!Instrument || !Instrument->SkeletalMeshActor)
	{
		UE_LOG(LogTemp, Error, TEXT("OnSaveClicked: Instrument or SkeletalMeshActor is null"));
		return FReply::Handled();
	}

	// 将 MappingPairs 转换为 TArray<FBoneControlPair>
	TArray<FBoneControlPair> MappingData;
	for (const TSharedPtr<FBoneControlPair>& Pair : MappingPairs)
	{
		if (Pair.IsValid())
		{
			MappingData.Add(*Pair);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("OnSaveClicked: Saving %d mappings"), MappingData.Num());

	// 保存到 ControlRigBlueprint 中的 BoneControlMapping 变量
	if (ControlRigBlueprint.IsValid())
	{
		if (FBoneControlMappingUtility::SetBoneControlMapping(
			ControlRigBlueprint.Get(), MappingData))
		{
			UE_LOG(LogTemp, Warning, TEXT("OnSaveClicked: Successfully saved %d mappings"), MappingData.Num());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("OnSaveClicked: Failed to save mappings"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OnSaveClicked: ControlRigBlueprint is not valid"));
	}

	return FReply::Handled();
}

FReply SBoneControlMappingEditPanel::OnSyncBoneControlPairsClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("OnSyncBoneControlPairsClicked: Starting sync"));

	if (!InstrumentActor.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnSyncBoneControlPairsClicked: InstrumentActor is not valid"));
		return FReply::Handled();
	}

	AInstrumentBase* Instrument = InstrumentActor.Get();
	if (!Instrument)
	{
		UE_LOG(LogTemp, Error, TEXT("OnSyncBoneControlPairsClicked: Instrument is null"));
		return FReply::Handled();
	}

	if (!ControlRigBlueprint.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("OnSyncBoneControlPairsClicked: ControlRigBlueprint is not valid"));
		return FReply::Handled();
	}

	int32 SyncedCount = 0;
	int32 FailedCount = 0;

	if (FBoneControlMappingUtility::SyncBoneControlPairs(
		ControlRigBlueprint.Get(), Instrument, SyncedCount, FailedCount))
	{
		UE_LOG(LogTemp, Warning, 
			TEXT("OnSyncBoneControlPairsClicked: Successfully synced %d pairs (Failed: %d)"),
			SyncedCount, FailedCount);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OnSyncBoneControlPairsClicked: Failed to sync bone-control pairs"));
	}

	return FReply::Handled();
}

void SBoneControlMappingEditPanel::OnBoneSelectionChanged(
	TSharedPtr<FString> InBone,
	ESelectInfo::Type SelectInfo,
	TSharedPtr<FBoneControlPair> InPair)
{
	if (InBone.IsValid() && InPair.IsValid())
	{
		InPair->BoneName = FName(**InBone);
		UE_LOG(LogTemp, Warning, TEXT("OnBoneSelectionChanged: Bone changed to %s"), **InBone);

		if (MappingListView.IsValid())
		{
			MappingListView->RequestListRefresh();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OnBoneSelectionChanged: InBone or InPair is not valid"));
	}
}

void SBoneControlMappingEditPanel::OnControlSelectionChanged(
	TSharedPtr<FString> InControl,
	ESelectInfo::Type SelectInfo,
	TSharedPtr<FBoneControlPair> InPair)
{
	if (InControl.IsValid() && InPair.IsValid())
	{
		InPair->ControlName = FName(**InControl);
		UE_LOG(LogTemp, Warning, TEXT("OnControlSelectionChanged: Control changed to %s"), **InControl);

		if (MappingListView.IsValid())
		{
			MappingListView->RequestListRefresh();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OnControlSelectionChanged: InControl or InPair is not valid"));
	}
}

void SBoneControlMappingEditPanel::ApplyBoneFilter(const FText& InFilterText)
{
	FilteredBoneNames.Reset();
	FString FilterString = InFilterText.ToString().ToLower();

	for (const TSharedPtr<FString>& BoneName : BoneNames)
	{
		if (BoneName.IsValid())
		{
			if (FilterString.IsEmpty() || BoneName->ToLower().Contains(FilterString))
			{
				FilteredBoneNames.Add(BoneName);
			}
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("ApplyBoneFilter: Filtered %d bones from %d total"), 
		FilteredBoneNames.Num(), BoneNames.Num());
}

void SBoneControlMappingEditPanel::ApplyControlFilter(const FText& InFilterText)
{
	FilteredControlNames.Reset();
	FString FilterString = InFilterText.ToString().ToLower();

	for (const TSharedPtr<FString>& ControlName : ControlNames)
	{
		if (ControlName.IsValid())
		{
			if (FilterString.IsEmpty() || ControlName->ToLower().Contains(FilterString))
			{
				FilteredControlNames.Add(ControlName);
			}
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("ApplyControlFilter: Filtered %d controls from %d total"), 
		FilteredControlNames.Num(), ControlNames.Num());
}

#undef LOCTEXT_NAMESPACE
