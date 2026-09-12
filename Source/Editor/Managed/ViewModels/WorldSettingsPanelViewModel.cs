using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    /// <summary>
    /// View model for the World Settings window. Unlike most editor panels this is not a
    /// PanelService-managed dynamic panel - it backs a standalone modal window (similar in
    /// spirit to VS Code's settings editor) and currently exposes management of the active
    /// world's WorldGrid layers (terrain etc.).
    /// </summary>
    public class WorldSettingsPanelViewModel : ViewModelBase
    {
        public ObservableCollection<WorldGridLayerViewModel> Layers { get; } = new ObservableCollection<WorldGridLayerViewModel>();

        public ObservableCollection<WorldGridLayerViewModel> FilteredLayers { get; } = new ObservableCollection<WorldGridLayerViewModel>();

        private string? _searchText;
        public string? SearchText
        {
            get => _searchText;
            set
            {
                if (SetProperty(ref _searchText, value))
                {
                    ApplyFilter();
                }
            }
        }

        private bool _hasWorldGrid;
        public bool HasWorldGrid
        {
            get => _hasWorldGrid;
            private set
            {
                if (SetProperty(ref _hasWorldGrid, value))
                {
                    OnPropertyChanged(nameof(ShowNoLayersHint));
                }
            }
        }

        public bool HasLayers => FilteredLayers.Count > 0;

        public bool ShowNoLayersHint => HasWorldGrid && !HasLayers;

        private bool _isRefreshing;
        public bool IsRefreshing
        {
            get => _isRefreshing;
            private set => SetProperty(ref _isRefreshing, value);
        }

        public ICommand RefreshCommand { get; }
        public ICommand AddTerrainCommand { get; }
        public ICommand RemoveLayerCommand { get; }

        public WorldSettingsPanelViewModel()
        {
            RefreshCommand = new RelayCommand(() => _ = RefreshAsync());
            AddTerrainCommand = new RelayCommand(() => _ = AddTerrainAsync());
            RemoveLayerCommand = new RelayCommand<WorldGridLayerViewModel>(layer => _ = RemoveLayerAsync(layer));

            _ = RefreshAsync();
        }

        public async Task RefreshAsync()
        {
            IsRefreshing = true;

            var snapshots = new List<LayerSnapshot>();
            bool hasWorldGrid = false;

            await EngineManager.PostToSimThread(() =>
            {
                World? world = EngineManager.CurrentProject?.World;

                if (world == null)
                {
                    return;
                }

                WorldGrid? worldGrid = world.WorldGrid;

                if (worldGrid == null || !worldGrid.IsValid)
                {
                    return;
                }

                hasWorldGrid = true;

                Array layers = worldGrid.GetLayers();

                foreach (WorldGridLayer? layer in layers)
                {
                    if (layer == null || !layer.IsValid)
                    {
                        continue;
                    }

                    Name layerName = layer.GetName();
                    string typeName = layer.Class.Name.ToString();
                    ObjIdBase id = layer.Id;

                    snapshots.Add(new LayerSnapshot(
                        worldGrid,
                        layer,
                        id.TypeId.Value,
                        id.Value,
                        layerName.ToString(),
                        typeName));
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                HasWorldGrid = hasWorldGrid;

                Layers.Clear();

                foreach (LayerSnapshot snapshot in snapshots)
                {
                    Layers.Add(new WorldGridLayerViewModel(
                        this,
                        snapshot.WorldGrid,
                        snapshot.Layer,
                        snapshot.LayerTypeId,
                        snapshot.LayerId,
                        snapshot.Name,
                        snapshot.TypeName));
                }

                ApplyFilter();
            });

            IsRefreshing = false;
        }

        private async Task AddTerrainAsync()
        {
            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommandAddTerrainLayer"));

            // The command is enqueued on the sim thread; this refresh is posted after it,
            // so it will observe the newly added layer.
            await RefreshAsync();
        }

        private async Task RemoveLayerAsync(WorldGridLayerViewModel? layer)
        {
            if (layer == null)
            {
                return;
            }

            WorldGrid worldGrid = layer.WorldGrid;
            WorldGridLayer worldGridLayer = layer.Layer;
            string label = !string.IsNullOrWhiteSpace(layer.DisplayName) ? layer.DisplayName.Trim() : layer.TypeName;

            await EngineManager.PostToSimThread(() =>
            {
                if (!worldGrid.IsValid || !worldGridLayer.IsValid)
                {
                    return;
                }

                worldGrid.RemoveLayer(worldGridLayer);

                EditorProject? project = EngineManager.CurrentProject;

                project?.ActionStack?.PushAction(new EditorAction(
                    $"Remove World Grid Layer: {label}",
                    execute: (_, _) => worldGrid.RemoveLayer(worldGridLayer),
                    revert: (_, _) => worldGrid.AddLayer(worldGridLayer)));
            });

            await RefreshAsync();
        }

        private void ApplyFilter()
        {
            string? search = SearchText?.Trim();

            IEnumerable<WorldGridLayerViewModel> filtered = Layers;

            if (!string.IsNullOrEmpty(search))
            {
                filtered = Layers.Where(layer =>
                    layer.DisplayName.Contains(search, StringComparison.OrdinalIgnoreCase)
                    || layer.TypeName.Contains(search, StringComparison.OrdinalIgnoreCase));
            }

            FilteredLayers.Clear();

            foreach (WorldGridLayerViewModel layer in filtered)
            {
                FilteredLayers.Add(layer);
            }

            OnPropertyChanged(nameof(HasLayers));
            OnPropertyChanged(nameof(ShowNoLayersHint));
        }

        private sealed class LayerSnapshot
        {
            public WorldGrid WorldGrid { get; }
            public WorldGridLayer Layer { get; }
            public uint LayerTypeId { get; }
            public uint LayerId { get; }
            public string Name { get; }
            public string TypeName { get; }

            public LayerSnapshot(
                WorldGrid worldGrid,
                WorldGridLayer layer,
                uint layerTypeId,
                uint layerId,
                string name,
                string typeName)
            {
                WorldGrid = worldGrid;
                Layer = layer;
                LayerTypeId = layerTypeId;
                LayerId = layerId;
                Name = name;
                TypeName = typeName;
            }
        }
    }
}
