using UnrealBuildTool;

public class BqLogEditor : ModuleRules
{
    public BqLogEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "BlueprintGraph", "KismetCompiler", "Kismet",
            "UnrealEd", "Slate", "SlateCore", "LevelEditor", "Projects", "BqLog", "BlueprintGraph"
        });
        
        // UE版本兼容性
        if (Target.Version.MajorVersion >= 5)
        {
            PrivateDependencyModuleNames.Add("ToolMenus");
            PrivateDependencyModuleNames.Add("EditorStyle");
        }
        else
        {
            // UE4 specific dependencies
            PrivateDependencyModuleNames.Add("EditorStyle");
        }
    }
}