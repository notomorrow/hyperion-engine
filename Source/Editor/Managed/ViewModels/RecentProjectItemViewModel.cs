using System;
using System.IO;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class RecentProjectItemViewModel : ViewModelBase
    {
        private const int MaxDisplayPathSegments = 5;

        private Bitmap? _thumbnail;

        public string FilePath { get; }
        public string Name { get; }
        public string Directory { get; }
        public string DisplayDirectory { get; }
        public bool Exists { get; }
        public bool IsPinned { get; }
        public string PinMenuHeader => IsPinned ? "Unpin" : "Pin";
        public DateTime? LastOpened { get; }
        public string LastOpenedText { get; }

        public Bitmap? Thumbnail
        {
            get => _thumbnail;
            private set => SetProperty(ref _thumbnail, value);
        }

        public RecentProjectItemViewModel(RecentProjectEntry entry)
        {
            FilePath = entry.FilePath;
            Name = RecentProjectsService.GetProjectName(entry.FilePath);
            Directory = Path.GetDirectoryName(entry.FilePath) ?? string.Empty;
            DisplayDirectory = ShortenPath(Directory);
            Exists = File.Exists(entry.FilePath);
            IsPinned = entry.IsPinned;

            LastOpened = entry.LastOpened ?? (Exists ? File.GetLastWriteTime(entry.FilePath) : null);
            LastOpenedText = !Exists ? "Missing" : LastOpened is { } time ? FormatTime(time) : string.Empty;

            if (Exists)
            {
                _ = LoadThumbnailAsync();
            }
        }

        private async Task LoadThumbnailAsync()
        {
            string path = RecentProjectsService.GetThumbnailPath(FilePath);

            Thumbnail = await Task.Run<Bitmap?>(() =>
            {
                try
                {
                    if (!File.Exists(path))
                    {
                        return null;
                    }

                    using FileStream stream = File.OpenRead(path);
                    return new Bitmap(stream);
                }
                catch (Exception e)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to load project thumbnail '{path}': {e.Message}");
                    return null;
                }
            });
        }

        public bool Matches(string filter) =>
            string.IsNullOrWhiteSpace(filter)
            || Name.Contains(filter.Trim(), StringComparison.OrdinalIgnoreCase)
            || Directory.Contains(filter.Trim(), StringComparison.OrdinalIgnoreCase);

        private static string ShortenPath(string directory)
        {
            string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            string path = directory;

            if (!string.IsNullOrEmpty(home) && path.StartsWith(home, OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)
                && (path.Length == home.Length || path[home.Length] == Path.DirectorySeparatorChar || path[home.Length] == Path.AltDirectorySeparatorChar))
            {
                path = "~" + path.Substring(home.Length);
            }

            char separator = Path.DirectorySeparatorChar;
            string[] segments = path.Split([Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar], StringSplitOptions.RemoveEmptyEntries);

            if (segments.Length <= MaxDisplayPathSegments)
            {
                return path;
            }

            string root = path.StartsWith(separator) ? separator + segments[0] : segments[0];

            return string.Join(separator, root, "...", segments[^3], segments[^2], segments[^1]);
        }

        private static string FormatTime(DateTime time)
        {
            TimeSpan age = DateTime.Now - time;

            if (age.TotalMinutes < 1)
                return "Just now";
            if (age.TotalHours < 1)
                return $"{(int)age.TotalMinutes} min ago";
            if (age.TotalDays < 1)
                return $"{(int)age.TotalHours} hr ago";
            if (age.TotalDays < 7)
                return $"{(int)age.TotalDays} day{((int)age.TotalDays == 1 ? "" : "s")} ago";

            return time.Year == DateTime.Now.Year ? time.ToString("MMM d") : time.ToString("MMM d, yyyy");
        }
    }
}
