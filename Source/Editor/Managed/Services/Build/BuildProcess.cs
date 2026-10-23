using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading.Tasks;

namespace Hyperion.Editor.Services
{
    internal static class BuildProcess
    {
        public static ProcessStartInfo CreateStartInfo(string fileName, string workingDirectory, IEnumerable<string> arguments)
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

            return startInfo;
        }

        public static async Task<int> RunAsync(ProcessStartInfo startInfo, LogChannel logChannel, Action<string> onOutputLine)
        {
            using Process process = new Process { StartInfo = startInfo };

            void OnLine(string? line)
            {
                if (line == null)
                {
                    return;
                }

                Logger.Log(logChannel, LogLevel.Info, line);

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
    }
}
