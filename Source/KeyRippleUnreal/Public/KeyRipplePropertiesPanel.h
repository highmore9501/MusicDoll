#pragma once

#include "CoreMinimal.h"
#include "CommonPropertiesPanelBase.h"
#include "KeyRippleDisplayPanelInterface.h"

class AKeyRippleUnreal;
class SBoneControlMappingEditPanel;

/**
 * Properties panel for KeyRippleUnreal actor
 * Displays and edits KeyRipple configuration properties
 */
class KEYRIPPLEUNREAL_API SKeyRipplePropertiesPanel : public SCommonPropertiesPanelBase, public IKeyRippleDisplayPanel
{
public:
	SLATE_BEGIN_ARGS(SKeyRipplePropertiesPanel)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// IKeyRippleDisplayPanel interface
	virtual TSharedPtr<SWidget> GetWidget() override;
	virtual void SetActor(AActor* InActor) override;
	virtual bool CanHandleActor(const AActor* InActor) const override;

	TSharedPtr<class SKeyRippleOperationsPanel> GetOperationsPanel() const { return OperationsPanel; }

private:
	virtual void RefreshPropertyList() override;

	// Create editable property row widgets
	TSharedRef<SWidget> CreateEnumPropertyRow(
		const FString& PropertyName,
		uint8 Value,
		const FString& EnumTypeName,
		const FString& PropertyPath);

	// Property change handlers
	void OnNumericPropertyChanged(const FString& PropertyPath, int32 NewValue);
	void OnStringPropertyChanged(const FString& PropertyPath, const FText& NewValue);
	void OnEnumPropertyChanged(const FString& PropertyPath, uint8 NewValue);
	void OnFilePathChanged(const FString& PropertyPath, const FString& NewFilePath);
	void OnVector3PropertyChanged(const FString& PropertyPath, int32 ComponentIndex, float NewValue);

	// Initialization operation handlers
	FReply OnCheckObjectsStatus();
	FReply OnSetupAllObjects();
	FReply OnExportRecorderInfo();
	FReply OnImportRecorderInfo();

	// Currently displayed actor
	TWeakObjectPtr<AKeyRippleUnreal> KeyRippleActor;

	// Operations panel
	TSharedPtr<class SKeyRippleOperationsPanel> OperationsPanel;

	// Bone Control Mapping panel
	TSharedPtr<class SBoneControlMappingEditPanel> BoneControlMappingPanel;
};