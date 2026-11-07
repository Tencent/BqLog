/* Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
using System.IO;
using UnrealBuildTool;

public class BqLogBPNodes : ModuleRules
{
	public BqLogBPNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDefinitions.Add("BQ_LOG_UE_PLUGIN=1");

		PublicDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine"
		});

		PrivateDependencyModuleNames.AddRange(new[] {
			"BlueprintGraph", "Kismet", "BqLog"
		});

		// UE6 folded the KismetCompiler module into BlueprintGraph (the headers keep their
		// names), so depend on it only where the engine still ships it as its own module.
		if (Directory.Exists(Path.Combine(EngineDirectory, "Source", "Editor", "KismetCompiler")))
		{
			PrivateDependencyModuleNames.Add("KismetCompiler");
		}

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[] {
				"UnrealEd",
				"GraphEditor",
				"Slate", "SlateCore",
				"EditorStyle"
			});

#if UE_4_24_OR_LATER
			PrivateDependencyModuleNames.Add("ToolMenus");
#endif
		}
	}
}