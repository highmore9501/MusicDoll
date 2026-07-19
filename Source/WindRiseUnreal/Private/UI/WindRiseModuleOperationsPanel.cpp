#include "UI/WindRiseModuleOperationsPanel.h"

#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "WindRiseUnreal.h"

#define LOCTEXT_NAMESPACE "WindRiseModuleOperationsPanel"

void SWindRiseModuleOperationsPanel::Construct(const FArguments& InArgs) {
    SModuleOperationsPanel::FArguments BaseArgs;
    SModuleOperationsPanel::Construct(BaseArgs);

    SelectedNote = MakeShareable(new FString(TEXT("C4 (60)")));

    // 创建 MT 调整面板
    CharacterMTPanel =
        SNew(SMorphTargetAdjustPanel).Title(TEXT("Character Morph Targets"));
    InstrumentMTPanel =
        SNew(SMorphTargetAdjustPanel).Title(TEXT("Instrument Morph Targets"));
}

void SWindRiseModuleOperationsPanel::SetActor(AActor* InActor) {
    WindRiseActor = Cast<AWindRiseUnreal>(InActor);
    if (WindRiseActor.IsValid()) {
        UpdateNoteOptions();
    }
    RefreshOperations();
}

bool SWindRiseModuleOperationsPanel::CanHandleActor(
    const AActor* InActor) const {
    return Cast<const AWindRiseUnreal>(InActor) != nullptr;
}

void SWindRiseModuleOperationsPanel::RefreshOperations() {
    if (!WindRiseActor.IsValid()) return;

    AWindRiseUnreal* WindRise = WindRiseActor.Get();

    // 刷新人物 MT 面板
    USkeletalMeshComponent* PerformerSkelComp =
        WindRise->SkeletalMeshActor
            ? WindRise->SkeletalMeshActor->GetSkeletalMeshComponent()
            : nullptr;
    UControlRig* PerformerCR = WindRise->GetCachedControlRig(TEXT("Performer"));
    if (CharacterMTPanel.IsValid()) {
        CharacterMTPanel->SetMorphTargets(WindRise->CharacterMorphTargets,
                                          PerformerSkelComp, PerformerCR);
    }

    // 刷新乐器 MT 面板
    USkeletalMeshComponent* InstrumentSkelComp =
        WindRise->InstrumentMesh
            ? WindRise->InstrumentMesh->GetSkeletalMeshComponent()
            : nullptr;
    UControlRig* InstrumentCR =
        WindRise->GetCachedControlRig(TEXT("Instrument"));
    if (InstrumentMTPanel.IsValid()) {
        InstrumentMTPanel->SetMorphTargets(WindRise->InstrumentMorphTargets,
                                           InstrumentSkelComp, InstrumentCR);
    }

    CreateOperationWidgets();
}

void SWindRiseModuleOperationsPanel::CreateOperationWidgets() {
    TSharedPtr<SVerticalBox> Container = GetOperationContainer();
    if (!Container.IsValid()) return;

    Container->ClearChildren();

    if (!WindRiseActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActorSelected",
                                    "No WindRise Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    // ========== 音高状态录制 ==========
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Note State Recording"))];

    // 当前音高选择
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot()
             .AutoWidth()
             .Padding(0.0f, 0.0f, 10.0f, 0.0f)
             .VAlign(VAlign_Center)[SNew(STextBlock)
                                        .Text(LOCTEXT("CurrentNoteLabel",
                                                      "Current Note:"))
                                        .Font(FAppStyle::GetFontStyle(
                                            "DetailsView.CategoryFont"))] +
         SHorizontalBox::Slot().FillWidth(1.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&NoteOptions)
                  .OnSelectionChanged(
                      this,
                      &SWindRiseModuleOperationsPanel::OnNoteSelectionChanged)
                  .OnGenerateWidget(
                      this,
                      &SWindRiseModuleOperationsPanel::OnGenerateNoteWidget)
                      [SNew(STextBlock).Text_Lambda([this]() -> FText {
                          if (SelectedNote.IsValid())
                              return FText::FromString(*SelectedNote);
                          return FText::GetEmpty();
                      })]]];

    // 人物 MT 调整
    if (CharacterMTPanel.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[CharacterMTPanel.ToSharedRef()];
    }

    // 乐器 MT 调整
    if (InstrumentMTPanel.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[InstrumentMTPanel.ToSharedRef()];
    }

    // Save / Load State 按钮
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("SaveStateButton", "Save State"))
                  .OnClicked(this, &SWindRiseModuleOperationsPanel::OnSaveState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("LoadStateButton", "Load State"))
                  .OnClicked(this, &SWindRiseModuleOperationsPanel::OnLoadState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

    // ========== Rest Offset ==========
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Rest Offset"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("CaptureRestOffsetButton", "Capture Rest Offset"))
             .OnClicked(this,
                        &SWindRiseModuleOperationsPanel::OnCaptureRestOffset)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ========== 乐器初始化 ==========
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Instrument Initialization"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("InitializeInstrumentCR",
                                "Initialize Instrument Control Rig"))
                  .OnClicked(
                      this,
                      &SWindRiseModuleOperationsPanel::OnInitializeInstrumentCR)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ========== 动画生成 ==========
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Animation Generation"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT(".wind_rise File"), TEXT(""), TEXT("WindRiseFilePath"),
        TEXT(".wind_rise"),
        [this](const FString& NewPath) {
            if (WindRiseActor.IsValid()) {
                WindRiseActor.Get()->AnimationFilePath = NewPath;
                WindRiseActor.Get()->Modify();
            }
        },
        false)];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("GenerateAnimationButton", "Generate Animation"))
             .OnClicked(this,
                        &SWindRiseModuleOperationsPanel::OnGenerateAnimation)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

// ============================================================
// 音高下拉
// ============================================================

void SWindRiseModuleOperationsPanel::UpdateNoteOptions() {
    NoteOptions.Empty();
    if (!WindRiseActor.IsValid()) return;

    AWindRiseUnreal* WindRise = WindRiseActor.Get();
    for (int32 Note = WindRise->MinNote; Note <= WindRise->MaxNote; ++Note) {
        FString NoteName = AWindRiseUnreal::NoteNumberToName(Note);
        FString Option = FString::Printf(TEXT("%s (%d)"), *NoteName, Note);
        NoteOptions.Add(MakeShareable(new FString(Option)));
    }

    if (NoteOptions.Num() > 0) {
        SelectedNote = NoteOptions[0];
        // 同步 CurrentNote
        WindRise->CurrentNote = WindRise->MinNote;
    }
}

void SWindRiseModuleOperationsPanel::OnNoteSelectionChanged(
    TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo) {
    if (!NewValue.IsValid() || !WindRiseActor.IsValid()) return;

    SelectedNote = NewValue;
    FString NoteStr = *NewValue;
    int32 ParenIdx = NoteStr.Find(TEXT("("));
    if (ParenIdx != INDEX_NONE) {
        FString NumStr =
            NoteStr.Mid(ParenIdx + 1, NoteStr.Len() - ParenIdx - 2);
        int32 NoteNum = FCString::Atoi(*NumStr);
        WindRiseActor.Get()->CurrentNote = NoteNum;
        WindRiseActor.Get()->Modify();

        // 同步 MT 面板
        RefreshOperations();
    }
}

TSharedRef<SWidget> SWindRiseModuleOperationsPanel::OnGenerateNoteWidget(
    TSharedPtr<FString> InItem) {
    return SNew(STextBlock).Text(FText::FromString(*InItem));
}

// ============================================================
// 回调
// ============================================================

FReply SWindRiseModuleOperationsPanel::OnSaveState() {
    if (!WindRiseActor.IsValid()) return FReply::Handled();

    AWindRiseUnreal* WindRise = WindRiseActor.Get();
    int32 Note = WindRise->CurrentNote;

    // 从 MT 调整面板获取当前值，存到 Actor 中
    if (CharacterMTPanel.IsValid()) {
        TArray<float> CharValues = CharacterMTPanel->GetAllValues();
        // 保存前先把当前 MT 面板的值刷新到 Actor 的 MT 状态
        for (int32 i = 0;
             i < CharValues.Num() && i < WindRise->CharacterMorphTargets.Num();
             ++i) {
            WindRise->SetCharacterMTValue(i, CharValues[i]);
        }
    }
    if (InstrumentMTPanel.IsValid()) {
        TArray<float> InstValues = InstrumentMTPanel->GetAllValues();
        for (int32 i = 0;
             i < InstValues.Num() && i < WindRise->InstrumentMorphTargets.Num();
             ++i) {
            WindRise->SetInstrumentMTValue(i, InstValues[i]);
        }
    }

    WindRise->SaveNoteState(Note);
    return FReply::Handled();
}

FReply SWindRiseModuleOperationsPanel::OnLoadState() {
    if (!WindRiseActor.IsValid()) return FReply::Handled();

    WindRiseActor.Get()->LoadNoteState(WindRiseActor.Get()->CurrentNote);
    // 刷新 MT 面板以同步加载后的值
    RefreshOperations();
    return FReply::Handled();
}

FReply SWindRiseModuleOperationsPanel::OnCaptureRestOffset() {
    if (WindRiseActor.IsValid()) {
        WindRiseActor.Get()->CaptureRestOffset();
    }
    return FReply::Handled();
}

FReply SWindRiseModuleOperationsPanel::OnInitializeInstrumentCR() {
    if (WindRiseActor.IsValid()) {
        WindRiseActor.Get()->InitializeInstrumentControlRig();
    }
    return FReply::Handled();
}

FReply SWindRiseModuleOperationsPanel::OnGenerateAnimation() {
    if (WindRiseActor.IsValid()) {
        WindRiseActor.Get()->GenerateAnimationFromWindRise(
            WindRiseActor.Get()->AnimationFilePath);
    }
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
