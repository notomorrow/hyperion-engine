using System.Collections.Generic;
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

        private async void OpenPicker()
        {
            if (DataContext is not ObjectPropertyViewModel vm || vm.IsReadOnly || !PART_TypeButton.IsEffectivelyVisible)
            {
                return;
            }

            IReadOnlyList<ClassPickerEntry> entries = await EngineManager.PostToSimThread<IReadOnlyList<ClassPickerEntry>>(vm.BuildSubclassEntries);

            if (DataContext == vm && PART_TypeButton.IsEffectivelyVisible)
            {
                ClassPicker.Show(PART_TypeButton, entries, vm.CurrentTypeName, allowNone: !vm.IsPending, vm.PickSubclass, PlacementMode.BottomEdgeAlignedRight);
            }
        }
    }
}
