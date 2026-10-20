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
    }
}
