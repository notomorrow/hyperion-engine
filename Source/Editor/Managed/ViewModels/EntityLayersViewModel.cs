using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class EntityLayerItemViewModel : ViewModelBase
    {
        public string Name { get; }

        public EntityLayerItemViewModel(string name)
        {
            Name = name;
        }
    }

    public class EntityLayerOptionViewModel : ViewModelBase
    {
        public string Name { get; }

        public EntityLayerOptionViewModel(string name)
        {
            Name = name;
        }
    }

    public class EntityLayersViewModel : ViewModelBase
    {
        private readonly Entity _entity;

        public ObservableCollection<EntityLayerItemViewModel> Layers { get; } = new ObservableCollection<EntityLayerItemViewModel>();
        public ObservableCollection<EntityLayerOptionViewModel> AvailableLayers { get; } = new ObservableCollection<EntityLayerOptionViewModel>();

        private EntityLayerOptionViewModel? _selectedAvailableLayer;
        public EntityLayerOptionViewModel? SelectedAvailableLayer
        {
            get => _selectedAvailableLayer;
            set
            {
                if (SetProperty(ref _selectedAvailableLayer, value) && AddLayerCommand is RelayCommand<EntityLayerOptionViewModel> relayCommand)
                {
                    relayCommand.RaiseCanExecuteChanged();
                }
            }
        }

        private bool _hasLayers;
        public bool HasLayers
        {
            get => _hasLayers;
            private set => SetProperty(ref _hasLayers, value);
        }

        public ICommand AddLayerCommand { get; }
        public ICommand RemoveLayerCommand { get; }
        public ICommand CreateNewLayerCommand { get; }

        public EntityLayersViewModel(Entity entity)
        {
            _entity = entity;

            AddLayerCommand = new RelayCommand<EntityLayerOptionViewModel>(
                option => _ = AddLayerAsync(option),
                option => option != null);

            RemoveLayerCommand = new RelayCommand<EntityLayerItemViewModel>(
                item => _ = RemoveLayerAsync(item));

            CreateNewLayerCommand = new RelayCommand(() =>
            {
                var panel = new AddNewLayerPanelViewModel(result =>
                {
                    if (string.IsNullOrEmpty(result))
                    {
                        return;
                    }

                    _ = CreateAndAssignLayerAsync(result);
                });

                PanelService.Instance.OpenPanel(panel);
            });

            _ = RefreshAsync();
        }

        public async Task RefreshAsync()
        {
            if (_entity == null || !_entity.IsValid)
            {
                return;
            }

            var allLayerNames = new List<string>();
            var currentLayerNames = new List<string>();

            await EngineManager.PostToSimThread(() =>
            {
                World? world = _entity.World;

                if (world == null)
                {
                    return;
                }

                foreach (Name layerName in world.GetLayerNames())
                {
                    string name = layerName.ToString();

                    allLayerNames.Add(name);

                    if (_entity.IsInLayerByName(layerName))
                    {
                        currentLayerNames.Add(name);
                    }
                }
            });

            Dispatcher.UIThread.Post(() =>
            {
                HashSet<string> currentValues = currentLayerNames.ToHashSet();

                Layers.Clear();

                foreach (string name in currentLayerNames)
                {
                    Layers.Add(new EntityLayerItemViewModel(name));
                }

                HasLayers = Layers.Count > 0;

                string? previousSelection = SelectedAvailableLayer?.Name;

                AvailableLayers.Clear();

                foreach (string name in allLayerNames)
                {
                    if (!currentValues.Contains(name))
                    {
                        AvailableLayers.Add(new EntityLayerOptionViewModel(name));
                    }
                }

                SelectedAvailableLayer = previousSelection != null
                    ? AvailableLayers.FirstOrDefault(o => o.Name == previousSelection)
                    : null;
            });
        }

        private async Task AddLayerAsync(EntityLayerOptionViewModel? option)
        {
            if (option == null || _entity == null || !_entity.IsValid)
            {
                return;
            }

            string layerName = option.Name;
            Entity capturedEntity = _entity;

            await EngineManager.PostToSimThread(() =>
            {
                Name name = new Name(layerName);

                capturedEntity.AddToLayerByName(name);

                EditorProject? project = EngineManager.CurrentProject;
                Debug.Assert(project != null, "No active project found when adding an entity to a layer");

                project?.ActionStack?.PushAction(new EditorAction(
                    $"Add Layer: {layerName}",
                    execute: (_, _) => capturedEntity.AddToLayerByName(name),
                    revert: (_, _) => capturedEntity.RemoveFromLayerByName(name)));
            });

            await RefreshAsync();
        }

        private async Task CreateAndAssignLayerAsync(string layerName)
        {
            if (_entity == null || !_entity.IsValid)
            {
                return;
            }

            Entity capturedEntity = _entity;

            await EngineManager.PostToSimThread(() =>
            {
                World? world = capturedEntity.World;

                if (world == null)
                {
                    return;
                }

                // AddToLayerByName only assigns to an already-registered layer (it does not
                // create one), so create it first.
                world.GetOrCreateLayer(new Name(layerName));

                Name name = new Name(layerName);

                capturedEntity.AddToLayerByName(name);

                EditorProject? project = EngineManager.CurrentProject;
                Debug.Assert(project != null, "No active project found when creating a new layer");

                project?.ActionStack?.PushAction(new EditorAction(
                    $"New Layer: {layerName}",
                    execute: (_, _) => capturedEntity.AddToLayerByName(name),
                    revert: (_, _) => capturedEntity.RemoveFromLayerByName(name)));
            });

            await RefreshAsync();
        }

        private async Task RemoveLayerAsync(EntityLayerItemViewModel? item)
        {
            if (item == null || _entity == null || !_entity.IsValid)
            {
                return;
            }

            string layerName = item.Name;
            Entity capturedEntity = _entity;

            await EngineManager.PostToSimThread(() =>
            {
                Name name = new Name(layerName);

                capturedEntity.RemoveFromLayerByName(name);

                EditorProject? project = EngineManager.CurrentProject;
                Debug.Assert(project != null, "No active project found when removing an entity from a layer");

                project?.ActionStack?.PushAction(new EditorAction(
                    $"Remove Layer: {layerName}",
                    execute: (_, _) => capturedEntity.RemoveFromLayerByName(name),
                    revert: (_, _) => capturedEntity.AddToLayerByName(name)));
            });

            await RefreshAsync();
        }
    }
}
