#include "MusicDollMainPanel.h"

#include "Animation/SkeletalMeshActor.h"
#include "EngineUtils.h"
#include "KeyRippleDisplayPanelInterface.h"
#include "KeyRipplePropertiesPanel.h"
#include "KeyRippleUnreal.h"
#include "InstrumentBase.h" // 包含InstrumentBase头文件
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

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
                  .Text(
                      LOCTEXT("SelectActorLabel", "Select Instrument Actor:"))
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
            if (!Actor->IsHidden()) {
                SceneActors.Add(TWeakObjectPtr<AInstrumentBase>(Actor));
            }
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
    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f)[SNew(STextBlock)
                            .Text(this, &SMusicDollMainPanel::GetPanelTitle)
                            .Font(FAppStyle::GetFontStyle(
                                "DetailsView.CategoryFont"))] +
              SVerticalBox::Slot().AutoHeight().Padding(
                  5.0f)[SAssignNew(ActorSelectorPanel, SActorSelectorPanel)
                            .OnActorSelected_Lambda([this]() {
                                AInstrumentBase* SelectedActor =
                                    ActorSelectorPanel->GetSelectedActor();
                                OnActorSelected(SelectedActor);
                            })] +
              SVerticalBox::Slot().FillHeight(1.0f).Padding(
                  5.0f)[SAssignNew(PropertiesPanelWidget, SVerticalBox)]];
}

SMusicDollMainPanel::~SMusicDollMainPanel() {
    // 完全清理所有子面板和widget
    if (PropertiesPanelWidget.IsValid()) {
        PropertiesPanelWidget->ClearChildren();
        PropertiesPanelWidget.Reset();
    }

    CurrentPropertiesPanel.Reset();
    ActorSelectorPanel.Reset();
    SelectedInstrumentActor.Reset();
}

void SMusicDollMainPanel::Tick(const FGeometry& AllottedGeometry,
                               const double InCurrentTime,
                               const float InDeltaTime) {
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    // Could add periodic refresh logic here if needed
}

FText SMusicDollMainPanel::GetPanelTitle() const {
    return FText::FromString(TEXT("Music Doll Panel"));
}

void SMusicDollMainPanel::OnActorSelected(AInstrumentBase* InActor) {
    SelectedInstrumentActor = InActor;

    if (!PropertiesPanelWidget.IsValid()) {
        return;
    }

    // 完全清理旧的面板
    PropertiesPanelWidget->ClearChildren();
    CurrentPropertiesPanel.Reset();  // 重要：完全释放旧的面板指针

    if (!InActor) {
        return;
    }

    // 检查选中的对象是否为AKeyRippleUnreal类型
    AKeyRippleUnreal* KeyRippleActor = Cast<AKeyRippleUnreal>(InActor);
    
    if (KeyRippleActor) {
        // Create appropriate properties panel for this actor
        // Try KeyRipplePropertiesPanel first
        TSharedPtr<SKeyRipplePropertiesPanel> Panel =
            SNew(SKeyRipplePropertiesPanel);
        if (Panel.IsValid() && Panel->CanHandleActor(KeyRippleActor)) {
            CurrentPropertiesPanel = Panel;
            Panel->SetActor(KeyRippleActor);

            if (PropertiesPanelWidget.IsValid()) {
                PropertiesPanelWidget->AddSlot().FillHeight(
                    1.0f)[Panel.ToSharedRef()];
            }
        }
    }
}