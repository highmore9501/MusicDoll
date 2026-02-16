#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"

class AActor;

/**
 * Utility class for common properties panel operations
 * Provides reusable widget creation and file dialog handling
 */
class COMMON_API FCommonPropertiesPanelUtility
{
public:
	/**
	 * Create a numeric property row widget
	 * @param PropertyName Display name for the property
	 * @param Value Current numeric value
	 * @param PropertyPath Internal property identifier
	 * @param OnValueChanged Callback when value changes
	 */
	static TSharedRef<SWidget> CreateNumericPropertyRow(
		const FString& PropertyName,
		int32 Value,
		const FString& PropertyPath,
		FSimpleDelegate OnValueChanged);

	/**
	 * Create a string property row widget
	 * @param PropertyName Display name for the property
	 * @param Value Current string value
	 * @param PropertyPath Internal property identifier
	 * @param OnValueChanged Callback when value changes
	 */
	static TSharedRef<SWidget> CreateStringPropertyRow(
		const FString& PropertyName,
		const FString& Value,
		const FString& PropertyPath,
		FSimpleDelegate OnValueChanged);

	/**
	 * Create a file path property row widget with browse button
	 * @param PropertyName Display name for the property
	 * @param FilePath Current file path
	 * @param PropertyPath Internal property identifier
	 * @param FileExtension File extension filter (e.g., ".txt")
	 * @param OnPathChanged Callback when path changes
	 * @param bAllowCreateNew Whether to allow creating new files
	 */
	static TSharedRef<SWidget> CreateFilePathPropertyRow(
		const FString& PropertyName,
		const FString& FilePath,
		const FString& PropertyPath,
		const FString& FileExtension,
		FSimpleDelegate OnPathChanged,
		bool bAllowCreateNew = false);

	/**
	 * Create a vector3 property row widget
	 * @param PropertyName Display name for the property
	 * @param Value Current vector value
	 * @param PropertyPath Internal property identifier
	 * @param OnComponentChanged Callback when component changes, receives (ComponentIndex, NewValue)
	 */
	static TSharedRef<SWidget> CreateVector3PropertyRow(
		const FString& PropertyName,
		const FVector& Value,
		const FString& PropertyPath,
		FSimpleDelegate OnComponentChanged);

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
	static TSharedRef<SWidget> CreateActionButton(
		const FText& ButtonText,
		FSimpleDelegate OnClicked);

	/**
	 * Browse for a file and return the selected path
	 * @param FileExtension File extension filter (e.g., ".txt")
	 * @param OutFilePath Output file path if successful
	 * @param bAllowCreateNew Whether to allow creating new files
	 * @return True if file was selected, false otherwise
	 */
	static bool BrowseForFile(
		const FString& FileExtension,
		FString& OutFilePath,
		bool bAllowCreateNew = false);

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
		const FText& PropertiesLabel,
		const FText& OperationsLabel,
		FSimpleDelegate OnPropertiesClicked,
		FSimpleDelegate OnOperationsClicked,
		bool bIsPropertiesActive);
};
