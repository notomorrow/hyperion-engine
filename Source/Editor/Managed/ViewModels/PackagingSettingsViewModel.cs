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
    // Packaging defaults for every platform, plus overrides for the platforms that need their own; the rows come from the reflected settings struct
    public class PackagingSettingsViewModel : ViewModelBase
    {
        public ObservableCollection<InspectorPropertyViewModelBase> DefaultProperties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        public ObservableCollection<PackagingPlatformOverrideViewModel> Overrides { get; } = new ObservableCollection<PackagingPlatformOverrideViewModel>();

        public ObservableCollection<TargetPlatformItemViewModel> AvailablePlatforms { get; } = new ObservableCollection<TargetPlatformItemViewModel>();

        private bool _hasProject;
        public bool HasProject
        {
            get => _hasProject;
            private set => SetProperty(ref _hasProject, value);
        }

        private bool _canAddOverride;
        public bool CanAddOverride
        {
            get => _canAddOverride;
            private set => SetProperty(ref _canAddOverride, value);
        }

        public ICommand AddOverrideCommand { get; }
        public ICommand RemoveOverrideCommand { get; }

        private EditorProject? _project;
        private PackagingSettings? _settings;

        public PackagingSettingsViewModel()
        {
            AddOverrideCommand = new RelayCommand<TargetPlatformItemViewModel>(item => _ = AddOverrideAsync(item));
            RemoveOverrideCommand = new RelayCommand<PackagingPlatformOverrideViewModel>(item => _ = RemoveOverrideAsync(item));

            _ = RefreshAsync();
        }

        private async Task RefreshAsync(bool rebuildDefaults = true)
        {
            EditorProject? project = null;
            PackagingSettings? settings = null;
            var overrides = new List<(TargetPlatform Platform, PackagingPlatformOverride Override)>();

            await EngineManager.PostToSimThread(() =>
            {
                project = EngineManager.CurrentProject;
                settings = project?.GetPackagingSettings();

                if (settings == null || !settings.IsValid)
                {
                    return;
                }

                foreach (PackagingPlatformOverride? platformOverride in settings.GetPlatformOverrides())
                {
                    if (platformOverride != null && platformOverride.IsValid)
                    {
                        overrides.Add((platformOverride.GetPlatform(), platformOverride));
                    }
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                _project = project;
                _settings = settings != null && settings.IsValid ? settings : null;

                HasProject = _settings != null;

                if (rebuildDefaults)
                {
                    DefaultProperties.Clear();
                }

                Overrides.Clear();
                AvailablePlatforms.Clear();

                if (_settings == null)
                {
                    CanAddOverride = false;

                    return;
                }

                if (rebuildDefaults)
                {
                    foreach (InspectorPropertyViewModelBase property in CreateStructFields(_settings, "Defaults", OnDefaultsWritten))
                    {
                        DefaultProperties.Add(property);
                    }
                }

                foreach ((TargetPlatform platform, PackagingPlatformOverride platformOverride) in overrides.OrderBy(entry => entry.Platform))
                {
                    var overrideViewModel = new PackagingPlatformOverrideViewModel(platform);

                    foreach (InspectorPropertyViewModelBase property in CreateStructFields(platformOverride, "Options", OnOverrideWritten))
                    {
                        overrideViewModel.Properties.Add(property);
                    }

                    Overrides.Add(overrideViewModel);
                }

                foreach (TargetPlatform platform in Enum.GetValues<TargetPlatform>())
                {
                    if (overrides.All(entry => entry.Platform != platform))
                    {
                        AvailablePlatforms.Add(new TargetPlatformItemViewModel(platform));
                    }
                }

                CanAddOverride = AvailablePlatforms.Count > 0;
            });
        }

        private IEnumerable<InspectorPropertyViewModelBase> CreateStructFields(ObjectBase target, string propertyName, Action postWriteCallback)
        {
            foreach (Property property in target.Class.Properties)
            {
                if (property.Name.ToString() != propertyName)
                {
                    continue;
                }

                InspectorPropertyViewModelBase viewModel;

                try
                {
                    viewModel = InspectorViewModelFactory.Create(target, property, isReadOnly: false, postWriteCallback: postWriteCallback);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"PackagingSettings: failed to create {propertyName}: {ex.Message}");

                    yield break;
                }

                if (viewModel is StructPropertyViewModel structViewModel)
                {
                    foreach (InspectorPropertyViewModelBase field in structViewModel.SubProperties)
                    {
                        yield return field;
                    }
                }
                else
                {
                    yield return viewModel;
                }

                yield break;
            }
        }

        // fields an override doesn't override follow the new defaults, so its card is rebuilt to show them
        private void OnDefaultsWritten()
        {
            _settings?.ApplyDefaultsToOverrides();

            MarkProjectDirty();

            Dispatcher.UIThread.Post(() => _ = RefreshAsync(rebuildDefaults: false));
        }

        private void OnOverrideWritten()
        {
            _settings?.UpdateOverriddenFields();

            MarkProjectDirty();
        }

        private void MarkProjectDirty()
        {
            EditorProject? project = _project;

            if (project == null || !project.IsValid)
            {
                return;
            }

            try
            {
                project.MarkSettingsDirty();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"PackagingSettings: failed to mark the project dirty: {ex.Message}");
            }
        }

        private async Task AddOverrideAsync(TargetPlatformItemViewModel? item)
        {
            PackagingSettings? settings = _settings;

            if (item == null || settings == null)
            {
                return;
            }

            await EngineManager.PostToSimThread(() =>
            {
                if (settings.IsValid)
                {
                    settings.AddPlatformOverride(item.Platform);
                }
            });

            MarkProjectDirty();

            await RefreshAsync();
        }

        private async Task RemoveOverrideAsync(PackagingPlatformOverrideViewModel? item)
        {
            PackagingSettings? settings = _settings;

            if (item == null || settings == null)
            {
                return;
            }

            await EngineManager.PostToSimThread(() =>
            {
                if (settings.IsValid)
                {
                    settings.RemovePlatformOverride(item.Platform);
                }
            });

            MarkProjectDirty();

            await RefreshAsync();
        }
    }

    public class PackagingPlatformOverrideViewModel : ViewModelBase
    {
        public TargetPlatform Platform { get; }

        public string DisplayName { get; }

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        public PackagingPlatformOverrideViewModel(TargetPlatform platform)
        {
            Platform = platform;
            DisplayName = TargetPlatformItemViewModel.GetDisplayName(platform);
        }
    }

    public class TargetPlatformItemViewModel : ViewModelBase
    {
        public TargetPlatform Platform { get; }

        public string DisplayName { get; }

        public TargetPlatformItemViewModel(TargetPlatform platform)
        {
            Platform = platform;
            DisplayName = GetDisplayName(platform);
        }

        public static string GetDisplayName(TargetPlatform platform)
        {
            return platform switch
            {
                TargetPlatform.Mac => "macOS",
                TargetPlatform.IOS => "iOS",
                _ => platform.ToString()
            };
        }
    }
}
