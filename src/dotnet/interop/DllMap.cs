using System;
using System.Collections;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Xml.Linq;

public static class DllMap
{
    // Register a call-back for native library resolution.
    public static void Register(Assembly assembly)
    {
        Console.WriteLine("Registering DllMap for assembly: " + assembly.FullName);
        NativeLibrary.SetDllImportResolver(assembly, MapAndLoad);
    }

    // The callback: which loads the mapped libray in place of the original
    private static IntPtr MapAndLoad(string libraryName, Assembly assembly, DllImportSearchPath? dllImportSearchPath)
    {
        string mappedName = null;
        mappedName = MapLibraryName(assembly.Location, libraryName, out mappedName) ? mappedName : libraryName;
        return NativeLibrary.Load(mappedName, assembly, dllImportSearchPath);
    }

    // Parse the assembly.xml file, and map the old name to the new name of a library.
    private static bool MapLibraryName(string assemblyLocation, string originalLibName, out string mappedLibName)
    {
        string xmlPath = Path.Combine(Path.GetDirectoryName(assemblyLocation),
            Path.GetFileNameWithoutExtension(assemblyLocation) + ".xml");
        mappedLibName = null;

        Console.WriteLine("Looking for DllMap file: " + xmlPath);

        if (!File.Exists(xmlPath))
            return false;

        XElement root = XElement.Load(xmlPath);
        var map =
            (from el in root.Elements("dllmap")
             where (string)el.Attribute("dll") == originalLibName
             select el).SingleOrDefault();

        if (map != null)
            mappedLibName = map.Attribute("target").Value;

        return (mappedLibName != null);
    }
}