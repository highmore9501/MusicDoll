#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

class FMusicDollStyle
{
public:
    static void Initialize();
    static void Shutdown();

    static TSharedPtr<class FSlateStyleSet> Get();

    static FName GetStyleSetName();

private:
    static FString InContent(const FString& RelativePath, const ANSICHAR* Extension);

    static TSharedPtr<class FSlateStyleSet> StyleSet;
};
