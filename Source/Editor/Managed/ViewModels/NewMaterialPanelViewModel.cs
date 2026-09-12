using System;
using System.Windows.Input;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class NewMaterialPanelViewModel : EditorPanelViewModel
    {
        private readonly Action<bool> _onCompleted;

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public NewMaterialPanelViewModel(Action<bool> onCompleted)
            : base("New Material")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);
        }

        private void OnConfirm()
        {
            _onCompleted(true);
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(false);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
