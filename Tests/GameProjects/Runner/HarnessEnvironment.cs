namespace Hyperion.GameProjectTests;

/// Where the harness finds the engine and writes its projects. Each can be overridden with an environment variable:
///   HYPERION_ROOT            the engine repository (default: found by walking up from the test assembly)
///   HYPERION_BIN_DIR         the engine build to test against (default: Binaries/<Platform>/<Release|RelWithDebInfo|Debug>)
///   HYPERION_GAME_TESTS_DIR  where the game projects are generated and built (default: Build/GameProjectTests)
public static class HarnessEnvironment
{
    public static string PlatformName { get; } =
        OperatingSystem.IsWindows() ? "Windows"
        : OperatingSystem.IsMacOS() ? "Darwin"
        : "Linux";

    public static string ExecutableExtension { get; } = OperatingSystem.IsWindows() ? ".exe" : "";

    public static string RepositoryRoot { get; } = FindRepositoryRoot();

    public static string EngineBinDir { get; } = FindEngineBinDir();

    public static string SdkDir => Path.Combine(EngineBinDir, "Sdk");

    public static string GetCommandletPath(string commandletName) => Path.Combine(EngineBinDir, commandletName + ExecutableExtension);

    public static string TemplatesDir => Path.Combine(RepositoryRoot, "Source", "Templates");

    public static string FixturesDir => Path.Combine(RepositoryRoot, "Tests", "GameProjects", "Fixtures");

    public static string WorkDir { get; } = Path.GetFullPath(
        Environment.GetEnvironmentVariable("HYPERION_GAME_TESTS_DIR")
            ?? Path.Combine(RepositoryRoot, "Build", "GameProjectTests"));

    public static TimeSpan BuildTimeout { get; } = TimeSpan.FromMinutes(20);

    public static TimeSpan RunTimeout { get; } = TimeSpan.FromMinutes(5);

    private static string NativeLibraryName =>
        OperatingSystem.IsWindows() ? "hyperion.dll"
        : OperatingSystem.IsMacOS() ? "libhyperion.dylib"
        : "libhyperion.so";

    private static string FindRepositoryRoot()
    {
        string? root = Environment.GetEnvironmentVariable("HYPERION_ROOT");

        if (!string.IsNullOrEmpty(root))
        {
            return Path.GetFullPath(root);
        }

        for (DirectoryInfo? directory = new DirectoryInfo(AppContext.BaseDirectory); directory != null; directory = directory.Parent)
        {
            if (Directory.Exists(Path.Combine(directory.FullName, "Source", "Templates")) && Directory.Exists(Path.Combine(directory.FullName, "Tests", "GameProjects")))
            {
                return directory.FullName;
            }
        }

        throw new InvalidOperationException($"Could not find the engine repository above {AppContext.BaseDirectory}; set HYPERION_ROOT");
    }

    private static string FindEngineBinDir()
    {
        string? binDir = Environment.GetEnvironmentVariable("HYPERION_BIN_DIR");

        if (!string.IsNullOrEmpty(binDir))
        {
            binDir = Path.GetFullPath(binDir);

            if (!File.Exists(Path.Combine(binDir, NativeLibraryName)))
            {
                throw new InvalidOperationException($"HYPERION_BIN_DIR ({binDir}) has no {NativeLibraryName}");
            }

            return binDir;
        }

        foreach (string configuration in new[] { "Release", "RelWithDebInfo", "Debug" })
        {
            string candidate = Path.Combine(RepositoryRoot, "Binaries", PlatformName, configuration);

            if (File.Exists(Path.Combine(candidate, NativeLibraryName)))
            {
                return candidate;
            }
        }

        throw new InvalidOperationException($"No engine build found under {Path.Combine(RepositoryRoot, "Binaries", PlatformName)}; build the engine or set HYPERION_BIN_DIR");
    }
}
