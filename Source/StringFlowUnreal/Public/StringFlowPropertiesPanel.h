#pragma once

#include "CoreMinimal.h"
#include "CommonPropertiesPanelBase.h"
#include "StringFlowDisplayPanelInterface.h"

class AStringFlowUnreal;

/**
 * Properties panel for StringFlowUnreal actor
 * Displays and edits StringFlow configuration properties
 */
class STRINGFLOWUNREAL_API SStringFlowPropertiesPanel : public SCommonPropertiesPanelBase, public IStringFlowDisplayPanel
{
public:
	SLATE_BEGIN_ARGS(SStringFlowPropertiesPanel)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// IStringFlowDisplayPanel interface
	virtual TSharedPtr<SWidget> GetWidget() override;
	virtual void SetActor(AActor* InActor) override;
	virtual bool CanHandleActor(const AActor* InActor) const override;

	TSharedPtr<class SStringFlowOperationsPanel> GetOperationsPanel() const { return OperationsPanel; }

private:
	virtual void RefreshPropertyList() override;

	// Property change handlers
	void OnNumericPropertyChanged(const FString& PropertyPath, int32 NewValue);
	void OnFilePathChanged(const FString& PropertyPath, const FString& NewFilePath);

	// Initialization operation handlers
	FReply OnCheckObjectsStatus();
	FReply OnSetupAllObjects();
	FReply OnExportRecorderInfo();
	FReply OnImportRecorderInfo();

	// Currently displayed actor
	TWeakObjectPtr<AStringFlowUnreal> StringFlowActor;

	// Operations panel
	TSharedPtr<class SStringFlowOperationsPanel> OperationsPanel;
};
