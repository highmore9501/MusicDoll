#include "Details/SBoneControlMappingEditPanel.h"

#include "BoneControlMappingUtility.h"
#include "ControlRigBlueprintLegacy.h"
#include "DesktopPlatformModule.h"
#include "InstrumentBase.h"
#include "InstrumentControlRigUtility.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Rigs/RigHierarchy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

#define LOCTEXT_NAMESPACE "SBoneControlMappingEditPanel"

void SBoneControlMappingEditPanel::Construct(const FArguments& InArgs) {
    MappingPairs.Reset();
    BoneNames.Reset();
    ControlNames.Reset();

    UE_LOG(LogTemp, Warning,
           TEXT("SBoneControlMappingEditPanel::Construct() - Initializing UI"));

    ChildSlot
        [SNew(SVerticalBox) +
         SVerticalBox::Slot().AutoHeight().Padding(10.0f)
             [SNew(STextBlock)
                  .Text(LOCTEXT("BoneControlMappingTitle",
                                "Bone Control Mapping Editor"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))] +
         SVerticalBox::Slot().AutoHeight().Padding(10.0f)
             [SNew(SHorizontalBox) +
              SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                  [SNew(SButton)
                       .Text(LOCTEXT("SyncBoneControlPairsButton",
                                     "SyncBoneControlPairs"))
                       .OnClicked(this, &SBoneControlMappingEditPanel::
                                            OnSyncBoneControlPairsClicked)]] +
         SVerticalBox::Slot().AutoHeight().Padding(10.0f)
             [SNew(SHorizontalBox) +
              SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                  5.0f)[SNew(SEditableTextBox)
                            .Text_Lambda([this]() -> FText {
                                return FText::FromString(ExportImportFilePath);
                            })
                            .OnTextCommitted_Lambda(
                                [this](const FText& InText,
                                       ETextCommit::Type CommitType) {
                                    if (CommitType == ETextCommit::OnEnter ||
                                        CommitType ==
                                            ETextCommit::OnUserMovedFocus) {
                                        ExportImportFilePath =
                                            InText.ToString();
                                    }
                                })
                            .HintText(FText::FromString(
                                TEXT("Select file path for export/import")))] +
              SHorizontalBox::Slot().AutoWidth().Padding(
                  5.0f, 0.0f, 0.0f,
                  0.0f)[SNew(SButton)
                            .Text(LOCTEXT("BrowseButton", "Browse"))
                            .OnClicked(this, &SBoneControlMappingEditPanel::
                                                 OnFilePathBrowse)]] +
         SVerticalBox::Slot().AutoHeight().Padding(10.0f)
             [SNew(SHorizontalBox) +
              SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
                  [SNew(SButton)
                       .Text(LOCTEXT("ExportButton", "Export"))
                       .OnClicked(
                           this, &SBoneControlMappingEditPanel::OnExportClicked)
                       .HAlign(HAlign_Center)] +
              SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
                  [SNew(SButton)
                       .Text(LOCTEXT("ImportButton", "Import"))
                       .OnClicked(
                           this, &SBoneControlMappingEditPanel::OnImportClicked)
                       .HAlign(HAlign_Center)]] +
         SVerticalBox::Slot().FillHeight(1.0f).Padding(10.0f)
             [SAssignNew(MappingListView,
                         SListView<TSharedPtr<FBoneControlPair>>)
                  .ListItemsSource(&MappingPairs)
                  .OnGenerateRow(
                      this,
                      &SBoneControlMappingEditPanel::GenerateMappingRow)
                  .HeaderRow(
                      SNew(SHeaderRow) +
                      SHeaderRow::Column("Bone")
                          .DefaultLabel(LOCTEXT("BoneHeader", "Bone"))
                          .FillWidth(0.4f) +
                      SHeaderRow::Column("Control")
                          .DefaultLabel(LOCTEXT("ControlHeader", "Control"))
                          .FillWidth(0.4f) +
                      SHeaderRow::Column("Action")
                          .DefaultLabel(LOCTEXT("ActionHeader", "Action"))
                          .FillWidth(0.2f))] +
         SVerticalBox::Slot().AutoHeight().Padding(10.0f)
             [SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(5.0f)
                  [SNew(SHorizontalBox) +
                   SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                       [SNew(SButton)
                            .Text(LOCTEXT("AddButton", "Add"))
                            .OnClicked(this, &SBoneControlMappingEditPanel::
                                                 OnAddRowClicked)] +
                   SHorizontalBox::Slot().AutoWidth().Padding(5.0f)
                       [SNew(SButton)
                            .Text(LOCTEXT("SaveButton", "Save"))
                            .OnClicked(this, &SBoneControlMappingEditPanel::
                                                 OnSaveClicked)] +
                   SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f)
                       [SNew(STextBlock)
                            .Text_Lambda([this]() -> FText {
                                return GetDuplicateWarningText();
                            })
                            .ColorAndOpacity_Lambda([this]() -> FSlateColor {
                                return (DuplicateBones.Num() > 0 ||
                                        DuplicateControls.Num() > 0)
                                           ? FSlateColor(
                                                 FLinearColor(1.0f, 0.5f, 0.5f))
                                           : FSlateColor(
                                                 FLinearColor::Transparent);
                            })]]]];
}

TSharedPtr<SWidget> SBoneControlMappingEditPanel::GetWidget() {
    return AsShared();
}

void SBoneControlMappingEditPanel::SetActor(AActor* InActor) {
    UE_LOG(
        LogTemp, Warning,
        TEXT("SBoneControlMappingEditPanel::SetActor() - Setting actor to: %s"),
        InActor ? *InActor->GetName() : TEXT("null"));
    InstrumentActor = Cast<AInstrumentBase>(InActor);
    RefreshMappingList();
}

bool SBoneControlMappingEditPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AInstrumentBase>();
}

void SBoneControlMappingEditPanel::RefreshMappingList() {
    UE_LOG(LogTemp, Warning,
           TEXT("SBoneControlMappingEditPanel::RefreshMappingList() - Starting "
                "refresh"));

    MappingPairs.Reset();

    if (!InstrumentActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("RefreshMappingList: InstrumentActor is not valid"));
        return;
    }

    AInstrumentBase* Instrument = InstrumentActor.Get();
    if (!Instrument || !Instrument->SkeletalMeshActor) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "RefreshMappingList: Instrument or SkeletalMeshActor is null"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("RefreshMappingList: Instrument found: %s"),
           *Instrument->GetName());

    // 尝试获取 ControlRigBlueprint
    if (!RetrieveControlRigBlueprint(Instrument)) {
        UE_LOG(
            LogTemp, Error,
            TEXT("RefreshMappingList: Failed to retrieve ControlRigBlueprint"));
        return;
    }

    // 现在获取 bone 和 control 名称
    BoneNames.Reset();
    ControlNames.Reset();
    FilteredBoneNames.Reset();
    FilteredControlNames.Reset();
    BoneFilterText.Reset();
    ControlFilterText.Reset();

    TArray<FString> BoneNameList = GetAllBoneNames();
    TArray<FString> ControlNameList = GetAllControlNames();

    UE_LOG(LogTemp, Warning,
           TEXT("RefreshMappingList: Found %d bones and %d controls"),
           BoneNameList.Num(), ControlNameList.Num());

    for (const FString& BoneName : BoneNameList) {
        BoneNames.Add(MakeShareable(new FString(BoneName)));
        UE_LOG(LogTemp, Verbose, TEXT("  Added bone: %s"), *BoneName);
    }

    for (const FString& ControlName : ControlNameList) {
        ControlNames.Add(MakeShareable(new FString(ControlName)));
        UE_LOG(LogTemp, Verbose, TEXT("  Added control: %s"), *ControlName);
    }

    // 初始化过滤列表（显示所有项）
    FilteredBoneNames = BoneNames;
    FilteredControlNames = ControlNames;

    // 从 Control Rig Blueprint 中读取现有的 Bone Control Mapping
    if (ControlRigBlueprint.IsValid()) {
        TArray<FBoneControlPair> ExistingPairs;
        if (FBoneControlMappingUtility::GetBoneControlMapping(
                ControlRigBlueprint.Get(), ExistingPairs)) {
            UE_LOG(LogTemp, Warning,
                   TEXT("RefreshMappingList: Found %d existing pairs"),
                   ExistingPairs.Num());
            for (const FBoneControlPair& Pair : ExistingPairs) {
                MappingPairs.Add(MakeShareable(new FBoneControlPair(Pair)));
                UE_LOG(LogTemp, Verbose,
                       TEXT("  Loaded pair: Bone=%s, Control=%s"),
                       *Pair.BoneName.ToString(), *Pair.ControlName.ToString());
            }
        } else {
            UE_LOG(LogTemp, Warning,
                   TEXT("RefreshMappingList: GetBoneControlMapping returned "
                        "false, or no existing pairs"));
        }
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("RefreshMappingList: ControlRigBlueprint is not valid "
                    "after retrieval"));
    }

    if (MappingListView.IsValid()) {
        MappingListView->RequestListRefresh();
        UE_LOG(LogTemp, Warning,
               TEXT("RefreshMappingList: RequestListRefresh called"));
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("RefreshMappingList: MappingListView is not valid"));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("RefreshMappingList: Refresh completed with %d pairs"),
           MappingPairs.Num());
}

TSharedRef<ITableRow> SBoneControlMappingEditPanel::GenerateMappingRow(
    TSharedPtr<FBoneControlPair> InPair,
    const TSharedRef<STableViewBase>& OwnerTable) {
    UE_LOG(
        LogTemp, Verbose,
        TEXT("GenerateMappingRow: Creating row for pair Bone=%s, Control=%s"),
        *InPair->BoneName.ToString(), *InPair->ControlName.ToString());
    UE_LOG(LogTemp, Verbose,
           TEXT("GenerateMappingRow: BoneNames list size: %d, ControlNames "
                "list size: %d"),
           BoneNames.Num(), ControlNames.Num());

    return SNew(STableRow<TSharedPtr<FBoneControlPair>>, OwnerTable)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.4f).Padding(FMargin(5.0f))
             [SAssignNew(BoneComboBox, SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&FilteredBoneNames)
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> InOption) {
                      return SNew(STextBlock)
                          .Text(FText::FromString(
                              InOption.IsValid() ? *InOption
                                                 : FString(TEXT("Invalid"))));
                  })
                  .OnSelectionChanged(
                      this,
                      &SBoneControlMappingEditPanel::OnBoneSelectionChanged,
                      InPair)[
                      // 搜索框 + 显示选中值
                      SNew(SVerticalBox) +
                      SVerticalBox::Slot().AutoHeight().Padding(FMargin(
                          5.0f,
                          2.0f))[SNew(SSearchBox)
                                     .HintText(FText::FromString(
                                         TEXT("Search Bones...")))
                                     .OnTextChanged_Lambda(
                                         [this, InPair](const FText& InText) {
                                             BoneFilterText = InText.ToString();
                                             ApplyBoneFilter(InText);
                                             if (BoneComboBox.IsValid()) {
                                                 BoneComboBox->RefreshOptions();
                                             }
                                         })] +
                      SVerticalBox::Slot().AutoHeight().Padding(FMargin(
                          5.0f))[SNew(STextBlock)
                                     .Text_Lambda([InPair]() -> FText {
                                         FString BoneName =
                                             InPair->BoneName.ToString();
                                         if (BoneName == TEXT("None") ||
                                             BoneName.IsEmpty()) {
                                             return FText::FromString(
                                                 TEXT("Select Bone"));
                                         }
                                         return FText::FromString(BoneName);
                                     })
                                     .ColorAndOpacity_Lambda(
                                         [InPair]() -> FSlateColor {
                                             FString BoneName =
                                                 InPair->BoneName.ToString();
                                             if (BoneName == TEXT("None") ||
                                                 BoneName.IsEmpty()) {
                                                 return FSlateColor(
                                                     FLinearColor(0.7f, 0.7f,
                                                                  0.7f));
                                             }
                                             return FSlateColor(
                                                 FLinearColor::White);
                                         })]]] +
         SHorizontalBox::Slot().FillWidth(0.4f).Padding(FMargin(5.0f))
             [SAssignNew(ControlComboBox, SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&FilteredControlNames)
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> InOption) {
                      return SNew(STextBlock)
                          .Text(FText::FromString(
                              InOption.IsValid() ? *InOption
                                                 : FString(TEXT("Invalid"))));
                  })
                  .OnSelectionChanged(
                      this,
                      &SBoneControlMappingEditPanel::OnControlSelectionChanged,
                      InPair)[
                      // 搜索框 + 显示选中值
                      SNew(SVerticalBox) +
                      SVerticalBox::Slot().AutoHeight().Padding(FMargin(
                          5.0f,
                          2.0f))[SNew(SSearchBox)
                                     .HintText(FText::FromString(
                                         TEXT("Search Controls...")))
                                     .OnTextChanged_Lambda(
                                         [this, InPair](const FText& InText) {
                                             ControlFilterText =
                                                 InText.ToString();
                                             ApplyControlFilter(InText);
                                             if (ControlComboBox.IsValid()) {
                                                 ControlComboBox
                                                     ->RefreshOptions();
                                             }
                                         })] +
                      SVerticalBox::Slot().AutoHeight().Padding(FMargin(
                          5.0f))[SNew(STextBlock)
                                     .Text_Lambda([InPair]() -> FText {
                                         FString ControlName =
                                             InPair->ControlName.ToString();
                                         if (ControlName == TEXT("None") ||
                                             ControlName.IsEmpty()) {
                                             return FText::FromString(
                                                 TEXT("Select Control"));
                                         }
                                         return FText::FromString(ControlName);
                                     })
                                     .ColorAndOpacity_Lambda(
                                         [InPair]() -> FSlateColor {
                                             FString ControlName =
                                                 InPair->ControlName.ToString();
                                             if (ControlName == TEXT("None") ||
                                                 ControlName.IsEmpty()) {
                                                 return FSlateColor(
                                                     FLinearColor(0.7f, 0.7f,
                                                                  0.7f));
                                             }
                                             return FSlateColor(
                                                 FLinearColor::White);
                                         })]]] +
         SHorizontalBox::Slot().AutoWidth().Padding(FMargin(5.0f))
             [SNew(SButton)
                  .Text(LOCTEXT("DeleteButton", "X"))
                  .OnClicked(this,
                             &SBoneControlMappingEditPanel::OnDeleteRowClicked,
                             InPair)]];
}

TSharedRef<SWidget> SBoneControlMappingEditPanel::GenerateComboBoxItem(
    TSharedPtr<FString> InOption,
    const TSharedRef<SComboBox<TSharedPtr<FString>>>&) {
    if (!InOption.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("GenerateComboBoxItem: InOption is null"));
        return SNew(STextBlock).Text(FText::FromString(TEXT("Invalid")));
    }

    UE_LOG(LogTemp, Verbose,
           TEXT("GenerateComboBoxItem: Creating item for: %s"), **InOption);

    return SNew(STextBlock).Text(FText::FromString(*InOption));
}

TArray<FString> SBoneControlMappingEditPanel::GetAllBoneNames() const {
    TArray<FString> Result;

    UE_LOG(LogTemp, Warning, TEXT("GetAllBoneNames: Starting..."));

    if (!ControlRigBlueprint.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("GetAllBoneNames: ControlRigBlueprint is not valid"));
        return Result;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("GetAllBoneNames: ControlRigBlueprint is valid, calling "
                "GetAllBoneNamesFromHierarchy"));
    FBoneControlMappingUtility::GetAllBoneNamesFromHierarchy(
        ControlRigBlueprint.Get(), Result);
    UE_LOG(LogTemp, Warning,
           TEXT("GetAllBoneNames: Found %d bones from hierarchy"),
           Result.Num());
    for (const FString& BoneName : Result) {
        UE_LOG(LogTemp, Verbose, TEXT("  - Bone: %s"), *BoneName);
    }

    return Result;
}

TArray<FString> SBoneControlMappingEditPanel::GetAllControlNames() const {
    TArray<FString> Result;

    UE_LOG(LogTemp, Warning, TEXT("GetAllControlNames: Starting..."));

    if (!ControlRigBlueprint.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("GetAllControlNames: ControlRigBlueprint is not valid"));
        return Result;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("GetAllControlNames: ControlRigBlueprint is valid, calling "
                "GetAllControlNamesFromHierarchy"));
    FBoneControlMappingUtility::GetAllControlNamesFromHierarchy(
        ControlRigBlueprint.Get(), Result);
    UE_LOG(LogTemp, Warning,
           TEXT("GetAllControlNames: Found %d controls from hierarchy"),
           Result.Num());
    for (const FString& ControlName : Result) {
        UE_LOG(LogTemp, Verbose, TEXT("  - Control: %s"), *ControlName);
    }

    return Result;
}

FReply SBoneControlMappingEditPanel::OnAddRowClicked() {
    UE_LOG(LogTemp, Warning, TEXT("OnAddRowClicked: Adding new row"));
    TSharedPtr<FBoneControlPair> NewPair =
        MakeShareable(new FBoneControlPair());
    MappingPairs.Add(NewPair);

    if (MappingListView.IsValid()) {
        MappingListView->RequestListRefresh();
        UE_LOG(LogTemp, Warning,
               TEXT("OnAddRowClicked: ListRefresh requested"));
    }

    DetectDuplicates();

    return FReply::Handled();
}

FReply SBoneControlMappingEditPanel::OnDeleteRowClicked(
    TSharedPtr<FBoneControlPair> InPair) {
    UE_LOG(LogTemp, Warning, TEXT("OnDeleteRowClicked: Deleting row"));
    if (InPair.IsValid()) {
        MappingPairs.Remove(InPair);

        if (MappingListView.IsValid()) {
            MappingListView->RequestListRefresh();
        }
    }

    return FReply::Handled();
}

FReply SBoneControlMappingEditPanel::OnSaveClicked() {
    UE_LOG(LogTemp, Warning, TEXT("OnSaveClicked: Starting save"));

    if (!InstrumentActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("OnSaveClicked: InstrumentActor is not valid"));
        return FReply::Handled();
    }

    AInstrumentBase* Instrument = InstrumentActor.Get();
    if (!Instrument || !Instrument->SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("OnSaveClicked: Instrument or SkeletalMeshActor is null"));
        return FReply::Handled();
    }

    if (!EnsureControlRigBlueprintValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("OnSaveClicked: ControlRigBlueprint is not valid and "
                    "cannot be retrieved"));
        return FReply::Handled();
    }

    // 将 MappingPairs 转换为 TArray<FBoneControlPair>
    TArray<FBoneControlPair> MappingData;
    for (const TSharedPtr<FBoneControlPair>& Pair : MappingPairs) {
        if (Pair.IsValid()) {
            MappingData.Add(*Pair);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("OnSaveClicked: Saving %d mappings"),
           MappingData.Num());

    // 保存到 ControlRigBlueprint 中的 BoneControlMapping 变量
    if (FBoneControlMappingUtility::SetBoneControlMapping(
            ControlRigBlueprint.Get(), MappingData)) {
        UE_LOG(LogTemp, Warning,
               TEXT("OnSaveClicked: Successfully saved %d mappings"),
               MappingData.Num());
    } else {
        UE_LOG(LogTemp, Error, TEXT("OnSaveClicked: Failed to save mappings"));
    }

    DetectDuplicates();

    return FReply::Handled();
}

FReply SBoneControlMappingEditPanel::OnSyncBoneControlPairsClicked() {
    UE_LOG(LogTemp, Warning,
           TEXT("OnSyncBoneControlPairsClicked: Starting sync"));

    if (!InstrumentActor.IsValid()) {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "OnSyncBoneControlPairsClicked: InstrumentActor is not valid"));
        return FReply::Handled();
    }

    AInstrumentBase* Instrument = InstrumentActor.Get();
    if (!Instrument) {
        UE_LOG(LogTemp, Error,
               TEXT("OnSyncBoneControlPairsClicked: Instrument is null"));
        return FReply::Handled();
    }

    if (!ControlRigBlueprint.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("OnSyncBoneControlPairsClicked: ControlRigBlueprint is not "
                    "valid"));
        return FReply::Handled();
    }

    int32 SyncedCount = 0;
    int32 FailedCount = 0;

    if (FBoneControlMappingUtility::SyncBoneControlPairs(
            ControlRigBlueprint.Get(), Instrument, SyncedCount, FailedCount)) {
        UE_LOG(LogTemp, Warning,
               TEXT("OnSyncBoneControlPairsClicked: Successfully synced %d "
                    "pairs (Failed: %d)"),
               SyncedCount, FailedCount);
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("OnSyncBoneControlPairsClicked: Failed to sync "
                    "bone-control pairs"));
    }

    return FReply::Handled();
}

void SBoneControlMappingEditPanel::OnBoneSelectionChanged(
    TSharedPtr<FString> InBone, ESelectInfo::Type SelectInfo,
    TSharedPtr<FBoneControlPair> InPair) {
    if (InBone.IsValid() && InPair.IsValid()) {
        InPair->BoneName = FName(**InBone);
        UE_LOG(LogTemp, Warning,
               TEXT("OnBoneSelectionChanged: Bone changed to %s"), **InBone);

        if (MappingListView.IsValid()) {
            MappingListView->RequestListRefresh();
        }
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("OnBoneSelectionChanged: InBone or InPair is not valid"));
    }
}

void SBoneControlMappingEditPanel::OnControlSelectionChanged(
    TSharedPtr<FString> InControl, ESelectInfo::Type SelectInfo,
    TSharedPtr<FBoneControlPair> InPair) {
    if (InControl.IsValid() && InPair.IsValid()) {
        InPair->ControlName = FName(**InControl);
        UE_LOG(LogTemp, Warning,
               TEXT("OnControlSelectionChanged: Control changed to %s"),
               **InControl);

        if (MappingListView.IsValid()) {
            MappingListView->RequestListRefresh();
        }
    } else {
        UE_LOG(
            LogTemp, Error,
            TEXT(
                "OnControlSelectionChanged: InControl or InPair is not valid"));
    }
}

void SBoneControlMappingEditPanel::ApplyBoneFilter(const FText& InFilterText) {
    FilteredBoneNames.Reset();
    FString FilterString = InFilterText.ToString().ToLower();

    for (const TSharedPtr<FString>& BoneName : BoneNames) {
        if (BoneName.IsValid()) {
            if (FilterString.IsEmpty() ||
                BoneName->ToLower().Contains(FilterString)) {
                FilteredBoneNames.Add(BoneName);
            }
        }
    }

    UE_LOG(LogTemp, Verbose,
           TEXT("ApplyBoneFilter: Filtered %d bones from %d total"),
           FilteredBoneNames.Num(), BoneNames.Num());
}

void SBoneControlMappingEditPanel::ApplyControlFilter(
    const FText& InFilterText) {
    FilteredControlNames.Reset();
    FString FilterString = InFilterText.ToString().ToLower();

    for (const TSharedPtr<FString>& ControlName : ControlNames) {
        if (ControlName.IsValid()) {
            if (FilterString.IsEmpty() ||
                ControlName->ToLower().Contains(FilterString)) {
                FilteredControlNames.Add(ControlName);
            }
        }
    }

    UE_LOG(LogTemp, Verbose,
           TEXT("ApplyControlFilter: Filtered %d controls from %d total"),
           FilteredControlNames.Num(), ControlNames.Num());
}

void SBoneControlMappingEditPanel::DetectDuplicates() {
    DuplicateBones.Reset();
    DuplicateControls.Reset();

    TMap<FString, int32> BoneCount;
    TMap<FString, int32> ControlCount;

    // Count occurrences of each bone and control
    for (const TSharedPtr<FBoneControlPair>& Pair : MappingPairs) {
        if (Pair.IsValid()) {
            FString BoneName = Pair->BoneName.ToString();
            FString ControlName = Pair->ControlName.ToString();

            // Skip empty entries
            if (BoneName != TEXT("None") && !BoneName.IsEmpty()) {
                int32& BoneCountRef = BoneCount.FindOrAdd(BoneName);
                BoneCountRef++;
                if (BoneCountRef == 2) {
                    DuplicateBones.AddUnique(BoneName);
                }
            }

            if (ControlName != TEXT("None") && !ControlName.IsEmpty()) {
                int32& ControlCountRef = ControlCount.FindOrAdd(ControlName);
                ControlCountRef++;
                if (ControlCountRef == 2) {
                    DuplicateControls.AddUnique(ControlName);
                }
            }
        }
    }

    if (DuplicateBones.Num() > 0 || DuplicateControls.Num() > 0) {
        UE_LOG(LogTemp, Warning,
               TEXT("DetectDuplicates: Found %d duplicate bones and %d "
                    "duplicate controls"),
               DuplicateBones.Num(), DuplicateControls.Num());
    }
}

FText SBoneControlMappingEditPanel::GetDuplicateWarningText() const {
    if (DuplicateBones.Num() == 0 && DuplicateControls.Num() == 0) {
        return FText::GetEmpty();
    }

    FString WarningText = TEXT("检测到重复项：");

    if (DuplicateBones.Num() > 0) {
        WarningText += TEXT(" bone - [");
        for (int32 i = 0; i < DuplicateBones.Num(); ++i) {
            WarningText += DuplicateBones[i];
            if (i < DuplicateBones.Num() - 1) {
                WarningText += TEXT(", ");
            }
        }
        WarningText += TEXT("]");
    }

    if (DuplicateControls.Num() > 0) {
        WarningText += TEXT(" control - [");
        for (int32 i = 0; i < DuplicateControls.Num(); ++i) {
            WarningText += DuplicateControls[i];
            if (i < DuplicateControls.Num() - 1) {
                WarningText += TEXT(", ");
            }
        }
        WarningText += TEXT("]");
    }

    return FText::FromString(WarningText);
}

FReply SBoneControlMappingEditPanel::OnFilePathBrowse() {
    FString FilePath;
    if (BrowseForFile(TEXT(".json"), FilePath)) {
        ExportImportFilePath = FilePath;
        UE_LOG(LogTemp, Warning, TEXT("File path selected: %s"),
               *ExportImportFilePath);
    }
    return FReply::Handled();
}

bool SBoneControlMappingEditPanel::BrowseForFile(const FString& FileExtension,
                                                 FString& OutFilePath) {
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) {
        UE_LOG(LogTemp, Error, TEXT("Failed to get DesktopPlatform"));
        return false;
    }

    FString FileFilter =
        FString::Printf(TEXT("JSON Files (*%s)|*%s|All Files (*.*)|*.*"),
                        *FileExtension, *FileExtension);
    FString DefaultPath = FPaths::ProjectDir();

    TArray<FString> OutFilenames;
    bool bOpened = DesktopPlatform->OpenFileDialog(
        nullptr, FString::Printf(TEXT("Select %s File"), *FileExtension),
        DefaultPath, TEXT(""), FileFilter, EFileDialogFlags::None,
        OutFilenames);

    if (bOpened && OutFilenames.Num() > 0) {
        OutFilePath = OutFilenames[0];
        return true;
    }

    return false;
}

FReply SBoneControlMappingEditPanel::OnExportClicked() {
    UE_LOG(LogTemp, Warning, TEXT("OnExportClicked: Starting export"));

    if (ExportImportFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("OnExportClicked: File path is empty"));
        return FReply::Handled();
    }

    if (!InstrumentActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("OnExportClicked: InstrumentActor is not valid"));
        return FReply::Handled();
    }

    if (!EnsureControlRigBlueprintValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("OnExportClicked: ControlRigBlueprint is not valid and "
                    "cannot be retrieved"));
        return FReply::Handled();
    }

    // Create JSON object
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> PairsArray;

    // Convert MappingPairs to JSON
    for (const TSharedPtr<FBoneControlPair>& Pair : MappingPairs) {
        if (Pair.IsValid()) {
            TSharedPtr<FJsonObject> PairObject =
                MakeShareable(new FJsonObject());
            PairObject->SetStringField(TEXT("BoneName"),
                                       Pair->BoneName.ToString());
            PairObject->SetStringField(TEXT("ControlName"),
                                       Pair->ControlName.ToString());
            PairsArray.Add(MakeShareable(new FJsonValueObject(PairObject)));
        }
    }

    RootObject->SetArrayField(TEXT("BoneControlMappings"), PairsArray);

    // Write to file
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    if (FFileHelper::SaveStringToFile(OutputString, *ExportImportFilePath)) {
        UE_LOG(LogTemp, Warning,
               TEXT("OnExportClicked: Successfully exported %d mappings to %s"),
               MappingPairs.Num(), *ExportImportFilePath);
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("OnExportClicked: Failed to write to file %s"),
               *ExportImportFilePath);
    }

    return FReply::Handled();
}

FReply SBoneControlMappingEditPanel::OnImportClicked() {
    UE_LOG(LogTemp, Warning, TEXT("OnImportClicked: Starting import"));

    if (ExportImportFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("OnImportClicked: File path is empty"));
        return FReply::Handled();
    }

    if (!FPaths::FileExists(*ExportImportFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("OnImportClicked: File does not exist: %s"),
               *ExportImportFilePath);
        return FReply::Handled();
    }

    if (!EnsureControlRigBlueprintValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("OnImportClicked: ControlRigBlueprint is not valid and "
                    "cannot be retrieved"));
        return FReply::Handled();
    }

    // Read file
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *ExportImportFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("OnImportClicked: Failed to read file %s"),
               *ExportImportFilePath);
        return FReply::Handled();
    }

    // Parse JSON
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(FileContent);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) ||
        !RootObject.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("OnImportClicked: Failed to parse JSON from file"));
        return FReply::Handled();
    }

    // Extract mappings from JSON
    TArray<FBoneControlPair> ImportedPairs;
    TArray<TSharedPtr<FJsonValue>> PairsArray =
        RootObject->GetArrayField(TEXT("BoneControlMappings"));

    for (const TSharedPtr<FJsonValue>& Value : PairsArray) {
        if (Value.IsValid() && Value->Type == EJson::Object) {
            TSharedPtr<FJsonObject> PairObject = Value->AsObject();
            if (PairObject.IsValid()) {
                FBoneControlPair Pair;
                Pair.BoneName =
                    FName(*PairObject->GetStringField(TEXT("BoneName")));
                Pair.ControlName =
                    FName(*PairObject->GetStringField(TEXT("ControlName")));
                ImportedPairs.Add(Pair);
            }
        }
    }

    // Clear current mappings and load imported ones
    MappingPairs.Reset();
    for (const FBoneControlPair& Pair : ImportedPairs) {
        MappingPairs.Add(MakeShareable(new FBoneControlPair(Pair)));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("OnImportClicked: Imported %d mappings from %s"),
           ImportedPairs.Num(), *ExportImportFilePath);

    // Save to ControlRigBlueprint
    if (FBoneControlMappingUtility::SetBoneControlMapping(
            ControlRigBlueprint.Get(), ImportedPairs)) {
        UE_LOG(LogTemp, Warning,
               TEXT("OnImportClicked: Successfully saved imported mappings"));
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("OnImportClicked: Failed to save imported mappings"));
    }

    // Refresh UI
    if (MappingListView.IsValid()) {
        MappingListView->RequestListRefresh();
    }

    DetectDuplicates();

    return FReply::Handled();
}

bool SBoneControlMappingEditPanel::EnsureControlRigBlueprintValid() {
    // 如果已有有效的 Blueprint，直接返回
    if (ControlRigBlueprint.IsValid()) {
        return true;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("EnsureControlRigBlueprintValid: Blueprint is not valid, "
                "attempting to retrieve"));

    // 检查 InstrumentActor 是否有效
    if (!InstrumentActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("EnsureControlRigBlueprintValid: InstrumentActor is not "
                    "valid"));
        return false;
    }

    AInstrumentBase* Instrument = InstrumentActor.Get();
    if (!Instrument) {
        UE_LOG(LogTemp, Error,
               TEXT("EnsureControlRigBlueprintValid: Instrument is null"));
        return false;
    }

    // 使用辅助方法获取 Blueprint
    return RetrieveControlRigBlueprint(Instrument);
}

bool SBoneControlMappingEditPanel::RetrieveControlRigBlueprint(
    AInstrumentBase* InInstrument) {
    if (!InInstrument) {
        UE_LOG(LogTemp, Error,
               TEXT("RetrieveControlRigBlueprint: InInstrument is null"));
        return false;
    }

    if (!InInstrument->SkeletalMeshActor) {
        UE_LOG(LogTemp, Error,
               TEXT("RetrieveControlRigBlueprint: SkeletalMeshActor is null"));
        return false;
    }

    // 尝试获取 ControlRigBlueprint
    UControlRig* ControlRigInstance = nullptr;
    UControlRigBlueprint* RetrievedBlueprint = nullptr;

    if (!FInstrumentControlRigUtility::GetControlRigFromSkeletalMeshActor(
            InInstrument->SkeletalMeshActor, ControlRigInstance,
            RetrievedBlueprint)) {
        UE_LOG(LogTemp, Error,
               TEXT("RetrieveControlRigBlueprint: Failed to get "
                    "ControlRigBlueprint from SkeletalMeshActor"));
        return false;
    }

    if (!RetrievedBlueprint) {
        UE_LOG(
            LogTemp, Error,
            TEXT("RetrieveControlRigBlueprint: Retrieved Blueprint is null"));
        return false;
    }

    // 保存检索到的 Blueprint
    ControlRigBlueprint = RetrievedBlueprint;

    return true;
}

#undef LOCTEXT_NAMESPACE
