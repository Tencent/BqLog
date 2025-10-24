using UnrealBuildTool;
using System.IO;

public class BqLog : ModuleRules
{
    public BqLog(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine"
        });

        //OptimizeCode = CodeOptimization.Never;
        //bUseUnity = false; 
        //MinFilesUsingPrecompiledHeaderOverride = 1;
    }
}