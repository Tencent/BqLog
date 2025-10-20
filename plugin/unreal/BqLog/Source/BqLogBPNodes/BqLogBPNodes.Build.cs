using UnrealBuildTool;

public class BqLogBPNodes : ModuleRules
{
	public BqLogBPNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine"
		});

		PrivateDependencyModuleNames.AddRange(new[] {
			"BlueprintGraph", "KismetCompiler", "Kismet", "BqLog"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[] {
				"UnrealEd",
				"GraphEditor",
				"Slate", "SlateCore",
				"EditorStyle"
			});

			if (Target.Version.MajorVersion >= 5)
			{
				PrivateDependencyModuleNames.Add("ToolMenus");
			}
		}
	}
}