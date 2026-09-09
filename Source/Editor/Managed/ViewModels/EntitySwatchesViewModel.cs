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

namespace Hyperion.Editor.ViewModels
{
    public class EntitySwatchItemViewModel : ViewModelBase
    {
        public string Name { get; }

        public EntitySwatchItemViewModel(string name)
        {
            Name = name;
        }
    }

    public class EntitySwatchOptionViewModel : ViewModelBase
    {
        public string Name { get; }

        public EntitySwatchOptionViewModel(string name)
        {
            Name = name;
        }
    }

    public class EntitySwatchesViewModel : ViewModelBase
    {
        private readonly Entity _entity;

        public ObservableCollection<EntitySwatchItemViewModel> Swatches { get; } = new ObservableCollection<EntitySwatchItemViewModel>();
        public ObservableCollection<EntitySwatchOptionViewModel> AvailableSwatches { get; } = new ObservableCollection<EntitySwatchOptionViewModel>();

        private EntitySwatchOptionViewModel? _selectedAvailableSwatch;
        public EntitySwatchOptionViewModel? SelectedAvailableSwatch
        {
            get => _selectedAvailableSwatch;
            set
            {
                if (SetProperty(ref _selectedAvailableSwatch, value) && AddSwatchCommand is RelayCommand<EntitySwatchOptionViewModel> relayCommand)
                {
                    relayCommand.RaiseCanExecuteChanged();
                }
            }
        }

        private bool _hasSwatches;
        public bool HasSwatches
        {
            get => _hasSwatches;
            private set => SetProperty(ref _hasSwatches, value);
        }

        private bool _hasAvailableSwatches;
        public bool HasAvailableSwatches
        {
            get => _hasAvailableSwatches;
            private set => SetProperty(ref _hasAvailableSwatches, value);
        }

        public ICommand AddSwatchCommand { get; }
        public ICommand RemoveSwatchCommand { get; }

        public EntitySwatchesViewModel(Entity entity)
        {
            _entity = entity;

            AddSwatchCommand = new RelayCommand<EntitySwatchOptionViewModel>(
                option => _ = AddSwatchAsync(option),
                option => option != null);

            RemoveSwatchCommand = new RelayCommand<EntitySwatchItemViewModel>(
                item => _ = RemoveSwatchAsync(item));

            _ = RefreshAsync();
        }

        public async Task RefreshAsync()
        {
            if (_entity == null || !_entity.IsValid)
            {
                return;
            }

            var allSwatchNames = new List<string>();
            var currentSwatchNames = new List<string>();

            await EngineManager.PostToSimThread(() =>
            {
                World? world = _entity.World;

                if (world == null)
                {
                    return;
                }

                foreach (Name swatchName in world.GetSwatchNames())
                {
                    string name = swatchName.ToString();

                    allSwatchNames.Add(name);

                    if (_entity.IsInSwatchByName(swatchName))
                    {
                        currentSwatchNames.Add(name);
                    }
                }
            });

            Dispatcher.UIThread.Post(() =>
            {
                HashSet<string> currentValues = currentSwatchNames.ToHashSet();

                Swatches.Clear();

                foreach (string name in currentSwatchNames)
                {
                    Swatches.Add(new EntitySwatchItemViewModel(name));
                }

                HasSwatches = Swatches.Count > 0;

                string? previousSelection = SelectedAvailableSwatch?.Name;

                AvailableSwatches.Clear();

                foreach (string name in allSwatchNames)
                {
                    if (!currentValues.Contains(name))
                    {
                        AvailableSwatches.Add(new EntitySwatchOptionViewModel(name));
                    }
                }

                HasAvailableSwatches = AvailableSwatches.Count > 0;

                SelectedAvailableSwatch = previousSelection != null
                    ? AvailableSwatches.FirstOrDefault(o => o.Name == previousSelection)
                    : null;
            });
        }

        private async Task AddSwatchAsync(EntitySwatchOptionViewModel? option)
        {
            if (option == null || _entity == null || !_entity.IsValid)
            {
                return;
            }

            string swatchName = option.Name;
            Entity capturedEntity = _entity;

            await EngineManager.PostToSimThread(() =>
            {
                Name name = new Name(swatchName);

                capturedEntity.AddToSwatchByName(name);

                EditorProject? project = EngineManager.CurrentProject;
                Debug.Assert(project != null, "No active project found when adding an entity to a swatch");

                project?.ActionStack?.PushAction(new EditorAction(
                    $"Add Swatch: {swatchName}",
                    execute: (_, _) => capturedEntity.AddToSwatchByName(name),
                    revert: (_, _) => capturedEntity.RemoveFromSwatchByName(name)));
            });

            await RefreshAsync();
        }

        private async Task RemoveSwatchAsync(EntitySwatchItemViewModel? item)
        {
            if (item == null || _entity == null || !_entity.IsValid)
            {
                return;
            }

            string swatchName = item.Name;
            Entity capturedEntity = _entity;

            await EngineManager.PostToSimThread(() =>
            {
                Name name = new Name(swatchName);

                capturedEntity.RemoveFromSwatchByName(name);

                EditorProject? project = EngineManager.CurrentProject;
                Debug.Assert(project != null, "No active project found when removing an entity from a swatch");

                project?.ActionStack?.PushAction(new EditorAction(
                    $"Remove Swatch: {swatchName}",
                    execute: (_, _) => capturedEntity.RemoveFromSwatchByName(name),
                    revert: (_, _) => capturedEntity.AddToSwatchByName(name)));
            });

            await RefreshAsync();
        }
    }
}
