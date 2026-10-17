using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.VisualTree;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views
{
    public partial class StartScreenWindow : Window
    {
        public StartScreenWindow()
        {
            InitializeComponent();

            Opened += OnWindowOpened;
        }

        private StartScreenViewModel? ViewModel => DataContext as StartScreenViewModel;

        private void OnWindowOpened(object? sender, EventArgs e)
        {
            DarkTitleBar.Apply(this, GetColor("Color.Background"), GetColor("Color.Text"), GetColor("Color.Border"));

            RecentList.Focus();

            if (RecentList.SelectedItem != null)
            {
                RecentList.ScrollIntoView(RecentList.SelectedItem);
                (RecentList.ContainerFromItem(RecentList.SelectedItem) as ListBoxItem)?.Focus();
            }
        }

        private Avalonia.Media.Color GetColor(string key) =>
            this.TryFindResource(key, ActualThemeVariant, out object? value) && value is Avalonia.Media.Color color ? color : Avalonia.Media.Colors.Black;

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Escape && ViewModel is { IsBusy: false } viewModel)
            {
                if (!string.IsNullOrEmpty(viewModel.FilterText))
                {
                    viewModel.FilterText = string.Empty;
                }
                else
                {
                    viewModel.NewProjectCommand.Execute(null);
                }

                e.Handled = true;
            }

            base.OnKeyDown(e);
        }

        protected override void OnClosed(EventArgs e)
        {
            (DataContext as IDisposable)?.Dispose();

            base.OnClosed(e);
        }

        private void OnRecentListKeyDown(object? sender, KeyEventArgs e)
        {
            if (ViewModel is not { } viewModel || viewModel.SelectedProject is not { } selected)
            {
                return;
            }

            switch (e.Key)
            {
                case Key.Enter:
                    viewModel.OpenRecentProjectCommand.Execute(selected);
                    e.Handled = true;
                    break;
                case Key.Delete:
                    viewModel.RemoveRecentProjectCommand.Execute(selected);
                    e.Handled = true;
                    break;
            }
        }

        private void OnSearchBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if (ViewModel is not { } viewModel)
            {
                return;
            }

            switch (e.Key)
            {
                case Key.Enter when viewModel.SelectedProject != null:
                    viewModel.OpenRecentProjectCommand.Execute(viewModel.SelectedProject);
                    e.Handled = true;
                    break;
                case Key.Down when viewModel.SelectedProject != null:
                    (RecentList.ContainerFromItem(viewModel.SelectedProject) as ListBoxItem)?.Focus();
                    e.Handled = true;
                    break;
            }
        }

        private void OnRecentProjectTapped(object? sender, TappedEventArgs e)
        {
            if (e.Source is Visual source && source.FindAncestorOfType<Button>(includeSelf: true) != null)
            {
                return;
            }

            if ((sender as Control)?.DataContext is RecentProjectItemViewModel item)
            {
                ViewModel?.OpenRecentProjectCommand.Execute(item);
            }
        }

        private void OnOpenMenuClicked(object? sender, RoutedEventArgs e)
        {
            if ((sender as Control)?.DataContext is RecentProjectItemViewModel item)
            {
                ViewModel?.OpenRecentProjectCommand.Execute(item);
            }
        }

        private void OnShowInExplorerMenuClicked(object? sender, RoutedEventArgs e)
        {
            if ((sender as Control)?.DataContext is RecentProjectItemViewModel item)
            {
                ViewModel?.ShowInExplorerCommand.Execute(item);
            }
        }

        private void OnTogglePinMenuClicked(object? sender, RoutedEventArgs e)
        {
            if ((sender as Control)?.DataContext is RecentProjectItemViewModel item)
            {
                ViewModel?.TogglePinCommand.Execute(item);
            }
        }

        private void OnRemoveMenuClicked(object? sender, RoutedEventArgs e)
        {
            if ((sender as Control)?.DataContext is RecentProjectItemViewModel item)
            {
                ViewModel?.RemoveRecentProjectCommand.Execute(item);
            }
        }
    }
}
