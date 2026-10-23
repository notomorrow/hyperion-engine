using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class SubclassPropertyEditor : UserControl
    {
        public SubclassPropertyEditor()
        {
            InitializeComponent();

            PART_TypeButton.Click += OnTypeButtonClicked;
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);

            if (DataContext is ObjectPropertyViewModel { IsPending: true })
            {
                Dispatcher.UIThread.Post(OpenPicker, DispatcherPriority.Loaded);
            }
        }

        private void OnTypeButtonClicked(object? sender, RoutedEventArgs e)
        {
            OpenPicker();
        }

        private void OpenPicker()
        {
            if (DataContext is not ObjectPropertyViewModel vm || vm.IsReadOnly || !PART_TypeButton.IsEffectivelyVisible)
            {
                return;
            }

            ClassPicker.Show(PART_TypeButton, vm.SubclassEntries, vm.CurrentTypeName, allowNone: !vm.IsPending, vm.PickSubclass, PlacementMode.BottomEdgeAlignedRight);
        }
    }
}
