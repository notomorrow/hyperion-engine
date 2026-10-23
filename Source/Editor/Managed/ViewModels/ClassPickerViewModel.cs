using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public sealed record ClassPickerEntry(string Name, int Depth, bool IsAbstract);

    public static class ClassPickerEntries
    {
        public static List<ClassPickerEntry> Build(Class baseClass, IEnumerable<string> concreteClassNames)
        {
            Dictionary<string, Class> classes = new Dictionary<string, Class>();
            Dictionary<string, List<string>> children = new Dictionary<string, List<string>>();
            string baseName = baseClass.Name.ToString();

            foreach (string name in concreteClassNames)
            {
                Class? cls = Class.TryGetClass(name);

                if (!cls.HasValue)
                {
                    continue;
                }

                Class current = cls.Value;

                while (classes.TryAdd(current.Name.ToString(), current) && current != baseClass)
                {
                    Class? parent = current.GetParent();

                    if (!parent.HasValue)
                    {
                        break;
                    }

                    string parentName = parent.Value.Name.ToString();

                    if (!children.TryGetValue(parentName, out List<string>? siblings))
                    {
                        siblings = new List<string>();
                        children.Add(parentName, siblings);
                    }

                    siblings.Add(current.Name.ToString());
                    current = parent.Value;
                }
            }

            List<ClassPickerEntry> entries = new List<ClassPickerEntry>();

            void Visit(string name, int depth)
            {
                if (!children.TryGetValue(name, out List<string>? names))
                {
                    return;
                }

                names.Sort(StringComparer.OrdinalIgnoreCase);

                foreach (string child in names)
                {
                    entries.Add(new ClassPickerEntry(child, depth, classes[child].IsAbstract));
                    Visit(child, depth + 1);
                }
            }

            if (classes.ContainsKey(baseName) && !baseClass.IsAbstract)
            {
                entries.Add(new ClassPickerEntry(baseName, 0, false));
                Visit(baseName, 1);
            }
            else
            {
                Visit(baseName, 0);
            }

            return entries;
        }

        public static List<ClassPickerEntry> BuildDerived(Class baseClass)
        {
            List<string> names = [];
            NameCallbackDelegate callback = (name, _) => names.Add(name);

            NativeBindings.Hyp_GetAllDerivedClassNames(baseClass.Name.ToString(), callback, IntPtr.Zero);

            return Build(baseClass, names);
        }
    }

    public class ClassPickerItemViewModel
    {
        public string? ClassName { get; }
        public bool IsSelectable { get; }
        public bool IsCurrent { get; }
        public Thickness Indent { get; }
        public string IconKind { get; }

        public string MatchBefore { get; }
        public string MatchText { get; }
        public string MatchAfter { get; }

        public ClassPickerItemViewModel(string? className, int depth, bool isSelectable, bool isCurrent, int matchStart = 0, int matchLength = 0)
        {
            ClassName = className;
            IsSelectable = isSelectable;
            IsCurrent = isCurrent;
            Indent = new Thickness(depth * 14, 0, 0, 0);
            IconKind = className == null ? "Close" : isSelectable ? "SymbolClass" : "ChevronDown";

            string text = className ?? "None";

            MatchBefore = text.Substring(0, matchStart);
            MatchText = text.Substring(matchStart, matchLength);
            MatchAfter = text.Substring(matchStart + matchLength);
        }
    }

    public class ClassPickerViewModel : ViewModelBase
    {
        private readonly IReadOnlyList<ClassPickerEntry> _entries;
        private readonly string? _currentClassName;
        private readonly bool _allowNone;

        public ObservableCollection<ClassPickerItemViewModel> Items { get; } = new();

        private string _filter = string.Empty;
        public string Filter
        {
            get => _filter;
            set
            {
                if (SetProperty(ref _filter, value ?? string.Empty))
                {
                    Rebuild();
                }
            }
        }

        private ClassPickerItemViewModel? _selectedItem;
        public ClassPickerItemViewModel? SelectedItem
        {
            get => _selectedItem;
            set => SetProperty(ref _selectedItem, value);
        }

        public bool HasNoMatches => Items.Count == 0;

        public ClassPickerViewModel(IReadOnlyList<ClassPickerEntry> entries, string? currentClassName, bool allowNone)
        {
            _entries = entries;
            _currentClassName = string.IsNullOrEmpty(currentClassName) ? null : currentClassName;
            _allowNone = allowNone;

            Rebuild();
        }

        public void MoveSelection(int delta)
        {
            int index = SelectedItem == null ? (delta > 0 ? -1 : Items.Count) : Items.IndexOf(SelectedItem);

            for (int i = index + delta; i >= 0 && i < Items.Count; i += delta)
            {
                if (Items[i].IsSelectable)
                {
                    SelectedItem = Items[i];
                    return;
                }
            }
        }

        private void Rebuild()
        {
            Items.Clear();

            string filter = _filter.Trim();

            if (filter.Length == 0)
            {
                if (_allowNone)
                {
                    Items.Add(new ClassPickerItemViewModel(null, 0, true, _currentClassName == null));
                }

                foreach (ClassPickerEntry entry in _entries)
                {
                    Items.Add(new ClassPickerItemViewModel(entry.Name, entry.Depth, !entry.IsAbstract, entry.Name == _currentClassName));
                }

                SelectedItem = Items.FirstOrDefault(item => item.IsCurrent && item.IsSelectable)
                    ?? Items.FirstOrDefault(item => item.IsSelectable);
            }
            else
            {
                var matches = _entries
                    .Where(entry => !entry.IsAbstract)
                    .Select(entry => (Entry: entry, Index: entry.Name.IndexOf(filter, StringComparison.OrdinalIgnoreCase)))
                    .Where(match => match.Index >= 0)
                    .OrderBy(match => match.Index == 0 ? 0 : 1)
                    .ThenBy(match => match.Entry.Name, StringComparer.OrdinalIgnoreCase);

                foreach (var (entry, index) in matches)
                {
                    Items.Add(new ClassPickerItemViewModel(entry.Name, 0, true, entry.Name == _currentClassName, index, filter.Length));
                }

                SelectedItem = Items.FirstOrDefault();
            }

            OnPropertyChanged(nameof(HasNoMatches));
        }
    }
}
