namespace Hyperion
{
    public static class HyperionScriptsProgram
    {
        public static int Main(string[] args)
        {
            return HyperionApp.Run<Game>(args, new HyperionAppOptions {
                UseGlobalConfigArgs = true,
                GameAssembly = typeof(HyperionScriptsProgram).Assembly
            });
        }
    }
}
