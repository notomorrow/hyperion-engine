using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorViewModel : ViewModelBase
    {
        public InspectorSectionViewModel ComponentsSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel TagsSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel LayersSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel ScriptSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel ActionsSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel ScenePropertiesSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel LayerOverridesSection { get; } = new() { IsExpanded = true };
        public InspectorSectionViewModel PropertiesSection { get; } = new() { IsExpanded = true };

        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();
        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new ObservableCollection<InspectorActionViewModel>();
        public ObservableCollection<InspectorComponentViewModelBase> Components { get; } = new ObservableCollection<InspectorComponentViewModelBase>();
        public ObservableCollection<AddComponentOptionViewModel> AddableComponents { get; } = new ObservableCollection<AddComponentOptionViewModel>();

        public ICommand AddComponentCommand { get; }
        public ICommand RemoveComponentCommand { get; }

        public ObservableCollection<LayerCopySourceOptionViewModel> CopyLayerSources { get; } = new ObservableCollection<LayerCopySourceOptionViewModel>();

        private LayerCopySourceOptionViewModel? _selectedCopyLayerSource;
        public LayerCopySourceOptionViewModel? SelectedCopyLayerSource
        {
            get => _selectedCopyLayerSource;
            set
            {
                if (SetProperty(ref _selectedCopyLayerSource, value) && ApplyCopyFromLayerCommand is AsyncRelayCommand relayCommand)
                {
                    relayCommand.RaiseCanExecuteChanged();
                }
            }
        }

        private bool _hasCopyLayerSources;
        public bool HasCopyLayerSources
        {
            get => _hasCopyLayerSources;
            private set => SetProperty(ref _hasCopyLayerSources, value);
        }

        private bool _hasActiveLayerOverrides;
        public bool HasActiveLayerOverrides
        {
            get => _hasActiveLayerOverrides;
            private set
            {
                if (SetProperty(ref _hasActiveLayerOverrides, value) && ResetLayerOverridesCommand is AsyncRelayCommand relayCommand)
                {
                    relayCommand.RaiseCanExecuteChanged();
                }
            }
        }

        /// <summary>Display-only view over <see cref="Properties"/>: the rows the active layer overrides.</summary>
        public ObservableCollection<InspectorPropertyViewModelBase> OverriddenProperties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        private int _overriddenPropertyCount;
        public int OverriddenPropertyCount
        {
            get => _overriddenPropertyCount;
            private set
            {
                if (SetProperty(ref _overriddenPropertyCount, value))
                {
                    OnPropertyChanged(nameof(HasOverriddenProperties));
                    OnPropertyChanged(nameof(ShowNoOverridesHint));
                    OnPropertyChanged(nameof(OverriddenPropertiesHeader));
                }
            }
        }

        public bool HasOverriddenProperties => _overriddenPropertyCount > 0;

        public bool ShowNoOverridesHint => CanUseLayerOverrides && !HasOverriddenProperties;

        public string OverriddenPropertiesHeader => _overriddenPropertyCount == 1
            ? $"1 PROPERTY OVERRIDDEN IN {ActiveLayerLabel.ToUpperInvariant()}"
            : $"{_overriddenPropertyCount} PROPERTIES OVERRIDDEN IN {ActiveLayerLabel.ToUpperInvariant()}";

        public ICommand ApplyCopyFromLayerCommand { get; }
        public ICommand ResetLayerOverridesCommand { get; }

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        private bool _hasComponents;
        public bool HasComponents
        {
            get => _hasComponents;
            private set => SetProperty(ref _hasComponents, value);
        }

        private bool _hasAddableComponents;
        public bool HasAddableComponents
        {
            get => _hasAddableComponents;
            private set => SetProperty(ref _hasAddableComponents, value);
        }

        private bool _isEntity;
        public bool IsEntity
        {
            get => _isEntity;
            private set => SetProperty(ref _isEntity, value);
        }

        private bool _isRootNode;
        public bool IsRootNode
        {
            get => _isRootNode;
            private set => SetProperty(ref _isRootNode, value);
        }

        private bool _hasSceneProperties;
        public bool HasSceneProperties
        {
            get => _hasSceneProperties;
            private set => SetProperty(ref _hasSceneProperties, value);
        }

        public ObservableCollection<InspectorPropertyViewModelBase> SceneProperties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        private AttachedScriptViewModel? _attachedScript;
        public AttachedScriptViewModel? AttachedScript
        {
            get => _attachedScript;
            private set => SetProperty(ref _attachedScript, value);
        }

        private bool _hasAttachedScript;
        public bool HasAttachedScript
        {
            get => _hasAttachedScript;
            private set => SetProperty(ref _hasAttachedScript, value);
        }

        private EntityTagsViewModel? _entityTags;
        public EntityTagsViewModel? EntityTags
        {
            get => _entityTags;
            private set => SetProperty(ref _entityTags, value);
        }

        private EntityLayersViewModel? _entityLayers;
        public EntityLayersViewModel? EntityLayers
        {
            get => _entityLayers;
            private set => SetProperty(ref _entityLayers, value);
        }

        public bool IsDefaultLayer
        {
            get => (_activeLayerDisplay ?? string.Empty) == string.Empty
                || (_activeLayerDisplay ?? string.Empty) == "Default";
        }

        /// <summary>False on the Default layer, whose values are the entity's base values - there is nothing to override into.</summary>
        public bool CanUseLayerOverrides => !IsDefaultLayer;

        /// <summary>
        /// Publishes the active layer to the shared edit context. The Default layer is the base
        /// values, so it is published as "no layer" and every edit routes to the base.
        /// </summary>
        private void ApplyActiveLayerToEditContext()
        {
            LayerOverrideEditContext.ActiveLayerName = IsDefaultLayer ? null : ActiveLayerDisplay;

            if (IsDefaultLayer)
            {
                LayerOverrideMode = false;
            }
        }

        private bool _layerOverrideMode;
        public bool LayerOverrideMode
        {
            get => _layerOverrideMode;
            set
            {
                if (SetProperty(ref _layerOverrideMode, value))
                {
                    LayerOverrideEditContext.OverrideModeActive = value;

                    _ = EngineManager.PostToSimThread(() =>
                    {
                        EngineManager.EditorGame?.EditorSubsystem?.SetLayerOverrideMode(value);
                    });
                    _ = RefreshCopyLayerSourcesAsync();
                }
            }
        }

        /// <summary>Active layer name for display, falling back to "Default" before the World reports one.</summary>
        public string ActiveLayerLabel => string.IsNullOrEmpty(_activeLayerDisplay) ? "Default" : _activeLayerDisplay!;

        private string? _activeLayerDisplay;
        public string? ActiveLayerDisplay
        {
            get => _activeLayerDisplay;
            private set
            {
                if (SetProperty(ref _activeLayerDisplay, value))
                {
                    OnPropertyChanged(nameof(IsDefaultLayer));
                    OnPropertyChanged(nameof(CanUseLayerOverrides));
                    OnPropertyChanged(nameof(ShowNoOverridesHint));
                    OnPropertyChanged(nameof(ActiveLayerLabel));
                    OnPropertyChanged(nameof(OverriddenPropertiesHeader));
                }
            }
        }

        private Node? _selectedNode;
        public Node? SelectedNode
        {
            get => _selectedNode;
            private set => SetProperty(ref _selectedNode, value);
        }

        private DelegateHandler? _transformUpdatedHandler;

        private Scene? _currentScene;
        public Scene? CurrentScene
        {
            get => _currentScene;
            private set => SetProperty(ref _currentScene, value);
        }

        public InspectorViewModel()
        {
            AddComponentCommand = new AsyncRelayCommand(AddComponentAsync, CanAddComponent);
            RemoveComponentCommand = new RelayCommand<object>(RemoveComponent, CanRemoveComponent);
            ApplyCopyFromLayerCommand = new AsyncRelayCommand(_ => ApplyCopyFromLayerAsync(), _ => SelectedCopyLayerSource != null);
            ResetLayerOverridesCommand = new AsyncRelayCommand(_ => ResetLayerOverridesAsync(), _ => HasActiveLayerOverrides);
        }

        ~InspectorViewModel()
        {
            _transformUpdatedHandler?.Remove();
        }

        public void SetSelectedNode(Node? node, Scene? scene = null, bool isRootNode = false)
        {
            Dispatcher.UIThread.VerifyAccess();

            // Unbind from previous node's TransformUpdated delegate
            _transformUpdatedHandler?.Remove();
            _transformUpdatedHandler = null;

            SelectedNode = node;
            CurrentScene = scene;
            IsRootNode = isRootNode;

            // Bind to the new node's TransformUpdated delegate
            if (SelectedNode != null)
            {
                _transformUpdatedHandler = SelectedNode.GetTransformUpdatedDelegate().Bind((Node updatedNode) =>
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        RefreshTransformProperties();
                    });
                });
            }

            RefreshProperties();
        }

        private void RefreshProperties()
        {
            Dispatcher.UIThread.VerifyAccess();

            Properties.Clear();
            Actions.Clear();
            Components.Clear();
            AddableComponents.Clear();
            SceneProperties.Clear();
            CopyLayerSources.Clear();

            AttachedScript = null;
            HasAttachedScript = false;
            EntityTags = null;
            EntityLayers = null;

            SelectedCopyLayerSource = null;
            HasCopyLayerSources = false;
            HasActiveLayerOverrides = false;

            OverriddenProperties.Clear();
            OverriddenPropertyCount = 0;

            LayerOverrideEditContext.Reset();

            HasActions = false;
            HasComponents = false;
            HasAddableComponents = false;
            HasSceneProperties = false;

            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            // If this is the root node, show scene properties
            if (IsRootNode && CurrentScene != null && CurrentScene.IsValid)
            {
                RefreshSceneProperties();
            }

            Class nodeClass = SelectedNode.Class;

            // sort by editororder attribute (if present), then by name
            List<Property> properties = nodeClass.Properties
                .Where(p =>
                {
                    ClassAttribute? attrEditCondition = p.GetAttribute("editcondition");

                    return EvaluateEditCondition(nodeClass, attrEditCondition, p.Name.ToString());
                })
                .OrderBy(p =>
                {
                    ClassAttribute? attrEditOrder = p.GetAttribute("editororder");

                    if (attrEditOrder != null)
                    {
                        return attrEditOrder.Value.GetInt();
                    }

                    return int.MaxValue;
                })
                .ThenBy(p => p.Name.ToString())
                .ToList();

            bool hasAddedMobility = false;

            var addMobility = () =>
            {
                try
                {
                    var mobilityVm = new MobilityPropertyViewModel(SelectedNode, Class.GetClass<Node>().GetProperty("NodeFlags") ?? throw new Exception("Failed to get NodeFlags property"));
                    mobilityVm.RefreshValue();
                    
                    Properties.Add(mobilityVm);

                    hasAddedMobility = true;
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create mobility selector: {ex.Message}");
                }
            };

            foreach (Property property in properties)
            {
                try
                {
                    if (property.Name == "Components")
                    {
                        continue; // skip Components property -- they're handled separately
                    }

                    if (property.Name == "Tags")
                    {
                        continue; // skip Entity Tags property -- they're handled separately (not NodeTags, which stays here)
                    }

                    // skip non-editor properties
                    ClassAttribute? attrEditor = property.GetAttribute("editor");

                    if (attrEditor != null && attrEditor.Value.GetBool() == false)
                    {
                        continue;
                    }

                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }

                    Properties.Add(InspectorViewModelFactory.Create(
                        SelectedNode, property, isReadOnly, 0, null, null, OnPropertyValueChanged));

                    if (Properties[Properties.Count - 1] is FlagsPropertyViewModel flagsVm)
                    {
                        flagsVm.ValueCommitted += RefreshActions;

                        // Insert Mobility selector right after the Flags property
                        if (!hasAddedMobility)
                        {
                            addMobility();
                        }
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for property '{property.Name}': {ex.Message}");
                }
            }

            if (!hasAddedMobility)
            {
                addMobility();
            }

            // collect actions (methods with editoraction attribute)
            foreach (InspectorActionViewModel actionVm in InspectorActionsHelper.GetActions(SelectedNode, OnPropertyValueChanged))
            {
                Actions.Add(actionVm);
            }

            Logger.Log(LogLevel.Debug, $"Inspector found {Actions.Count} actions for node '{SelectedNode.Name}'");

            HasActions = Actions.Count > 0;

            // collect components
            if (SelectedNode is Entity entity)
            {
                IsEntity = true;

                AttachedScript = new AttachedScriptViewModel(entity);
                HasAttachedScript = true;
                EntityTags = new EntityTagsViewModel(entity);
                EntityLayers = new EntityLayersViewModel(entity);

                LayerOverrideEditContext.CurrentEntity = entity;

                _ = RefreshActiveLayerInfoAsync();
                _ = RefreshOverrideSignifiersAsync();

                _ = EngineManager.PostToSimThread(() =>
                {
                    EntityManager? mgr = entity.EntityManager;
                    if (mgr == null)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to get EntityManager for entity '{entity.Name}'");

                        return;
                    }

                    List<TypeId> componentTypeIds = mgr.GetComponentTypeIds(entity).ToList();

                    Dispatcher.UIThread.Post(() =>
                    {
                        Components.Clear();

                        foreach (TypeId typeId in componentTypeIds)
                        {
                            InspectorComponentViewModelBase? componentVm = null;

                            ComponentTypeDescriptor? descriptor = s_registeredComponents.Value
                                .FirstOrDefault(d => d.TypeId == typeId);

                            if (descriptor != null)
                                componentVm = descriptor.CreateViewModel(entity);
                            else
                                Logger.Log(LogLevel.Debug, $"Inspector has no view model for component type '{typeId}'");

                            if (componentVm != null && componentVm.IsEditorVisible)
                            {
                                Components.Add(componentVm);
                                componentVm.PopulateProperties();
                            }
                        }

                        HasComponents = Components.Count > 0;

                        UpdateAddableComponents(componentTypeIds);
                    });
                });
            }
            else
            {
                IsEntity = false;
                AttachedScript = null;
                HasAttachedScript = false;
                AddableComponents.Clear();
                HasAddableComponents = false;
            }
        }

        private void RefreshActions()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (SelectedNode == null || !SelectedNode.IsValid)
                return;

            Actions.Clear();

            foreach (InspectorActionViewModel actionVm in InspectorActionsHelper.GetActions(SelectedNode, OnPropertyValueChanged))
            {
                Actions.Add(actionVm);
            }

            HasActions = Actions.Count > 0;
        }

        /// <summary>
        /// Re-reads every property shown for the selected node. A setter can change more than the
        /// value it was given (clamped ranges, packed flag bits, side effects on other members), so
        /// after any write the whole object is read back instead of trusting what the UI sent.
        /// </summary>
        private void OnPropertyValueChanged()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            foreach (InspectorPropertyViewModelBase propertyVm in Properties)
            {
                propertyVm.RefreshValue();
            }

            foreach (InspectorComponentViewModelBase componentVm in Components)
            {
                componentVm.RefreshProperties();
            }

            if (EntityTags != null)
            {
                _ = EntityTags.RefreshAsync();
            }

            if (EntityLayers != null)
            {
                _ = EntityLayers.RefreshAsync();
            }

            _ = RefreshOverrideSignifiersAsync();
            _ = RefreshActiveLayerOverridesAsync();
        }

        /// <summary>
        /// Reads the World's active layer name for the selected entity and updates the
        /// layer-override edit context + panel hint text.
        /// </summary>
        public async Task RefreshActiveLayerInfoAsync()
        {
            string activeLayerName = string.Empty;

            await EngineManager.PostToSimThread(() =>
            {
                World? world = null;

                if (SelectedNode is Entity selectedEntity && selectedEntity.IsValid)
                {
                    world = selectedEntity.World;
                }

                if (world == null)
                {
                    EditorProject? project = EngineManager.CurrentProject;

                    if (project != null)
                    {
                        world = project.World;
                    }
                }

                if (world != null)
                {
                    activeLayerName = world.GetActiveLayerName().ToString();
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                ActiveLayerDisplay = activeLayerName.Length > 0 ? activeLayerName : null;
                ApplyActiveLayerToEditContext();
            });

            _ = RefreshCopyLayerSourcesAsync();
            _ = RefreshActiveLayerOverridesAsync();
        }

        /// <summary>
        /// Refreshes per-row override signifiers ("LayerA, LayerB override this value") for the
        /// selected entity's entity-level property rows.
        /// </summary>
        public async Task RefreshOverrideSignifiersAsync()
        {
            if (SelectedNode is not Entity entity || !entity.IsValid)
            {
                return;
            }

            // Entity-level rows only (component / delegate-backed rows are not overridable in v1);
            // Name is identity metadata and never overridable
            List<InspectorPropertyViewModelBase> rows = Properties
                .Where(p => p.IsEntityLevelRow && p.Property.Name != new Name("Name", weak: true))
                .ToList();

            List<string> layerNames = new();
            List<bool> overriddenFlags = new();

            await EngineManager.PostToSimThread(() =>
            {
                Name[] sets = EntityLayerOverrides.GetSetLayerNames(entity);

                foreach (Name set in sets)
                {
                    layerNames.Add(set.ToString());
                }

                foreach (InspectorPropertyViewModelBase row in rows)
                {
                    Name propertyName = row.Property.Name;

                    foreach (Name layer in sets)
                    {
                        overriddenFlags.Add(EntityLayerOverrides.IsPropertyOverridden(entity, layer, propertyName));
                    }
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                if (SelectedNode is not Entity selectedEntity || selectedEntity.NativeAddress != entity.NativeAddress)
                {
                    return;
                }

                int flagIndex = 0;
                string? currentLayerName = LayerOverrideEditContext.ActiveLayerName;

                OverriddenProperties.Clear();

                foreach (InspectorPropertyViewModelBase row in rows)
                {
                    List<string> overriddenLayers = new();

                    foreach (string layerName in layerNames)
                    {
                        if (flagIndex < overriddenFlags.Count && overriddenFlags[flagIndex++])
                        {
                            overriddenLayers.Add(layerName);
                        }
                    }

                    row.SetOverrideInfo(overriddenLayers, currentLayerName);

                    if (row.IsOverriddenByCurrentLayer)
                    {
                        OverriddenProperties.Add(row);
                    }
                }

                OverriddenPropertyCount = OverriddenProperties.Count;
            });
        }

        /// <summary>
        /// Called when the World's active layer changes; refreshes property rows and override
        /// signifiers (overrides may have applied natively) and updates the edit context.
        /// </summary>
        public void OnWorldActiveLayerChanged(string layerName)
        {
            Dispatcher.UIThread.VerifyAccess();

            ActiveLayerDisplay = string.IsNullOrEmpty(layerName) ? null : layerName;
            ApplyActiveLayerToEditContext();

            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            // Overrides for the new active layer were just applied natively - re-read all rows
            foreach (InspectorPropertyViewModelBase propertyVm in Properties)
            {
                propertyVm.RefreshValue();
            }

            _ = RefreshOverrideSignifiersAsync();
            _ = RefreshCopyLayerSourcesAsync();
            _ = RefreshActiveLayerOverridesAsync();
        }

        /// <summary>
        /// Refreshes whether the selected entity has any property overrides in the World's
        /// active layer; drives the "Reset Layer Overrides" button enablement.
        /// </summary>
        private async Task RefreshActiveLayerOverridesAsync()
        {
            Entity? entity = SelectedNode as Entity;
            string? layerName = ActiveLayerDisplay;

            bool hasOverrides = false;
            Entity? capturedEntity = null;

            if (entity != null && entity.IsValid && !string.IsNullOrEmpty(layerName))
            {
                capturedEntity = entity;

                string capturedLayer = layerName;

                await EngineManager.PostToSimThread(() =>
                {
                    hasOverrides = EntityLayerOverrides.HasValues(capturedEntity, new Name(capturedLayer));
                });
            }

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                if (capturedEntity == null
                    || SelectedNode is not Entity currentEntity || !currentEntity.IsValid
                    || currentEntity.NativeAddress != capturedEntity.NativeAddress)
                {
                    return;
                }

                HasActiveLayerOverrides = hasOverrides;
            });
        }

        /// <summary>
        /// Invokes the native <c>EditorCommandResetLayerOverrides</c> command, removing all
        /// property overrides the entity has in the World's active layer (restoring base
        /// values). Undoable.
        /// </summary>
        private async Task ResetLayerOverridesAsync()
        {
            if (SelectedNode is not Entity entity || !entity.IsValid)
            {
                return;
            }

            if (string.IsNullOrEmpty(ActiveLayerDisplay))
            {
                return;
            }

            Entity capturedEntity = entity;

            await EngineManager.PostToSimThread(() =>
            {
                EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                    new Name("EditorCommandResetLayerOverrides"),
                    capturedEntity.NativeAddress.ToString());
            });

            // Update enablement immediately - there is nothing left to reset on this layer now
            await RefreshActiveLayerOverridesAsync();

            // Re-read all rows + override signifiers (overrides were dropped; rows show base values)
            OnPropertyValueChanged();
        }

        /// <summary>
        /// Rebuilds the "Copy Properties from Layer" source options: the base state plus every
        /// World layer except the current edit target (the active layer when override mode is
        /// on, base otherwise). Copying a source onto itself would be a no-op.
        /// </summary>
        private async Task RefreshCopyLayerSourcesAsync()
        {
            Entity? entity = SelectedNode as Entity;

            if (entity == null || !entity.IsValid)
            {
                CopyLayerSources.Clear();
                SelectedCopyLayerSource = null;
                HasCopyLayerSources = false;

                return;
            }

            Entity capturedEntity = entity;
            List<string> layerNames = new List<string>();

            await EngineManager.PostToSimThread(() =>
            {
                World? world = capturedEntity.World;

                if (world == null)
                {
                    EditorProject? project = EngineManager.CurrentProject;

                    if (project != null)
                    {
                        world = project.World;
                    }
                }

                if (world == null)
                {
                    return;
                }

                foreach (Name layerName in world.GetLayerNames())
                {
                    layerNames.Add(layerName.ToString());
                }
            });

            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                if (SelectedNode is not Entity currentEntity || !currentEntity.IsValid
                    || currentEntity.NativeAddress != capturedEntity.NativeAddress)
                {
                    return;
                }

                string? targetLayerName = LayerOverrideMode ? ActiveLayerDisplay : null;
                string? previousSelection = SelectedCopyLayerSource?.Name;

                CopyLayerSources.Clear();

                if (targetLayerName != null)
                {
                    CopyLayerSources.Add(new LayerCopySourceOptionViewModel("Base", isBase: true));
                }

                foreach (string layerName in layerNames)
                {
                    if (layerName == targetLayerName)
                    {
                        continue;
                    }

                    // Default holds the base values, so it is already covered by the "Base" option
                    if (layerName == "Default")
                    {
                        continue;
                    }

                    CopyLayerSources.Add(new LayerCopySourceOptionViewModel(layerName, isBase: false));
                }

                HasCopyLayerSources = CopyLayerSources.Count > 0;

                SelectedCopyLayerSource = previousSelection != null
                    ? CopyLayerSources.FirstOrDefault(option => option.Name == previousSelection)
                    : null;
            });
        }

        /// <summary>
        /// Invokes the native <c>EditorCommandCopyLayerProperties</c> command: it computes the
        /// minimal diff to make the current edit target (the active layer's override set when
        /// override mode is on, otherwise the entity's base values) match the selected source's
        /// effective values, applies it as a single undoable action, and prunes overrides that
        /// would end up redundant (e.g. copying from Base empties the target layer's override
        /// set).
        /// </summary>
        private async Task ApplyCopyFromLayerAsync()
        {
            LayerCopySourceOptionViewModel? sourceOption = SelectedCopyLayerSource;

            if (sourceOption == null)
            {
                return;
            }

            if (SelectedNode is not Entity entity || !entity.IsValid)
            {
                return;
            }

            // The Default layer is the base values, so copying while it is active targets base
            bool targetIsLayer = !IsDefaultLayer && !string.IsNullOrEmpty(ActiveLayerDisplay);
            string targetDisplay = targetIsLayer ? ActiveLayerDisplay! : "Base";

            if (!targetIsLayer && sourceOption.IsBase)
            {
                return; // base -> base is a no-op
            }

            if (targetIsLayer && !sourceOption.IsBase && sourceOption.Name == targetDisplay)
            {
                return; // layer -> itself is a no-op
            }

            await EngineManager.PostToSimThread(() =>
            {
                EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(
                    new Name("EditorCommandCopyLayerProperties"),
                    entity.NativeAddress.ToString(),
                    sourceOption.IsBase ? "1" : "0",
                    sourceOption.IsBase ? "-" : sourceOption.Name,
                    targetIsLayer ? "0" : "1",
                    targetIsLayer ? targetDisplay : "-");
            });

            // Re-read all rows + override signifiers (the command may have changed values)
            OnPropertyValueChanged();
        }

        private void OnScenePropertyValueChanged()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (CurrentScene == null || !CurrentScene.IsValid)
            {
                return;
            }

            foreach (InspectorPropertyViewModelBase propertyVm in SceneProperties)
            {
                propertyVm.RefreshValue();
            }
        }

        private void RefreshTransformProperties()
        {
            Dispatcher.UIThread.VerifyAccess();

            foreach (InspectorPropertyViewModelBase propertyVm in Properties)
            {
                // Refresh transform-related properties
                if (propertyVm is TransformViewModel transformVm)
                {
                    transformVm.RefreshValue();
                }
            }
        }

        private void RefreshSceneProperties()
        {
            if (CurrentScene == null || !CurrentScene.IsValid)
            {
                return;
            }

            Class sceneClass = CurrentScene.Class;

            List<Property> sceneProps = sceneClass.Properties
                .Where(p =>
                {
                    if (p.Name.ToString() == "Root")
                    {
                        return false;
                    }

                    ClassAttribute? attrEditor = p.GetAttribute("editor");

                    if (attrEditor != null && attrEditor.Value.GetBool() == false)
                    {
                        return false;
                    }

                    return true;
                })
                .OrderBy(p =>
                {
                    ClassAttribute? attrEditOrder = p.GetAttribute("editororder");
                    if (attrEditOrder != null)
                    {
                        return attrEditOrder.Value.GetInt();
                    }
                    return int.MaxValue;
                })
                .ThenBy(p => p.Name.ToString())
                .ToList();

            foreach (Property property in sceneProps)
            {
                try
                {
                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }

                    SceneProperties.Add(InspectorViewModelFactory.Create(
                        CurrentScene, property, isReadOnly, 0, null, null, OnScenePropertyValueChanged));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for scene property '{property.Name}': {ex.Message}");
                }
            }

            HasSceneProperties = SceneProperties.Count > 0;
        }

        private void UpdateAddableComponents(IEnumerable<TypeId> existingComponentTypes)
        {
            HashSet<TypeId> existingTypes = [.. existingComponentTypes];

            AddableComponents.Clear();

            foreach ((string label, TypeId typeId) in GetSupportedComponentTypes())
            {
                bool canAdd = !existingTypes.Contains(typeId);
                AddableComponents.Add(new AddComponentOptionViewModel(label, typeId, canAdd));
            }

            HasAddableComponents = AddableComponents.Any(option => option.IsEnabled);

            if (AddComponentCommand is AsyncRelayCommand relayCommand)
            {
                relayCommand.RaiseCanExecuteChanged();
            }
        }

        private static IEnumerable<(string Label, TypeId TypeId)> GetSupportedComponentTypes()
            => s_registeredComponents.Value
                .Where(d => d.IsEditorEnabled)
                .Select(d => (d.Label, d.TypeId));

        private sealed record ComponentTypeDescriptor(
            string Label,
            Func<TypeId> GetTypeId,
            Func<Entity, InspectorComponentViewModelBase> CreateViewModel,
            Action<EntityManager, Entity> AddComponent)
        {
            // Evaluated on first access rather than at static init time
            public TypeId TypeId => GetTypeId();
            public bool IsEditorEnabled => Class.TryGetClass(TypeId)?.GetAttribute("editor")?.GetBool() ?? true;
        }

        private static ComponentTypeDescriptor? BuildDescriptor(Type componentType)
        {
            try
            {
                Class? cls = Class.TryGetClass(componentType);

                if (cls == null || !cls.Value.IsValid)
                    return null;

                Class componentClass = cls.Value;
                ClassAttribute? attrLabel = componentClass.GetAttribute("label");
                string label = attrLabel.HasValue ? attrLabel.Value.GetString() : componentClass.Name.ToString();

                Type vmType = typeof(InspectorComponentViewModel<>).MakeGenericType(componentType);

                return new ComponentTypeDescriptor(
                    label,
                    () => componentClass.TypeId,
                    entity => (InspectorComponentViewModelBase)Activator.CreateInstance(vmType, entity)!,
                    (mgr, entity) => mgr.AddDefaultComponent(entity, componentClass));
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Inspector failed to build descriptor for '{componentType.Name}': {ex.Message}");
                return null;
            }
        }

        private static readonly Lazy<ComponentTypeDescriptor[]> s_registeredComponents = new(() =>
            AppDomain.CurrentDomain.GetAssemblies()
                .SelectMany(a =>
                {
                    try { return a.GetTypes(); }
                    catch { return Array.Empty<Type>(); }
                })
                .Where(t => t.IsValueType && t.GetInterfaces().Contains(typeof(IComponent)))
                .Select(BuildDescriptor)
                .OfType<ComponentTypeDescriptor>()
                .ToArray());

        private bool CanAddComponent(object? parameter)
        {
            return parameter is AddComponentOptionViewModel option && option.IsEnabled && SelectedNode is Entity;
        }

        private async Task AddComponentAsync(object? parameter)
        {
            if (parameter is not AddComponentOptionViewModel option)
            {
                return;
            }

            if (SelectedNode is not Entity entity || entity.EntityManager == null)
            {
                return;
            }

            try
            {
                await EngineManager.PostToSimThread(() =>
                {
                    EntityManager? mgr = entity.EntityManager;

                    if (mgr == null)
                    {
                        Logger.Log(LogLevel.Warning, "Inspector failed to get EntityManager while adding component");

                        return;
                    }

                    try
                    {
                        ComponentTypeDescriptor? descriptor = s_registeredComponents.Value
                            .FirstOrDefault(d => d.TypeId == option.TypeId);

                        if (descriptor != null)
                            descriptor.AddComponent(mgr, entity);
                        else
                            Logger.Log(LogLevel.Warning, $"Inspector cannot add unsupported component type '{option.Label}'");
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to add component '{option.Label}': {ex.Message}");
                    }
                });
            }
            finally
            {
                Dispatcher.UIThread.Post(RefreshProperties);
            }
        }

        private bool CanRemoveComponent(object? parameter)
        {
            return parameter is InspectorComponentViewModelBase && SelectedNode is Entity;
        }

        private void RemoveComponent(object? parameter)
        {
            if (parameter is not InspectorComponentViewModelBase componentVm)
                return;

            if (SelectedNode is not Entity entity || entity.EntityManager == null)
                return;

            MessageBox.Info("Remove Component", $"Are you sure you want to remove the {componentVm.Label} component from {entity.Name}?")
                .Button("Remove", () => _ = RemoveComponentConfirmed(componentVm, entity))
                .Button("Cancel", () => { })
                .Show();
        }

        private async Task RemoveComponentConfirmed(InspectorComponentViewModelBase componentVm, Entity entity)
        {
            try
            {
                await EngineManager.PostToSimThread(() =>
                {
                    EntityManager? mgr = entity.EntityManager;

                    if (mgr == null)
                    {
                        Logger.Log(LogLevel.Warning, "Inspector failed to get EntityManager while removing component");
                        return;
                    }

                    try
                    {
                        mgr.RemoveComponent(entity, componentVm.TypeId);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to remove component '{componentVm.Label}': {ex.Message}");
                    }
                });
            }
            finally
            {
                Dispatcher.UIThread.Post(RefreshProperties);
            }
        }

        private bool EvaluateEditCondition(Class nodeClass, ClassAttribute? attrEditCondition, string memberName)
        {
            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return false;
            }

            if (attrEditCondition == null)
            {
                return true;
            }

            if (attrEditCondition.Value.IsString)
            {
                string methodName = attrEditCondition.Value.GetString();
                Method? conditionMethod = nodeClass.GetMethod(methodName);

                if (conditionMethod != null)
                {
                    using BoxedValue resultData = conditionMethod.Value.Invoke(SelectedNode);
                    object? result = resultData.GetValue();

                    if (result is bool boolResult)
                    {
                        return boolResult;
                    }

                    Logger.Log(LogLevel.Warning, $"Inspector editcondition method '{methodName}' on member '{memberName}' did not return a bool");
                }
            }
            else if (attrEditCondition.Value.IsBool)
            {
                return attrEditCondition.Value.GetBool();
            }
            else
            {
                Logger.Log(LogLevel.Warning, $"Inspector editcondition attribute on member '{memberName}' is not a valid type");
            }

            return true; // continue if no condition or invalid condition
        }
    }

    /// <summary>Expand/collapse state + chevron glyph for one inspector section header.</summary>
    public class InspectorSectionViewModel : ViewModelBase
    {
        private bool _isExpanded;
        public bool IsExpanded
        {
            get => _isExpanded;
            set
            {
                if (SetProperty(ref _isExpanded, value))
                {
                    OnPropertyChanged(nameof(ChevronKind));
                }
            }
        }

        public string ChevronKind => _isExpanded ? "ChevronDown" : "ChevronRight";
    }

    public class AddComponentOptionViewModel : ViewModelBase
    {
        private bool _isEnabled;

        public AddComponentOptionViewModel(string label, TypeId typeId, bool isEnabled)
        {
            Label = label;
            TypeId = typeId;
            _isEnabled = isEnabled;
        }

        public string Label { get; }
        public TypeId TypeId { get; }

        public bool IsEnabled
        {
            get => _isEnabled;
            set => SetProperty(ref _isEnabled, value);
        }
    }

    public class LayerCopySourceOptionViewModel : ViewModelBase
    {
        public LayerCopySourceOptionViewModel(string name, bool isBase)
        {
            Name = name;
            IsBase = isBase;
        }

        public string Name { get; }

        /// <summary>True when this option refers to the entity's base state rather than a layer.</summary>
        public bool IsBase { get; }
    }
}
