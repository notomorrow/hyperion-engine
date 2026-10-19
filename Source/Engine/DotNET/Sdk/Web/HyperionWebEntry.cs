using System;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion.Web
{
    // Names the game class; the web build adds it to the game assembly (HyperionWebGameClass)
    [AttributeUsage(AttributeTargets.Assembly)]
    public sealed class HyperionWebGameAttribute : Attribute
    {
        public HyperionWebGameAttribute([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicParameterlessConstructor)] Type gameType)
        {
            GameType = gameType;
        }

        [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicParameterlessConstructor)]
        public Type GameType { get; }
    }

    // Compiled into a C# game built for the web. The page calls Initialize before Hyp_Initialize, then CreateGame
    public static class HyperionWebEntry
    {
        [UnmanagedCallersOnly(EntryPoint = "HypManaged_Initialize")]
        public static void Initialize()
        {
            HyperionApp.InitializeHosted(typeof(HyperionWebEntry).Assembly);
        }

        [UnmanagedCallersOnly(EntryPoint = "HypManaged_CreateGame")]
        public static IntPtr CreateGame()
        {
            HyperionWebGameAttribute? attribute = typeof(HyperionWebEntry).Assembly.GetCustomAttribute<HyperionWebGameAttribute>();

            if (attribute == null)
            {
                Logger.Log(LogLevel.Error, "The game assembly names no game class (HyperionWebGameClass)");

                return IntPtr.Zero;
            }

            return HyperionApp.StartHostedGame((Game)Activator.CreateInstance(attribute.GameType)!);
        }
    }
}
