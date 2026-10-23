using Xunit;

// the game builds share the machine, and their logs are easier to follow one at a time
[assembly: CollectionBehavior(DisableTestParallelization = true)]

namespace Hyperion.GameProjectTests;

/// The templates as the editor generates them, unmodified
public class TemplateTests
{
    [Fact]
    public void ManagedTemplate_Builds()
    {
        GameProject project = GameProject.Create("ManagedTemplate", GameProjectLanguage.Managed);

        GameAssert.Succeeded(project.Build());

        Assert.True(File.Exists(project.ExecutablePath), $"{project.ExecutablePath} was not built");
    }

    [Fact]
    public void NativeTemplate_Builds()
    {
        GameProject project = GameProject.Create("NativeTemplate", GameProjectLanguage.Native);

        GameAssert.Succeeded(project.Build());

        Assert.True(File.Exists(project.ExecutablePath), $"{project.ExecutablePath} was not built");
        Assert.True(File.Exists(project.NativeModulePath), $"{project.NativeModulePath} was not built");
    }
}
