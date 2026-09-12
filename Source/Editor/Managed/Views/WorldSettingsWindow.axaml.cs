using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views
{
    public partial class WorldSettingsWindow : Window
    {
        public WorldSettingsWindow()
        {
            InitializeComponent();

            // Mirror MainWindow's inspector behavior: property editors commit on focus loss
            // or Enter (the property view models then push undoable EditorActions).
            AddHandler(InputElement.GotFocusEvent, OnPropertyEditorGotFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.LostFocusEvent, OnPropertyEditorLostFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.KeyDownEvent, OnPropertyEditorKeyDown, RoutingStrategies.Bubble);
        }

        private void OnPropertyEditorGotFocus(object? sender, FocusChangedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: InspectorPropertyViewModelBase viewModel })
            {
                viewModel.IsEditing = true;
            }
        }

        private void OnPropertyEditorLostFocus(object? sender, FocusChangedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: InspectorPropertyViewModelBase viewModel })
            {
                viewModel.IsEditing = false;
                viewModel.CommitValue();
            }
        }

        private void OnPropertyEditorKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Key == Key.Return && e.Source is TextBox { DataContext: InspectorPropertyViewModelBase viewModel })
            {
                viewModel.CommitValue();
                e.Handled = true;
            }
            else if (e.Key == Key.Escape && e.Source is not TextBox)
            {
                Close();
            }
        }

        private void OnCloseClick(object? sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
