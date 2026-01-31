#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "KeyRippleDisplayPanelInterface.h"

class AKeyRippleUnreal;

/**
 * Properties panel for KeyRippleUnreal actor
 * Displays and edits KeyRipple configuration properties
 */
class KEYRIPPLEUNREAL_API SKeyRipplePropertiesPanel : public SCompoundWidget, public IKeyRippleDisplayPanel
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

private:
	void RefreshPropertyList();

	// Create editable property row widgets
	TSharedRef<SWidget> CreateNumericPropertyRow(const FString& PropertyName, int32 Value, const FString& PropertyPath);
	TSharedRef<SWidget> CreateStringPropertyRow(const FString& PropertyName, const FString& Value, const FString& PropertyPath);
	TSharedRef<SWidget> CreateEnumPropertyRow(const FString& PropertyName, uint8 Value, const FString& EnumTypeName, const FString& PropertyPath);
	TSharedRef<SWidget> CreateFilePathPropertyRow(const FString& PropertyName, const FString& FilePath, const FString& PropertyPath, const FString& FileExtension);
	TSharedRef<SWidget> CreateVector3PropertyRow(const FString& PropertyName, const FVector& Value, const FString& PropertyPath);

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

	// File picker helper
	bool BrowseForFile(const FString& FileExtension, FString& OutFilePath);

	// Tab switching
	FReply OnPropertiesTabClicked();
	FReply OnOperationsTabClicked();
	FText GetPropertiesTabLabel() const;
	FText GetOperationsTabLabel() const;
	FSlateColor GetTabButtonColor(bool bIsActive) const;
	FLinearColor GetTabButtonTextColor(bool bIsActive) const;

	// Currently displayed actor
	TWeakObjectPtr<AKeyRippleUnreal> KeyRippleActor;

	// Properties container
	TSharedPtr<SVerticalBox> PropertiesContainer;

	// Tab management
	TSharedPtr<SVerticalBox> ContentContainer;
	enum class EActiveTab
	{
		Properties,
		Operations
	};
	EActiveTab ActiveTab;

	// Operations panel
	TSharedPtr<class SKeyRippleOperationsPanel> OperationsPanel;

public:
	/**
	 * Get the operations panel (used for sharing actor to sub-panels)
	 */
	TSharedPtr<class SKeyRippleOperationsPanel> GetOperationsPanel() const { return OperationsPanel; }
};