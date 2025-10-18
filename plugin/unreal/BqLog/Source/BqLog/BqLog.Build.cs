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
        
#if UE_5_0_OR_LATER
        //OptimizeCode = CodeOptimization.Never; // 
#else
        //bOptimizeCode = false;
#endif
        //bUseUnity = false; // 
        //MinFilesUsingPrecompiledHeaderOverride = 1;
    }
}