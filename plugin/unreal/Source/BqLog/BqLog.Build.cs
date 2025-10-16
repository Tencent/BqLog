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
        
        // UE版本兼容性
        if (Target.Version.MajorVersion >= 5)
        {
            PublicDependencyModuleNames.Add("ToolMenus");
        }
        PrivateDefinitions.Add("BQ_DYNAMIC_LIB=1");
        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // 注册 APL，将 Java 源与可选清单/Gradle 注入生效
            string APL = Path.Combine(ModuleDirectory, "Android", "BqLog_APL.xml");
            if (File.Exists(APL))
            {
                AdditionalPropertiesForReceipt.Add("AndroidPlugin", APL);
            }
        }

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            // iOS 静态链接（如需系统框架在此添加）
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            // PublicFrameworks.Add("Cocoa");
        }
    }
}