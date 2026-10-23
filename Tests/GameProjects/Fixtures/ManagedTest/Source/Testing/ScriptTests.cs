using System.Reflection;
using System.Runtime.Loader;

using Hyperion;

namespace GameTests
{
    public static class ScriptTests
    {
        private const string ScriptClassName = "HarnessScript";

        // Content/Scripts is compiled into the game assembly here, as it is for a packaged game
        [GameTest]
        public static void CompiledInScript_UsesGameType(GameTestContext context)
        {
            Type scriptType = Check.NotNull(typeof(ManagedTestGame).Assembly.GetType(ScriptClassName), $"{ScriptClassName} in the game assembly");

            CheckScriptReferencesGameTypes(scriptType);

            Script script = (Script)Activator.CreateInstance(scriptType)!;
            scriptType.GetField("Pickup")!.SetValue(script, new TestPickup { PickupName = "Energy Cell", Quantity = 30 });

            script.Update(1.0f / 60.0f);

            Check.Equal(1, (int)scriptType.GetField("UpdateCount")!.GetValue(script)!, "UpdateCount");
            Check.Equal("Energy Cell x30", (string)scriptType.GetField("LastDescription")!.GetValue(script)!, "LastDescription");
        }

        [GameTest]
        public static void CompiledInScript_HasNativeClassWithGameTypedField(GameTestContext context)
        {
            Type scriptType = Check.NotNull(typeof(ManagedTestGame).Assembly.GetType(ScriptClassName), $"{ScriptClassName} in the game assembly");

            Check.True(Class.TryGetDynamicClass(scriptType, out Class cls), $"{ScriptClassName} has no dynamic class");
            Check.True(cls.GetProperty(new Name("Pickup")) != null, $"{ScriptClassName} class has no Pickup property");
            Check.True(cls.GetProperty(new Name("UpdateCount")) != null, $"{ScriptClassName} class has no UpdateCount property");
        }

        // The editor builds Content/Scripts as a module of its own that references the game assembly
        [GameTest]
        public static void EditorScriptCompiler_BuildsScriptsAgainstGameAssembly(GameTestContext context)
        {
            string scriptsDirectory = Path.Combine(context.ContentDirectory, "Scripts");
            string compilerDirectory = Path.Combine(context.ProjectDirectory, "Build", "ScriptCompiler");
            string outputDirectory = Path.Combine(compilerDirectory, "bin");
            string gameAssemblyPath = typeof(ManagedTestGame).Assembly.Location;

            CSharpScriptCompiler compiler = new CSharpScriptCompiler(context.ProjectDirectory, Path.Combine(compilerDirectory, "obj"), outputDirectory);
            compiler.SetReferenceAssemblies(new[] { new ScriptReferenceAssembly(context.ProjectDirectory, gameAssemblyPath) });

            ScriptDesc scriptDesc = new ScriptDesc();

            Check.True(compiler.ResolveAssembly(Path.Combine(scriptsDirectory, ScriptClassName + ".cs"), ref scriptDesc), "The script compiler failed, see the ScriptCompiler log channel");

            string moduleName = Path.GetFileNameWithoutExtension(scriptDesc.AssemblyPath);
            string modulePath = Path.Combine(outputDirectory, moduleName + ".dll");

            Check.True(File.Exists(modulePath), $"{modulePath} was not written");

            // the game assembly and Hyperion.NET.* resolve to the copies already loaded, as they would in the editor
            AssemblyLoadContext loadContext = new AssemblyLoadContext("GameTests.Scripts", isCollectible: true);

            try
            {
                Assembly scriptsAssembly = loadContext.LoadFromAssemblyPath(modulePath);

                Check.True(scriptsAssembly != typeof(ManagedTestGame).Assembly, "The script module is the game assembly");

                Type scriptType = Check.NotNull(scriptsAssembly.GetType(ScriptClassName), $"{ScriptClassName} in {moduleName}");

                CheckScriptReferencesGameTypes(scriptType);
            }
            finally
            {
                loadContext.Unload();
            }
        }

        private static void CheckScriptReferencesGameTypes(Type scriptType)
        {
            Check.True(typeof(Script).IsAssignableFrom(scriptType), $"{scriptType.Name} does not derive from Script");

            FieldInfo pickupField = Check.NotNull(scriptType.GetField("Pickup"), $"{scriptType.Name}.Pickup");

            Check.True(pickupField.FieldType == typeof(TestPickup),
                $"{scriptType.Name}.Pickup is {pickupField.FieldType.AssemblyQualifiedName}, not the game's {typeof(TestPickup).AssemblyQualifiedName}");
        }
    }
}
