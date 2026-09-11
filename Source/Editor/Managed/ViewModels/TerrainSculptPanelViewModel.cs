using System;
using System.Collections.ObjectModel;
using Hyperion;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class TerrainSculptModeEntry
    {
        public string DisplayName { get; }
        public TerrainSculptMode Mode { get; }

        public TerrainSculptModeEntry(string displayName, TerrainSculptMode mode)
        {
            DisplayName = displayName;
            Mode = mode;
        }
    }

    public class TerrainSculptPanelViewModel : EditorPanelViewModel
    {
        private readonly TerrainSculpting _terrainSculpting;

        public ObservableCollection<TerrainSculptModeEntry> Modes { get; } = new();

        private TerrainSculptModeEntry? _selectedMode;
        public TerrainSculptModeEntry? SelectedMode
        {
            get => _selectedMode;
            set
            {
                if (SetProperty(ref _selectedMode, value) && value != null)
                {
                    _terrainSculpting.SetMode(value.Mode);
                    OnPropertyChanged(nameof(IsPaintMode));
                }
            }
        }

        public bool IsPaintMode => SelectedMode?.Mode == TerrainSculptMode.PaintSplat;

        private double _radius = 5.0;
        public double Radius
        {
            get => _radius;
            set
            {
                if (SetProperty(ref _radius, value))
                {
                    _terrainSculpting.SetRadius((float)value);
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
                    _terrainSculpting.SetStrength((float)value);
                    OnPropertyChanged(nameof(StrengthText));
                }
            }
        }

        public string StrengthText => $"{_strength:0.00}";

        private int _paintLayerIndex = 0;
        public int PaintLayerIndex
        {
            get => _paintLayerIndex;
            set
            {
                if (SetProperty(ref _paintLayerIndex, value))
                {
                    _terrainSculpting.SetPaintLayer(value);
                }
            }
        }

        public TerrainSculptPanelViewModel(TerrainSculpting terrainSculpting, Action? onClosed)
            : base("Terrain Sculptor", onClosed)
        {
            _terrainSculpting = terrainSculpting ?? throw new ArgumentNullException(nameof(terrainSculpting));

            Modes.Add(new TerrainSculptModeEntry("Raise", TerrainSculptMode.Raise));
            Modes.Add(new TerrainSculptModeEntry("Lower", TerrainSculptMode.Lower));
            Modes.Add(new TerrainSculptModeEntry("Paint Splat", TerrainSculptMode.PaintSplat));

            _selectedMode = Modes[(int)terrainSculpting.GetMode()];
            _paintLayerIndex = terrainSculpting.GetPaintLayer();
        }
    }
}
