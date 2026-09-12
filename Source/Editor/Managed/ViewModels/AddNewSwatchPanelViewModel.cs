using System;
using System.Windows.Input;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class AddNewSwatchPanelViewModel : EditorPanelViewModel
    {
        private readonly Action<string?> _onCompleted;

        private string _swatchName = "NewSwatch";

        public string SwatchName
        {
            get => _swatchName;
            set => SetProperty(ref _swatchName, value);
        }

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public AddNewSwatchPanelViewModel(Action<string?> onCompleted)
            : base("New Swatch")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);
        }

        private void OnConfirm()
        {
            _onCompleted(SwatchName);
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(null);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
