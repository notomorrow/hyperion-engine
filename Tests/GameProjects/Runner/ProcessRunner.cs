using System.Diagnostics;
using System.Text;

namespace Hyperion.GameProjectTests;

public sealed record ProcessResult(string Description, int ExitCode, bool TimedOut, string Output, string LogPath)
{
    public bool Succeeded => !TimedOut && ExitCode == 0;

    public string Summary
    {
        get
        {
            string status = TimedOut ? "timed out" : $"exited with code {ExitCode}";

            return $"{Description} {status} (full log: {LogPath})\n{Tail(Output, 80)}";
        }
    }

    private static string Tail(string text, int lineCount)
    {
        string[] lines = text.Split('\n');

        return lines.Length <= lineCount ? text : string.Join('\n', lines[^lineCount..]);
    }
}

public static class ProcessRunner
{
    public static ProcessResult Run(
        string description,
        string fileName,
        IEnumerable<string> arguments,
        string workingDirectory,
        string logPath,
        TimeSpan timeout,
        IReadOnlyDictionary<string, string>? environment = null)
    {
        ProcessStartInfo startInfo = new ProcessStartInfo(fileName)
        {
            WorkingDirectory = workingDirectory,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };

        foreach (string argument in arguments)
        {
            startInfo.ArgumentList.Add(argument);
        }

        if (environment != null)
        {
            foreach ((string key, string value) in environment)
            {
                startInfo.Environment[key] = value;
            }
        }

        Directory.CreateDirectory(Path.GetDirectoryName(logPath)!);

        // written as it arrives, so a game that hangs until the timeout still leaves a log to look at
        using StreamWriter logWriter = new StreamWriter(logPath) { AutoFlush = true };

        StringBuilder output = new StringBuilder();

        void AppendLine(string? line)
        {
            if (line == null)
            {
                return;
            }

            lock (output)
            {
                output.AppendLine(line);
                logWriter.WriteLine(line);
            }
        }

        AppendLine($"> {fileName} {string.Join(' ', startInfo.ArgumentList)}");

        using Process process = new Process { StartInfo = startInfo };
        process.OutputDataReceived += (_, e) => AppendLine(e.Data);
        process.ErrorDataReceived += (_, e) => AppendLine(e.Data);

        process.Start();
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        bool timedOut = !process.WaitForExit(timeout);

        if (timedOut)
        {
            process.Kill(entireProcessTree: true);
            process.WaitForExit();
        }
        else
        {
            // flushes the redirected streams
            process.WaitForExit();
        }

        string text;

        lock (output)
        {
            text = output.ToString();
        }

        return new ProcessResult(description, timedOut ? -1 : process.ExitCode, timedOut, text, logPath);
    }
}
