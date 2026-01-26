#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class ASkeletalMeshActor;
class AActor;
class AInstrumentBase;
class IKeyRippleDisplayPanel;

// Forward declarations for subpanels
class SActorSelectorPanel;

/**
 * Main panel for Music Doll UI
 * Displays actor selector and uses generic properties panel from selected actor type
 */
class MUSICDOLLUI_API SMusicDollMainPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMusicDollMainPanel)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual ~SMusicDollMainPanel();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FText GetPanelTitle() const;
	
	// Called when user selects an InstrumentBase actor
	void OnActorSelected(AInstrumentBase* InActor);

	// References to subpanels
	TSharedPtr<SActorSelectorPanel> ActorSelectorPanel;
	TSharedPtr<SVerticalBox> PropertiesPanelWidget;

	// Current selected actor - Use TWeakObjectPtr since UObjects are GC-managed
	TWeakObjectPtr<AInstrumentBase> SelectedInstrumentActor;
	TSharedPtr<IKeyRippleDisplayPanel> CurrentPropertiesPanel;
};

/**
 * Subpanel for selecting InstrumentBase actors from scene
 */
class MUSICDOLLUI_API SActorSelectorPanel : public SCompoundWidget
{
public:
	DECLARE_DELEGATE(FOnActorSelected);

	SLATE_BEGIN_ARGS(SActorSelectorPanel)
	{
	}
	SLATE_ARGUMENT(TWeakObjectPtr<AInstrumentBase>, SelectedActor)
	SLATE_EVENT(FOnActorSelected, OnActorSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	AInstrumentBase* GetSelectedActor() const;

private:
	void RefreshActorList();
	FReply OnRefreshActorList();
	
	TSharedRef<SWidget> GenerateActorComboItem(TWeakObjectPtr<AInstrumentBase> InActor) const;
	void OnActorComboSelectionChanged(TWeakObjectPtr<AInstrumentBase> InActor, ESelectInfo::Type SelectInfo);

	FText GetSelectedActorName() const;

	// Actors list - Use TWeakObjectPtr for UObject references
	// UObjects are managed by the garbage collector, not by TSharedPtr
	TArray<TWeakObjectPtr<AInstrumentBase>> SceneActors;
	TSharedPtr<SComboBox<TWeakObjectPtr<AInstrumentBase>>> ActorComboBox;

	// Selected actor - Use TWeakObjectPtr to avoid interfering with GC
	TWeakObjectPtr<AInstrumentBase> SelectedActor;

	// Delegate
	FOnActorSelected OnActorSelectedDelegate;
};