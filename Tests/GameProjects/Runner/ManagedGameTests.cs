using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

/// A C# game (Fixtures/ManagedTest) with its own ObjectBase and AssetObject types, and a script in Content/Scripts that uses them.
/// The game runs its checks in-engine; see Fixtures/ManagedTest/Source/Testing
public class ManagedGameTests : IClassFixture<ManagedTestFixture>
{
    private readonly ManagedTestFixture _fixture;
    private readonly ITestOutputHelper _output;

    public ManagedGameTests(ManagedTestFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    [Fact]
    public void Builds_WithScriptsUsingGameTypes()
    {
        GameAssert.Succeeded(_fixture.Build);

        Assert.True(File.Exists(_fixture.Project.ExecutablePath), $"{_fixture.Project.ExecutablePath} was not built");
    }

    [Fact]
    public void GameTypesScriptsAndAssets_PassInEngine()
    {
        GameAssert.Passed(_fixture.WritePhase, _output);
    }

    [Fact]
    public void SavedAssets_LoadInNewSession()
    {
        GameAssert.Passed(_fixture.ReadPhase, _output);
    }
}
