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

        public ICommand ApplyCopyFromLayerCommand { get; }

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

                    // The copy target (active layer vs base) depends on override mode
                    _ = RefreshCopyLayerSourcesAsync();
                }
            }
        }

        private string? _activeLayerDisplay;
        public string? ActiveLayerDisplay
        {
            get => _activeLayerDisplay;
            private set
            {
                if (SetProperty(ref _activeLayerDisplay, value))
                {
                    OnPropertyChanged(nameof(IsDefaultLayer));
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
                LayerOverrideEditContext.ActiveLayerName = ActiveLayerDisplay;
            });

            _ = RefreshCopyLayerSourcesAsync();
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

                    row.SetOverrideInfo(overriddenLayers);
                }
            });
        }

        /// <summary>
        /// Called when the World's active layer changes; refreshes property rows and override
        /// signifiers (overrides may have applied natively) and updates the edit context.
        /// </summary>
        public void OnWorldActiveLayerChanged(string layerName)
        {
            Dispatcher.UIThread.VerifyAccess();

            LayerOverrideEditContext.ActiveLayerName = string.IsNullOrEmpty(layerName) ? null : layerName;
            ActiveLayerDisplay = LayerOverrideEditContext.ActiveLayerName;

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

                    CopyLayerSources.Add(new LayerCopySourceOptionViewModel(layerName, isBase: false));
                }

                HasCopyLayerSources = CopyLayerSources.Count > 0;

                SelectedCopyLayerSource = previousSelection != null
                    ? CopyLayerSources.FirstOrDefault(option => option.Name == previousSelection)
                    : null;
            });
        }

        /// <summary>
        /// Copies the selected source's effective property values onto the current edit target
        /// (the active layer's override set when override mode is on, otherwise the entity's
        /// base values). Diff-based: only properties whose target value actually changes are
        /// written, and an override that would end up equal to the base value is pruned rather
        /// than stored (e.g. copying from Base empties the target layer's override set).
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

            bool targetIsLayer = LayerOverrideMode && !string.IsNullOrEmpty(ActiveLayerDisplay);
            string targetDisplay = targetIsLayer ? ActiveLayerDisplay! : "Base";
            string sourceDisplay = sourceOption.IsBase ? "Base" : sourceOption.Name;

            if (!targetIsLayer && sourceOption.IsBase)
            {
                return; // base -> base is a no-op
            }

            if (targetIsLayer && !sourceOption.IsBase && sourceOption.Name == targetDisplay)
            {
                return; // layer -> itself is a no-op
            }

            Entity capturedEntity = entity;
            bool capturedTargetIsLayer = targetIsLayer;
            string capturedTargetName = targetDisplay;
            string capturedSourceName = sourceOption.Name;
            bool capturedSourceIsBase = sourceOption.IsBase;
            string capturedSourceDisplay = sourceDisplay;
            string capturedTargetDisplay = targetDisplay;

            int changedCount = 0;

            await EngineManager.PostToSimThread(() =>
            {
                Name? targetLayer = capturedTargetIsLayer ? new Name(capturedTargetName) : null;
                Name? sourceLayer = capturedSourceIsBase ? null : new Name(capturedSourceName);

                List<LayerCopyPlanEntry> plan = BuildLayerCopyPlan(capturedEntity, targetLayer, sourceLayer);

                if (plan.Count == 0)
                {
                    return;
                }

                changedCount = plan.Count;

                void ApplyEntries() => ApplyLayerCopyEntries(capturedEntity, targetLayer, plan, applyNewState: true);
                void RevertEntries() => ApplyLayerCopyEntries(capturedEntity, targetLayer, plan, applyNewState: false);

                EditorProject? project = EngineManager.CurrentProject;

                if (project != null)
                {
                    project.ActionStack.PushAction(new EditorAction(
                        $"Copy Properties ({capturedSourceDisplay} -> {capturedTargetDisplay})",
                        (_, _) => ApplyEntries(),
                        (_, _) => RevertEntries()));
                }
                else
                {
                    // No project to record undo against - still apply the copy.
                    ApplyEntries();
                }
            });

            Logger.Log(LogLevel.Info, changedCount > 0
                ? $"Copy layer properties: {sourceDisplay} -> {targetDisplay} ({changedCount} propert{(changedCount == 1 ? "y" : "ies")} changed)"
                : $"Copy layer properties: {sourceDisplay} -> {targetDisplay} (no differences)");

            if (changedCount > 0)
            {
                OnPropertyValueChanged();
            }
        }

        /// <summary>
        /// Computes the minimal set of writes needed to make the target (base, or a layer's
        /// override set) match the source's effective values. Only properties whose target
        /// value would actually change produce entries, and overrides that would end up
        /// redundant (equal to the base value) are pruned instead of copied.
        /// Must be called on the sim thread.
        /// </summary>
        private static List<LayerCopyPlanEntry> BuildLayerCopyPlan(Entity entity, Name? targetLayer, Name? sourceLayer)
        {
            List<LayerCopyPlanEntry> plan = new List<LayerCopyPlanEntry>();

            if (!targetLayer.HasValue && !sourceLayer.HasValue)
            {
                return plan; // base -> base, nothing to do
            }

            foreach (Property property in entity.Class.Properties)
            {
                Name propertyName = property.Name;

                // Entity name is identity metadata, not layer-overridable state
                if (propertyName == "Name")
                {
                    continue;
                }

                if (property.GetAttribute("jsonignore") != null)
                {
                    continue;
                }

                object? baseValueObj = null;
                bool hasBaseValue = false;

                if (TryGetEntityBaseValue(entity, propertyName, out BoxedValue baseValue))
                {
                    try
                    {
                        baseValueObj = baseValue.GetValue();
                        hasBaseValue = true;
                    }
                    catch
                    {
                        hasBaseValue = false;
                    }
                    finally
                    {
                        baseValue.Dispose();
                    }
                }

                if (!hasBaseValue)
                {
                    continue;
                }

                //-- Source effective value: the source layer's override when present, base otherwise

                object? sourceValueObj = baseValueObj;

                if (sourceLayer.HasValue
                    && EntityLayerOverrides.IsPropertyOverridden(entity, sourceLayer.Value, propertyName)
                    && EntityLayerOverrides.GetValue(entity, sourceLayer.Value, propertyName, out BoxedValue sourceOverride))
                {
                    try
                    {
                        sourceValueObj = sourceOverride.GetValue();
                    }
                    catch
                    {
                        sourceValueObj = baseValueObj;
                    }
                    finally
                    {
                        sourceOverride.Dispose();
                    }
                }

                if (Equals(sourceValueObj, baseValueObj))
                {
                    // Source matches base here: the only change a copy can need is pruning the
                    // target's (redundant) override of this property
                    if (!targetLayer.HasValue
                        || !EntityLayerOverrides.IsPropertyOverridden(entity, targetLayer.Value, propertyName))
                    {
                        continue;
                    }

                    object? oldOverrideObj = null;
                    bool hadOldOverride = false;

                    if (EntityLayerOverrides.GetValue(entity, targetLayer.Value, propertyName, out BoxedValue oldOverride))
                    {
                        try
                        {
                            oldOverrideObj = oldOverride.GetValue();
                            hadOldOverride = true;
                        }
                        catch
                        {
                            hadOldOverride = false;
                        }
                        finally
                        {
                            oldOverride.Dispose();
                        }
                    }

                    plan.Add(new LayerCopyPlanEntry(LayerCopyEntryKind.RemoveOverride, propertyName,
                        newValue: null, hasNewValue: false, oldValue: oldOverrideObj, hasOldValue: hadOldOverride));

                    continue;
                }

                //-- Source differs from base

                if (!targetLayer.HasValue)
                {
                    // Copying into base: write the source value as the new base
                    plan.Add(new LayerCopyPlanEntry(LayerCopyEntryKind.SetBase, propertyName,
                        newValue: sourceValueObj, hasNewValue: true, oldValue: baseValueObj, hasOldValue: true));

                    continue;
                }

                object? currentTargetObj = null;
                bool hasCurrentTarget = false;
                bool targetOverridden = EntityLayerOverrides.IsPropertyOverridden(entity, targetLayer.Value, propertyName);

                if (targetOverridden
                    && EntityLayerOverrides.GetValue(entity, targetLayer.Value, propertyName, out BoxedValue currentOverride))
                {
                    try
                    {
                        currentTargetObj = currentOverride.GetValue();
                        hasCurrentTarget = true;
                    }
                    catch
                    {
                        hasCurrentTarget = false;
                    }
                    finally
                    {
                        currentOverride.Dispose();
                    }
                }

                if (targetOverridden && hasCurrentTarget && Equals(currentTargetObj, sourceValueObj))
                {
                    continue; // target already matches the source
                }

                plan.Add(new LayerCopyPlanEntry(LayerCopyEntryKind.SetOverride, propertyName,
                    newValue: sourceValueObj, hasNewValue: true, oldValue: currentTargetObj, hasOldValue: targetOverridden && hasCurrentTarget));
            }

            return plan;
        }

        /// <summary>
        /// Reads the entity's true base value for a property. While an override layer is applied
        /// the live values contain that layer's overrides, so the applied layer's base snapshot
        /// is consulted first; properties outside the snapshot fall back to the live value (the
        /// applied set does not modify them). Must be called on the sim thread.
        /// </summary>
        private static bool TryGetEntityBaseValue(Entity entity, Name propertyName, out BoxedValue baseValue)
        {
            Name appliedLayer = EntityLayerOverrides.GetAppliedLayer(entity);

            if (appliedLayer.Valid && EntityLayerOverrides.GetBaseValue(entity, appliedLayer, propertyName, out baseValue))
            {
                return true;
            }

            // Querying with any layer name other than the applied one reads the live value
            return EntityLayerOverrides.GetBaseValue(entity, LiveBaseQueryLayerName, propertyName, out baseValue);
        }

        /// <summary>
        /// Applies or reverts a copy plan. Must be called on the sim thread.
        /// </summary>
        private static void ApplyLayerCopyEntries(Entity entity, Name? targetLayer, List<LayerCopyPlanEntry> entries, bool applyNewState)
        {
            foreach (LayerCopyPlanEntry entry in entries)
            {
                try
                {
                    switch (entry.Kind)
                    {
                        case LayerCopyEntryKind.SetBase:
                        {
                            object? valueObj = applyNewState ? entry.NewValue : entry.OldValue;
                            bool hasValue = applyNewState ? entry.HasNewValue : entry.HasOldValue;

                            if (!hasValue)
                            {
                                break;
                            }

                            using BoxedValue value = new BoxedValue(valueObj);

                            EntityLayerOverrides.SetBaseValue(entity, entry.PropertyName, value);

                            break;
                        }

                        case LayerCopyEntryKind.SetOverride:
                        case LayerCopyEntryKind.RemoveOverride:
                        {
                            if (!targetLayer.HasValue)
                            {
                                break;
                            }

                            // Forward: write a new override / prune; Revert: restore the previous
                            // override / prune again
                            bool writeOverride = applyNewState == (entry.Kind == LayerCopyEntryKind.SetOverride);

                            if (!writeOverride)
                            {
                                EntityLayerOverrides.RemoveValue(entity, targetLayer.Value, entry.PropertyName);

                                break;
                            }

                            object? valueObj = applyNewState ? entry.NewValue : entry.OldValue;
                            bool hasValue = applyNewState ? entry.HasNewValue : entry.HasOldValue;

                            if (!hasValue)
                            {
                                EntityLayerOverrides.RemoveValue(entity, targetLayer.Value, entry.PropertyName);

                                break;
                            }

                            EnsureLayerOverrideSet(entity, targetLayer.Value);

                            using BoxedValue value = new BoxedValue(valueObj);

                            EntityLayerOverrides.SetValue(entity, targetLayer.Value, entry.PropertyName, value);

                            break;
                        }
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Layer copy failed for property '{entry.PropertyName}': {ex.Message}");
                }
            }
        }

        private static void EnsureLayerOverrideSet(Entity entity, Name layerName)
        {
            if (!EntityLayerOverrides.HasSet(entity, layerName))
            {
                EntityLayerOverrides.AddSet(entity, layerName);
            }
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

        // Never a real layer: querying base values with this name makes the native side read the
        // entity's live property value (any name != applied layer behaves this way)
        private static readonly Name LiveBaseQueryLayerName = new Name("$EditorLiveBaseQuery");

        private enum LayerCopyEntryKind
        {
            SetBase,
            SetOverride,
            RemoveOverride
        }

        private sealed class LayerCopyPlanEntry
        {
            public LayerCopyPlanEntry(LayerCopyEntryKind kind, Name propertyName,
                object? newValue, bool hasNewValue, object? oldValue, bool hasOldValue)
            {
                Kind = kind;
                PropertyName = propertyName;
                NewValue = newValue;
                HasNewValue = hasNewValue;
                OldValue = oldValue;
                HasOldValue = hasOldValue;
            }

            public LayerCopyEntryKind Kind { get; }
            public Name PropertyName { get; }
            public object? NewValue { get; }
            public bool HasNewValue { get; }
            public object? OldValue { get; }
            public bool HasOldValue { get; }
        }

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
