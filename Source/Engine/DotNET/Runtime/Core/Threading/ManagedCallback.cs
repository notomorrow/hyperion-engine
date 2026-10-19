using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // A one-shot native callback: a static trampoline plus a GCHandle context, instead of a marshalled delegate
    // (which needs runtime code generation, missing under NativeAOT on the web)
    internal static unsafe class ManagedCallback
    {
        public static delegate* unmanaged<IntPtr, void> Invoke => &InvokeOnce;
        public static delegate* unmanaged<IntPtr, IntPtr, void> InvokeWithArgument => &InvokeOnceWithArgument;

        public static IntPtr Create(Action action)
        {
            return GCHandle.ToIntPtr(GCHandle.Alloc(action));
        }

        public static IntPtr Create(Action<IntPtr> action)
        {
            return GCHandle.ToIntPtr(GCHandle.Alloc(action));
        }

        [UnmanagedCallersOnly]
        private static void InvokeOnce(IntPtr context)
        {
            GCHandle handle = GCHandle.FromIntPtr(context);
            Action action = (Action)handle.Target!;
            handle.Free();

            action();
        }

        [UnmanagedCallersOnly]
        private static void InvokeOnceWithArgument(IntPtr context, IntPtr argument)
        {
            GCHandle handle = GCHandle.FromIntPtr(context);
            Action<IntPtr> action = (Action<IntPtr>)handle.Target!;
            handle.Free();

            action(argument);
        }
    }
}
