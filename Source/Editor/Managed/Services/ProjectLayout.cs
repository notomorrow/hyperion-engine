using System.IO;

namespace Hyperion.Editor.Services
{
    public static class ProjectLayout
    {
        public static string GetContentDirectory(string projectFilePath)
        {
            string? contentDirectory = EngineManager.EditorGame?.EditorSubsystem?.GetProjectContentDirectory(projectFilePath);

            if (string.IsNullOrEmpty(contentDirectory))
            {
                return Path.GetDirectoryName(Path.GetFullPath(projectFilePath))!;
            }

            return contentDirectory;
        }

        public static string GetModuleName(string projectFilePath)
        {
            string? moduleName = EngineManager.EditorGame?.EditorSubsystem?.GetProjectModuleName(projectFilePath);

            return string.IsNullOrEmpty(moduleName) ? Path.GetFileNameWithoutExtension(projectFilePath) : moduleName;
        }

        public static string GetGameTitle(string projectFilePath)
        {
            string? title = EngineManager.EditorGame?.EditorSubsystem?.GetProjectGameTitle(projectFilePath);

            return string.IsNullOrEmpty(title) ? Path.GetFileNameWithoutExtension(projectFilePath) : title;
        }
    }
}
