using Hyperion;

using GameTests;

public class ManagedTestGame : Game
{
    protected override void OnLaunch()
    {
        Logger.Log(LogLevel.Info, "ManagedTestGame launched");

        GameTestRunner.RunFromEnvironment(this);

        HyperionApp.Quit();
    }
}
