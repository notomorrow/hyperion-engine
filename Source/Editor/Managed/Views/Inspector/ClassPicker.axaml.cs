using System;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class ClassPicker : UserControl
    {
        public event Action<string?>? Picked;
        public event Action? Dismissed;

        public ClassPicker()
        {
            InitializeComponent();

            PART_Search.AddHandler(KeyDownEvent, OnSearchKeyDown, RoutingStrategies.Tunnel);
            PART_List.Tapped += OnListTapped;
            PART_List.SelectionChanged += OnListSelectionChanged;
        }

        public static void Show(Control anchor, IReadOnlyList<ClassPickerEntry> entries, string? currentClassName, bool allowNone, Action<string?> onPicked,
            PlacementMode placement = PlacementMode.BottomEdgeAlignedLeft)
        {
            ClassPicker picker = new ClassPicker
            {
                DataContext = new ClassPickerViewModel(entries, currentClassName, allowNone)
            };

            Flyout flyout = new Flyout
            {
                Content = picker,
                Placement = placement,
                ShowMode = FlyoutShowMode.Standard
            };

            picker.Picked += className =>
            {
                flyout.Hide();
                onPicked(className);
            };

            picker.Dismissed += () => flyout.Hide();

            flyout.Opened += (_, _) => Dispatcher.UIThread.Post(picker.FocusSearch, DispatcherPriority.Input);

            flyout.ShowAt(anchor);
        }

        private void FocusSearch()
        {
            PART_Search.Focus();

            if (PART_List.SelectedItem != null)
            {
                PART_List.ScrollIntoView(PART_List.SelectedItem);
            }
        }

        private void OnSearchKeyDown(object? sender, KeyEventArgs e)
        {
            if (DataContext is not ClassPickerViewModel vm)
            {
                return;
            }

            switch (e.Key)
            {
                case Key.Down:
                    vm.MoveSelection(1);
                    e.Handled = true;
                    break;
                case Key.Up:
                    vm.MoveSelection(-1);
                    e.Handled = true;
                    break;
                case Key.Enter:
                    Commit(vm.SelectedItem);
                    e.Handled = true;
                    break;
                case Key.Escape:
                    Dismissed?.Invoke();
                    e.Handled = true;
                    break;
            }
        }

        private void OnListTapped(object? sender, TappedEventArgs e)
        {
            if (e.Source is Visual source && source.FindAncestorOfType<ListBoxItem>(includeSelf: true) is { DataContext: ClassPickerItemViewModel item })
            {
                Commit(item);
            }
        }

        private void OnListSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            if (PART_List.SelectedItem != null)
            {
                PART_List.ScrollIntoView(PART_List.SelectedItem);
            }
        }

        private void Commit(ClassPickerItemViewModel? item)
        {
            if (item is { IsSelectable: true })
            {
                Picked?.Invoke(item.ClassName);
            }
        }
    }
}
