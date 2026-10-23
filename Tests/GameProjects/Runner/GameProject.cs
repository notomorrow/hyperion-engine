using System.Text.Json;

namespace Hyperion.GameProjectTests;

public enum GameProjectLanguage
{
    Managed,
    Native
}

public sealed record GameTestResult(string Name, bool Passed, string? Message, double DurationMs);

public sealed record GameTestReport(string Phase, List<GameTestResult> Tests);

public sealed record GameRunResult(ProcessResult Process, GameTestReport? Report);

/// A game project generated from Source/Templates the way the editor's Generate C# / C++ Project does, optionally with
/// a fixture from Tests/GameProjects/Fixtures copied over it
public sealed class GameProject
{
    // the editor's projects keep their content in a Content subfolder, which the templates reach from Binaries/<Platform>/
    private const string ContentDirFromBinaries = "../../Content/";

    private static readonly JsonSerializerOptions s_reportJsonOptions = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };

    public string Name { get; }
    public GameProjectLanguage Language { get; }
    public string Directory { get; }

    public string ClassName => Name + "Game";
    public string SourceDirectory => Path.Combine(Directory, "Source");
    public string ContentDirectory => Path.Combine(Directory, "Content");
    public string BinariesDirectory => Path.Combine(Directory, "Binaries", HarnessEnvironment.PlatformName);
    public string NativeBuildDirectory => Path.Combine(Directory, "Build", HarnessEnvironment.PlatformName);
    public string LogsDirectory => Path.Combine(Directory, "Logs");
    public string ExecutablePath => Path.Combine(BinariesDirectory, Name + HarnessEnvironment.ExecutableExtension);

    public string NativeModulePath => Path.Combine(BinariesDirectory,
        OperatingSystem.IsWindows() ? $"{Name}Game.dll"
        : OperatingSystem.IsMacOS() ? $"lib{Name}Game.dylib"
        : $"lib{Name}Game.so");

    private GameProject(string name, GameProjectLanguage language, string directory)
    {
        Name = name;
        Language = language;
        Directory = directory;
    }

    public static GameProject Create(string name, GameProjectLanguage language, string? fixtureName = null)
    {
        string directory = Path.Combine(HarnessEnvironment.WorkDir, name);

        if (System.IO.Directory.Exists(directory))
        {
            System.IO.Directory.Delete(directory, recursive: true);
        }

        GameProject project = new GameProject(name, language, directory);

        System.IO.Directory.CreateDirectory(project.ContentDirectory);

        foreach ((string templatePath, string outputPath) in project.GetTemplateFiles())
        {
            System.IO.Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);
            File.WriteAllText(outputPath, project.RenderTemplate(templatePath));
        }

        if (language == GameProjectLanguage.Managed)
        {
            project.WriteManagedProjectProps();
        }

        if (fixtureName != null)
        {
            CopyDirectory(Path.Combine(HarnessEnvironment.FixturesDir, fixtureName), directory);
        }

        return project;
    }

    public ProcessResult Build()
    {
        return Language == GameProjectLanguage.Managed ? BuildManaged() : BuildNative();
    }

    /// Launches the built game headless; the test game runs the tests for <paramref name="phase"/> and quits
    public GameRunResult Run(string phase)
    {
        string resultsPath = Path.Combine(LogsDirectory, $"results-{phase}.json");

        if (File.Exists(resultsPath))
        {
            File.Delete(resultsPath);
        }

        System.IO.Directory.CreateDirectory(LogsDirectory);

        Dictionary<string, string> environment = new Dictionary<string, string>
        {
            ["HYP_GAME_TEST_PHASE"] = phase,
            ["HYP_GAME_TEST_RESULTS"] = resultsPath
        };

        ProcessResult process = ProcessRunner.Run(
            $"{Name} ({phase} phase)",
            ExecutablePath,
            new[] { "--headless" },
            BinariesDirectory,
            Path.Combine(LogsDirectory, $"run-{phase}.log"),
            HarnessEnvironment.RunTimeout,
            environment);

        GameTestReport? report = File.Exists(resultsPath)
            ? JsonSerializer.Deserialize<GameTestReport>(File.ReadAllText(resultsPath), s_reportJsonOptions)
            : null;

        return new GameRunResult(process, report);
    }

    // mirrors GetGameProjectFiles() in Source/Editor/EditorNativeModule.cpp
    private IEnumerable<(string TemplatePath, string OutputPath)> GetTemplateFiles()
    {
        if (Language == GameProjectLanguage.Managed)
        {
            string templateDir = Path.Combine(HarnessEnvironment.TemplatesDir, "ManagedGame");

            yield return (Path.Combine(templateDir, "Game.csproj.in"), Path.Combine(SourceDirectory, Name + ".csproj"));
            yield return (Path.Combine(templateDir, "Game.cs.in"), Path.Combine(SourceDirectory, ClassName + ".cs"));
            yield return (Path.Combine(templateDir, "Program.cs.in"), Path.Combine(SourceDirectory, "Program.cs"));
            yield return (Path.Combine(templateDir, "Game.slnx.in"), Path.Combine(Directory, Name + ".slnx"));
        }
        else
        {
            string templateDir = Path.Combine(HarnessEnvironment.TemplatesDir, "NativeGame");

            yield return (Path.Combine(templateDir, "CMakeLists.txt.in"), Path.Combine(SourceDirectory, "CMakeLists.txt"));
            yield return (Path.Combine(templateDir, "Game", "Game.hpp.in"), Path.Combine(SourceDirectory, "Game", ClassName + ".hpp"));
            yield return (Path.Combine(templateDir, "Game", "Game.cpp.in"), Path.Combine(SourceDirectory, "Game", ClassName + ".cpp"));
            yield return (Path.Combine(templateDir, "Launcher", "main.cpp.in"), Path.Combine(SourceDirectory, "Launcher", "main.cpp"));
        }
    }

    // mirrors RenderGameProjectTemplate() in Source/Editor/EditorNativeModule.cpp
    private string RenderTemplate(string templatePath)
    {
        return File.ReadAllText(templatePath)
            .Replace("@NAME@", Name)
            .Replace("@CLASS@", ClassName)
            .Replace("@CONTENT_DIR@", ContentDirFromBinaries);
    }

    // mirrors WriteManagedProjectProps() in Source/Editor/EditorNativeModule.cpp
    private void WriteManagedProjectProps()
    {
        string binDir = HarnessEnvironment.EngineBinDir.Replace('\\', '/').TrimEnd('/');
        string baseDir = HarnessEnvironment.RepositoryRoot.Replace('\\', '/').TrimEnd('/');

        File.WriteAllText(Path.Combine(SourceDirectory, "Hyperion.Local.props"),
            "<Project>\n  <PropertyGroup>\n"
            + $"    <HyperionBinDir>{binDir}/</HyperionBinDir>\n"
            + $"    <HyperionBaseDir>{baseDir}/</HyperionBaseDir>\n"
            + $"    <HyperionSdkDir>{baseDir}/Source/Engine/DotNET/Sdk/</HyperionSdkDir>\n"
            + "  </PropertyGroup>\n</Project>\n");
    }

    // the same command the editor's ManagedGameBuildService runs
    private ProcessResult BuildManaged()
    {
        return ProcessRunner.Run(
            $"Building {Name}",
            "dotnet",
            new[] { "build", Path.Combine(SourceDirectory, Name + ".csproj"), "--configuration", "Release", "--disable-build-servers", "--nologo" },
            SourceDirectory,
            Path.Combine(LogsDirectory, "build.log"),
            HarnessEnvironment.BuildTimeout);
    }

    // the same configure + build as the editor's NativeGameBuildService; the compiler comes from CXX, as for any CMake project
    private ProcessResult BuildNative()
    {
        List<string> configureArguments = new List<string>
        {
            "-S", SourceDirectory,
            "-B", NativeBuildDirectory,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DHYPERION_SDK_DIR=" + HarnessEnvironment.SdkDir.Replace('\\', '/')
        };

        if (FindOnPath("ninja") != null)
        {
            configureArguments.AddRange(new[] { "-G", "Ninja" });
        }

        ProcessResult configure = ProcessRunner.Run(
            $"Configuring {Name}",
            "cmake",
            configureArguments,
            Directory,
            Path.Combine(LogsDirectory, "configure.log"),
            HarnessEnvironment.BuildTimeout);

        if (!configure.Succeeded)
        {
            return configure;
        }

        return ProcessRunner.Run(
            $"Building {Name}",
            "cmake",
            new[] { "--build", NativeBuildDirectory, "--config", "Release", "--parallel", Environment.ProcessorCount.ToString() },
            Directory,
            Path.Combine(LogsDirectory, "build.log"),
            HarnessEnvironment.BuildTimeout);
    }

    private static string? FindOnPath(string program)
    {
        string[] extensions = OperatingSystem.IsWindows() ? new[] { ".exe", ".cmd", ".bat" } : new[] { "" };

        foreach (string directory in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            foreach (string extension in extensions)
            {
                string candidate = Path.Combine(directory, program + extension);

                if (File.Exists(candidate))
                {
                    return candidate;
                }
            }
        }

        return null;
    }

    private static void CopyDirectory(string sourceDirectory, string destinationDirectory)
    {
        if (!System.IO.Directory.Exists(sourceDirectory))
        {
            throw new DirectoryNotFoundException($"Fixture directory {sourceDirectory} does not exist");
        }

        foreach (string sourcePath in System.IO.Directory.EnumerateFiles(sourceDirectory, "*", SearchOption.AllDirectories))
        {
            string destinationPath = Path.Combine(destinationDirectory, Path.GetRelativePath(sourceDirectory, sourcePath));

            System.IO.Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);
            File.Copy(sourcePath, destinationPath, overwrite: true);
        }
    }
}
