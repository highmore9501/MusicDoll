// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MusicDollUI : ModuleRules
{
    public MusicDollUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "InputCore",
                "EditorStyle",
                "UnrealEd",
                "EditorFramework",
                "WorkspaceMenuStructure",
                "AppFramework",
                "Common"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "KeyRippleUnreal",
                "StringFlowUnreal",
            }
        );

        // Add Resources directory for custom icons
        if (Target.Type == TargetType.Editor)
        {
            PrivateIncludePathModuleNames.AddRange(
                new string[]
                {
                    "Slate",
                    "SlateCore"
                }
            );
        }
    }
}
