using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="AudioPlaybackStatus")]
    public enum AudioPlaybackStatus : byte
    {
        Stopped = 0,
        Paused = 1,
        Playing = 2
    }

    [ClassBinding(Name="AudioLoopMode")]
    public enum AudioLoopMode : byte
    {
        Once = 0,
        Repeat = 1
    }

    [ClassBinding(Name="AudioPlaybackState")]
    [StructLayout(LayoutKind.Sequential)]
    public ref struct AudioPlaybackState
    {
        public AudioPlaybackStatus Status;
        public AudioLoopMode LoopMode;
        public float Speed;
        public float CurrentTime;
    }

    [ClassBinding(Name="AudioComponent")]
    [StructLayout(LayoutKind.Explicit, Size = Layout.Size, Pack = 16)]
    public ref struct AudioComponent : IComponent
    {
        private static class Layout
        {
#if HYP_WEB
            public const int PlaybackState = 4, LastPosition = 16, Timer = 32, Size = 48;
#else
            public const int PlaybackState = 8, LastPosition = 32, Timer = 48, Size = 64;
#endif
        }

        [FieldOffset(0)]
        public Handle<AudioSource> AudioSource;

        [FieldOffset(Layout.PlaybackState)]
        public AudioPlaybackState PlaybackState;

        [FieldOffset(Layout.LastPosition)]
        public Vec3f LastPosition;

        [FieldOffset(Layout.Timer)]
        public float Timer;

        public void Dispose()
        {
            AudioSource.Dispose();
        }
    }
}