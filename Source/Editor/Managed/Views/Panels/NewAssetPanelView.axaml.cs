using Avalonia.Controls;
using Avalonia.Interactivity;
using Hyperion.Editor.ViewModels;
using Hyperion.Editor.Views.Inspector;

namespace Hyperion.Editor.Views.Panels
{
    public partial class NewAssetPanelView : UserControl
    {
        public NewAssetPanelView()
        {
            InitializeComponent();

            PART_ClassButton.Click += OnClassButtonClicked;
        }

        private void OnClassButtonClicked(object? sender, RoutedEventArgs e)
        {
            if (DataContext is not NewAssetPanelViewModel vm)
            {
                return;
            }

            ClassPicker.Show(PART_ClassButton, vm.ClassEntries, vm.SelectedClassName, allowNone: false, className =>
            {
                if (className != null)
                {
                    vm.SelectedClassName = className;
                }
            });
        }
    }
}
