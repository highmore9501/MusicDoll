#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnFilePathChanged, const FString&);

/**
 * Standardized file path editor widget
 * Provides consistent file browsing and path editing functionality
 */
class MUSICDOLLCOMMON_API SFilePathEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFilePathEditor)
		: _FilePath(TEXT(""))
		, _FileDescription(TEXT("File"))
		, _FileFilter(TEXT("All Files (*.*)|*.*"))
		, _AllowEditing(true)
	{}
		SLATE_ATTRIBUTE(FString, FilePath)
		SLATE_ATTRIBUTE(FString, FileDescription)
		SLATE_ATTRIBUTE(FString, FileFilter)
		SLATE_ATTRIBUTE(bool, AllowEditing)
		SLATE_EVENT(FOnFilePathChanged, OnFilePathChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// Current file path
	TAttribute<FString> FilePathAttr;
	
	// File description for dialog title
	TAttribute<FString> FileDescriptionAttr;
	
	// File filter for dialog
	TAttribute<FString> FileFilterAttr;
	
	// Whether editing is allowed
	TAttribute<bool> AllowEditingAttr;
	
	// Callback when path changes
	FOnFilePathChanged OnFilePathChangedDelegate;

	// UI components
	TSharedPtr<class SEditableTextBox> PathTextBox;

	// Button handler
	FReply OnBrowseButtonClicked();

	// Text committed handler
	void OnPathTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
};