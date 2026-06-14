#include "MusicDollMainPanel.h"

#include "Animation/SkeletalMeshActor.h"
#include "BeatBloomUnreal.h"
#include "EngineUtils.h"
#include "FretDanceUnreal.h"
#include "HarpGlideUnreal.h"
#include "InstrumentBase.h"
#include "KeyRippleUnreal.h"
#include "Misc/Paths.h"
#include "MusicDollStyle.h"
#include "StringFlowUnreal.h"
#include "UI/BakeQueuePanel.h"
#include "UI/BeatBloomModuleMainPanel.h"
#include "UI/FretDanceModuleMainPanel.h"
#include "UI/HarpGlideModuleMainPanel.h"
#include "UI/KeyRippleModuleMainPanel.h"
#include "UI/ModuleMainPanelInterface.h"
#include "UI/StringFlowModuleMainPanel.h"
#include "UI/ZhengDriftModuleMainPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ZhengDriftUnreal.h"

#define LOCTEXT_NAMESPACE "SMusicDollMainPanel"

// ==================== SActorSelectorPanel ====================

void SActorSelectorPanel::Construct(const FArguments& InArgs) {
    SelectedActor = InArgs._SelectedActor;
    OnActorSelectedDelegate = InArgs._OnActorSelected;

    RefreshActorList();

    ChildSlot
        [SNew(SVerticalBox) +
         SVerticalBox::Slot().AutoHeight().Padding(5.0f)
             [SNew(STextBlock)
                  .Text(LOCTEXT("SelectActorLabel", "Select Instrument Actor:"))
                  .Font(FAppStyle::GetFontStyle("DetailsView.CategoryFont"))] +
         SVerticalBox::Slot().AutoHeight().Padding(5.0f)
             [SNew(SHorizontalBox) +
              SHorizontalBox::Slot().FillWidth(1.0f)
                  [SAssignNew(ActorComboBox,
                              SComboBox<TWeakObjectPtr<AInstrumentBase>>)
                       .OptionsSource(&SceneActors)
                       .OnGenerateWidget(
                           this, &SActorSelectorPanel::GenerateActorComboItem)
                       .OnSelectionChanged(
                           this,
                           &SActorSelectorPanel::OnActorComboSelectionChanged)
                           [SNew(STextBlock)
                                .Text(this, &SActorSelectorPanel::
                                                GetSelectedActorName)]] +
              SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
                  [SNew(SButton)
                       .Text(LOCTEXT("RefreshButton", "Refresh"))
                       .OnClicked(this,
                                  &SActorSelectorPanel::OnRefreshActorList)
                       .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]]];
}

void SActorSelectorPanel::RefreshActorList() {
    SceneActors.Empty();

    // 遍历场景中所有的AInstrumentBase派生类实例
    for (TActorIterator<AInstrumentBase> ActorItr(GWorld); ActorItr;
         ++ActorItr) {
        if (AInstrumentBase* Actor = *ActorItr) {
            SceneActors.Add(TWeakObjectPtr<AInstrumentBase>(Actor));
        }
    }

    if (ActorComboBox.IsValid()) {
        ActorComboBox->RefreshOptions();
    }
}

FReply SActorSelectorPanel::OnRefreshActorList() {
    RefreshActorList();
    return FReply::Handled();
}

TSharedRef<SWidget> SActorSelectorPanel::GenerateActorComboItem(
    TWeakObjectPtr<AInstrumentBase> InActor) const {
    FString ActorDisplayName;
    if (InActor.IsValid()) {
        AInstrumentBase* Actor = InActor.Get();
        if (Actor && Actor->IsValidLowLevel()) {
            // Try to get the actor label first, fall back to name
            ActorDisplayName = Actor->GetActorLabel();
            if (ActorDisplayName.IsEmpty()) {
                ActorDisplayName = Actor->GetName();
            }
        } else {
            ActorDisplayName = TEXT("None");
        }
    } else {
        ActorDisplayName = TEXT("None");
    }

    return SNew(STextBlock).Text(FText::FromString(ActorDisplayName));
}

void SActorSelectorPanel::OnActorComboSelectionChanged(
    TWeakObjectPtr<AInstrumentBase> InActor, ESelectInfo::Type SelectInfo) {
    if (InActor.IsValid()) {
        SelectedActor = InActor;
        OnActorSelectedDelegate.ExecuteIfBound();
    }
}

FText SActorSelectorPanel::GetSelectedActorName() const {
    if (SelectedActor.IsValid()) {
        AInstrumentBase* Actor = SelectedActor.Get();
        if (Actor) {
            // Display label if available, otherwise name
            FString DisplayName = Actor->GetActorLabel();
            if (DisplayName.IsEmpty()) {
                DisplayName = Actor->GetName();
            }
            return FText::FromString(DisplayName);
        }
    }
    return LOCTEXT("NoActorSelected", "No Actor Selected");
}

AInstrumentBase* SActorSelectorPanel::GetSelectedActor() const {
    return SelectedActor.Get();
}

// ==================== SMusicDollMainPanel ====================

void SMusicDollMainPanel::Construct(const FArguments& InArgs) {
    // Create bake queue panel
    SAssignNew(BakeQueuePanel, SBakeQueuePanel);

    ChildSlot
        [SNew(SVerticalBox)
         // Top row: Icon + Actor Selector
         +
         SVerticalBox::Slot().AutoHeight().Padding(5.0f)
             [SNew(SHorizontalBox) +
              SHorizontalBox::Slot()
                  .AutoWidth()
                  .Padding(10.0f, 0.0f)
                  .VAlign(VAlign_Center)
                      [SNew(SBox).WidthOverride(32.0f).HeightOverride(
                          32.0f)[SNew(SImage).Image(
                          this, &SMusicDollMainPanel::GetSelectedActorIcon)]] +
              SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                  5.0f)[SAssignNew(ActorSelectorPanel, SActorSelectorPanel)
                            .OnActorSelected_Lambda([this]() {
                                AInstrumentBase* SelectedActor =
                                    ActorSelectorPanel->GetSelectedActor();
                                OnActorSelected(SelectedActor);
                            })]]
         // Properties panel
         + SVerticalBox::Slot().FillHeight(0.8f).Padding(
               5.0f)[SAssignNew(PropertiesPanelWidget, SVerticalBox)]
         // Bake queue panel
         + SVerticalBox::Slot().AutoHeight().Padding(
               5.0f)[BakeQueuePanel.ToSharedRef()]];
}

SMusicDollMainPanel::~SMusicDollMainPanel() {
    // 完全清理所有子面板和widget
    if (PropertiesPanelWidget.IsValid()) {
        PropertiesPanelWidget->ClearChildren();
        PropertiesPanelWidget.Reset();
    }

    CurrentModulePanel.Reset();
    ActorSelectorPanel.Reset();
    SelectedInstrumentActor.Reset();
}

void SMusicDollMainPanel::Tick(const FGeometry& AllottedGeometry,
                               const double InCurrentTime,
                               const float InDeltaTime) {
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    // Could add periodic refresh logic here if needed
}

FText SMusicDollMainPanel::GetSelectedActorTypeLabel() const {
    if (!SelectedInstrumentActor.IsValid()) {
        return FText::FromString(TEXT(""));
    }

    AInstrumentBase* Actor = SelectedInstrumentActor.Get();
    if (!Actor) {
        return FText::FromString(TEXT(""));
    }

    // 检查是否为 KeyRipple
    if (Actor->IsA<AKeyRippleUnreal>()) {
        return FText::FromString(TEXT("KeyRipple"));
    }

    // 检查是否为 StringFlow
    if (Actor->IsA<AStringFlowUnreal>()) {
        return FText::FromString(TEXT("StringFlow"));
    }

    // 检查是否为 BeatBloom
    if (Actor->IsA<ABeatBloomUnreal>()) {
        return FText::FromString(TEXT("BeatBloom"));
    }

    // 默认情况
    return FText::FromString(TEXT(""));
}

const FSlateBrush* SMusicDollMainPanel::GetSelectedActorIcon() const {
    // 如果没有选中任何actor，显示MusicDoll的默认图标
    if (!SelectedInstrumentActor.IsValid()) {
        return FMusicDollStyle::Get()->GetBrush("MusicDoll.Icon");
    }

    AInstrumentBase* Actor = SelectedInstrumentActor.Get();
    if (!Actor) {
        return FMusicDollStyle::Get()->GetBrush("MusicDoll.Icon");
    }

    // 检查是否为 KeyRipple
    if (Actor->IsA<AKeyRippleUnreal>()) {
        return FMusicDollStyle::Get()->GetBrush("MusicDoll.KeyRipple.Icon");
    }

    // 检查是否为 StringFlow
    if (Actor->IsA<AStringFlowUnreal>()) {
        return FMusicDollStyle::Get()->GetBrush("MusicDoll.StringFlow.Icon");
    }

    // 检查是否为 BeatBloom
    if (Actor->IsA<ABeatBloomUnreal>()) {
        return FMusicDollStyle::Get()->GetBrush("MusicDoll.BeatBloom.Icon");
    }

    // 检查是否为 FretDance
    if (Actor->IsA<AFretDanceUnreal>()) {
        return FMusicDollStyle::Get()->GetBrush("MusicDoll.FretDance.Icon");
    }

    // 检查是否为 ZhengDrift
    if (Actor->IsA<AZhengDriftUnreal>()) {
        return FMusicDollStyle::Get()->GetBrush("MusicDoll.ZhengDrift.Icon");
    }

    // 默认情况
    return FMusicDollStyle::Get()->GetBrush("MusicDoll.Icon");
}

void SMusicDollMainPanel::OnActorSelected(AInstrumentBase* InActor) {
    UE_LOG(
        LogTemp, Warning,
        TEXT("SMusicDollMainPanel::OnActorSelected() - Switching to actor: %s"),
        InActor ? *InActor->GetName() : TEXT("nullptr"));

    SelectedInstrumentActor = InActor;

    if (!PropertiesPanelWidget.IsValid()) {
        return;
    }

    // 完全清理旧的面板
    PropertiesPanelWidget->ClearChildren();

    // 显式重置模块面板指针，确保触发析构
    if (CurrentModulePanel.IsValid()) {
        UE_LOG(LogTemp, Warning,
               TEXT("SMusicDollMainPanel::OnActorSelected() - Resetting "
                    "CurrentModulePanel"));
        CurrentModulePanel.Reset();
    }

    if (!InActor) {
        return;
    }

    // 检查选中的对象是否为AKeyRippleUnreal类型
    AKeyRippleUnreal* KeyRippleActor = Cast<AKeyRippleUnreal>(InActor);
    if (KeyRippleActor) {
        // Create KeyRipple module main panel using new architecture
        TSharedPtr<SModuleMainPanelBase> ModulePanel =
            SNew(SKeyRippleModuleMainPanel);
        if (ModulePanel.IsValid() &&
            ModulePanel->CanHandleActor(KeyRippleActor)) {
            ModulePanel->SetActor(KeyRippleActor);
            CurrentModulePanel =
                StaticCastSharedPtr<IModuleMainPanel>(ModulePanel);

            if (PropertiesPanelWidget.IsValid()) {
                PropertiesPanelWidget->AddSlot().FillHeight(
                    1.0f)[ModulePanel->GetWidget().ToSharedRef()];
            }
        }
        return;
    }

    // 检查选中的对象是否为 AStringFlowUnreal 类型
    AStringFlowUnreal* StringFlowActor = Cast<AStringFlowUnreal>(InActor);
    if (StringFlowActor) {
        // Create StringFlow module main panel using new architecture
        TSharedPtr<SModuleMainPanelBase> ModulePanel =
            SNew(SStringFlowModuleMainPanel);
        if (ModulePanel.IsValid() &&
            ModulePanel->CanHandleActor(StringFlowActor)) {
            ModulePanel->SetActor(StringFlowActor);
            CurrentModulePanel =
                StaticCastSharedPtr<IModuleMainPanel>(ModulePanel);

            if (PropertiesPanelWidget.IsValid()) {
                PropertiesPanelWidget->AddSlot().FillHeight(
                    1.0f)[ModulePanel->GetWidget().ToSharedRef()];
            }
        }
        return;
    }

    // 检查选中的对象是否为 AFretDanceUnreal 类型
    AFretDanceUnreal* FretDanceActor = Cast<AFretDanceUnreal>(InActor);
    if (FretDanceActor) {
        TSharedPtr<SModuleMainPanelBase> ModulePanel =
            SNew(SFretDanceModuleMainPanel);
        if (ModulePanel.IsValid() &&
            ModulePanel->CanHandleActor(FretDanceActor)) {
            ModulePanel->SetActor(FretDanceActor);
            CurrentModulePanel =
                StaticCastSharedPtr<IModuleMainPanel>(ModulePanel);

            if (PropertiesPanelWidget.IsValid()) {
                PropertiesPanelWidget->AddSlot().FillHeight(
                    1.0f)[ModulePanel->GetWidget().ToSharedRef()];
            }
        }
        return;
    }

    // 检查选中的对象是否为 ABeatBloomUnreal 类型
    ABeatBloomUnreal* BeatBloomActor = Cast<ABeatBloomUnreal>(InActor);
    if (BeatBloomActor) {
        // Create BeatBloom module main panel
        TSharedPtr<SModuleMainPanelBase> ModulePanel =
            SNew(SBeatBloomModuleMainPanel);
        if (ModulePanel.IsValid() &&
            ModulePanel->CanHandleActor(BeatBloomActor)) {
            ModulePanel->SetActor(BeatBloomActor);
            CurrentModulePanel =
                StaticCastSharedPtr<IModuleMainPanel>(ModulePanel);

            if (PropertiesPanelWidget.IsValid()) {
                PropertiesPanelWidget->AddSlot().FillHeight(
                    1.0f)[ModulePanel->GetWidget().ToSharedRef()];
            }
        }
        return;
    }

    // 检查选中的对象是否为 AHarpGlideUnreal 类型
    AHarpGlideUnreal* HarpGlideActor = Cast<AHarpGlideUnreal>(InActor);
    if (HarpGlideActor) {
        TSharedPtr<SModuleMainPanelBase> ModulePanel =
            SNew(SHarpGlideModuleMainPanel);
        if (ModulePanel.IsValid() &&
            ModulePanel->CanHandleActor(HarpGlideActor)) {
            ModulePanel->SetActor(HarpGlideActor);
            CurrentModulePanel =
                StaticCastSharedPtr<IModuleMainPanel>(ModulePanel);

            if (PropertiesPanelWidget.IsValid()) {
                PropertiesPanelWidget->AddSlot().FillHeight(
                    1.0f)[ModulePanel->GetWidget().ToSharedRef()];
            }
        }
        return;
    }

    // 检查选中的对象是否为 AZhengDriftUnreal 类型
    AZhengDriftUnreal* ZhengDriftActor = Cast<AZhengDriftUnreal>(InActor);
    if (ZhengDriftActor) {
        TSharedPtr<SModuleMainPanelBase> ModulePanel =
            SNew(SZhengDriftModuleMainPanel);
        if (ModulePanel.IsValid() &&
            ModulePanel->CanHandleActor(ZhengDriftActor)) {
            ModulePanel->SetActor(ZhengDriftActor);
            CurrentModulePanel =
                StaticCastSharedPtr<IModuleMainPanel>(ModulePanel);

            if (PropertiesPanelWidget.IsValid()) {
                PropertiesPanelWidget->AddSlot().FillHeight(
                    1.0f)[ModulePanel->GetWidget().ToSharedRef()];
            }
        }
        return;
    }
}
#undef LOCTEXT_NAMESPACE