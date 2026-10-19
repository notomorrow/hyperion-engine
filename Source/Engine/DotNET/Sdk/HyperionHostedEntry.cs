using System;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Hyperion.Hosting
{
    [AttributeUsage(AttributeTargets.Assembly)]
    public sealed class HyperionHostedGameAttribute : Attribute
    {
        public HyperionHostedGameAttribute([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicParameterlessConstructor)] Type gameType)
        {
            GameType = gameType;
        }

        [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicParameterlessConstructor)]
        public Type GameType { get; }
    }

    public static class HyperionHostedEntry
    {
        [UnmanagedCallersOnly(EntryPoint = "HypManaged_Initialize")]
        public static void Initialize()
        {
            HyperionApp.InitializeHosted(typeof(HyperionHostedEntry).Assembly);
        }

        [UnmanagedCallersOnly(EntryPoint = "HypManaged_CreateGame")]
        public static IntPtr CreateGame()
        {
            HyperionHostedGameAttribute? attribute = typeof(HyperionHostedEntry).Assembly.GetCustomAttribute<HyperionHostedGameAttribute>();

            if (attribute == null)
            {
                Logger.Log(LogLevel.Error, "The game assembly names no game class (HyperionHostedGameClass)");

                return IntPtr.Zero;
            }

            return HyperionApp.StartHostedGame((Game)Activator.CreateInstance(attribute.GameType)!);
        }
    }
}
