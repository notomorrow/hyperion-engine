using System.Diagnostics;
using System.Reflection;
using System.Text.Json;

using Hyperion;

namespace GameTests
{
    public static class GamePhases
    {
        // the first launch, which writes the content the second launch reads back
        public const string Write = "write";
        public const string Read = "read";
    }

    [AttributeUsage(AttributeTargets.Method)]
    public sealed class GameTestAttribute : Attribute
    {
        public string Phase { get; }

        public GameTestAttribute(string phase = GamePhases.Write)
        {
            Phase = phase;
        }
    }

    public sealed class GameTestFailure : Exception
    {
        public GameTestFailure(string message)
            : base(message)
        {
        }
    }

    public static class Check
    {
        public static void True(bool condition, string message)
        {
            if (!condition)
            {
                throw new GameTestFailure(message);
            }
        }

        public static void Equal<T>(T expected, T actual, string what)
        {
            if (!EqualityComparer<T>.Default.Equals(expected, actual))
            {
                throw new GameTestFailure($"{what}: expected '{expected}', got '{actual}'");
            }
        }

        public static T NotNull<T>(T? value, string what) where T : class
        {
            if (value == null)
            {
                throw new GameTestFailure($"{what} is null");
            }

            return value;
        }
    }

    public sealed class GameTestContext
    {
        public required Game Game { get; init; }
        public required AssetRegistry AssetRegistry { get; init; }
        public required string Phase { get; init; }
        public required string ContentDirectory { get; init; }

        public string ProjectDirectory => Path.GetFullPath(Path.Combine(ContentDirectory, ".."));
    }

    public sealed record GameTestResult(string Name, bool Passed, string? Message, double DurationMs);

    public sealed record GameTestReport(string Phase, List<GameTestResult> Tests);

    /// Runs the [GameTest] methods of this assembly for the phase given by HYP_GAME_TEST_PHASE, writing a report to
    /// HYP_GAME_TEST_RESULTS for the harness (Tests/GameProjects) to read
    public static class GameTestRunner
    {
        public const string PhaseVariable = "HYP_GAME_TEST_PHASE";
        public const string ResultsVariable = "HYP_GAME_TEST_RESULTS";

        private static readonly JsonSerializerOptions s_jsonOptions = new JsonSerializerOptions
        {
            PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
            WriteIndented = true
        };

        public static void RunFromEnvironment(Game game)
        {
            string phase = Environment.GetEnvironmentVariable(PhaseVariable) ?? GamePhases.Write;
            string? resultsPath = Environment.GetEnvironmentVariable(ResultsVariable);

            GameTestReport report = new GameTestReport(phase, new List<GameTestResult>());

            try
            {
                AssetRegistry assetRegistry = Check.NotNull(game.AssetRegistry, "Game.AssetRegistry");

                GameTestContext context = new GameTestContext
                {
                    Game = game,
                    AssetRegistry = assetRegistry,
                    Phase = phase,
                    ContentDirectory = Path.GetFullPath(assetRegistry.GetRootPath())
                };

                foreach (MethodInfo method in FindTests(phase))
                {
                    report.Tests.Add(RunTest(method, context));
                }
            }
            catch (Exception ex)
            {
                report.Tests.Add(new GameTestResult("Setup", false, ex.ToString(), 0.0));
            }

            int failedCount = report.Tests.Count(result => !result.Passed);

            Logger.Log(failedCount == 0 ? LogLevel.Info : LogLevel.Error, "Game tests ({0}): {1} passed, {2} failed", phase, report.Tests.Count - failedCount, failedCount);

            if (!string.IsNullOrEmpty(resultsPath))
            {
                File.WriteAllText(resultsPath, JsonSerializer.Serialize(report, s_jsonOptions));
            }
        }

        private static IEnumerable<MethodInfo> FindTests(string phase)
        {
            return typeof(GameTestRunner).Assembly.GetTypes()
                .SelectMany(type => type.GetMethods(BindingFlags.Public | BindingFlags.Static))
                .Where(method => method.GetCustomAttribute<GameTestAttribute>()?.Phase == phase)
                .OrderBy(method => method.DeclaringType!.Name)
                .ThenBy(method => method.MetadataToken);
        }

        private static GameTestResult RunTest(MethodInfo method, GameTestContext context)
        {
            string name = $"{method.DeclaringType!.Name}.{method.Name}";
            Stopwatch stopwatch = Stopwatch.StartNew();

            try
            {
                method.Invoke(null, new object[] { context });

                Logger.Log(LogLevel.Info, "[PASS] {0}", name);

                return new GameTestResult(name, true, null, stopwatch.Elapsed.TotalMilliseconds);
            }
            catch (TargetInvocationException ex) when (ex.InnerException != null)
            {
                string message = ex.InnerException is GameTestFailure ? ex.InnerException.Message : ex.InnerException.ToString();

                Logger.Log(LogLevel.Error, "[FAIL] {0}: {1}", name, message);

                return new GameTestResult(name, false, message, stopwatch.Elapsed.TotalMilliseconds);
            }
        }
    }
}
