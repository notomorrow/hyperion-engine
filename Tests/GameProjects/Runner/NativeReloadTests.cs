using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

/// Builds Fixtures/NativeTest three times, and loads the builds in turn into one editor project the way the editor does when the
/// game is rebuilt while it's open (Source/Editor/EditorGameTestCommandlet.cpp, no editor UI): the first build, one that fails to
/// load (Fixtures/NativeTestReload/Broken), then one whose game classes changed (Fixtures/NativeTestReload/V2)
public sealed class NativeReloadFixture
{
    private const string TestClassName = "NativeTestEditorTests";

    private readonly Lazy<EditorTestRunResult> _run = new Lazy<EditorTestRunResult>(BuildAndReload);

    public EditorTestRunResult Run => _run.Value;

    private static EditorTestRunResult BuildAndReload()
    {
        GameProject project = GameProject.Create("NativeTest", GameProjectLanguage.Native, fixtureName: "NativeTest", directoryName: "NativeReload");

        string BuildVariant(string label, string? fixtureName)
        {
            if (fixtureName != null)
            {
                project.ApplyFixture(fixtureName);
            }

            GameAssert.Succeeded(project.Build());

            return project.SaveNativeModuleBuild(label);
        }

        string firstBuild = BuildVariant("First", null);
        string brokenBuild = BuildVariant("Broken", "NativeTestReload/Broken");
        string changedBuild = BuildVariant("Changed", "NativeTestReload/V2");

        return project.RunEditorTests(
            "reload",
            new[] { firstBuild, brokenBuild, changedBuild },
            new[] { "reload-setup", "reload-broken", "reload-verify" },
            TestClassName,
            releasePhase: "reload-release");
    }
}

public class NativeReloadTests : IClassFixture<NativeReloadFixture>
{
    private readonly NativeReloadFixture _fixture;
    private readonly ITestOutputHelper _output;

    public NativeReloadTests(NativeReloadFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    private EditorTestReport Report
    {
        get
        {
            _output.WriteLine($"Log: {_fixture.Run.Process.LogPath}");

            Assert.True(_fixture.Run.Report != null, $"The reload host wrote no results.\n{_fixture.Run.Process.Summary}");
            Assert.True(_fixture.Run.Report!.Steps.Count == 3, $"The reload host stopped after {_fixture.Run.Report.Steps.Count} of 3 builds.\n{_fixture.Run.Process.Summary}");

            return _fixture.Run.Report;
        }
    }

    private void AssertLoadedAndPassed(EditorTestStep step)
    {
        Assert.True(step.Loaded, $"{step.Build} didn't load: {step.LoadError}");
        Assert.True(step.Report != null, $"{step.Build} loaded, but the {step.Phase} phase wrote no report");

        GameAssert.TestsPassed(step.Report!, _output);
    }

    [Fact]
    public void FirstBuild_LoadsWithObjectsOfGameClasses()
    {
        AssertLoadedAndPassed(Report.Steps[0]);
    }

    [Fact]
    public void BrokenBuild_FailsToLoad()
    {
        EditorTestStep step = Report.Steps[1];

        Assert.False(step.Loaded, $"{step.Build} names a Game class that doesn't exist, but it loaded");
        Assert.Contains("NativeTestMissingGame", step.LoadError ?? "");
    }

    // the objects of the first build wait through the broken build, and move to the first build that loads
    [Fact]
    public void ChangedBuild_RebuildsLiveObjects()
    {
        AssertLoadedAndPassed(Report.Steps[2]);
    }

    [Fact]
    public void PreviousBuild_StaysLoadedWhileObjectsItMadeLive()
    {
        Assert.True(Report.RetiredBeforeRelease, "The first build unloaded while objects it made (a NativeTestObsoleteEntity, which the changed build removes, and engine objects using its vtables) were still in the scene");
    }

    [Fact]
    public void PreviousBuild_UnloadsOnceUnused()
    {
        Assert.True(Report.Release != null, "The release phase wrote no report");

        GameAssert.TestsPassed(Report.Release!, _output);

        Assert.True(Report.Unloaded, "The first build stayed loaded after everything it made was removed");
    }

    [Fact]
    public void ReloadHost_ExitsCleanly()
    {
        Assert.NotNull(Report);

        GameAssert.Succeeded(_fixture.Run.Process);
    }
}
