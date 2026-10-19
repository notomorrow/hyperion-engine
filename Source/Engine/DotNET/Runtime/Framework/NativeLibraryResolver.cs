using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Hyperion
{
    internal static class NativeLibraryResolver
    {
        [ModuleInitializer]
        internal static void Initialize()
        {
            NativeLibrary.SetDllImportResolver(typeof(NativeLibraryResolver).Assembly, Resolve);
        }

        private static IntPtr Resolve(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
        {
            if (libraryName != "hyperion-core" && libraryName != "hyperion-net")
            {
                return IntPtr.Zero;
            }

            if (NativeLibrary.TryLoad(libraryName, assembly, searchPath, out IntPtr handle))
            {
                return handle;
            }

            return NativeLibrary.TryLoad("hyperion", assembly, searchPath, out handle) ? handle : IntPtr.Zero;
        }
    }
}
