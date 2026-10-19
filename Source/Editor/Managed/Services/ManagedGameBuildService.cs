using System;
using System.Diagnostics;
using System.Reflection.PortableExecutable;
using System.Runtime.InteropServices;
using System.IO;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;

namespace Hyperion.Editor.Services
{
    public sealed class ManagedGameBuildService
    {
        public static ManagedGameBuildService Instance { get; } = new ManagedGameBuildService();

        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("GameBuild");

        private static readonly Regex ErrorLineRegex = new Regex(@"\berror\b", RegexOptions.Compiled);

        private int _isBuilding;

        private ManagedGameBuildService()
        {
        }

        public static string? FindProjectFile(string? projectFilePath)
        {
            string? projectDirectory = string.IsNullOrEmpty(projectFilePath) ? null : Path.GetDirectoryName(Path.GetFullPath(projectFilePath));
            string? sourceDirectory = projectDirectory == null ? null : Path.Combine(projectDirectory, "Source");

            if (sourceDirectory == null || !Directory.Exists(sourceDirectory))
            {
                return null;
            }

            string[] projectFiles = Directory.GetFiles(sourceDirectory, "*.csproj");

            return projectFiles.Length > 0 ? projectFiles[0] : null;
        }

        public async Task<bool> BuildAsync(string? projectFilePath)
        {
            string? managedProjectFile = FindProjectFile(projectFilePath);

            if (managedProjectFile == null)
            {
                ShowMessage(MessageBox.Info("No C# project", "Generate the C# project first.").Button("OK", () => { }));

                return false;
            }

            if (Interlocked.CompareExchange(ref _isBuilding, 1, 0) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A game build is already running");

                return false;
            }

            try
            {
                Logger.Log(BuildLogChannel, LogLevel.Info, "Building game project {0}", managedProjectFile);

                string? firstError = null;

                string[] arguments = { "build", managedProjectFile, "--configuration", "Release", "--disable-build-servers", "--nologo" };

                int exitCode = await RunDotNetAsync(managedProjectFile, arguments, null, line =>
                {
                    if (firstError == null && ErrorLineRegex.IsMatch(line))
                    {
                        firstError = line;
                    }
                }).ConfigureAwait(false);

                if (exitCode != 0)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "Game build failed (exit code {0})", exitCode);

                    ShowMessage(MessageBox.Critical("Build failed", firstError ?? "See the log for details.").Button("OK", () => { }));

                    return false;
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "Game build succeeded");

                return true;
            }
            catch (System.ComponentModel.Win32Exception)
            {
                ShowMessage(MessageBox.Critical(".NET SDK required", "dotnet was not found. Install the .NET 10 SDK to build a C# game.").Button("OK", () => { }));

                return false;
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Game build failed: {0}", ex.Message);

                return false;
            }
            finally
            {
                Interlocked.Exchange(ref _isBuilding, 0);
            }
        }

        public bool PublishAot(string projectFilePath, string binariesDirectory, string outputDirectory)
        {
            string? managedProjectFile = FindProjectFile(projectFilePath);

            if (managedProjectFile == null)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "No C# project found for {0}", projectFilePath);

                return false;
            }

            string? toolchainError = FindNativeToolchain(out string? toolchainPath);

            if (toolchainError != null)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "{0}", toolchainError);

                ShowMessage(MessageBox.Critical("NativeAOT toolchain required", toolchainError).Button("OK", () => { }));

                return false;
            }

            if (Interlocked.CompareExchange(ref _isBuilding, 1, 0) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A game build is already running");

                return false;
            }

            try
            {
                string runtimeIdentifier = GetRuntimeIdentifier();
                string intermediateDirectory = Path.GetFullPath(Path.Combine(binariesDirectory, "..", "AotIntermediate", runtimeIdentifier));
                string publishDirectory = Path.Combine(intermediateDirectory, "publish");

                if (Directory.Exists(publishDirectory))
                {
                    Directory.Delete(publishDirectory, recursive: true);
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "Publishing {0} with NativeAOT for {1}", managedProjectFile, runtimeIdentifier);

                string[] arguments =
                {
                    "publish", managedProjectFile,
                    "--configuration", "Release",
                    "--runtime", runtimeIdentifier,
                    "--output", publishDirectory,
                    "--disable-build-servers", "--nologo",
                    "-p:PublishAot=true",
                    "-p:HyperionOutputToBinDir=false",
                    "-p:HyperionWriteGlobalConfig=false",
                    "-p:OutputPath=" + Path.Combine(intermediateDirectory, "bin") + Path.DirectorySeparatorChar,
                    "-p:IntermediateOutputPath=" + Path.Combine(intermediateDirectory, "obj") + Path.DirectorySeparatorChar
                };

                string? firstError = null;

                int exitCode = RunDotNetAsync(managedProjectFile, arguments, toolchainPath, line =>
                {
                    if (firstError == null && ErrorLineRegex.IsMatch(line))
                    {
                        firstError = line;
                    }
                }).GetAwaiter().GetResult();

                if (exitCode != 0)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "NativeAOT publish failed (exit code {0})", exitCode);

                    ShowMessage(MessageBox.Critical("NativeAOT publish failed", firstError ?? "See the log for details.").Button("OK", () => { }));

                    return false;
                }

                Directory.CreateDirectory(outputDirectory);

                foreach (string file in Directory.GetFiles(publishDirectory))
                {
                    if (IsDebugFile(file))
                    {
                        continue;
                    }

                    File.Copy(file, Path.Combine(outputDirectory, Path.GetFileName(file)), overwrite: true);
                }

                foreach (string file in Directory.GetFiles(AppContext.BaseDirectory))
                {
                    if (IsNativeLibrary(file))
                    {
                        File.Copy(file, Path.Combine(outputDirectory, Path.GetFileName(file)), overwrite: true);
                    }
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "NativeAOT publish succeeded");

                return true;
            }
            catch (System.ComponentModel.Win32Exception)
            {
                ShowMessage(MessageBox.Critical(".NET SDK required", "dotnet was not found. Install the .NET 10 SDK to package a C# game.").Button("OK", () => { }));

                return false;
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "NativeAOT publish failed: {0}", ex.Message);

                return false;
            }
            finally
            {
                Interlocked.Exchange(ref _isBuilding, 0);
            }
        }

        private static string GetRuntimeIdentifier()
        {
            string os = OperatingSystem.IsWindows() ? "win" : OperatingSystem.IsMacOS() ? "osx" : "linux";
            string architecture = RuntimeInformation.OSArchitecture == Architecture.Arm64 ? "arm64" : "x64";

            return os + "-" + architecture;
        }

        private static string? FindNativeToolchain(out string? toolchainPath)
        {
            toolchainPath = null;

            if (!OperatingSystem.IsWindows())
            {
                return null;
            }

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

            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = vswherePath,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true
            };

            foreach (string argument in new[] { "-latest", "-products", "*", "-requires", component, "-property", "installationPath" })
            {
                startInfo.ArgumentList.Add(argument);
            }

            using Process? process = Process.Start(startInfo);

            string installationPath = process?.StandardOutput.ReadToEnd().Trim() ?? string.Empty;
            process?.WaitForExit();

            if (installationPath.Length == 0)
            {
                return MissingToolchainMessage;
            }

            toolchainPath = installerDirectory;

            return null;
        }

        private static bool IsDebugFile(string path)
        {
            string extension = Path.GetExtension(path);

            return extension == ".pdb" || extension == ".dbg" || extension == ".dSYM";
        }

        private static bool IsNativeLibrary(string path)
        {
            string extension = Path.GetExtension(path);

            if (extension != ".dll" && extension != ".so" && extension != ".dylib")
            {
                return false;
            }

            try
            {
                using FileStream stream = File.OpenRead(path);
                using PEReader reader = new PEReader(stream);

                return !reader.HasMetadata;
            }
            catch (BadImageFormatException)
            {
                return true;
            }
        }

        private static async Task<int> RunDotNetAsync(string managedProjectFile, string[] arguments, string? extraPath, Action<string> onOutputLine)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = OperatingSystem.IsMacOS() && File.Exists("/usr/local/share/dotnet/dotnet") ? "/usr/local/share/dotnet/dotnet" : "dotnet",
                WorkingDirectory = Path.GetDirectoryName(managedProjectFile)!,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            foreach (string argument in arguments)
            {
                startInfo.ArgumentList.Add(argument);
            }

            // a stale MSBuildSdksPath breaks resolution of Microsoft.NET.Sdk
            startInfo.Environment.Remove("MSBuildSdksPath");

            if (extraPath != null)
            {
                startInfo.Environment["PATH"] = extraPath + Path.PathSeparator + startInfo.Environment["PATH"];
            }

            using Process process = new Process { StartInfo = startInfo };

            void OnLine(string? line)
            {
                if (line == null)
                {
                    return;
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, line);

                onOutputLine(line);
            }

            process.OutputDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);
            process.ErrorDataReceived += (_, eventArgs) => OnLine(eventArgs.Data);

            process.Start();

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            await process.WaitForExitAsync().ConfigureAwait(false);

            return process.ExitCode;
        }

        private static void ShowMessage(MessageBox messageBox)
        {
            Dispatcher.UIThread.Post(() => messageBox.Show());
        }
    }
}
