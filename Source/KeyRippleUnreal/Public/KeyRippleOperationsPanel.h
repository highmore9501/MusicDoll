#pragma once

#include "CoreMinimal.h"
#include "KeyRippleDisplayPanelInterface.h"
#include "Widgets/SCompoundWidget.h"

class AKeyRippleUnreal;

/**
 * Operations panel for KeyRippleUnreal actor
 * Provides buttons to execute various KeyRipple operations
 */
class KEYRIPPLEUNREAL_API SKeyRippleOperationsPanel
    : public SCompoundWidget,
      public IKeyRippleDisplayPanel {
   public:
    SLATE_BEGIN_ARGS(SKeyRippleOperationsPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // IKeyRippleDisplayPanel interface
    virtual TSharedPtr<SWidget> GetWidget() override;
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;

   private:
    // File picker for KeyRipple data
    FReply OnKeyRippleFilePathBrowse();
    bool BrowseForFile(const FString& FileExtension, FString& OutFilePath);

    // Operation button handlers
    FReply OnSaveState();
    FReply OnLoadState();
    FReply OnClearControlRigKeyframes();
    FReply OnGeneratePerformerAnimation();
    FReply OnGeneratePianoKeyAnimation();
    FReply OnGenerateAllAnimation();
    FReply OnInitPiano();

    // Create enum property row (copied from properties panel)
    TSharedRef<SWidget> CreateEnumPropertyRow(const FString& PropertyName,
                                              uint8 Value,
                                              const FString& EnumTypeName,
                                              const FString& PropertyPath);

    // Status display
    FText GetStatusText() const;

    // Currently displayed actor
    TWeakObjectPtr<AKeyRippleUnreal> KeyRippleActor;

    // Operations container
    TSharedPtr<SVerticalBox> OperationsContainer;

    // Status text block
    TSharedPtr<STextBlock> StatusTextBlock;

    // Last operation status message
    FString LastStatusMessage;

    // Persistent option lists for combo boxes
    TArray<TSharedPtr<FString>> KeyTypeOptions;
    TArray<TSharedPtr<FString>> PositionTypeOptions;

    // Getters return pointer to persistent arrays
    TArray<TSharedPtr<FString>>* GetKeyTypeOptions();
    TArray<TSharedPtr<FString>>* GetPositionTypeOptions();
};