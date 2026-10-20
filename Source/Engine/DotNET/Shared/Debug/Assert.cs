using System;
using System.Runtime.InteropServices;
using System.Diagnostics;

namespace Hyperion
{
    public class Assert
    {
        // @TODO Refactor to use [CallerMemberName] and friends.

        public static void Throw(bool condition, string? message = null, params object?[] args)
        {
            if (!condition)
            {
                StackFrame frame = new StackFrame(1, true);

                if (message == null || message.Length == 0)
                {
                    Assert_Throw(null, DiagnosticMethodInfo.Create(frame)?.Name, (uint)frame.GetFileLineNumber());
                }
                else
                {
                    if (args == null)
                    {
                        args = Array.Empty<object>();
                    }

                    try
                    {
                        string formattedMessage = string.Format(message, args);

                        Assert_Throw(message, DiagnosticMethodInfo.Create(frame)?.Name, (uint)frame.GetFileLineNumber());
                    }
                    catch (FormatException)
                    {
                        // Do nothing, just log as is
                        Assert_Throw(message, DiagnosticMethodInfo.Create(frame)?.Name, (uint)frame.GetFileLineNumber());
                    }
                }
            }
        }

        [DllImport("hyperion", EntryPoint = "Assert_Throw")]
        private static extern void Assert_Throw(
            [MarshalAs(UnmanagedType.LPStr)] string? message,
            [MarshalAs(UnmanagedType.LPStr)] string? funcName,
            uint line);
    }
}