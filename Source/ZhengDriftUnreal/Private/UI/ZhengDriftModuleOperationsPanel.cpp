#include "UI/ZhengDriftModuleOperationsPanel.h"

#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ZhengDriftAnimationProcessor.h"
#include "ZhengDriftControlRigProcessor.h"

#define LOCTEXT_NAMESPACE "SZhengDriftModuleOperationsPanel"

void SZhengDriftModuleOperationsPanel::Construct(const FArguments& InArgs) {
    SModuleOperationsPanel::FArguments BaseArgs;
    SModuleOperationsPanel::Construct(BaseArgs);

    // 左手位置选项
    LeftHandPositionOptions.Add(MakeShareable(new FString(TEXT("FAR"))));
    LeftHandPositionOptions.Add(MakeShareable(new FString(TEXT("MIDDLE"))));
    LeftHandPositionOptions.Add(MakeShareable(new FString(TEXT("NEAR"))));

    // 左手动作选项
    LeftHandActionOptions.Add(MakeShareable(new FString(TEXT("NORMAL"))));
    LeftHandActionOptions.Add(MakeShareable(new FString(TEXT("PRESS"))));

    // 右手位置选项
    RightHandPositionOptions.Add(MakeShareable(new FString(TEXT("FAR"))));
    RightHandPositionOptions.Add(MakeShareable(new FString(TEXT("MIDDLE"))));
    RightHandPositionOptions.Add(MakeShareable(new FString(TEXT("NEAR"))));

    // 右手动作选项
    RightHandActionOptions.Add(MakeShareable(new FString(TEXT("NORMAL"))));
    RightHandActionOptions.Add(MakeShareable(new FString(TEXT("TREMOLO"))));
}

void SZhengDriftModuleOperationsPanel::SetActor(AActor* InActor) {
    ZhengDriftActor = Cast<AZhengDriftUnreal>(InActor);
    RefreshOperations();
}

bool SZhengDriftModuleOperationsPanel::CanHandleActor(
    const AActor* InActor) const {
    return Cast<const AZhengDriftUnreal>(InActor) != nullptr;
}

void SZhengDriftModuleOperationsPanel::RefreshOperations() {
    CreateOperationWidgets();
}

void SZhengDriftModuleOperationsPanel::CreateOperationWidgets() {
    auto Container = GetOperationContainer();
    if (!Container.IsValid()) return;

    Container->ClearChildren();

    if (!ZhengDriftActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActor", "No ZhengDrift Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    // ---- Hand State Configuration ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Hand State Configuration"))];

    // 标签行
    Container->AddSlot().AutoHeight().Padding(5.0f, 5.0f, 5.0f, 2.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot()
             .FillWidth(1.0f)
             .Padding(5.0f, 0.0f)
             .VAlign(
                 VAlign_Center)[SNew(STextBlock)
                                    .Text(LOCTEXT("LPosLabel", "L Position:"))
                                    .Justification(ETextJustify::Center)] +
         SHorizontalBox::Slot()
             .FillWidth(1.0f)
             .Padding(5.0f, 0.0f)
             .VAlign(VAlign_Center)[SNew(STextBlock)
                                        .Text(LOCTEXT("LActLabel", "L Action:"))
                                        .Justification(ETextJustify::Center)] +
         SHorizontalBox::Slot()
             .FillWidth(1.0f)
             .Padding(5.0f, 0.0f)
             .VAlign(
                 VAlign_Center)[SNew(STextBlock)
                                    .Text(LOCTEXT("RPosLabel", "R Position:"))
                                    .Justification(ETextJustify::Center)] +
         SHorizontalBox::Slot()
             .FillWidth(1.0f)
             .Padding(5.0f, 0.0f)
             .VAlign(VAlign_Center)[SNew(STextBlock)
                                        .Text(LOCTEXT("RActLabel", "R Action:"))
                                        .Justification(ETextJustify::Center)]];

    // 下拉菜单行
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox)
         // Left Hand Position
         + SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
               [SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&LeftHandPositionOptions)
                    .OnSelectionChanged_Lambda(
                        [this](TSharedPtr<FString> S, ESelectInfo::Type) {
                            if (ZhengDriftActor.IsValid() && S.IsValid()) {
                                ZhengDriftActor->Modify();
                                if (*S == TEXT("FAR"))
                                    ZhengDriftActor->CurrentLeftHandPosition =
                                        EZhengDriftHandPosition::FAR;
                                else if (*S == TEXT("NEAR"))
                                    ZhengDriftActor->CurrentLeftHandPosition =
                                        EZhengDriftHandPosition::NEAR;
                                else
                                    ZhengDriftActor->CurrentLeftHandPosition =
                                        EZhengDriftHandPosition::MIDDLE;
                            }
                        })
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                        return SNew(STextBlock).Text(FText::FromString(*Item));
                    })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                        if (!ZhengDriftActor.IsValid())
                            return FText::FromString(TEXT("MIDDLE"));
                        switch (ZhengDriftActor->CurrentLeftHandPosition) {
                            case EZhengDriftHandPosition::FAR:
                                return FText::FromString(TEXT("FAR"));
                            case EZhengDriftHandPosition::NEAR:
                                return FText::FromString(TEXT("NEAR"));
                            default:
                                return FText::FromString(TEXT("MIDDLE"));
                        }
                    })]]
         // Left Hand Action
         + SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
               [SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&LeftHandActionOptions)
                    .OnSelectionChanged_Lambda(
                        [this](TSharedPtr<FString> S, ESelectInfo::Type) {
                            if (ZhengDriftActor.IsValid() && S.IsValid()) {
                                ZhengDriftActor->Modify();
                                ZhengDriftActor->CurrentLeftHandAction =
                                    (*S == TEXT("PRESS"))
                                        ? EZhengDriftLeftHandAction::PRESS
                                        : EZhengDriftLeftHandAction::NORMAL;
                            }
                        })
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                        return SNew(STextBlock).Text(FText::FromString(*Item));
                    })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                        if (!ZhengDriftActor.IsValid())
                            return FText::FromString(TEXT("NORMAL"));
                        return FText::FromString(
                            ZhengDriftActor->CurrentLeftHandAction ==
                                    EZhengDriftLeftHandAction::PRESS
                                ? TEXT("PRESS")
                                : TEXT("NORMAL"));
                    })]]
         // Right Hand Position
         + SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
               [SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&RightHandPositionOptions)
                    .OnSelectionChanged_Lambda(
                        [this](TSharedPtr<FString> S, ESelectInfo::Type) {
                            if (ZhengDriftActor.IsValid() && S.IsValid()) {
                                ZhengDriftActor->Modify();
                                if (*S == TEXT("FAR"))
                                    ZhengDriftActor->CurrentRightHandPosition =
                                        EZhengDriftHandPosition::FAR;
                                else if (*S == TEXT("NEAR"))
                                    ZhengDriftActor->CurrentRightHandPosition =
                                        EZhengDriftHandPosition::NEAR;
                                else
                                    ZhengDriftActor->CurrentRightHandPosition =
                                        EZhengDriftHandPosition::MIDDLE;
                            }
                        })
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                        return SNew(STextBlock).Text(FText::FromString(*Item));
                    })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                        if (!ZhengDriftActor.IsValid())
                            return FText::FromString(TEXT("MIDDLE"));
                        switch (ZhengDriftActor->CurrentRightHandPosition) {
                            case EZhengDriftHandPosition::FAR:
                                return FText::FromString(TEXT("FAR"));
                            case EZhengDriftHandPosition::NEAR:
                                return FText::FromString(TEXT("NEAR"));
                            default:
                                return FText::FromString(TEXT("MIDDLE"));
                        }
                    })]]
         // Right Hand Action
         + SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
               [SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&RightHandActionOptions)
                    .OnSelectionChanged_Lambda(
                        [this](TSharedPtr<FString> S, ESelectInfo::Type) {
                            if (ZhengDriftActor.IsValid() && S.IsValid()) {
                                ZhengDriftActor->Modify();
                                ZhengDriftActor->CurrentRightHandAction =
                                    (*S == TEXT("TREMOLO"))
                                        ? EZhengDriftRightHandAction::TREMOLO
                                        : EZhengDriftRightHandAction::NORMAL;
                            }
                        })
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                        return SNew(STextBlock).Text(FText::FromString(*Item));
                    })[SNew(STextBlock).Text_Lambda([this]() -> FText {
                        if (!ZhengDriftActor.IsValid())
                            return FText::FromString(TEXT("NORMAL"));
                        return FText::FromString(
                            ZhengDriftActor->CurrentRightHandAction ==
                                    EZhengDriftRightHandAction::TREMOLO
                                ? TEXT("TREMOLO")
                                : TEXT("NORMAL"));
                    })]]];

    // ---- State Management ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("State Management"))];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(
             0.0f, 0.0f, 5.0f,
             0.0f)[SNew(SButton)
                       .Text(LOCTEXT("SaveLeftBtn", "Save Left"))
                       .OnClicked(this,
                                  &SZhengDriftModuleOperationsPanel::OnSaveLeft)
                       .HAlign(HAlign_Center)
                       .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(
             5.0f, 0.0f, 0.0f,
             0.0f)[SNew(SButton)
                       .Text(LOCTEXT("SaveRightBtn", "Save Right"))
                       .OnClicked(
                           this, &SZhengDriftModuleOperationsPanel::OnSaveRight)
                       .HAlign(HAlign_Center)
                       .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("LoadStateBtn", "Load State"))
                  .OnClicked(this,
                             &SZhengDriftModuleOperationsPanel::OnLoadState)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ---- Animation Generation ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 15.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Animation Generation"))];

    // ---- Animation File Path ----
    TSharedPtr<SEditableTextBox> AnimFilePathBox;
    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SHorizontalBox) +
              SHorizontalBox::Slot().FillWidth(1.0f).Padding(5.0f, 0.0f)
                  [SAssignNew(AnimFilePathBox, SEditableTextBox)
                       .Text_Lambda([this]() -> FText {
                           if (ZhengDriftActor.IsValid())
                               return FText::FromString(
                                   ZhengDriftActor->AnimationFilePath);
                           return FText::FromString(TEXT(""));
                       })
                       .OnTextCommitted_Lambda(
                           [this](const FText& T, ETextCommit::Type Commit) {
                               if ((Commit == ETextCommit::OnEnter ||
                                    Commit == ETextCommit::OnUserMovedFocus) &&
                                   ZhengDriftActor.IsValid()) {
                                   ZhengDriftActor->AnimationFilePath =
                                       T.ToString();
                                   ZhengDriftActor->Modify();
                               }
                           })] +
              SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
                  [SNew(SButton)
                       .Text(LOCTEXT("BrowseBtn", "Browse"))
                       .OnClicked_Lambda([this, AnimFilePathBox]() -> FReply {
                           if (!ZhengDriftActor.IsValid())
                               return FReply::Handled();
                           FString FilePath;
                           if (FCommonPanelUtility::BrowseForFile(
                                   TEXT(".zhengdrift"), FilePath, false)) {
                               if (AnimFilePathBox.IsValid())
                                   AnimFilePathBox->SetText(
                                       FText::FromString(FilePath));
                               ZhengDriftActor->AnimationFilePath = FilePath;
                               ZhengDriftActor->Modify();
                           }
                           return FReply::Handled();
                       })]];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenPerformerBtn",
                                "Generate Performer Animation"))
                  .OnClicked(this, &SZhengDriftModuleOperationsPanel::
                                       OnGeneratePerformerAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenInstrumentBtn",
                                "Generate Instrument Animation"))
                  .OnClicked(this, &SZhengDriftModuleOperationsPanel::
                                       OnGenerateInstrumentAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("GenAllBtn", "Generate All Animation"))
                  .OnClicked(
                      this,
                      &SZhengDriftModuleOperationsPanel::OnGenerateAllAnimation)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ---- Misc ----
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        15.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Misc"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(
                      LOCTEXT("ReregBtn", "Trigger ControlRig Re-registration"))
                  .OnClicked(this, &SZhengDriftModuleOperationsPanel::
                                       OnTriggerControlRigReregistration)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("LinearDistributeBtn", "Linear Distribute Controls"))
             .OnClicked(
                 this,
                 &SZhengDriftModuleOperationsPanel::OnLinearDistributeControls)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

// ---- Button handlers ----

FReply SZhengDriftModuleOperationsPanel::OnSaveLeft() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    UZhengDriftControlRigProcessor::SaveLeftHandState(ZhengDriftActor.Get());
    return FReply::Handled();
}

FReply SZhengDriftModuleOperationsPanel::OnSaveRight() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    UZhengDriftControlRigProcessor::SaveRightHandState(ZhengDriftActor.Get());
    return FReply::Handled();
}

FReply SZhengDriftModuleOperationsPanel::OnLoadState() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    TMap<FString, FTransform> Empty;
    UZhengDriftControlRigProcessor::LoadState(ZhengDriftActor.Get(), Empty);
    return FReply::Handled();
}

FReply SZhengDriftModuleOperationsPanel::OnGeneratePerformerAnimation() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    UZhengDriftAnimationProcessor::GeneratePerformerAnimation(
        ZhengDriftActor.Get());
    return FReply::Handled();
}

FReply SZhengDriftModuleOperationsPanel::OnGenerateInstrumentAnimation() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    UZhengDriftAnimationProcessor::GenerateInstrumentAnimation(
        ZhengDriftActor.Get());
    return FReply::Handled();
}

FReply SZhengDriftModuleOperationsPanel::OnGenerateAllAnimation() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    UZhengDriftAnimationProcessor::GenerateAllAnimation(ZhengDriftActor.Get());
    return FReply::Handled();
}

FReply SZhengDriftModuleOperationsPanel::OnTriggerControlRigReregistration() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    ZhengDriftActor->TriggerControlRigReregistration(
        TEXT("Manual trigger from UI panel"));
    return FReply::Handled();
}

FReply SZhengDriftModuleOperationsPanel::OnLinearDistributeControls() {
    if (!ZhengDriftActor.IsValid()) return FReply::Handled();
    UZhengDriftControlRigProcessor::LinearDistributeControls(
        ZhengDriftActor.Get());
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
