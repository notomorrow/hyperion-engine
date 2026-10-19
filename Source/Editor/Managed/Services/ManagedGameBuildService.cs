using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection.PortableExecutable;
using System.Runtime.InteropServices;
using System.IO;
using System.Linq;
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

        private static readonly string CookCommandletExeName = OperatingSystem.IsWindows()
            ? "BlobStorageCookCommandlet.exe"
            : "BlobStorageCookCommandlet";

        private const string ScriptsLibraryName = "HyperionScripts";

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

        public static bool HasScripts(string projectFilePath)
        {
            string scriptsDirectory = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(projectFilePath))!, "Scripts");

            return Directory.Exists(scriptsDirectory) && Directory.EnumerateFiles(scriptsDirectory, "*.cs", SearchOption.AllDirectories).Any();
        }

        public bool BuildPackage(string projectFilePath, string binariesDirectory, string outputDirectory)
        {
            if (!CookPackage(projectFilePath, outputDirectory))
            {
                return false;
            }

            if (FindProjectFile(projectFilePath) == null && !HasScripts(projectFilePath))
            {
                return NativeGameBuildService.Instance.BuildPackage(projectFilePath, outputDirectory, scriptsLinkInputsFile: null);
            }

            return PublishAot(projectFilePath, binariesDirectory, outputDirectory);
        }

        private static bool CookPackage(string projectFilePath, string outputDirectory)
        {
            string editorDirectory = AppContext.BaseDirectory;
            string cookExecutable = Path.Combine(editorDirectory, CookCommandletExeName);

            if (!File.Exists(cookExecutable))
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "The cook commandlet is missing ({0})", cookExecutable);

                ShowMessage(MessageBox.Critical("Cook failed", $"The cook commandlet is missing ({cookExecutable}).").Button("OK", () => { }));

                return false;
            }

            string projectDirectory = Path.GetDirectoryName(Path.GetFullPath(projectFilePath))!;
            string cookCacheDirectory = Path.Combine(projectDirectory, "Build", "PackageCook");
            Directory.CreateDirectory(cookCacheDirectory);

            string[] arguments =
            {
                "--project=" + projectDirectory,
                "--out-cache=" + Path.Combine(outputDirectory, "Cache"),
                "--out-content=" + Path.Combine(outputDirectory, "Content"),
                "--out-engine-content=" + Path.Combine(outputDirectory, "Content", "Engine"),
                "--shader-target=host",
                "--target-platform=" + (OperatingSystem.IsWindows() ? "windows" : OperatingSystem.IsMacOS() ? "mac" : "linux"),
                "--target-config=" + Path.Combine(editorDirectory, "Config", "EngineConfig.json"),
                "--cachedir=" + cookCacheDirectory
            };

            Logger.Log(BuildLogChannel, LogLevel.Info, "Cooking {0} into {1}", projectDirectory, outputDirectory);

            bool isComplete = false;
            string? firstError = null;

            int exitCode = RunProcessAsync(cookExecutable, editorDirectory, arguments, null, line =>
            {
                if (line.Contains("Blob storage cook complete."))
                {
                    isComplete = true;
                }
                else if (firstError == null && line.Contains("[Error]"))
                {
                    firstError = line;
                }
            }).GetAwaiter().GetResult();

            // the commandlet can crash shutting down after a finished cook, so its log decides
            if (!isComplete)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Cook failed (exit code {0})", exitCode);

                ShowMessage(MessageBox.Critical("Cook failed", firstError ?? "See the log for details.").Button("OK", () => { }));

                return false;
            }

            return true;
        }

        private bool PublishAot(string projectFilePath, string binariesDirectory, string outputDirectory)
        {
            string projectDirectory = Path.GetDirectoryName(Path.GetFullPath(projectFilePath))!;
            string? managedProjectFile = FindProjectFile(projectFilePath);
            bool isScriptsLibrary = false;

            if (managedProjectFile == null)
            {
                if (!HasScripts(projectFilePath))
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "No C# project or scripts found for {0}", projectFilePath);

                    return false;
                }

                string? sdkDirectory = FindManagedSdkDirectory();

                if (sdkDirectory == null)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "No C# SDK (Source/Engine/DotNET/Sdk) found near {0}", AppContext.BaseDirectory);

                    return false;
                }

                isScriptsLibrary = Directory.Exists(Path.Combine(projectDirectory, "Source", "Game"));

                string cmakeListsFile = Path.Combine(projectDirectory, "Source", "CMakeLists.txt");

                if (isScriptsLibrary && (!File.Exists(cmakeListsFile) || !File.ReadAllText(cmakeListsFile).Contains("hyperion_link_scripts")))
                {
                    string message = $"Source/CMakeLists.txt must call hyperion_link_scripts({Path.GetFileNameWithoutExtension(projectFilePath)}) after the launcher's target_link_libraries to link the scripts.";

                    Logger.Log(BuildLogChannel, LogLevel.Error, "{0}", message);

                    ShowMessage(MessageBox.Critical("Scripts can't be linked", message).Button("OK", () => { }));

                    return false;
                }

                string projectName = Path.GetFileNameWithoutExtension(projectFilePath);
                string scriptsProjectDirectory = Path.GetFullPath(Path.Combine(binariesDirectory, "..", "AotIntermediate", "ScriptsProject"));
                Directory.CreateDirectory(scriptsProjectDirectory);

                managedProjectFile = Path.Combine(scriptsProjectDirectory, (isScriptsLibrary ? ScriptsLibraryName : projectName) + ".csproj");

                File.WriteAllText(managedProjectFile,
                    "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
                    + "  <PropertyGroup>\n"
                    + "    <HyperionProjectDir>" + projectDirectory.Replace('\\', '/') + "/</HyperionProjectDir>\n"
                    + "    <HyperionBinDir>" + AppContext.BaseDirectory.Replace('\\', '/') + "</HyperionBinDir>\n"
                    + (isScriptsLibrary
                        ? "    <OutputType>Library</OutputType>\n    <NativeLib>Static</NativeLib>\n    <HyperionHostedEntry>true</HyperionHostedEntry>\n"
                        : "    <OutputType>Exe</OutputType>\n    <HyperionScriptsProgram>true</HyperionScriptsProgram>\n")
                    + "  </PropertyGroup>\n"
                    + "  <Import Project=\"" + Path.Combine(sdkDirectory, "HyperionScripts.props").Replace('\\', '/') + "\" />\n"
                    + "</Project>\n");
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
                string scriptsLinkInputsFile = Path.Combine(intermediateDirectory, "ScriptsLinkInputs.txt");

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
                    "-p:HyperionIncludeScripts=true",
                    "-p:HyperionWriteGlobalConfig=false",
                    "-p:HyperionStaticLinkInputsFile=" + scriptsLinkInputsFile,
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

                if (isScriptsLibrary)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Info, "Linking the scripts into the game");

                    return NativeGameBuildService.Instance.BuildPackage(projectFilePath, outputDirectory, scriptsLinkInputsFile);
                }

                foreach (string file in Directory.GetFiles(publishDirectory))
                {
                    if (IsDebugFile(file))
                    {
                        continue;
                    }

                    File.Copy(file, Path.Combine(outputDirectory, Path.GetFileName(file)), overwrite: true);
                }

                foreach (string file in GetRuntimeLibraries())
                {
                    File.Copy(file, Path.Combine(outputDirectory, Path.GetFileName(file)), overwrite: true);
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

        private static string? FindManagedSdkDirectory()
        {
            string? directory = AppContext.BaseDirectory;

            for (int depth = 0; depth < 5 && directory != null; depth++)
            {
                string candidate = Path.Combine(directory, "Source", "Engine", "DotNET", "Sdk");

                if (File.Exists(Path.Combine(candidate, "HyperionScripts.props")))
                {
                    return candidate;
                }

                directory = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(directory));
            }

            return null;
        }

        private static string GetRuntimeIdentifier()
        {
            string? osName = null;
            if (OperatingSystem.IsWindows())
            {
                osName = "win";
            }
            else if (OperatingSystem.IsMacOS())
            {
                osName = "osx";
            }
            else if (OperatingSystem.IsLinux())
            {
                osName = "linux";
            }

            string? archName = null;
            switch (RuntimeInformation.OSArchitecture)
            {
                case Architecture.Arm64:
                    archName = "arm64";
                    break;
                case Architecture.X64:
                    archName = "x64";
                    break;
            }

            Debug.Assert(osName != null && archName != null);

            if (osName == null || archName == null)
            {
                return "invalid-invalid";
            }

            return osName + "-" + archName;
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

        private static IEnumerable<string> GetRuntimeLibraries()
        {
            string runtimeLibrariesFile = Path.Combine(AppContext.BaseDirectory, "Sdk", "RuntimeLibraries.txt");

            if (!File.Exists(runtimeLibrariesFile))
            {
                return Directory.GetFiles(AppContext.BaseDirectory).Where(IsNativeLibrary);
            }

            return File.ReadAllLines(runtimeLibrariesFile)
                .Where(line => !string.IsNullOrWhiteSpace(line))
                .Select(line => Path.Combine(AppContext.BaseDirectory, line.Trim()))
                .Where(File.Exists);
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

        private static Task<int> RunDotNetAsync(string managedProjectFile, string[] arguments, string? extraPath, Action<string> onOutputLine)
        {
            string dotnet = OperatingSystem.IsMacOS() && File.Exists("/usr/local/share/dotnet/dotnet") ? "/usr/local/share/dotnet/dotnet" : "dotnet";

            return RunProcessAsync(dotnet, Path.GetDirectoryName(managedProjectFile)!, arguments, extraPath, onOutputLine);
        }

        private static async Task<int> RunProcessAsync(string fileName, string workingDirectory, string[] arguments, string? extraPath, Action<string> onOutputLine)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = fileName,
                WorkingDirectory = workingDirectory,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            foreach (string argument in arguments)
            {
                startInfo.ArgumentList.Add(argument);
            }

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
