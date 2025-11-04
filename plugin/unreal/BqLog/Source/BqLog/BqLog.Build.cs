using UnrealBuildTool;
using System;
using System.Collections.Generic;
using System.IO;

public class BqLog : ModuleRules
{
    private readonly HashSet<string> RuntimeDependencyCache = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

    public BqLog(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine"
        });

        ConfigurePrebuilt(Target);

        //OptimizeCode = CodeOptimization.Never;
        //bUseUnity = false;
        //MinFilesUsingPrecompiledHeaderOverride = 1;
    }

    private void ConfigurePrebuilt(ReadOnlyTargetRules Target)
    {
        string moduleDir = ModuleDirectory;
        string pluginRoot = Path.GetFullPath(Path.Combine(moduleDir, "..", ".."));
        string thirdPartyRoot = Path.Combine(moduleDir, "ThirdParty", "BqLog");

        if (!Directory.Exists(thirdPartyRoot))
        {
            return;
        }

        string sharedInclude = Path.Combine(thirdPartyRoot, "Shared", "include");
        if (Directory.Exists(sharedInclude))
        {
            PublicIncludePaths.Add(sharedInclude);
        }

        switch (Target.Platform)
        {
            case UnrealTargetPlatform.Win64:
                ConfigureWindows(Target, thirdPartyRoot, pluginRoot);
                break;
            case UnrealTargetPlatform.Linux:
                ConfigureLinux(thirdPartyRoot, pluginRoot, "x86_64", "Linux");
                break;
            case UnrealTargetPlatform.LinuxAArch64:
                ConfigureLinux(thirdPartyRoot, pluginRoot, "arm64", "LinuxArm64");
                break;
            case UnrealTargetPlatform.Mac:
                ConfigureMac(thirdPartyRoot, pluginRoot);
                break;
            case UnrealTargetPlatform.IOS:
                ConfigureIOS(thirdPartyRoot, pluginRoot);
                break;
            case UnrealTargetPlatform.Android:
                ConfigureAndroid(thirdPartyRoot, pluginRoot);
                break;
        }
    }

    private void ConfigureWindows(ReadOnlyTargetRules Target, string thirdPartyRoot, string pluginRoot)
    {
        string archKey = GetWindowsArchitecture(Target);
        string binariesFolder = archKey == "arm64" ? "Win64Arm64" : "Win64";
        string libDir = Path.Combine(thirdPartyRoot, "Win64", archKey, "lib");
        string libPath = Path.Combine(libDir, "BqLog.lib");
        if (File.Exists(libPath))
        {
            PublicAdditionalLibraries.Add(libPath);
        }

        string dllRelative = Path.Combine("Binaries", binariesFolder, "BqLog.dll");
        if (AddRuntimeDependencyIfExists(pluginRoot, dllRelative))
        {
            AddDelayLoadOnce("BqLog.dll");
        }

        AddRuntimeDependencyIfExists(pluginRoot, Path.Combine("Binaries", binariesFolder, "BqLog.pdb"));
    }

    private void ConfigureLinux(string thirdPartyRoot, string pluginRoot, string archKey, string binariesFolder)
    {
        string libPath = Path.Combine(thirdPartyRoot, "Linux", archKey, "lib", "libBqLog.so");
        if (File.Exists(libPath))
        {
            PublicAdditionalLibraries.Add(libPath);
        }

        AddRuntimeDependencyIfExists(pluginRoot, Path.Combine("Binaries", binariesFolder, "libBqLog.so"));
    }

    private void ConfigureMac(string thirdPartyRoot, string pluginRoot)
    {
        string dylibPath = Path.Combine(thirdPartyRoot, "Mac", "lib", "libBqLog.dylib");
        if (File.Exists(dylibPath))
        {
            PublicAdditionalLibraries.Add(dylibPath);
        }

        AddRuntimeDependencyIfExists(pluginRoot, Path.Combine("Binaries", "Mac", "libBqLog.dylib"));
    }

    private void ConfigureIOS(string thirdPartyRoot, string pluginRoot)
    {
        string xcframeworkPath = Path.Combine(thirdPartyRoot, "IOS", "BqLog.xcframework");
        if (Directory.Exists(xcframeworkPath))
        {
            PublicAdditionalFrameworks.Add(new Framework("BqLog", xcframeworkPath));
        }

        string stagedRelative = Path.Combine("Binaries", "IOS", "BqLog.xcframework");
        string full = Path.Combine(pluginRoot, stagedRelative);
        if (Directory.Exists(full))
        {
            string normalized = "$(PluginDir)/" + stagedRelative.Replace("\\", "/") + "/**";
            if (RuntimeDependencyCache.Add(normalized))
            {
                RuntimeDependencies.Add(normalized);
            }
        }
    }

    private void ConfigureAndroid(string thirdPartyRoot, string pluginRoot)
    {
        string androidRoot = Path.Combine(thirdPartyRoot, "Android");
        string[] abis = { "armeabi-v7a", "arm64-v8a", "x86", "x86_64" };
        foreach (string abi in abis)
        {
            string soPath = Path.Combine(androidRoot, abi, "libBqLog.so");
            if (File.Exists(soPath))
            {
                PublicAdditionalLibraries.Add(soPath);
            }
        }

        string aplPath = Path.Combine(pluginRoot, "Binaries", "Android", "Android_APL.xml");
        if (File.Exists(aplPath))
        {
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", "$(PluginDir)/Binaries/Android/Android_APL.xml");
        }

        AddRuntimeDependencyIfExists(pluginRoot, Path.Combine("Binaries", "Android", "bqlog.aar"));
    }

    private void AddDelayLoadOnce(string dllName)
    {
        if (!PublicDelayLoadDLLs.Contains(dllName))
        {
            PublicDelayLoadDLLs.Add(dllName);
        }
    }

    private bool AddRuntimeDependencyIfExists(string pluginRoot, string relativePath)
    {
        string absolutePath = Path.Combine(pluginRoot, relativePath);
        if (!File.Exists(absolutePath))
        {
            return false;
        }

        string normalized = "$(PluginDir)/" + relativePath.Replace("\\", "/");
        if (RuntimeDependencyCache.Add(normalized))
        {
            RuntimeDependencies.Add(normalized);
        }

        return true;
    }

    private string GetWindowsArchitecture(ReadOnlyTargetRules Target)
    {
        object platform = GetPropertyValue(Target, "WindowsPlatform");
        string architecture = GetPropertyString(platform, "Architecture");
        if (!string.IsNullOrEmpty(architecture))
        {
            string lowered = architecture.ToLowerInvariant();
            if (lowered.Contains("arm"))
            {
                return "arm64";
            }

            if (lowered.Contains("x86") || lowered.Contains("x64"))
            {
                return "x86_64";
            }
        }

        return "x86_64";
    }

    private static object GetPropertyValue(object target, string propertyName)
    {
        if (target == null)
        {
            return null;
        }

        var property = target.GetType().GetProperty(propertyName);
        return property != null ? property.GetValue(target, null) : null;
    }

    private static string GetPropertyString(object target, string propertyName)
    {
        object value = GetPropertyValue(target, propertyName);
        return value != null ? value.ToString() : null;
    }
}