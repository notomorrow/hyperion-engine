using System;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;

namespace Hyperion.Editor.Services
{
    public sealed class WebGameBuildService
    {
        public static WebGameBuildService Instance { get; } = new WebGameBuildService();

        private static readonly LogChannel BuildLogChannel = LogChannel.ByName("WebBuild");

        private static readonly Regex ErrorLineRegex = new Regex(@"(^ERROR:|\berror\b|CMake Error)", RegexOptions.Compiled);

        private static readonly Regex StepLineRegex = new Regex(@"^==> \[(\d+)%\] (.*)$", RegexOptions.Compiled);

        private int _isBuilding;

        private WebGameBuildService()
        {
        }

        public async Task<bool> BuildAsync(string? projectFilePath, bool run)
        {
            if (!OperatingSystem.IsWindows())
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Building for the web from the editor is only supported on Windows for now");

                return false;
            }

            string? projectDirectory = string.IsNullOrEmpty(projectFilePath) ? null : Path.GetDirectoryName(Path.GetFullPath(projectFilePath));

            if (projectDirectory == null)
            {
                ShowMessage(MessageBox.Info("Save the project first", "The project needs to be saved before it can be built for the web.").Button("OK", () => { }));

                return false;
            }

            if (ManagedGameBuildService.FindProjectFile(projectFilePath) != null)
            {
                ShowMessage(MessageBox.Info("Not available for C# projects", "C# games can't be built for the web yet.").Button("OK", () => { }));

                return false;
            }

            string editorDirectory = Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory;
            string scriptPath = Path.GetFullPath(Path.Combine(editorDirectory, "..", "..", "..", "Tools", "Scripts", "BuildWebGame.ps1"));

            if (!File.Exists(scriptPath))
            {
                ShowMessage(MessageBox.Info("Not available in this editor", "Building for the web needs an editor built from the engine's source for now.").Button("OK", () => { }));

                return false;
            }

            if (Interlocked.CompareExchange(ref _isBuilding, 1, 0) != 0)
            {
                Logger.Log(BuildLogChannel, LogLevel.Warning, "A web build is already running");

                return false;
            }

            string projectName = Path.GetFileNameWithoutExtension(projectFilePath) ?? Path.GetFileName(projectDirectory);

            ExternalEditorTask task = new ExternalEditorTask($"Building {projectName} for Web", "Starting");
            task.SetIsForegroundTask(true);
            task.Commit();

            try
            {
                Logger.Log(BuildLogChannel, LogLevel.Info, "Building {0} for the web", projectDirectory);

                string? firstError = null;
                bool isCancelled = false;

                int exitCode = await RunAsync(scriptPath, projectDirectory, run, line =>
                {
                    Match stepMatch = StepLineRegex.Match(line);

                    if (stepMatch.Success)
                    {
                        task.SetStep(stepMatch.Groups[2].Value, int.Parse(stepMatch.Groups[1].Value) / 100.0f);
                    }
                    else if (firstError == null && ErrorLineRegex.IsMatch(line))
                    {
                        firstError = line;
                    }
                }, process =>
                {
                    task.OnCancelAction = () =>
                    {
                        isCancelled = true;

                        try
                        {
                            process.Kill(entireProcessTree: true);
                        }
                        catch (Exception)
                        {
                        }
                    };
                }).ConfigureAwait(false);

                if (isCancelled)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Warning, "Web build cancelled");

                    return false;
                }

                if (exitCode != 0)
                {
                    Logger.Log(BuildLogChannel, LogLevel.Error, "Web build failed (exit code {0})", exitCode);

                    ShowMessage(MessageBox.Critical("Web build failed", firstError ?? "See the log for details.").Button("OK", () => { }));

                    return false;
                }

                Logger.Log(BuildLogChannel, LogLevel.Info, "Web build succeeded: {0}", Path.Combine(projectDirectory, "Binaries", "Web"));

                return true;
            }
            catch (Exception ex)
            {
                Logger.Log(BuildLogChannel, LogLevel.Error, "Web build failed: {0}", ex.Message);

                return false;
            }
            finally
            {
                task.OnCancelAction = null;
                task.Complete();

                Interlocked.Exchange(ref _isBuilding, 0);
            }
        }

        private static async Task<int> RunAsync(string scriptPath, string projectDirectory, bool run, Action<string> onOutputLine, Action<Process> onStarted)
        {
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                WorkingDirectory = projectDirectory,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            startInfo.ArgumentList.Add("-NoProfile");
            startInfo.ArgumentList.Add("-ExecutionPolicy");
            startInfo.ArgumentList.Add("Bypass");
            startInfo.ArgumentList.Add("-File");
            startInfo.ArgumentList.Add(scriptPath);
            startInfo.ArgumentList.Add("-Project");
            startInfo.ArgumentList.Add(projectDirectory);

            if (run)
            {
                startInfo.ArgumentList.Add("-Run");
                startInfo.ArgumentList.Add("-FastLink");
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

            onStarted(process);

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
