using System.Collections.Generic;
using System.Runtime.Versioning;

namespace Hyperion.Editor.Services
{
    [SupportedOSPlatform("linux")]
    public sealed class LinuxGameBuildPlatform : PosixGameBuildPlatform
    {
        public override string Name => "Linux";

        public override string CookTargetPlatform => "linux";

        protected override string RuntimeIdentifierOs => "linux";

        private static readonly string[] s_dotNetInstallDirectories = { "/usr/share/dotnet", "/usr/lib/dotnet", "/usr/lib64/dotnet" };

        protected override IReadOnlyList<string> DotNetInstallDirectories => s_dotNetInstallDirectories;

        protected override IReadOnlyList<string> GetCompilerNames(string sdkCompiler)
        {
            return sdkCompiler == "Clang"
                ? ["clang++"]
                : ["g++"];
        }

        protected override string DescribeCompiler(string sdkCompiler)
        {
            return sdkCompiler == "Clang"
                ? "Clang C++ compiler (clang++)"
                : "GCC C++ compiler (g++)";
        }

        public override void PromptInstallNativeToolchain(IReadOnlyList<string> missing)
        {
            ShowMessage(MessageBox.Warning("C++ build tools required",
                    FormatMissing(missing) + "\n\nInstall usimh your distribution's package manager, for example:\n  sudo apt install cmake ninja-build g++\n  sudo dnf install cmake ninja-build gcc-c++")
                .Button("OK", () => { }));
        }

        public override string? FindNativeAotToolchainError(out string? extraPath)
        {
            extraPath = null;

            if (FindOnPath("clang") != null || FindOnPath("gcc") != null)
            {
                return null;
            }

            return "Packaging a C# game compiles it with NativeAOT, which needs clang (or gcc) to link. "
                 + "Install using your distribution's package manager, e.g. sudo apt install clang";
        }
    }
}
