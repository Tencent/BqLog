/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#nullable enable

using System;
using System.IO;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace BqLog.GodotIntegration
{
    // Automatically resolves the native BqLog library from addons/bqlog/bin/ for BqLog.Managed.dll
    // so Godot C# (.NET) and GDScript share the exact same loaded native library instance without
    // requiring AllowUnsafeBlocks, NuGet, or DYLD_LIBRARY_PATH / LD_LIBRARY_PATH workarounds.
    // Uses assembly-name lookup (no compile-time dependency on bq.log) so projects that have not
    // yet added <Reference Include="addons/bqlog/csharp/BqLog.Managed.dll" /> still build cleanly.
    internal static class BqLogGodotResolver
    {
        private const string ManagedAssemblyName = "BqLog.Managed";
        private const string NativeLibraryName = "BqLog";

        [ModuleInitializer]
        internal static void Initialize()
        {
            foreach (Assembly asm in AppDomain.CurrentDomain.GetAssemblies())
            {
                TryAttachResolver(asm);
            }
            AppDomain.CurrentDomain.AssemblyLoad += OnAssemblyLoad;
        }

        private static void OnAssemblyLoad(object? sender, AssemblyLoadEventArgs args)
        {
            TryAttachResolver(args.LoadedAssembly);
        }

        private static void TryAttachResolver(Assembly asm)
        {
            if (!string.Equals(asm.GetName().Name, ManagedAssemblyName, StringComparison.Ordinal))
            {
                return;
            }
            try
            {
                NativeLibrary.SetDllImportResolver(asm, ResolveNativeLibrary);
            }
            catch (InvalidOperationException)
            {
                // Resolver already registered for this assembly.
            }
        }

        private static IntPtr ResolveNativeLibrary(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            if (!string.Equals(libraryName, NativeLibraryName, StringComparison.Ordinal))
            {
                return IntPtr.Zero;
            }

            string? relPath = GetRelativeNativeLibPath();
            if (relPath == null)
            {
                return IntPtr.Zero;
            }

            string?[] roots = new[]
            {
                Environment.CurrentDirectory,
                AppContext.BaseDirectory,
                Path.GetDirectoryName(assembly.Location)
            };

            foreach (string? root in roots)
            {
                if (string.IsNullOrEmpty(root))
                {
                    continue;
                }
                string? dir = root;
                for (int depth = 0; depth < 8 && !string.IsNullOrEmpty(dir); depth++)
                {
                    string candidate = Path.Combine(dir, relPath);
                    if (File.Exists(candidate) && NativeLibrary.TryLoad(candidate, out IntPtr handle))
                    {
                        return handle;
                    }
                    dir = Path.GetDirectoryName(dir);
                }
            }

            return IntPtr.Zero;
        }

        private static string? GetRelativeNativeLibPath()
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                return Path.Combine("addons", "bqlog", "bin", "macos", "libBqLog.dylib");
            }
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                string arch = RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "arm64" : "x86_64";
                return Path.Combine("addons", "bqlog", "bin", "windows", arch, "BqLog.dll");
            }
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            {
                string arch = RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "arm64" : "x86_64";
                return Path.Combine("addons", "bqlog", "bin", "linux", arch, "libBqLog.so");
            }
            return null;
        }
    }
}
