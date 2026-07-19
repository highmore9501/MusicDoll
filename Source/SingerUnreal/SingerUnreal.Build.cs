using UnrealBuildTool;

public class SingerUnreal : ModuleRules
{
    public SingerUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "ControlRig",
            "ControlRigDeveloper",    // 提供 UControlRigBlueprint 的反射数据
            "LevelSequence",
            "Slate",
            "SlateCore",
            "MusicDollCommon"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "UnrealEd",
                    "EditorStyle",
                    "WorkspaceMenuStructure"
                }
            );
        }
    }
}
