using System;
using System.Diagnostics;
using System.IO;

namespace Hyperion.Editor.Services
{
    public static class ShellService
    {
        /// <summary>Opens the system file manager with <paramref name="path"/> selected if it is a file, or if it is a directory, at the directory</summary>
        public static void RevealInFileManager(string path)
        {
            try
            {
                bool isFile = File.Exists(path);
                string directory = isFile ? Path.GetDirectoryName(path)! : path;

                if (OperatingSystem.IsWindows())
                {
                    ProcessStartInfo startInfo = new ProcessStartInfo("explorer.exe") { UseShellExecute = false };
                    startInfo.ArgumentList.Add(isFile ? $"/select,{path}" : directory);

                    Process.Start(startInfo)?.Dispose();
                }
                else if (Directory.Exists(directory))
                {
                    ProcessStartInfo startInfo = new ProcessStartInfo(OperatingSystem.IsMacOS() ? "open" : "xdg-open") { UseShellExecute = false };
                    startInfo.ArgumentList.Add(directory);

                    Process.Start(startInfo)?.Dispose();
                }
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, $"Could not show {path}: {ex.Message}");
            }
        }
    }
}
