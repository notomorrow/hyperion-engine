using System;
using System.Runtime.InteropServices;
using Avalonia.Controls;

namespace Hyperion.Editor.Views
{
    public static class DarkTitleBar
    {
        private const int DwmwaUseImmersiveDarkMode = 20;
        private const int DwmwaBorderColor = 34;
        private const int DwmwaCaptionColor = 35;
        private const int DwmwaTextColor = 36;

        [DllImport("dwmapi.dll")]
        private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);

        public static void Apply(Window window, Avalonia.Media.Color caption, Avalonia.Media.Color text, Avalonia.Media.Color border)
        {
            if (!OperatingSystem.IsWindows() || window.TryGetPlatformHandle()?.Handle is not { } hwnd || hwnd == IntPtr.Zero)
            {
                return;
            }

            try
            {
                int darkMode = 1;
                DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkMode, ref darkMode, sizeof(int));

                int captionColor = ToColorRef(caption);
                DwmSetWindowAttribute(hwnd, DwmwaCaptionColor, ref captionColor, sizeof(int));

                int textColor = ToColorRef(text);
                DwmSetWindowAttribute(hwnd, DwmwaTextColor, ref textColor, sizeof(int));

                int borderColor = ToColorRef(border);
                DwmSetWindowAttribute(hwnd, DwmwaBorderColor, ref borderColor, sizeof(int));
            }
            catch (Exception)
            {
            }
        }

        private static int ToColorRef(Avalonia.Media.Color color) => color.R | (color.G << 8) | (color.B << 16);
    }
}
