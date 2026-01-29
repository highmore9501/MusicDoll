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
            "AnimationCore",
            "RigVM",      // Rig Unit需依赖RigVM模块
            "ControlRig"  // Control Rig模块
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
        });
    }
}