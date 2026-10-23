namespace Hyperion.GameProjectTests;

/// Generates, builds and runs a test game once for all the tests of a class. The read phase is a second launch of the
/// game, so it loads what the write phase saved from disk rather than from memory
public abstract class GameFixture
{
    private readonly Lazy<GameProject> _project;
    private readonly Lazy<ProcessResult> _build;
    private readonly Lazy<GameRunResult> _writePhase;
    private readonly Lazy<GameRunResult> _readPhase;

    protected GameFixture(string name, GameProjectLanguage language)
    {
        _project = new Lazy<GameProject>(() => GameProject.Create(name, language, fixtureName: name));
        _build = new Lazy<ProcessResult>(() => Project.Build());
        _writePhase = new Lazy<GameRunResult>(() => Project.Run("write"));
        _readPhase = new Lazy<GameRunResult>(() => Project.Run("read"));
    }

    public GameProject Project => _project.Value;

    public ProcessResult Build => _build.Value;

    public GameRunResult WritePhase
    {
        get
        {
            GameAssert.Succeeded(Build);

            return _writePhase.Value;
        }
    }

    public GameRunResult ReadPhase
    {
        get
        {
            Xunit.Assert.True(WritePhase.Report != null, $"The write phase must run first, but it wrote no results.\n{WritePhase.Process.Summary}");

            return _readPhase.Value;
        }
    }
}

public sealed class ManagedTestFixture : GameFixture
{
    public ManagedTestFixture()
        : base("ManagedTest", GameProjectLanguage.Managed)
    {
    }
}

public sealed class NativeTestFixture : GameFixture
{
    public NativeTestFixture()
        : base("NativeTest", GameProjectLanguage.Native)
    {
    }
}
