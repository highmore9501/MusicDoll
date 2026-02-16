#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "StringFlowDisplayPanelInterface.h"

class AStringFlowUnreal;

/**
 * Operations panel for StringFlowUnreal actor
 * Provides buttons to execute various StringFlow animation operations
 */
class STRINGFLOWUNREAL_API SStringFlowOperationsPanel : public SCompoundWidget, public IStringFlowDisplayPanel
{
public:
	SLATE_BEGIN_ARGS(SStringFlowOperationsPanel)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// IStringFlowDisplayPanel interface
	virtual TSharedPtr<SWidget> GetWidget() override;
	virtual void SetActor(AActor* InActor) override;
	virtual bool CanHandleActor(const AActor* InActor) const override;

private:
	// File picker for StringFlow data
	FReply OnStringFlowFilePathBrowse();
	bool BrowseForFile(const FString& FileExtension, FString& OutFilePath);

	// State management handlers
	FReply OnSaveState();
	FReply OnSaveLeft();
	FReply OnSaveRight();
	FReply OnLoadState();

	// Animation generation handlers
	FReply OnGeneratePerformerAnimation();
	FReply OnGenerateInstrumentAnimation();
	FReply OnGenerateAllAnimation();
	FReply OnClearStringControlRigKeyframes();
	FReply OnInitializeStringInstrument();

	// Status display
	FText GetStatusText() const;

	// Currently displayed actor
	TWeakObjectPtr<AStringFlowUnreal> StringFlowActor;

	// Operations container
	TSharedPtr<SVerticalBox> OperationsContainer;

	// Status text block
	TSharedPtr<STextBlock> StatusTextBlock;

	// Last operation status message
	FString LastStatusMessage;

	// Persistent option lists for combo boxes
	TArray<TSharedPtr<FString>> LeftHandPositionOptions;
	TArray<TSharedPtr<FString>> RightHandPositionOptions;
	TArray<TSharedPtr<FString>> LeftHandFretIndexOptions;
	TArray<TSharedPtr<FString>> RightHandStringIndexOptions;

	// Getters return pointer to persistent arrays
	TArray<TSharedPtr<FString>>* GetLeftHandPositionOptions();
	TArray<TSharedPtr<FString>>* GetRightHandPositionOptions();
	TArray<TSharedPtr<FString>>* GetLeftHandFretIndexOptions();
	TArray<TSharedPtr<FString>>* GetRightHandStringIndexOptions();
};
