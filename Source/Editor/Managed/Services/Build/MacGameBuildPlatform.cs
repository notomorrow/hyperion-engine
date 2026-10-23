using System.Collections.Generic;
using System.Runtime.Versioning;

namespace Hyperion.Editor.Services
{
    [SupportedOSPlatform("macos")]
    public sealed class MacGameBuildPlatform : PosixGameBuildPlatform
    {
        public override string Name => "Mac";

        public override string CookTargetPlatform => "mac";

        protected override string RuntimeIdentifierOs => "osx";

        private static readonly string[] s_dotNetInstallDirectories = { "/usr/local/share/dotnet", "/opt/homebrew/share/dotnet" };

        protected override IReadOnlyList<string> DotNetInstallDirectories => s_dotNetInstallDirectories;

        protected override IReadOnlyList<string> GetCompilerNames(string sdkCompiler)
        {
            return sdkCompiler == "GNU"
                ? ["g++"]
                : ["clang++"];
        }

        protected override string DescribeCompiler(string sdkCompiler)
        {
            return sdkCompiler == "GNU"
                ? "GCC C++ compiler (g++)"
                : "Xcode command line tools (clang++)";
        }

        public override void PromptInstallNativeToolchain(IReadOnlyList<string> missing)
        {
            ShowMessage(MessageBox.Warning("C++ build tools required",
                    FormatMissing(missing) + "\n\nInstall them with:\n  xcode-select --install\n  brew install cmake ninja")
                .Button("OK", () => { }));
        }

        public override string? FindNativeAotToolchainError(out string? extraPath)
        {
            extraPath = null;

            return null;
        }
    }
}
