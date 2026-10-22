using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ProjectSettingsViewModel : ViewModelBase
    {
        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new();

        private bool _hasProject;
        public bool HasProject
        {
            get => _hasProject;
            private set => SetProperty(ref _hasProject, value);
        }

        private EditorProject? _project;
        private string _gameTitle = string.Empty;

        public ProjectSettingsViewModel()
        {
            _ = RefreshAsync();
        }

        private async Task RefreshAsync()
        {
            EditorProject? project = null;
            EditorProjectSettings? settings = null;
            string gameTitle = string.Empty;

            await EngineManager.PostToSimThread(() =>
            {
                project = EngineManager.CurrentProject;
                settings = project?.GetProjectSettings();

                if (project != null && project.IsValid)
                {
                    gameTitle = project.GetGameTitle();
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                _project = project;
                _gameTitle = gameTitle;

                Properties.Clear();

                HasProject = settings != null && settings.IsValid;

                if (!HasProject)
                {
                    return;
                }

                foreach (InspectorPropertyViewModelBase property in CreateProperties(settings!))
                {
                    Properties.Add(property);
                }
            });
        }

        private IEnumerable<InspectorPropertyViewModelBase> CreateProperties(EditorProjectSettings settings)
        {
            IEnumerable<Property> editorProperties = settings.Class.Properties.Where(property =>
            {
                ClassAttribute? attrEditor = property.GetAttribute("editor");

                return attrEditor != null && attrEditor.Value.GetBool();
            });

            foreach (Property property in editorProperties)
            {
                InspectorPropertyViewModelBase viewModel;

                try
                {
                    viewModel = InspectorViewModelFactory.Create(settings, property, isReadOnly: false, postWriteCallback: OnSettingWritten);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ProjectSettings: failed to create {property.Name}: {ex.Message}");

                    continue;
                }

                yield return viewModel;
            }
        }

        private void OnSettingWritten()
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
                Logger.Log(LogLevel.Warning, $"failed to mark the project dirty: {ex.Message}");
            }

            string gameTitle = project.GetGameTitle();

            if (gameTitle == _gameTitle)
            {
                return;
            }

            _gameTitle = gameTitle;

            if (!project.HasGameCode())
            {
                return;
            }

            string oldName = project.GetModuleName();
            string newName = project.GetModuleNameFromTitle();

            if (string.IsNullOrEmpty(newName) || newName == oldName)
            {
                return;
            }

            Dispatcher.UIThread.Post(() =>
            {
                MessageBox.Info("Rename game code?", $"Rename {oldName}Game and the game's project files to {newName}Game in its code?\n\nChoosing Rename will save the project, then rebuild the game.")
                    .Button("Rename", () => _ = RenameGameCodeAsync(project, newName))
                    .Button("Keep Names", () => { })
                    .Show();
            });
        }

        private static async Task RenameGameCodeAsync(EditorProject project, string newName)
        {
            bool isRenamed = false;

            await EngineManager.PostToSimThread(() =>
            {
                isRenamed = project.IsValid && project.RenameGameCode(newName).IsValid;
            });

            if (!isRenamed)
            {
                Dispatcher.UIThread.Post(() =>
                    MessageBox.Critical("Renaming failed", "The game code couldn't be renamed. See the log for more details.")
                        .Button("OK", () => { })
                        .Show());

                return;
            }

            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommandBuildNativeGame"));
        }
    }
}
