using System.Collections.Generic;

namespace Hyperion.Editor.Services
{
    public sealed class NativeToolchain
    {
        public string? CMakePath { get; set; }
        public string Generator { get; set; } = "Ninja";
        public string? MakeProgramPath { get; set; }
        public string? CompilerPath { get; set; }

        public string? EnvironmentScriptPath { get; set; }

        public List<string> Missing { get; } = new List<string>();
    }
}
