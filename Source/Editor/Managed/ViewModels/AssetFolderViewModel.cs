using System;
using System.Collections.ObjectModel;

namespace Hyperion.Editor.ViewModels
{
    public class AssetFolderViewModel : ViewModelBase
    {
        public const string RootName = "Assets";

        public string Path { get; }

        public string Name { get; }

        public AssetFolderViewModel? Parent { get; }

        public ObservableCollection<AssetFolderViewModel> Children { get; } = new ObservableCollection<AssetFolderViewModel>();

        public bool IsRoot => Path.Length == 0;

        public string IconKind => "AssetFolder";

        public AssetFolderViewModel(string path, AssetFolderViewModel? parent)
        {
            Path = path;
            Name = path.Length == 0 ? RootName : GetFolderName(path);
            Parent = parent;
        }

        private bool _isExpanded;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        private bool _isDropTarget;
        public bool IsDropTarget
        {
            get => _isDropTarget;
            set => SetProperty(ref _isDropTarget, value);
        }

        private bool _isEditingName;
        public bool IsEditingName
        {
            get => _isEditingName;
            set => SetProperty(ref _isEditingName, value);
        }

        private string _editingName = string.Empty;
        public string EditingName
        {
            get => _editingName;
            set => SetProperty(ref _editingName, value);
        }

        private int _assetCount;
        public int AssetCount
        {
            get => _assetCount;
            set => SetProperty(ref _assetCount, value);
        }

        public void BeginRename()
        {
            EditingName = Name;
            IsEditingName = true;
        }

        public void CancelRename()
        {
            IsEditingName = false;
        }

        public bool IsSameOrDescendantOf(AssetFolderViewModel ancestor)
        {
            return IsSameOrDescendantPath(Path, ancestor.Path);
        }

        public static bool IsSameOrDescendantPath(string path, string ancestorPath)
        {
            if (ancestorPath.Length == 0 || string.Equals(path, ancestorPath, StringComparison.Ordinal))
            {
                return true;
            }

            return path.StartsWith(ancestorPath + "/", StringComparison.Ordinal);
        }

        public static string GetFolderName(string path)
        {
            int separatorIndex = path.LastIndexOf('/');

            return separatorIndex < 0 ? path : path.Substring(separatorIndex + 1);
        }

        public static string GetParentPath(string path)
        {
            int separatorIndex = path.LastIndexOf('/');

            return separatorIndex < 0 ? string.Empty : path.Substring(0, separatorIndex);
        }

        public static string JoinPath(string parentPath, string name)
        {
            return parentPath.Length == 0 ? name : parentPath + "/" + name;
        }
    }
}
