#pragma once

#include "CoreMinimal.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"

class AActor;

/**
 * Utility class for common panel operations
 * Provides reusable widget creation and file dialog handling for all panel
 * types
 */
class MUSICDOLLCOMMON_API FCommonPanelUtility {
   public:
    /**
     * Create a numeric property row widget
     * @param PropertyName Display name for the property
     * @param Value Current numeric value
     * @param PropertyPath Internal property identifier
     * @param OnValueChanged Callback when value changes, receives (PropertyPath, NewValue)
     */
    static TSharedRef<SWidget> CreateNumericPropertyRow(
        const FString& PropertyName, int32 Value, const FString& PropertyPath,
        TFunction<void(const FString&, int32)> OnValueChanged);

    /**
     * Create a string property row widget
     * @param PropertyName Display name for the property
     * @param Value Current string value
     * @param PropertyPath Internal property identifier
     * @param OnValueChanged Callback when value changes
     */
    static TSharedRef<SWidget> CreateStringPropertyRow(
        const FString& PropertyName, const FString& Value,
        const FString& PropertyPath, FSimpleDelegate OnValueChanged);

    /**
     * Create a file path property row widget with browse button - Enhanced
     * version This version provides a callback that receives the new file path
     * value allowing the caller to directly update their properties with the
     * selected path
     *
     * Usage example:
     * Container->AddSlot().AutoHeight().Padding(5.0f)
     *     [FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
     *         TEXT("IO File Path"), FretDance->IOFilePath, TEXT("IOFilePath"),
     *         TEXT(".json"),
     *         [this](const FString& NewPath) {
     *             if (FretDanceActor.IsValid()) {
     *                 FretDanceActor->Modify();
     *                 FretDanceActor->IOFilePath = NewPath;
     *             }
     *         },
     *         true)];
     *
     * @param PropertyName Display name for the property
     * @param FilePath Current file path
     * @param PropertyPath Internal property identifier
     * @param FileExtension File extension filter (e.g., ".json")
     * @param OnPathUpdated Callback that receives the new file path (const
     * FString& NewPath)
     * @param bAllowCreateNew Whether to allow creating new files
     */
    static TSharedRef<SWidget> CreateFilePathPropertyRowWithCallback(
        const FString& PropertyName, const FString& FilePath,
        const FString& PropertyPath, const FString& FileExtension,
        TFunction<void(const FString&)> OnPathUpdated,
        bool bAllowCreateNew = false);

    /**
     * Create a vector3 property row widget
     * @param PropertyName Display name for the property
     * @param Value Current vector value
     * @param PropertyPath Internal property identifier
     * @param OnComponentChanged Callback when component changes, receives
     * (ComponentIndex, NewValue)
     */
    static TSharedRef<SWidget> CreateVector3PropertyRow(
        const FString& PropertyName, const FVector& Value,
        const FString& PropertyPath, FSimpleDelegate OnComponentChanged);

    /**
     * Create a section header widget
     * @param SectionTitle Title of the section
     */
    static TSharedRef<SWidget> CreateSectionHeader(const FString& SectionTitle);

    /**
     * Create a simple button widget
     * @param ButtonText Display text on button
     * @param OnClicked Callback when button is clicked
     */
    static TSharedRef<SWidget> CreateActionButton(const FText& ButtonText,
                                                  FSimpleDelegate OnClicked);

    /**
     * Browse for a file and return the selected path
     * @param FileExtension File extension filter (e.g., ".txt")
     * @param OutFilePath Output file path if successful
     * @param bAllowCreateNew Whether to allow creating new files
     * @return True if file was selected, false otherwise
     */
    static bool BrowseForFile(const FString& FileExtension,
                              FString& OutFilePath,
                              bool bAllowCreateNew = false);

    /**
     * Show a confirmation dialog before overwriting a file
     * Warns the user that the export operation will overwrite all existing data
     * @param FilePath The file path that will be overwritten
     * @return True if the user confirmed, false if cancelled
     */
    static bool ConfirmExportOverwrite(const FString& FilePath);

    /**
     * Get the color for active/inactive tab buttons
     * @param bIsActive Whether the tab is currently active
     * @return Color for the tab button text
     */
    static FLinearColor GetTabButtonTextColor(bool bIsActive);

    /**
     * Create tab button widgets in a horizontal box
     * @param PropertiesLabel Properties tab label text
     * @param OperationsLabel Operations tab label text
     * @param OnPropertiesClicked Callback for properties tab
     * @param OnOperationsClicked Callback for operations tab
     * @param bIsPropertiesActive Whether properties tab is currently active
     */
    static TSharedRef<SWidget> CreateTabButtons(
        const FText& PropertiesLabel, const FText& OperationsLabel,
        FSimpleDelegate OnPropertiesClicked,
        FSimpleDelegate OnOperationsClicked, bool bIsPropertiesActive);
};