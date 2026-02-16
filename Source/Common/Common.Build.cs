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
            "AutomationController",
            "RigVM",                    // Rig Unit需依赖RigVM模块
            "ControlRig",               // Control Rig模块
            "ControlRigEditor",         // Control Rig Editor模块
            "ControlRigDeveloper",      // Control Rig Developer模块（包含ControlRigBlueprintLegacy.h）
            "UnrealEd",                 // 编辑器库
            "Sequencer",                // Sequencer 相关
            "MovieScene",               // MovieScene 相关
            "MovieSceneTools",
            "MovieSceneTracks",         // MovieScene Tracks (for Component Material Track)
            "LevelSequence",            // LevelSequence 相关
            "LevelSequenceEditor",      // LevelSequence Editor (for RefreshCurrentLevelSequence)
            "Slate",                    // Slate UI库
            "SlateCore",                // Slate核心库
            "DesktopPlatform",          // 桌面平台（文件对话框）
            "Json",                     // JSON parsing
            "JsonUtilities"             // JSON utilities
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
        });
    }
}