using System;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Windows.Input;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public record struct WeaponTypeEntry(WeaponType WeaponType);

    public class NewWeaponPanelViewModel : EditorPanelViewModel
    {
        private readonly Action<WeaponType?> _onCompleted;

        public ObservableCollection<WeaponTypeEntry> WeaponTypes { get; } = new ObservableCollection<WeaponTypeEntry>();

        private WeaponTypeEntry? _selectedWeaponType;
        public WeaponTypeEntry? SelectedWeaponType
        {
            get => _selectedWeaponType;
            set => SetProperty(ref _selectedWeaponType, value);
        }

        public string? WeaponTypeArgument => Convert.ToString(((byte?)SelectedWeaponType?.WeaponType) ?? 0, CultureInfo.InvariantCulture);

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public NewWeaponPanelViewModel(Action<WeaponType?> onCompleted)
            : base("New Weapon")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);

            for (uint i = 0; i < (uint)WeaponType.Max; i++)
            {
                WeaponType weaponType = (WeaponType)i;
                WeaponTypes.Add(new WeaponTypeEntry(weaponType));
            }

            SelectedWeaponType = WeaponTypes[0];
        }

        private void OnConfirm()
        {
            _onCompleted(SelectedWeaponType?.WeaponType);
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(null);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
