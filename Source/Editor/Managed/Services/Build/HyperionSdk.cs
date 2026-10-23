using System;
using System.IO;
using System.Text.RegularExpressions;

namespace Hyperion.Editor.Services
{
    // The Sdk folder next to the editor that native game projects build against (see Source/HyperionSdk.cmake.in)
    public sealed class HyperionSdk
    {
        public static string ExpectedDirectory => Path.Combine(Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory, "Sdk");

        public string Directory { get; }

        // CMake's compiler id for the engine build, or cl / clang-cl on Windows
        public string Compiler { get; }

        private HyperionSdk(string directory, string compiler)
        {
            Directory = directory;
            Compiler = compiler;
        }

        public static HyperionSdk? Find()
        {
            string sdkDirectory = ExpectedDirectory;
            string sdkFile = Path.Combine(sdkDirectory, "CMake", "HyperionSdk.cmake");

            if (!File.Exists(sdkFile))
            {
                return null;
            }

            Match compilerMatch = Regex.Match(File.ReadAllText(sdkFile), "set\\(HYPERION_SDK_COMPILER \"([^\"]*)\"\\)");

            return new HyperionSdk(sdkDirectory, compilerMatch.Success ? compilerMatch.Groups[1].Value : string.Empty);
        }
    }
}
