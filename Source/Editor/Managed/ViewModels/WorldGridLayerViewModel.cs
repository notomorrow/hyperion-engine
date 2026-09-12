using System;
using System.Collections.ObjectModel;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class WorldGridLayerViewModel : ViewModelBase
    {
        private readonly WorldSettingsPanelViewModel _owner;

        internal readonly WorldGrid WorldGrid;
        internal readonly WorldGridLayer Layer;

        public uint LayerTypeId { get; }
        public uint LayerId { get; }

        public string TypeName { get; }

        public bool IsTerrain { get; }

        public string IconKind => IsTerrain ? "Landscape" : "Layers";

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        private string _displayName = string.Empty;
        public string DisplayName
        {
            get => _displayName;
            private set => SetProperty(ref _displayName, value);
        }

        private bool _isExpanded;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        public WorldGridLayerViewModel(
            WorldSettingsPanelViewModel owner,
            WorldGrid worldGrid,
            WorldGridLayer layer,
            uint layerTypeId,
            uint layerId,
            string name,
            string typeName)
        {
            _owner = owner ?? throw new ArgumentNullException(nameof(owner));
            WorldGrid = worldGrid ?? throw new ArgumentNullException(nameof(worldGrid));
            Layer = layer ?? throw new ArgumentNullException(nameof(layer));

            LayerTypeId = layerTypeId;
            LayerId = layerId;

            TypeName = typeName ?? string.Empty;
            IsTerrain = TypeName.Equals("TerrainWorldGridLayer", StringComparison.Ordinal);

            _displayName = name ?? string.Empty;

            BuildPropertyViewModels();
        }

        private void BuildPropertyViewModels()
        {
            Name layerInfoPropertyName = new Name("LayerInfo");
            Name namePropertyName = new Name("Name");

            foreach (Property property in Layer.Class.Properties)
            {
                InspectorPropertyViewModelBase viewModel;

                try
                {
                    viewModel = InspectorViewModelFactory.Create(
                        Layer,
                        property,
                        isReadOnly: false,
                        postWriteCallback: property.Name == layerInfoPropertyName
                            ? CycleLayerInWorldGrid
                            : null,
                        valueChangedCallback: property.Name == namePropertyName
                            ? UpdateDisplayName
                            : null);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Debug, $"WorldSettings: skipping property '{property.Name}' for layer '{DisplayName}': {ex.Message}");
                    continue;
                }

                Properties.Add(viewModel);
            }
        }

        private void CycleLayerInWorldGrid()
        {
            if (!WorldGrid.IsValid || !Layer.IsValid)
            {
                return;
            }

            WorldGrid.RemoveLayer(Layer);
            WorldGrid.AddLayer(Layer);
        }

        private void UpdateDisplayName()
        {
            if (!Layer.IsValid)
            {
                return;
            }

            try
            {
                DisplayName = Layer.GetName().ToString();
            }
            catch
            {
                // layer no longer valid / engine shutting down - keep the old name
            }
        }
    }
}
