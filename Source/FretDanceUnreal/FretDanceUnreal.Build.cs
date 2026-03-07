using UnrealBuildTool;

public class FretDanceUnreal : ModuleRules
{
    public FretDanceUnreal(ReadOnlyTargetRules Target) : base(Target)
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
                "ControlRigDeveloper",
                "ControlRigEditor",
                "MovieScene",
                "MovieSceneTracks",
                "LevelSequence",
                "Slate",
                "SlateCore",
                "AssetRegistry",
                "MovieRenderPipelineCore",
                "MusicDollCommon"
            }
        );

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "UnrealEd",
                    "ControlRigEditor",
                    "ControlRigDeveloper",
                    "LevelEditor",
                    "LevelSequenceEditor",
                    "Sequencer",
                    "MovieSceneTools",
                    "EditorStyle",
                    "PropertyEditor",
                    "DesktopPlatform",
                    "WorkspaceMenuStructure",
                    "MovieRenderPipelineCore",
                    "MovieRenderPipelineEditor"
                }
            );
        }
    }
}