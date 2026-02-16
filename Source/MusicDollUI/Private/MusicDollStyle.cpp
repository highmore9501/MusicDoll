#include "MusicDollStyle.h"

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Misc/Paths.h"

TSharedPtr<FSlateStyleSet> FMusicDollStyle::StyleSet = nullptr;

void FMusicDollStyle::Initialize()
{
    if (!StyleSet.IsValid())
    {
        StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

        // Get the plugin resources directory
        FString ContentDir = FPaths::ProjectPluginsDir() / TEXT("MusicDoll/Source/MusicDollUI/Resources");

        // Add the icon resources
        StyleSet->SetContentRoot(ContentDir);

        // Register the MusicDoll icon (40x40 and 16x16)
        StyleSet->Set(
            "MusicDoll.Icon",
            new FSlateImageBrush(ContentDir / TEXT("MusicDoll.png"), FVector2D(40.0f, 40.0f))
        );

        StyleSet->Set(
            "MusicDoll.Icon.Small",
            new FSlateImageBrush(ContentDir / TEXT("MusicDoll.png"), FVector2D(16.0f, 16.0f))
        );

        // Register the KeyRipple icon
        StyleSet->Set(
            "MusicDoll.KeyRipple.Icon",
            new FSlateImageBrush(ContentDir / TEXT("KeyRipple.png"), FVector2D(40.0f, 40.0f))
        );

        StyleSet->Set(
            "MusicDoll.KeyRipple.Icon.Small",
            new FSlateImageBrush(ContentDir / TEXT("KeyRipple.png"), FVector2D(16.0f, 16.0f))
        );

        // Register the StringFlow icon
        StyleSet->Set(
            "MusicDoll.StringFlow.Icon",
            new FSlateImageBrush(ContentDir / TEXT("StringFlow.png"), FVector2D(40.0f, 40.0f))
        );

        StyleSet->Set(
            "MusicDoll.StringFlow.Icon.Small",
            new FSlateImageBrush(ContentDir / TEXT("StringFlow.png"), FVector2D(16.0f, 16.0f))
        );

        FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
    }
}

void FMusicDollStyle::Shutdown()
{
    if (StyleSet.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
        ensure(StyleSet.IsUnique());
        StyleSet.Reset();
    }
}

TSharedPtr<FSlateStyleSet> FMusicDollStyle::Get()
{
    return StyleSet;
}

FName FMusicDollStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("MusicDollStyle"));
    return StyleSetName;
}

FString FMusicDollStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
    static FString ContentDir = FPaths::ProjectPluginsDir() / TEXT("MusicDoll/Source/MusicDollUI/Resources");
    return (ContentDir / RelativePath) + Extension;
}
