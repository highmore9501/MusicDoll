using UnrealBuildTool;

public class Common : ModuleRules
{
    public Common(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "AnimationCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
        });
    }
}