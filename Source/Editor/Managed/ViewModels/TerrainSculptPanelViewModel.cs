using System;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class TerrainSculptPanelViewModel : EditorPanelViewModel
    {
        private readonly EditorTerrainState _terrainState;

        private bool _isRaiseMode;
        public bool IsRaiseMode
        {
            get => _isRaiseMode;
            private set
            {
                if (SetProperty(ref _isRaiseMode, value) && value)
                {
                    _isLowerMode = false;
                    OnPropertyChanged(nameof(IsLowerMode));

                    _terrainState.SetMode(TerrainSculptMode.Raise);
                }
            }
        }

        private bool _isLowerMode;
        public bool IsLowerMode
        {
            get => _isLowerMode;
            private set
            {
                if (SetProperty(ref _isLowerMode, value) && value)
                {
                    _isRaiseMode = false;
                    OnPropertyChanged(nameof(IsRaiseMode));

                    _terrainState.SetMode(TerrainSculptMode.Lower);
                }
            }
        }

        public ICommand SetRaiseModeCommand { get; }
        public ICommand SetLowerModeCommand { get; }

        private double _radius = 5.0;
        public double Radius
        {
            get => _radius;
            set
            {
                if (SetProperty(ref _radius, value))
                {
                    _terrainState.SetRadius((float)value);
                    OnPropertyChanged(nameof(RadiusText));
                }
            }
        }

        public string RadiusText => $"{_radius:0.#} m";

        private double _strength = 2.0;
        public double Strength
        {
            get => _strength;
            set
            {
                if (SetProperty(ref _strength, value))
                {
                    _terrainState.SetStrength((float)value);
                    OnPropertyChanged(nameof(StrengthText));
                }
            }
        }

        public string StrengthText => $"{_strength:0.00}";

        public TerrainSculptPanelViewModel(EditorTerrainState terrainState, Action? onClosed)
            : base("Terrain Sculpting", onClosed)
        {
            _terrainState = terrainState ?? throw new ArgumentNullException(nameof(terrainState));

            SetRaiseModeCommand = new RelayCommand(() => IsRaiseMode = true);
            SetLowerModeCommand = new RelayCommand(() => IsLowerMode = true);

            _isRaiseMode = terrainState.GetMode() != TerrainSculptMode.Lower;
            _isLowerMode = !_isRaiseMode;
        }
    }
}
