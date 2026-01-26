// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KeyRippleUnreal : ModuleRules
{
    public KeyRippleUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "AnimationCore",
                "AnimGraphRuntime",
                "Json",
                "JsonUtilities",
                "EnhancedInput",
                "ControlRig",
                "ControlRigEditor",
                "ControlRigDeveloper",
                "LevelEditor",
                "MovieScene",
                "MovieSceneTracks",
                "LevelSequence",
                "LevelSequenceEditor",
                "Sequencer",
                "UnrealEd",
                "MovieSceneTools",
                "Slate",
                "SlateCore",
                "AssetTools",
                "AssetRegistry",
                "Common"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "EditorStyle",
                "PropertyEditor",
                "DesktopPlatform",
                "WorkspaceMenuStructure"
            }
        );
    }
}