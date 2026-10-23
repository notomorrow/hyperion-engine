using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace Hyperion.Editor.Services
{
    public sealed class WindowsGameBuildPlatform : GameBuildPlatform
    {
        private const string BuildToolsPackageId = "Microsoft.VisualStudio.BuildTools";

        private const string BuildToolsInstallerArguments = "--passive --wait"
            + " --add Microsoft.VisualStudio.Workload.VCTools"
            + " --add Microsoft.VisualStudio.Component.VC.Llvm.Clang"
            + " --add Microsoft.VisualStudio.Component.VC.CMake.Project"
            + " --includeRecommended";

        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("GameBuild");

        private static readonly string[] s_stripToolNames = { "strip.exe", "llvm-strip.exe" };

        public override string Name => "Windows";

        public override string CookTargetPlatform => "windows";

        public override string ExecutableExtension => ".exe";

        protected override string RuntimeIdentifierOs => "win";

        protected override IReadOnlyList<string> StripToolNames => s_stripToolNames;

        public override NativeToolchain FindNativeToolchain(string sdkCompiler)
        {
            NativeToolchain? best = null;

            foreach (string installPath in FindVisualStudioInstalls())
            {
                NativeToolchain candidate = new NativeToolchain { Generator = "Ninja" };

                string vcVarsPath = Path.Combine(installPath, "VC", "Auxiliary", "Build", "vcvars64.bat");
                string cmakeRoot = Path.Combine(installPath, "Common7", "IDE", "CommonExtensions", "Microsoft", "CMake");

                candidate.EnvironmentScriptPath = File.Exists(vcVarsPath) ? vcVarsPath : null;
                candidate.CMakePath = FirstExisting(Path.Combine(cmakeRoot, "CMake", "bin", "cmake.exe")) ?? FindOnPath("cmake.exe");
                candidate.MakeProgramPath = FirstExisting(Path.Combine(cmakeRoot, "Ninja", "ninja.exe")) ?? FindOnPath("ninja.exe");

                if (sdkCompiler == "cl")
                {
                    // on PATH once vcvars has run
                    candidate.CompilerPath = candidate.EnvironmentScriptPath != null ? "cl.exe" : null;
                }
                else
                {
                    candidate.CompilerPath = FirstExisting(Path.Combine(installPath, "VC", "Tools", "Llvm", "x64", "bin", "clang-cl.exe")) ?? FindOnPath("clang-cl.exe");
                }

                if (candidate.EnvironmentScriptPath == null)
                {
                    candidate.Missing.Add("MSVC C++ build tools");
                }

                if (candidate.CompilerPath == null)
                {
                    candidate.Missing.Add("C++ Clang tools for Windows (clang-cl)");
                }

                if (candidate.CMakePath == null)
                {
                    candidate.Missing.Add("CMake");
                }

                if (candidate.MakeProgramPath == null)
                {
                    candidate.Missing.Add("Ninja");
                }

                if (best == null || candidate.Missing.Count < best.Missing.Count)
                {
                    best = candidate;
                }
            }

            if (best == null)
            {
                best = new NativeToolchain();
                best.Missing.Add("Visual Studio or Visual Studio Build Tools with C++ workload");
            }

            return best;
        }

        public override void PromptInstallNativeToolchain(IReadOnlyList<string> missing)
        {
            string text = "Building the game needs:\n  - " + string.Join("\n  - ", missing)
                + "\n\nInstall Visual Studio Build Tools?";

            ShowMessage(MessageBox.Warning("C++ build tools required", text)
                .Button("Install", StartBuildToolsInstall)
                .Button("Cancel", () => { }));
        }

        public override string? FindNativeAotToolchainError(out string? extraPath)
        {
            extraPath = null;

            string installerDirectory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Microsoft Visual Studio", "Installer");
            string vswherePath = Path.Combine(installerDirectory, "vswhere.exe");

            const string MissingToolchainMessage = "Packaging a C# game compiles it with NativeAOT, which needs the Visual Studio C++ build tools. Install Visual Studio (or Build Tools) with the \"Desktop development with C++\" workload.";

            if (!File.Exists(vswherePath))
            {
                return MissingToolchainMessage;
            }

            string component = RuntimeInformation.OSArchitecture == Architecture.Arm64
                ? "Microsoft.VisualStudio.Component.VC.Tools.ARM64"
                : "Microsoft.VisualStudio.Component.VC.Tools.x86.x64";

            string installationPath = RunVsWhere(vswherePath, "-latest", "-products", "*", "-requires", component, "-property", "installationPath").Trim();

            if (installationPath.Length == 0)
            {
                return MissingToolchainMessage;
            }

            // NativeAOT finds vswhere on PATH to locate the linker
            extraPath = installerDirectory;

            return null;
        }

        public override string? FindStripTool()
        {
            return base.FindStripTool()
                ?? FindVisualStudioInstalls()
                    .Select(installPath => Path.Combine(installPath, "VC", "Tools", "Llvm", "x64", "bin", "llvm-strip.exe"))
                    .FirstOrDefault(File.Exists);
        }

        public override string WriteBuildScript(string buildDirectory, NativeToolchain toolchain, IReadOnlyList<IReadOnlyList<string>> commands)
        {
            List<string> lines = new List<string> { "@echo off" };

            if (toolchain.EnvironmentScriptPath != null)
            {
                lines.Add($"call {Quote(toolchain.EnvironmentScriptPath)} >nul");
                lines.Add("if errorlevel 1 exit /b 1");
            }

            foreach (IReadOnlyList<string> command in commands)
            {
                lines.Add(string.Join(" ", command.Select(Quote)));
                lines.Add("if errorlevel 1 exit /b 1");
            }

            string scriptPath = Path.Combine(buildDirectory, "Build.bat");
            File.WriteAllLines(scriptPath, lines);

            return scriptPath;
        }

        public override ProcessStartInfo CreateBuildScriptStartInfo(string scriptPath, string workingDirectory)
        {
            return BuildProcess.CreateStartInfo(Environment.GetEnvironmentVariable("ComSpec") ?? "cmd.exe", workingDirectory, new[] { "/d", "/c", scriptPath });
        }

        public static List<string> FindVisualStudioInstalls()
        {
            string vswherePath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
                "Microsoft Visual Studio", "Installer", "vswhere.exe");

            if (!File.Exists(vswherePath))
            {
                return new List<string>();
            }

            string output = RunVsWhere(vswherePath, "-products", "*", "-prerelease", "-sort", "-format", "value", "-property", "installationPath");

            return output.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
                .Where(Directory.Exists)
                .ToList();
        }

        private static string RunVsWhere(string vswherePath, params string[] arguments)
        {
            try
            {
                ProcessStartInfo startInfo = new ProcessStartInfo
                {
                    FileName = vswherePath,
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true
                };

                foreach (string argument in arguments)
                {
                    startInfo.ArgumentList.Add(argument);
                }

                using Process process = Process.Start(startInfo)!;

                string output = process.StandardOutput.ReadToEnd();
                process.WaitForExit();

                return output;
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "Could not run vswhere: {0}", ex.Message);

                return string.Empty;
            }
        }

        private static void StartBuildToolsInstall()
        {
            try
            {
                ProcessStartInfo startInfo = new ProcessStartInfo
                {
                    FileName = "winget",
                    UseShellExecute = true
                };

                foreach (string argument in new[] { "install", "--id", BuildToolsPackageId, "--exact", "--override", BuildToolsInstallerArguments })
                {
                    startInfo.ArgumentList.Add(argument);
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "Starting winget {0}", string.Join(" ", startInfo.ArgumentList));

                Process.Start(startInfo)?.Dispose();
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Could not start winget: {0}", ex.Message);

                ShowMessage(MessageBox.Critical("Could not start the install", "winget was not found. Install Visual Studio Build Tools with the C++ workload, Clang and CMake components manually.").Button("OK", () => { }));
            }
        }

        // batch expands %VAR% even inside quotes
        private static string Quote(string argument)
        {
            return "\"" + argument.Replace("%", "%%").Replace("\"", "\\\"") + "\"";
        }
    }
}
