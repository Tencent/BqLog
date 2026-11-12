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

// UE6 only. The Verse/ directory of this module is compiled by VNI into the Verse package
// /Tencent.com/BqLog; user Verse code reaches it with `using { /Tencent.com/BqLog }`.
public class BqLogVerse : ModuleRules
{
    public BqLogVerse(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "VerseNative"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "BqLog"
        });

        // A package that declares <native> functions must be InternalUser: the Verse compiler
        // only allows <native> outside Epic's own path prefixes for InternalUser packages, and a
        // PublicAPI package outside those prefixes is rejected with V3593. InternalUser does not
        // leak to the user packages that import this one; they keep their own scope.
        SetupVerse("/Tencent.com/BqLog", VerseScope.InternalUser);
    }
}
