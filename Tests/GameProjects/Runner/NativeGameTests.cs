using Xunit;
using Xunit.Abstractions;

namespace Hyperion.GameProjectTests;

/// A C++ game (Fixtures/NativeTest) with its own ObjectBase and AssetObject types, registered by hand as game code must
public class NativeGameTests : IClassFixture<NativeTestFixture>
{
    private readonly NativeTestFixture _fixture;
    private readonly ITestOutputHelper _output;

    public NativeGameTests(NativeTestFixture fixture, ITestOutputHelper output)
    {
        _fixture = fixture;
        _output = output;
    }

    [Fact]
    public void Builds()
    {
        GameAssert.Succeeded(_fixture.Build);

        Assert.True(File.Exists(_fixture.Project.ExecutablePath), $"{_fixture.Project.ExecutablePath} was not built");
        Assert.True(File.Exists(_fixture.Project.NativeModulePath), $"{_fixture.Project.NativeModulePath} was not built");
    }

    [Fact]
    public void GameTypesAndAssets_PassInEngine()
    {
        GameAssert.Passed(_fixture.WritePhase, _output);
    }

    [Fact]
    public void SavedAssets_LoadInNewSession()
    {
        GameAssert.Passed(_fixture.ReadPhase, _output);
    }
}
