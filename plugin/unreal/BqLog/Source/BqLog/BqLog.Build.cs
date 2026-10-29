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
using UnrealBuildTool;
using System;
using System.Collections.Generic;
using System.IO;

public class BqLog : ModuleRules
{
    private readonly List<string> RuntimeDependencyCache = new List<string>();
    private const string BqLogMajorVersion = "2";

    public BqLog(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDefinitions.Add("BQ_LOG_UE_PLUGIN=1");

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine"
        });

        ConfigureSourceBuild();
        ConfigureConsolePlatformMacros(Target);
        ConfigurePrebuilt(Target);
        //OptimizeCode = CodeOptimization.Never;
        //bUseUnity = false;
        //MinFilesUsingPrecompiledHeaderOverride = 1;
    }

    /// <summary>
    /// Source build: BqLog core headers live under ThirdParty/BqLog/include (kept in the
    /// ThirdParty tree for Fab's third-party attribution requirement). The core .cpp
    /// files are shipped inside Source/BqLog/Private/BqLogCore/ by the packaging scripts
    /// </summary>
    private void ConfigureSourceBuild()
    {
        string thirdParty = Path.Combine(ModuleDirectory, "..", "ThirdParty", "BqLog");
        string includeDir = Path.Combine(thirdParty, "include");
        if (Directory.Exists(includeDir))
        {
            PublicIncludePaths.Add(includeDir);
        }

        string coreSrcDir = Path.Combine(ModuleDirectory, "Private", "BqLogCore");
        if (Directory.Exists(coreSrcDir))
        {
            PrivateIncludePaths.Add(coreSrcDir);
        }
    }

    private void ConfigureConsolePlatformMacros(ReadOnlyTargetRules Target)
    {
        string platform = Target.Platform.ToString();
        if (platform == "PS4")
        {
            PublicDefinitions.Add("__ORBIS__=1");
        }
        else if (platform == "PS5")
        {
            PublicDefinitions.Add("__PROSPERO__=1");
        }
        else if (platform == "Switch")
        {
            PublicDefinitions.Add("NN_NINTENDO_SDK=1");
        }
    }

    private void ConfigurePrebuilt(ReadOnlyTargetRules Target)
    {
        string moduleDir = ModuleDirectory;
        string pluginRoot = Path.GetFullPath(Path.Combine(moduleDir, "..", ".."));
        string thirdPartyRoot = Path.Combine(moduleDir, "ThirdParty", "BqLog");
        // Only proceed if this is the prebuilt variant (has Shared/ with platform binaries).
        // The source-compile variant also has ThirdParty/BqLog/ but with include/ and src/ instead.
        if (!Directory.Exists(Path.Combine(thirdPartyRoot, "Shared")))
        {
            return;
        }
        if (Target.Platform.Equals(UnrealTargetPlatform.Win64))
        {
            ConfigureWindows(Target, thirdPartyRoot, pluginRoot);
        }else if (Target.Platform.Equals(UnrealTargetPlatform.Linux))
        {
            ConfigureLinux(thirdPartyRoot, pluginRoot, "x86_64", UnrealTargetPlatform.Linux.ToString());
        }
#if UE_5_0_OR_LATER
        else if (Target.Platform.Equals(UnrealTargetPlatform.LinuxArm64))
        {
            ConfigureLinux(thirdPartyRoot, pluginRoot, "arm64", "LinuxAArch64");
        }
#elif UE_4_24_OR_LATER
        else if (Target.Platform.Equals(UnrealTargetPlatform.LinuxAArch64))
        {
            ConfigureLinux(thirdPartyRoot, pluginRoot, "arm64", "LinuxAArch64");
        }
#endif
        else if (Target.Platform.Equals(UnrealTargetPlatform.Mac))
        {
            ConfigureMac(thirdPartyRoot, pluginRoot);
        }else if (Target.Platform.Equals(UnrealTargetPlatform.IOS))
        {
            ConfigureIOS(thirdPartyRoot, pluginRoot);
        }else if (Target.Platform.Equals(UnrealTargetPlatform.Android))
        {
            ConfigureAndroid(thirdPartyRoot, pluginRoot);
        }
    }

    private void AddStaticInclude(string thirdPartyRoot)
    {
        string staticInclude = Path.Combine(thirdPartyRoot, "Shared", "Static", "include");
        if (Directory.Exists(staticInclude) && !PublicIncludePaths.Contains(staticInclude))
        {
            PublicIncludePaths.Add(staticInclude);
        }
    }

    private void AddDynamicInclude(string thirdPartyRoot)
    {
        string dynamicInclude = Path.Combine(thirdPartyRoot, "Shared", "Dynamic", "include");
        if (Directory.Exists(dynamicInclude) && !PublicIncludePaths.Contains(dynamicInclude))
        {
            PublicIncludePaths.Add(dynamicInclude);
        }
    }

    private void ConfigureWindows(ReadOnlyTargetRules Target, string thirdPartyRoot, string pluginRoot)
    {
        AddDynamicInclude(thirdPartyRoot);
        string archKey = GetWindowsArchitecture(Target);
        string binariesFolder = archKey == "arm64" ? "Win64Arm64" : "Win64";
        string libDir = Path.Combine(thirdPartyRoot, "Win64", archKey, "lib");
        string libPath = Path.Combine(libDir, "BqLog.lib");
        if (File.Exists(libPath))
        {
            PublicAdditionalLibraries.Add(libPath);
        }

        string runtimeSrcDir = Path.Combine(ModuleDirectory, "../.." , "Binaries", binariesFolder);
        string runtimeDstDir = Path.Combine("$(ProjectDir)", "Binaries", binariesFolder);
        RuntimeDependencies.Add(Path.Combine(runtimeDstDir, "BqLog.dll"), Path.Combine(runtimeSrcDir, "BqLog.dll"));
        RuntimeDependencies.Add(Path.Combine(runtimeDstDir, "BqLog.pdb"), Path.Combine(runtimeSrcDir, "BqLog.pdb"));
    }

    private void ConfigureLinux(string thirdPartyRoot, string pluginRoot, string archKey, string binariesFolder)
    {
        AddDynamicInclude(thirdPartyRoot);
        string libPath = Path.Combine(thirdPartyRoot, "Linux", archKey, "lib", "libBqLog.so");
        if (File.Exists(libPath))
        {
            PublicAdditionalLibraries.Add(libPath);
        }
        string runtimeSrcDir = Path.Combine(ModuleDirectory, "../..", "Binaries", binariesFolder);
        string runtimeDstDir = Path.Combine("$(ProjectDir)", "Binaries", binariesFolder);
        string destName = "libBqLog.so." + BqLogMajorVersion;
        string exact = Path.Combine(runtimeSrcDir, destName);
        RuntimeDependencies.Add(Path.Combine(runtimeDstDir, destName), exact);
    }

    private void ConfigureMac(string thirdPartyRoot, string pluginRoot)
    {
        AddDynamicInclude(thirdPartyRoot);
        string dylibPath = Path.Combine(thirdPartyRoot, "Mac", "lib", "libBqLog.dylib");
        if (File.Exists(dylibPath))
        {
            PublicAdditionalLibraries.Add(dylibPath);
        }
        string binariesFolder = UnrealTargetPlatform.Mac.ToString();
        string runtimeSrcDir = Path.Combine(ModuleDirectory, "../..", "Binaries", binariesFolder);
        string runtimeDstDir = Path.Combine("$(ProjectDir)", "Binaries", binariesFolder);
        string destName = "libBqLog." + BqLogMajorVersion + ".dylib";
        string exact = Path.Combine(runtimeSrcDir, destName);
        RuntimeDependencies.Add(Path.Combine(runtimeDstDir, destName), exact);
    }

    private void ConfigureIOS(string thirdPartyRoot, string pluginRoot)
    {
        AddDynamicInclude(thirdPartyRoot);
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
            if (!RuntimeDependencyCache.Contains(normalized))
            {
                RuntimeDependencyCache.Add(normalized);
                RuntimeDependencies.Add(normalized);
            }
        }
    }

    private void ConfigureAndroid(string thirdPartyRoot, string pluginRoot)
    {
        AddDynamicInclude(thirdPartyRoot);
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
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", aplPath);
        }
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
