#include "UI/SLipSyncPanel.h"

#include "ControlRigBlueprintLegacy.h"
#include "ControlRigCacheSubsystem.h"
#include "DesktopPlatformModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "InstrumentAnimationUtility.h"
#include "InstrumentBase.h"
#include "InstrumentMorphTargetUtility.h"
#include "LipSyncUtility.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SLipSyncPanel"

// ===== 固定口型字母列表（兼容 Lisa 8 符号 + Cherry 12 符号）=====
static const TArray<FString> FixedPhonemes = {
    TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F"),
    TEXT("G"), TEXT("H"), TEXT("I"), TEXT("J"), TEXT("K"), TEXT("X")};

void SLipSyncPanel::Construct(const FArguments& InArgs) {
    // 初始化 9 行固定映射
    MappingPairs.Reset();
    for (const FString& Phoneme : FixedPhonemes) {
        MappingPairs.Add(
            MakeShareable(new FLipSyncMappingPair(Phoneme, TEXT(""))));
    }

    MorphTargetOptions.Reset();
    ParseResultText = TEXT("No file parsed yet.");

    ChildSlot
        [SNew(SScrollBox) +
         SScrollBox::Slot()
             [SNew(SVerticalBox)

              // ===== Mapping 区域 =====
              + SVerticalBox::Slot().AutoHeight().Padding(10.0f)
                    [SNew(SVerticalBox)

                     // 标题
                     + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f,
                                                                 0.0f, 5.0f)
                           [SNew(STextBlock)
                                .Text(LOCTEXT(
                                    "MappingSectionTitle", "Lip Sync Mapping"))
                                .Font(FAppStyle::GetFontStyle(
                                    "DetailsView.CategoryFont"))]

                     // 映射列表
                     +
                     SVerticalBox::Slot().AutoHeight().Padding(
                         0.0f, 0.0f, 0.0f, 5.0f)[SNew(SBox).MaxDesiredHeight(
                         320.0f)[SAssignNew(
                                     MappingListView,
                                     SListView<TSharedPtr<FLipSyncMappingPair>>)
                                     .ListItemsSource(&MappingPairs)
                                     .OnGenerateRow(
                                         this,
                                         &SLipSyncPanel::GenerateMappingRow)
                                     .HeaderRow(
                                         SNew(SHeaderRow) +
                                         SHeaderRow::Column(TEXT("Phoneme"))
                                             .DefaultLabel(LOCTEXT(
                                                 "PhonemeColumn", "Phoneme"))
                                             .FillWidth(0.3f) +
                                         SHeaderRow::Column(TEXT("MorphTarget"))
                                             .DefaultLabel(
                                                 LOCTEXT("MorphTargetColumn",
                                                         "Morph Target"))
                                             .FillWidth(0.7f))]]

                     // 按钮行
                     +
                     SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f,
                                                               0.0f)
                         [SNew(SHorizontalBox) +
                          SHorizontalBox::Slot().AutoWidth().Padding(
                              0.0f, 0.0f, 10.0f, 0.0f)
                              [SNew(SButton)
                                   .Text(LOCTEXT("SaveMappingButton",
                                                 "Save Mapping"))
                                   .OnClicked(
                                       this,
                                       &SLipSyncPanel::OnSaveMappingClicked)] +
                          SHorizontalBox::Slot().AutoWidth()
                              [SNew(SButton)
                                   .Text(LOCTEXT("InitLipSyncControlButton",
                                                 "Init Lip Sync Control"))
                                   .OnClicked(
                                       this, &SLipSyncPanel::
                                                 OnInitLipSyncControlClicked)] +
                          SHorizontalBox::Slot().AutoWidth().Padding(
                              10.0f, 0.0f, 0.0f, 0.0f)
                              [SNew(SButton)
                                   .Text(LOCTEXT("ApplyMappingButton",
                                                 "Apply Mapping to Rig"))
                                   .OnClicked(this,
                                              &SLipSyncPanel::
                                                  OnApplyMappingToRigClicked)]]]

              // ===== 分隔线 =====
              + SVerticalBox::Slot().AutoHeight().Padding(
                    10.0f, 0.0f)[SNew(SBox).HeightOverride(
                    2.0f)[SNew(SBorder).BorderBackgroundColor(
                    FLinearColor(0.3f, 0.3f, 0.3f, 1.0f))]]

              // ===== Generation 区域 =====
              + SVerticalBox::Slot().AutoHeight().Padding(10.0f)
                    [SNew(SVerticalBox)

                     // 标题
                     + SVerticalBox::Slot().AutoHeight().Padding(
                           0.0f, 0.0f, 0.0f,
                           5.0f)[SNew(STextBlock)
                                     .Text(LOCTEXT("GenerationSectionTitle",
                                                   "Lip Sync Generation"))
                                     .Font(FAppStyle::GetFontStyle(
                                         "DetailsView.CategoryFont"))]

                     // 文件浏览行
                     + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f,
                                                                 0.0f, 5.0f)
                           [SNew(SHorizontalBox) +
                            SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(0.0f, 0.0f, 5.0f, 0.0f)
                                    [SNew(STextBlock)
                                         .Text(LOCTEXT("LipSyncFileLabel",
                                                       "File:"))] +
                            SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                                0.0f, 0.0f,
                                5.0f, 0.0f)[SNew(SEditableTextBox)
                                                .Text_Lambda([this]() -> FText {
                                                    return FText::FromString(
                                                        JsonFilePath);
                                                })
                                                .IsReadOnly(true)] +
                            SHorizontalBox::Slot().AutoWidth()
                                [SNew(SButton)
                                     .Text(LOCTEXT("BrowseButton", "Browse"))
                                     .OnClicked(this, &SLipSyncPanel::
                                                          OnBrowseJsonClicked)]]

                     // 解析结果
                     +
                     SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f,
                                                               5.0f)
                         [SNew(SBorder)
                              .BorderImage(
                                  FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                              .Padding(10.0f)
                                  [SAssignNew(ParseResultTextBlock, STextBlock)
                                       .Text(FText::FromString(ParseResultText))
                                       .AutoWrapText(true)]]

                     // 操作按钮行
                     +
                     SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f,
                                                               0.0f)
                         [SNew(SHorizontalBox) +
                          SHorizontalBox::Slot().AutoWidth().Padding(
                              0.0f, 0.0f, 10.0f, 0.0f)
                              [SNew(SButton)
                                   .Text(LOCTEXT("ParsePreviewButton",
                                                 "Parse && Preview"))
                                   .OnClicked(
                                       this,
                                       &SLipSyncPanel::OnParsePreviewClicked)] +
                          SHorizontalBox::Slot().AutoWidth()
                              [SNew(SButton)
                                   .Text(LOCTEXT("GenerateLipSyncButton",
                                                 "Generate Lip Sync"))
                                   .OnClicked(
                                       this, &SLipSyncPanel::
                                                 OnGenerateLipSyncClicked)]]]]];

    UE_LOG(LogTemp, Log, TEXT("[SLipSyncPanel] Constructed"));
}

// ===== Actor 管理 =====

void SLipSyncPanel::SetActor(AActor* InActor) {
    InstrumentActor = Cast<AInstrumentBase>(InActor);

    if (InstrumentActor.IsValid()) {
        UE_LOG(LogTemp, Log, TEXT("[SLipSyncPanel] SetActor: %s"),
               *InstrumentActor->GetName());

        // 刷新 Morph Target 选项
        RefreshMorphTargetOptions();

        // 尝试获取 ControlRigBlueprint 并读取已有映射
        if (RetrieveControlRigBlueprint()) {
            TArray<FLipSyncMappingPair> SavedMapping;
            if (ULipSyncUtility::GetLipSyncMapping(ControlRigBlueprint.Get(),
                                                   SavedMapping)) {
                // 将保存的映射回填到固定行中
                for (const FLipSyncMappingPair& SavedPair : SavedMapping) {
                    for (TSharedPtr<FLipSyncMappingPair>& Pair : MappingPairs) {
                        if (Pair->Phoneme == SavedPair.Phoneme) {
                            Pair->MorphTargetName = SavedPair.MorphTargetName;
                            break;
                        }
                    }
                }
                MappingListView->RequestListRefresh();
            }
        }
    } else {
        UE_LOG(LogTemp, Warning,
               TEXT("[SLipSyncPanel] SetActor: Actor is not AInstrumentBase"));
        ControlRigBlueprint.Reset();
    }

    RefreshMappingList();
}

bool SLipSyncPanel::CanHandleActor(const AActor* InActor) const {
    return InActor && InActor->IsA<AInstrumentBase>();
}

// ===== Mapping 区域 =====

void SLipSyncPanel::RefreshMappingList() {
    if (MappingListView.IsValid()) {
        MappingListView->RequestListRefresh();
    }
}

void SLipSyncPanel::RefreshMorphTargetOptions() {
    MorphTargetOptions.Reset();

    TArray<FString> Names = GetPerformerMorphTargetNames();
    for (const FString& Name : Names) {
        MorphTargetOptions.Add(MakeShareable(new FString(Name)));
    }

    UE_LOG(LogTemp, Log,
           TEXT("[SLipSyncPanel] Refreshed %d morph target options"),
           MorphTargetOptions.Num());
}

TSharedRef<ITableRow> SLipSyncPanel::GenerateMappingRow(
    TSharedPtr<FLipSyncMappingPair> InPair,
    const TSharedRef<STableViewBase>& OwnerTable) {
    // 初始化选中项
    TSharedPtr<FString> SelectedMorphTarget;
    if (!InPair->MorphTargetName.IsEmpty()) {
        for (const TSharedPtr<FString>& Option : MorphTargetOptions) {
            if (Option.IsValid() && *Option == InPair->MorphTargetName) {
                SelectedMorphTarget = Option;
                break;
            }
        }
    }

    return SNew(STableRow<TSharedPtr<FLipSyncMappingPair>>, OwnerTable)
        .Padding(2.0f)
            [SNew(SHorizontalBox)
             // Phoneme 列
             + SHorizontalBox::Slot()
                   .FillWidth(0.3f)
                   .VAlign(VAlign_Center)
                   .Padding(
                       5.0f)[SNew(STextBlock)
                                 .Text(FText::FromString(InPair->Phoneme))
                                 .Font(FAppStyle::GetFontStyle("NormalText"))]
             // Morph Target 下拉列
             + SHorizontalBox::Slot()
                   .FillWidth(0.7f)
                   .VAlign(VAlign_Center)
                   .Padding(5.0f)
                       [SNew(SComboBox<TSharedPtr<FString>>)
                            .OptionsSource(&MorphTargetOptions)
                            .InitiallySelectedItem(SelectedMorphTarget)
                            .OnSelectionChanged(
                                this,
                                &SLipSyncPanel::OnMorphTargetSelectionChanged,
                                InPair)
                            .OnGenerateWidget(
                                this,
                                &SLipSyncPanel::GenerateMorphTargetComboItem)
                                [SNew(STextBlock)
                                     .Text(this,
                                           &SLipSyncPanel::
                                               GetSelectedMorphTargetText,
                                           InPair)]]];
}

void SLipSyncPanel::OnMorphTargetSelectionChanged(
    TSharedPtr<FString> InSelection, ESelectInfo::Type SelectInfo,
    TSharedPtr<FLipSyncMappingPair> InPair) {
    if (InPair.IsValid() && InSelection.IsValid()) {
        InPair->MorphTargetName = *InSelection;
    }
}

TSharedRef<SWidget> SLipSyncPanel::GenerateMorphTargetComboItem(
    TSharedPtr<FString> InOption) {
    return SNew(STextBlock)
        .Text(FText::FromString(InOption.IsValid() ? *InOption : TEXT("")));
}

FText SLipSyncPanel::GetSelectedMorphTargetText(
    TSharedPtr<FLipSyncMappingPair> InPair) const {
    if (InPair.IsValid() && !InPair->MorphTargetName.IsEmpty()) {
        return FText::FromString(InPair->MorphTargetName);
    }
    return LOCTEXT("SelectMorphTarget", "Select Morph Target...");
}

FReply SLipSyncPanel::OnSaveMappingClicked() {
    if (!EnsureControlRigBlueprintValid()) {
        ShowNotification(
            LOCTEXT("SaveMappingFailed_NoCRB",
                    "Failed to save mapping: No ControlRigBlueprint found."),
            false);
        return FReply::Handled();
    }

    // 确保变量存在
    if (!ULipSyncUtility::AddLipSyncMappingVariable(
            ControlRigBlueprint.Get())) {
        ShowNotification(
            LOCTEXT("SaveMappingFailed_AddVar",
                    "Failed to save mapping: Could not add variable."),
            false);
        return FReply::Handled();
    }

    // 收集映射（只保存 MorphTargetName 非空的）
    TArray<FLipSyncMappingPair> ValidMapping;
    for (const TSharedPtr<FLipSyncMappingPair>& Pair : MappingPairs) {
        if (Pair.IsValid() && !Pair->MorphTargetName.IsEmpty()) {
            ValidMapping.Add(*Pair);
        }
    }

    if (!ULipSyncUtility::SetLipSyncMapping(ControlRigBlueprint.Get(),
                                            ValidMapping)) {
        ShowNotification(
            LOCTEXT("SaveMappingFailed_Set", "Failed to save mapping."), false);
        return FReply::Handled();
    }

    ShowNotification(FText::Format(
        LOCTEXT("SaveMappingSuccess", "Saved {0} mapping entries."),
        FText::AsNumber(ValidMapping.Num())));
    return FReply::Handled();
}

FReply SLipSyncPanel::OnInitLipSyncControlClicked() {
    if (!EnsureControlRigBlueprintValid()) {
        ShowNotification(
            LOCTEXT("InitControlFailed_NoCRB",
                    "Failed to init: No ControlRigBlueprint found."),
            false);
        return FReply::Handled();
    }

    const bool bInitSuccess =
        ULipSyncUtility::InitializeLipSyncControl(ControlRigBlueprint.Get());

    if (bInitSuccess) {
        ShowNotification(LOCTEXT("InitControlSuccess",
                                 "Lip sync control initialized successfully."),
                         true);
    } else {
        ShowNotification(
            LOCTEXT("InitControlFailed",
                    "Failed to initialize lip sync control. Check log for "
                    "details."),
            false);
    }
    return FReply::Handled();
}

FReply SLipSyncPanel::OnApplyMappingToRigClicked() {
    if (!EnsureControlRigBlueprintValid()) {
        ShowNotification(LOCTEXT("ApplyMappingFailed_NoCRB",
                                 "Failed: No ControlRigBlueprint found."),
                         false);
        return FReply::Handled();
    }

    const int32 ChannelCount =
        ULipSyncUtility::ApplyMappingToRig(ControlRigBlueprint.Get());

    if (ChannelCount > 0) {
        ShowNotification(
            FText::Format(LOCTEXT("ApplyMappingSuccess",
                                  "Applied mapping: created {0} channels."),
                          FText::AsNumber(ChannelCount)),
            true);
    } else {
        ShowNotification(
            LOCTEXT("ApplyMappingNoChannels",
                    "No channels created. Check mapping is filled and "
                    "Control Rig is compiled."),
            false);
    }
    return FReply::Handled();
}

// ===== Generation 区域 =====

FReply SLipSyncPanel::OnBrowseJsonClicked() {
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) {
        return FReply::Handled();
    }

    TArray<FString> OutFiles;
    const bool bOpened = DesktopPlatform->OpenFileDialog(
        nullptr, TEXT("Select Lip Sync File"), FPaths::GetPath(JsonFilePath),
        TEXT(""),
        TEXT("Lip Sync Files (*.json, *.tsv)|*.json;*.tsv|JSON Files "
             "(*.json)|*.json|TSV Files (*.tsv)|*.tsv"),
        EFileDialogFlags::None, OutFiles);

    if (bOpened && OutFiles.Num() > 0) {
        JsonFilePath = OutFiles[0];
        UE_LOG(LogTemp, Log, TEXT("[SLipSyncPanel] Selected file: %s"),
               *JsonFilePath);
    }

    return FReply::Handled();
}

FReply SLipSyncPanel::OnParsePreviewClicked() {
    if (JsonFilePath.IsEmpty()) {
        ParseResultText = TEXT("No lip sync file selected.");
        if (ParseResultTextBlock.IsValid()) {
            ParseResultTextBlock->SetText(FText::FromString(ParseResultText));
        }
        return FReply::Handled();
    }

    TArray<FLipSyncMouthCue> Cues;
    float Duration = 0.0f;
    if (!ULipSyncUtility::ParseLipSyncFile(JsonFilePath, Cues, Duration)) {
        ParseResultText = TEXT("Failed to parse lip sync file.");
        if (ParseResultTextBlock.IsValid()) {
            ParseResultTextBlock->SetText(FText::FromString(ParseResultText));
        }
        ShowNotification(
            LOCTEXT("ParseFailed", "Failed to parse lip sync file."), false);
        return FReply::Handled();
    }

    // 收集使用的口型种类
    TSet<FString> UsedPhonemes;
    for (const FLipSyncMouthCue& Cue : Cues) {
        if (!Cue.Value.IsEmpty()) {
            UsedPhonemes.Add(Cue.Value.ToUpper());
        }
    }

    // 构建摘要文本
    ParseResultText = FString::Printf(TEXT("Duration: %.2fs\nTotal Cues: %d\n"),
                                      Duration, Cues.Num());

    // 口型种类
    FString PhonemesStr;
    for (const FString& P : UsedPhonemes) {
        if (!PhonemesStr.IsEmpty()) PhonemesStr += TEXT(", ");
        PhonemesStr += P;
    }
    ParseResultText += FString::Printf(TEXT("Used Phonemes: %s"), *PhonemesStr);

    if (ParseResultTextBlock.IsValid()) {
        ParseResultTextBlock->SetText(FText::FromString(ParseResultText));
    }

    ShowNotification(FText::Format(
        LOCTEXT("ParseSuccess", "Parsed {0} cues, {1} phoneme types."),
        FText::AsNumber(Cues.Num()), FText::AsNumber(UsedPhonemes.Num())));
    return FReply::Handled();
}

FReply SLipSyncPanel::OnGenerateLipSyncClicked() {
    if (JsonFilePath.IsEmpty()) {
        ShowNotification(
            LOCTEXT("GenerateFailed_NoFile", "No lip sync file selected."),
            false);
        return FReply::Handled();
    }

    if (!InstrumentActor.IsValid()) {
        ShowNotification(
            LOCTEXT("GenerateFailed_NoActor", "No instrument actor selected."),
            false);
        return FReply::Handled();
    }

    if (!EnsureControlRigBlueprintValid()) {
        ShowNotification(
            LOCTEXT("GenerateFailed_NoCRB", "No ControlRigBlueprint found."),
            false);
        return FReply::Handled();
    }

    ASkeletalMeshActor* Performer = InstrumentActor->SkeletalMeshActor;
    if (!Performer) {
        ShowNotification(LOCTEXT("GenerateFailed_NoPerformer",
                                 "Instrument actor has no SkeletalMeshActor."),
                         false);
        return FReply::Handled();
    }

    const int32 WrittenCount = ULipSyncUtility::GenerateLipSyncFromJson(
        Performer, ControlRigBlueprint.Get(), JsonFilePath);

    if (WrittenCount > 0) {
        ShowNotification(FText::Format(
            LOCTEXT("GenerateSuccess",
                    "Lip sync generated! Written {0} morph targets."),
            FText::AsNumber(WrittenCount)));
    } else {
        ShowNotification(
            LOCTEXT("GenerateFailed", "Failed to generate lip sync animation."),
            false);
    }

    return FReply::Handled();
}

// ===== 内部工具 =====

bool SLipSyncPanel::EnsureControlRigBlueprintValid() {
    if (!ControlRigBlueprint.IsValid()) {
        return RetrieveControlRigBlueprint();
    }
    return true;
}

bool SLipSyncPanel::RetrieveControlRigBlueprint() {
    if (!InstrumentActor.IsValid()) {
        UE_LOG(LogTemp, Warning,
               TEXT("[SLipSyncPanel] RetrieveControlRigBlueprint: No "
                    "InstrumentActor"));
        return false;
    }

    if (!InstrumentActor->SkeletalMeshActor) {
        UE_LOG(LogTemp, Warning,
               TEXT("[SLipSyncPanel] RetrieveControlRigBlueprint: "
                    "SkeletalMeshActor is null"));
        return false;
    }

    UControlRigCacheSubsystem* CacheSubsystem =
        GEngine->GetEngineSubsystem<UControlRigCacheSubsystem>();
    if (!CacheSubsystem) {
        UE_LOG(LogTemp, Error,
               TEXT("[SLipSyncPanel] RetrieveControlRigBlueprint: "
                    "CacheSubsystem not available"));
        return false;
    }

    ULevelSequence* LevelSequence =
        UInstrumentAnimationUtility::GetCurrentLevelSequence();
    if (!LevelSequence) {
        UE_LOG(LogTemp, Warning,
               TEXT("[SLipSyncPanel] RetrieveControlRigBlueprint: No active "
                    "LevelSequence"));
        return false;
    }

    UControlRigBlueprint* RetrievedBlueprint =
        CacheSubsystem->GetControlRigBlueprint(
            InstrumentActor->SkeletalMeshActor, LevelSequence);

    if (!RetrievedBlueprint) {
        UE_LOG(LogTemp, Warning,
               TEXT("[SLipSyncPanel] RetrieveControlRigBlueprint: Failed to "
                    "get ControlRigBlueprint"));
        return false;
    }

    ControlRigBlueprint = RetrievedBlueprint;
    UE_LOG(LogTemp, Log,
           TEXT("[SLipSyncPanel] RetrieveControlRigBlueprint: Success"));

    return true;
}

TArray<FString> SLipSyncPanel::GetPerformerMorphTargetNames() const {
    TArray<FString> Names;

    if (!InstrumentActor.IsValid() || !InstrumentActor->SkeletalMeshActor) {
        return Names;
    }

    USkeletalMeshComponent* SkeletalMeshComp =
        InstrumentActor->SkeletalMeshActor->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp) {
        return Names;
    }

    UInstrumentMorphTargetUtility::GetMorphTargetNames(SkeletalMeshComp, Names);
    return Names;
}

void SLipSyncPanel::ShowNotification(const FText& Message,
                                     bool bIsSuccess) const {
    FNotificationInfo Info(Message);
    Info.ExpireDuration = 3.0f;
    Info.bUseLargeFont = false;
    Info.bFireAndForget = true;
    Info.bUseSuccessFailIcons = true;

    if (bIsSuccess) {
        Info.Image = FAppStyle::GetBrush("NotificationList.SuccessImage");
    } else {
        Info.Image = FAppStyle::GetBrush("NotificationList.FailImage");
    }

    FSlateNotificationManager::Get().AddNotification(Info);
}

#undef LOCTEXT_NAMESPACE
