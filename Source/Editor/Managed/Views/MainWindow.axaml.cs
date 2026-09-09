using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Platform;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Dock.Model;
using Dock.Model.Core;
using Dock.Model.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using Hyperion;
using Hyperion.Editor.ViewModels;
using Hyperion.Editor.Services;
using Document = Dock.Model.Avalonia.Controls.Document;
using DocumentDock = Dock.Model.Avalonia.Controls.DocumentDock;
using ProportionalDock = Dock.Model.Avalonia.Controls.ProportionalDock;
using ProportionalDockSplitter = Dock.Model.Avalonia.Controls.ProportionalDockSplitter;
using RootDock = Dock.Model.Avalonia.Controls.RootDock;
using Tool = Dock.Model.Avalonia.Controls.Tool;
using ToolDock = Dock.Model.Avalonia.Controls.ToolDock;

namespace Hyperion.Editor
{
    public partial class MainWindow : Window
    {
        private const bool CappedFrameRate = true;
        private const bool IsRenderingOnMainThread = true;

        private static readonly DataFormat<NodeViewModel> NodeViewModelDragFormat =
            DataFormat.CreateInProcessFormat<NodeViewModel>("hyperion-nodeviewmodel");
        private static readonly DataFormat<string> AssetDragFormat =
            DataFormat.CreateStringApplicationFormat("hyperion-asset");
        private NodeViewModel? _dragCandidate;
        private PointerPressedEventArgs? _dragPressedArgs;
        private Point _dragStartPoint;
        private bool _isDragging;
        private bool _suppressTreeSelectionHandling;

        // Content browser drag tracking
        private ListBox? _contentBrowserAssetList;
        private AssetObjectViewModel? _assetDragCandidate;
        private PointerPressedEventArgs? _assetDragPressedArgs;
        private Point _assetDragStartPoint;
        private bool _isDraggingAsset;

        // Viewport drop tracking
        private Border? _viewportDropTarget;

        // Drop-indicator tracking and auto-scroll
        private TreeView? _sceneTree;
        private ScrollViewer? _sceneTreeScrollViewer;
        private DispatcherTimer? _autoScrollTimer;
        private double _autoScrollDelta;
        private const double AutoScrollZone = 36;  // px from edge that triggers scroll
        private const double AutoScrollSpeed = 10; // px per timer tick (~50 ms)

        private DropDownButton? _sceneDropDown;
        private EditorViewportControl? _editorViewport;
        private Tool? _panelTool;
        private IDockWindow? _panelWindow;
        private int _frameCounter;
        private bool _viewportNativeHidden;
        private bool _isClosing;
        private bool _suppressPanelSync;
        private readonly Dictionary<string, object?> _dockContents = new Dictionary<string, object?>();

        public MainWindow()
        {
            var viewModel = new MainWindowViewModel();
            MainWindowViewModel.Instance = viewModel;
            DataContext = viewModel;

            InitializeComponent();

            PanelService.Instance.ActivePanelChanged += OnPanelServiceActivePanelChanged;

            // Keep the floating panel window lifecycle in sync with dock operations.
            DockControl.Factory.DockableDocked += OnFactoryDockableChanged;
            DockControl.Factory.DockableMoved += OnFactoryDockableChanged;
            DockControl.Factory.DockableClosed += OnFactoryDockableChanged;

            AddHandler(InputElement.GotFocusEvent, OnInspectorTextBoxGotFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.LostFocusEvent, OnInspectorTextBoxLostFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.KeyDownEvent, OnInspectorTextBoxKeyDown, RoutingStrategies.Bubble);

            AddHandler(InputElement.LostFocusEvent, OnNodeRenameTextBoxLostFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.KeyDownEvent, OnNodeRenameTextBoxKeyDown, RoutingStrategies.Bubble);

            Opened += OnWindowOpened;
        }

        private void OnWindowOpened(object? sender, EventArgs e)
        {
            if (IsRenderingOnMainThread)
            {
                var topLevel = TopLevel.GetTopLevel(this);
                topLevel?.RequestAnimationFrame(OnFrame);
            }

            // The dock layout hosts the panel content inside its own template namescope, so the
            // named controls are not reachable via FindControl from the window. Locate them by
            // walking the visual tree once the dock has rendered and laid out.
            Dispatcher.UIThread.Post(OnWindowLoaded, DispatcherPriority.Loaded);
        }

        private void OnWindowLoaded()
        {
            InitializeViewportControl();
            InitializeSceneFlyout();
            SetupSceneHierarchyDragDrop();
            SetupContentBrowserDragDrop();
            SetupViewportDropTarget();
            CaptureDockContents();
        }

        private void OnResetLayoutClick(object? sender, RoutedEventArgs e)
        {
            ResetLayout();
        }

        /// <summary>
        /// Records each dockable's content by id so "Reset Layout" can rebuild the default
        /// layout tree with the original panel views.
        /// </summary>
        private void CaptureDockContents()
        {
            _dockContents.Clear();

            if (DockControl.Layout is not { } root)
            {
                return;
            }

            CollectDockContents(root);
        }

        private void CollectDockContents(IDockable dockable)
        {
            if (!string.IsNullOrEmpty(dockable.Id))
            {
                // Content is exposed by the concrete Tool/Document models, not by IDockable.
                object? content = dockable switch
                {
                    Tool tool => tool.Content,
                    Document document => document.Content,
                    _ => null
                };

                _dockContents[dockable.Id] = content;
            }

            if (dockable is IDock { VisibleDockables: { } visible })
            {
                foreach (IDockable child in visible)
                {
                    CollectDockContents(child);
                }
            }

            if (dockable is not IRootDock root)
            {
                return;
            }

            foreach (var list in new[] { root.LeftPinnedDockables, root.RightPinnedDockables, root.TopPinnedDockables, root.BottomPinnedDockables, root.HiddenDockables })
            {
                if (list == null)
                {
                    continue;
                }

                foreach (IDockable child in list)
                {
                    CollectDockContents(child);
                }
            }

            if (root.Windows != null)
            {
                foreach (IDockWindow? window in root.Windows)
                {
                    if (window?.Layout != null)
                    {
                        CollectDockContents(window.Layout);
                    }
                }
            }
        }

        private object? GetDockContent(string id)
        {
            return _dockContents.TryGetValue(id, out object? value) ? value : null;
        }

        /// <summary>
        /// Dynamic editor panels (edit asset, new physics shape, new scene, ...) open as a
        /// floating tool window. Opening another panel reuses and updates the same window.
        /// </summary>
        private void OnFactoryDockableChanged(object? sender, EventArgs e)
        {
            CleanupOrphanedPanelWindow();
        }

        /// <summary>
        /// Closes the visible floating window and deregisters it from the dock layout.
        /// The DockWindow model is not an Avalonia control - the visible window is its
        /// Host - so closing must go through the model's Exit().
        /// </summary>
        private void ClosePanelWindow()
        {
            if (_panelWindow == null)
            {
                return;
            }


            IDockWindow window = _panelWindow;
            _panelWindow = null;
            _panelTool = null;

            if (window.Host is Window hostWindow)
            {
                hostWindow.Closed -= OnPanelWindowClosed;
            }

            window.Exit();

            if (DockControl.Factory is FactoryBase factory)
            {
                factory.RemoveWindow(window);
            }
        }

        /// <summary>
        /// If the panel tool left the floating window (docked into the main layout, or
        /// closed via the chrome button), the now-empty floating window is closed.
        /// </summary>
        private void CleanupOrphanedPanelWindow()
        {
            if (_panelTool == null || _panelWindow == null || _isClosing || _suppressPanelSync)
            {
                return;
            }

            if (_panelWindow.Layout is IRootDock root && ContainsVisibleDockable(root, _panelTool))
            {
                return;
            }

            // The tool no longer lives visibly in the floating window. If it was docked
            // somewhere else it stays alive; if it was hidden/closed, release the panel.
            bool stillVisible = DockControl.Layout != null && ContainsVisibleDockable(DockControl.Layout, _panelTool);


            ClosePanelWindow();

            if (!stillVisible)
            {
                _suppressPanelSync = true;
                try
                {
                    PanelService.Instance.ClosePanel();
                }
                finally
                {
                    _suppressPanelSync = false;
                }
            }
        }

        private void OnPanelServiceActivePanelChanged(object? sender, EventArgs e)
        {
            SyncFloatingPanelWindow();
        }

        private IDockWindow? FindPanelWindow()
        {
            if (_panelTool == null || DockControl.Layout is not IRootDock mainRoot || mainRoot.Windows == null)
            {
                return null;
            }

            foreach (IDockWindow? window in mainRoot.Windows)
            {
                if (window?.Layout is IRootDock root && ContainsVisibleDockable(root, _panelTool))
                {
                    return window;
                }
            }

            return null;
        }

        private static bool ContainsVisibleDockable(IDockable container, IDockable target)
        {
            if (ReferenceEquals(container, target))
            {
                return true;
            }

            if (container is IDock dock && dock.VisibleDockables != null)
            {
                foreach (IDockable child in dock.VisibleDockables)
                {
                    if (ContainsVisibleDockable(child, target))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        private void SyncFloatingPanelWindow()
        {
            if (_suppressPanelSync || _isClosing)
            {
                return;
            }

            EditorPanelViewModel? panel = PanelService.Instance.ActivePanel;

            if (panel == null)
            {
                // Nothing active - make sure the floating panel window is gone.
                ClosePanelWindow();
                return;
            }

            ShowFloatingPanel(panel);
        }

        private void ShowFloatingPanel(EditorPanelViewModel panel)
        {
            try
            {
                if (DockControl.Factory is not FactoryBase factory || DockControl.Layout is not IRootDock mainRoot)
                {
                    Logger.Log(LogLevel.Warning,
                        $"PanelWindow: cannot open, Factory={DockControl.Factory?.GetType().Name ?? "null"}, Layout={DockControl.Layout?.GetType().Name ?? "null"}.");
                    return;
                }

                // A floating window for panels is already up - update it in place and focus it.
                if (_panelTool != null && FindPanelWindow() is { } existing)
                {
                    // Tool.Content must be a control (it is [TemplateContent]); the wrapper
                    // resolves the panel's view via the app data templates.
                    _panelTool.Content = new ContentControl { Content = panel };
                    _panelTool.Title = panel.Title;
                    existing.SetActive();
                    existing.Present(false);
                    return;
                }

                if (_panelTool != null)
                {
                    // The window still exists but the panel tool inside it was closed/hidden
                    // (HideToolsOnClose) - tear the stale window down and start fresh.
                    ClosePanelWindow();
                }

                _panelTool = new Tool
                {
                    Id = "EditorPanel",
                    Title = panel.Title,
                    Content = new ContentControl { Content = panel },
                    CanFloat = true
                };

                // Host the tool in its own floating window (non-modal).
                IDockWindow? window = factory.CreateWindowFrom(_panelTool);
                if (window == null)
                {
                    Logger.Log(LogLevel.Error, "Failed to create the editor panel window: CreateWindowFrom returned null.");
                    _panelTool = null;
                    return;
                }

                window.Title = panel.Title;

                if (window.Layout is not IRootDock floatingRoot)
                {
                    Logger.Log(LogLevel.Error, "PanelWindow: created window has no layout root.");
                    _panelTool = null;
                    return;
                }

                floatingRoot.Factory = factory;

                // Bind the window model to its host: sets window.Host / host.Window and
                // initializes the layout tree. Without this, Present skips sizing and
                // owner resolution and the window never becomes visible.
                factory.InitDockWindow(window, null, window as IHostWindow);
                floatingRoot.Window = window;

                mainRoot.Windows ??= factory.CreateList<IDockWindow>();
                mainRoot.Windows.Add(window);
                _panelWindow = window;

                // Present as a tall panel anchored to the right edge of the main window.
                double scaling = RenderScaling;
                const double panelWidth = 360.0;
                double panelHeight = Math.Max(240.0, ClientSize.Height - 16.0);
                window.Width = panelWidth;
                window.Height = panelHeight;
                window.X = Position.X + (int)((ClientSize.Width - panelWidth) * scaling) - (int)(8 * scaling);
                window.Y = Position.Y + (int)(8 * scaling);

                Logger.Log(LogLevel.Info,
                    $"PanelWindow: presenting. MainWindow Position=({Position.X},{Position.Y}) ClientSize=({ClientSize.Width},{ClientSize.Height}) RenderScaling={scaling}. " +
                    $"Target window X={window.X} Y={window.Y} Width={window.Width} Height={window.Height}. HostBeforePresent={(window.Host == null ? "null" : window.Host.GetType().Name)}.");

                window.Present(false);

                // The host window is resolved during Present; that Avalonia Window is what
                // actually closes, so watch it for user-initiated closes.
                if (window.Host is Window hostWindow)
                {
                    hostWindow.Closed += OnPanelWindowClosed;

                    // fix for macOS not showing window
                    if (hostWindow.Bounds.Width <= 0 || hostWindow.Bounds.Height <= 0)
                    {
                        hostWindow.Width = panelWidth;
                        hostWindow.Height = panelHeight;
                        hostWindow.Position = new PixelPoint((int)window.X, (int)window.Y);
                    }

                    hostWindow.Activate();

                    Logger.Log(LogLevel.Info,
                        $"PanelWindow: presented. IsVisible={hostWindow.IsVisible} Position=({hostWindow.Position.X},{hostWindow.Position.Y}) " +
                        $"Bounds={hostWindow.Bounds} Width={hostWindow.Width} Height={hostWindow.Height} WindowState={hostWindow.WindowState} " +
                        $"ShowInTaskbar={hostWindow.ShowInTaskbar} Opacity={hostWindow.Opacity} Screens={hostWindow.Screens?.All.Count ?? -1}.");
                }
                else
                {
                    Logger.Log(LogLevel.Error,
                        $"PanelWindow: after Present(), window.Host is {(window.Host == null ? "null" : window.Host.GetType().Name)} (not an Avalonia Window) - nothing was shown.");
                }

            }
            catch (Exception ex)
            {
                _panelTool = null;
                Logger.Log(LogLevel.Error, $"Failed to show editor panel window: {ex}");
            }
        }

        private void OnPanelWindowClosed(object? sender, EventArgs e)
        {

            if (sender is Window window)
            {
                window.Closed -= OnPanelWindowClosed;
            }

            _panelWindow = null;
            _panelTool = null;

            if (_isClosing)
            {
                return;
            }

            // The user closed the floating window - release the active panel so its
            // OnClosed callbacks run, without re-triggering this sync recursively.
            _suppressPanelSync = true;
            try
            {
                PanelService.Instance.ClosePanel();
            }
            finally
            {
                _suppressPanelSync = false;
            }
        }

        private IList<IDockable> CreateDockList(params IDockable[] items)
        {
            if (DockControl.Factory is FactoryBase factory)
            {
                return factory.CreateList<IDockable>(items);
            }

            return new List<IDockable>(items);
        }

        /// <summary>
        /// Builds the default dock layout, mirroring the layout declared in MainWindow.axaml.
        /// </summary>
        private IDock BuildDefaultLayout()
        {
            var hierarchyTool = new Tool { Id = "SceneHierarchy", Title = "Scene", Content = GetDockContent("SceneHierarchy") };
            var inspectorTool = new Tool { Id = "Inspector", Title = "Inspector", Content = GetDockContent("Inspector") };
            var assetsTool = new Tool { Id = "ContentBrowser", Title = "Assets", Content = GetDockContent("ContentBrowser") };
            var consoleTool = new Tool { Id = "Console", Title = "Console", Content = GetDockContent("Console") };
            var sceneDocument = new Document
            {
                Id = "Scene",
                Title = "Viewport",
                Content = GetDockContent("Scene"),
                CanDrag = false,
                CanFloat = false,
                CanPin = false,
                CanClose = false
            };

            var hierarchyPane = new ToolDock { Id = "HierarchyPane", Alignment = Alignment.Left, ActiveDockable = hierarchyTool, VisibleDockables = CreateDockList(hierarchyTool) };
            var inspectorPane = new ToolDock { Id = "InspectorPane", Alignment = Alignment.Bottom, ActiveDockable = inspectorTool, VisibleDockables = CreateDockList(inspectorTool) };
            var leftDock = new ProportionalDock { Id = "Left", Orientation = Orientation.Vertical, Proportion = 0.25, VisibleDockables = CreateDockList(hierarchyPane, new ProportionalDockSplitter(), inspectorPane) };

            var documentsPane = new DocumentDock { Id = "Documents", ActiveDockable = sceneDocument, VisibleDockables = CreateDockList(sceneDocument) };
            var assetsPane = new ToolDock { Id = "AssetsPane", Alignment = Alignment.Left, ActiveDockable = assetsTool, VisibleDockables = CreateDockList(assetsTool) };
            var consolePane = new ToolDock { Id = "ConsolePane", Alignment = Alignment.Right, ActiveDockable = consoleTool, VisibleDockables = CreateDockList(consoleTool) };
            var bottomDock = new ProportionalDock { Id = "Bottom", Orientation = Orientation.Horizontal, Proportion = 0.3, VisibleDockables = CreateDockList(assetsPane, new ProportionalDockSplitter(), consolePane) };
            var centerDock = new ProportionalDock { Id = "Center", Orientation = Orientation.Vertical, VisibleDockables = CreateDockList(documentsPane, new ProportionalDockSplitter(), bottomDock) };

            var mainLayout = new ProportionalDock { Id = "MainLayout", Orientation = Orientation.Horizontal, VisibleDockables = CreateDockList(leftDock, new ProportionalDockSplitter(), centerDock) };

            return new RootDock { Id = "Root", IsCollapsable = false, DefaultDockable = mainLayout, VisibleDockables = CreateDockList(mainLayout) };
        }

        /// <summary>
        /// Restores the default dock layout. The XAML-declared layout instance is mutated
        /// in place as panels are moved, so reset rebuilds a fresh tree from code and
        /// re-assigns the captured panel contents.
        /// </summary>
        private void ResetLayout()
        {
            if (_dockContents.Count == 0)
            {
                CaptureDockContents();
            }

            if (DockControl.Layout is IDock oldLayout && oldLayout.Close.CanExecute(null))
            {
                oldLayout.Close.Execute(null);
            }

            IDock root = BuildDefaultLayout();
            DockControl.Layout = root;

            // Mirror DockControl.OnAttachedToVisualTree: give the new root its window model.
            if (root is IRootDock rootDock && rootDock.Window == null && root.Factory is { } factory)
            {
                IDockWindow? windowModel = factory.CreateWindowFrom(rootDock);
                if (windowModel != null)
                {
                    windowModel.Layout = rootDock;
                    rootDock.Window = windowModel;
                }
            }
        }

        private T? FindVisualChildByName<T>(string name) where T : Control
        {
            return this.GetVisualDescendants().OfType<T>().FirstOrDefault(c => c.Name == name);
        }

        private void InitializeViewportControl()
        {
            _editorViewport = FindVisualChildByName<EditorViewportControl>("EditorViewportControl");
            if (_editorViewport == null)
            {
                Logger.Log(LogLevel.Warning, "EditorViewportControl control not found in the dock layout.");
                return;
            }

            _editorViewport.Focus();
        }

        /// <summary>
        /// The viewport is a native child window, which always renders above Avalonia
        /// overlays (airspace). Hide it while docking drag indicators or pinned panel
        /// previews are active so they stay visible over the viewport.
        /// </summary>
        private void UpdateViewportNativeVisibility()
        {
            if (_editorViewport == null)
            {
                return;
            }

            bool hide = DockControl.IsDraggingDock || IsPinnedPanelActive();

            if (hide != _viewportNativeHidden)
            {
                _viewportNativeHidden = hide;
                _editorViewport.SetNativeVisibility(!hide);
            }
        }

        private bool IsPinnedPanelActive()
        {
            if (DockControl.Layout is not IRootDock root || root.ActiveDockable is not IDockable active)
            {
                return false;
            }

            return DockControl.Factory?.IsDockablePinned(active, root) == true;
        }

        private void InitializeSceneFlyout()
        {
            _sceneDropDown = FindVisualChildByName<DropDownButton>("SceneDropDown");
            if (_sceneDropDown?.Flyout is Flyout flyout)
            {
                flyout.Opened += OnSceneFlyoutOpened;
            }
        }

        // While a property's text box has focus its view model must not overwrite the text from an
        // async read, or a refresh triggered by an edit elsewhere wipes out what is being typed.
        private void OnInspectorTextBoxGotFocus(object? sender, FocusChangedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.IsEditing = true;
            }
        }

        private void OnInspectorTextBoxLostFocus(object? sender, FocusChangedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.IsEditing = false;
                vm.CommitValue();
            }
        }

        private void OnInspectorTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Key == Key.Return && e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.CommitValue();
                e.Handled = true;
            }
        }

        private void OnNodeContextMenuOpened(object? sender, RoutedEventArgs e)
        {
            if ((sender as ContextMenu)?.DataContext is NodeViewModel nodeViewModel)
            {
                nodeViewModel.RefreshActions();
            }
        }

        private void OnAssetContextMenuOpened(object? sender, RoutedEventArgs e)
        {
            if ((sender as ContextMenu)?.DataContext is AssetObjectViewModel assetViewModel)
            {
                assetViewModel.RefreshActions();
            }
        }

        private void OnRenameNodeMenuItemClick(object? sender, RoutedEventArgs e)
        {
            if ((sender as MenuItem)?.DataContext is not NodeViewModel nodeViewModel)
                return;

            nodeViewModel.BeginRename();

            Dispatcher.UIThread.Post(() => FocusNodeRenameTextBox(nodeViewModel), DispatcherPriority.Loaded);
        }

        private void OnMoveToGrandparentMenuItemClick(object? sender, RoutedEventArgs e)
        {
            if ((sender as MenuItem)?.DataContext is not NodeViewModel nodeViewModel)
                return;

            NodeViewModel? grandparent = nodeViewModel.Parent?.Parent;
            if (grandparent == null)
                return;

            var vm = DataContext as MainWindowViewModel;
            vm?.SceneHierarchy.ReparentNode(nodeViewModel, grandparent);
        }

        private void FocusNodeRenameTextBox(NodeViewModel nodeViewModel)
        {
            if (_sceneTree == null)
                return;

            TextBox? textBox = _sceneTree.GetVisualDescendants()
                .OfType<TextBox>()
                .FirstOrDefault(t => t.DataContext == nodeViewModel);

            if (textBox != null)
            {
                textBox.Focus();
                textBox.SelectAll();
            }
        }

        private void OnNodeRenameTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Source is not TextBox { DataContext: NodeViewModel nodeViewModel } textBox)
                return;

            if (e.Key == Key.Return)
            {
                CommitNodeRename(nodeViewModel, textBox.Text ?? string.Empty);
                e.Handled = true;
            }
            else if (e.Key == Key.Escape)
            {
                nodeViewModel.CancelRename();
                e.Handled = true;
            }
        }

        private void OnNodeRenameTextBoxLostFocus(object? sender, FocusChangedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: NodeViewModel nodeViewModel } textBox && nodeViewModel.IsEditingName)
            {
                CommitNodeRename(nodeViewModel, textBox.Text ?? string.Empty);
            }
        }

        private void CommitNodeRename(NodeViewModel nodeViewModel, string newName)
        {
            nodeViewModel.IsEditingName = false;

            string trimmedName = newName.Trim();

            if (string.IsNullOrEmpty(trimmedName) || trimmedName == nodeViewModel.Name)
                return;

            if (DataContext is MainWindowViewModel mvm)
            {
                mvm.SceneHierarchy.RenameNode(nodeViewModel, trimmedName);
            }
        }

        private void SetupSceneHierarchyDragDrop()
        {
            _sceneTree = FindVisualChildByName<TreeView>("SceneHierarchyTreeView");
            if (_sceneTree == null)
                return;

            DragDrop.SetAllowDrop(_sceneTree, true);


            _sceneTree.AddHandler(InputElement.PointerPressedEvent, OnSceneTreePointerPressed, RoutingStrategies.Tunnel);
            _sceneTree.AddHandler(InputElement.PointerMovedEvent, OnSceneTreePointerMoved, RoutingStrategies.Tunnel);
            _sceneTree.AddHandler(InputElement.PointerReleasedEvent, OnSceneTreePointerReleased, RoutingStrategies.Tunnel);
            _sceneTree.AddHandler(DragDrop.DragOverEvent, OnSceneTreeDragOver);
            _sceneTree.AddHandler(DragDrop.DragLeaveEvent, OnSceneTreeDragLeave);
            _sceneTree.AddHandler(DragDrop.DropEvent, OnSceneTreeDrop);

            _sceneTree.SelectionChanged += OnSceneTreeSelectionChanged;

            // Lazily find the internal ScrollViewer once the template is applied.
            _sceneTree.TemplateApplied += (_, _) =>
            {
                _sceneTreeScrollViewer = _sceneTree.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
            };
        }

        private void OnSceneTreePointerPressed(object? sender, PointerPressedEventArgs e)
        {
            var point = e.GetCurrentPoint(sender as Visual);

            if (point.Properties.IsRightButtonPressed)
            {
                var nodeVm = FindNodeViewModelInEventSource(e.Source);
                if (nodeVm != null
                    && DataContext is MainWindowViewModel mvm
                    && !mvm.SceneHierarchy.SelectedNodes.Contains(nodeVm))
                {
                    mvm.SelectSingleNodeExclusive(nodeVm);

                    _suppressTreeSelectionHandling = true;
                    try
                    {
                        _sceneTree.SelectedItem = nodeVm;
                    }
                    finally
                    {
                        _suppressTreeSelectionHandling = false;
                    }
                }

                return;
            }

            if (point.Properties.IsLeftButtonPressed)
            {
                var keyModifiers = e.KeyModifiers;

                if ((keyModifiers & KeyModifiers.Shift) != 0)
                {
                    var nodeVm = FindNodeViewModelInEventSource(e.Source);
                    if (nodeVm != null)
                    {
                        var vm = DataContext as MainWindowViewModel;
                        vm?.HandleShiftClick(nodeVm);
                    }

                    e.Handled = true;
                    return;
                }

                // Suppress SelectedNodeChanged notification until SelectionChanged handles the sync
                if (DataContext is MainWindowViewModel mvm)
                {
                    mvm.SceneHierarchy.SetSuppressSelectionNotifications(true);
                }

                _dragCandidate = FindNodeViewModelInEventSource(e.Source);
                _dragPressedArgs = e;
                _dragStartPoint = e.GetPosition(sender as Visual);
                _isDragging = false;
            }
        }

        private async void OnSceneTreePointerMoved(object? sender, PointerEventArgs e)
        {
            if (_dragCandidate == null || _isDragging)
                return;

            if (!e.GetCurrentPoint(sender as Visual).Properties.IsLeftButtonPressed)
            {
                _dragCandidate = null;
                _dragPressedArgs = null;
                return;
            }

            var pos = e.GetPosition(sender as Visual);
            var delta = pos - _dragStartPoint;

            // Only start a drag after a small movement threshold to avoid stealing normal clicks.
            if (Math.Abs(delta.X) < 5 && Math.Abs(delta.Y) < 5)
                return;

            if (_dragPressedArgs == null)
            {
                _dragCandidate = null;
                return;
            }

            _isDragging = true;
            var candidate = _dragCandidate;

            e.Pointer.Capture(_sceneTree);

            var data = new DataTransfer();
            data.Add(DataTransferItem.Create(NodeViewModelDragFormat, candidate));

            try
            {
                await DragDrop.DoDragDropAsync(_dragPressedArgs, data, DragDropEffects.Move);
            }
            catch (Exception ex) when (ex is System.Runtime.InteropServices.COMException)
            {
                // DoDragDrop can throw on Windows if the drag is cancelled externally
                // or the pointer state is unexpected; treat as a cancelled drag.
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, $"Scene hierarchy drag-and-drop failed: {ex}");
            }
            finally
            {
                EndDrag();
            }
        }

        private void OnSceneTreePointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            if (!_isDragging)
            {
                _dragCandidate = null;
                _dragPressedArgs = null;
            }
        }

        private void OnSceneTreeDragOver(object? sender, DragEventArgs e)
        {
            if (e.DataTransfer.Contains(AssetDragFormat))
            {
                var t = FindNodeViewModelInEventSource(e.Source);
                e.DragEffects = t != null ? DragDropEffects.Copy : DragDropEffects.None;

                var vm = DataContext as MainWindowViewModel;
                vm?.SceneHierarchy.SetDropTarget(t);
                UpdateAutoScroll(e.GetPosition(_sceneTree));
                e.Handled = true;
                return;
            }

            if (!e.DataTransfer.Contains(NodeViewModelDragFormat))
            {
                e.DragEffects = DragDropEffects.None;
                return;
            }

            var vm_node = DataContext as MainWindowViewModel;
            var dragged = e.DataTransfer.TryGetValue(NodeViewModelDragFormat);
            var target = FindNodeViewModelInEventSource(e.Source);

            bool valid = dragged != null
                && target != null
                && target != dragged
                && !SceneHierarchyViewModel.IsAncestorOf(dragged, target);

            e.DragEffects = valid ? DragDropEffects.Move : DragDropEffects.None;

            vm_node?.SceneHierarchy.SetDropTarget(valid ? target : null);

            UpdateAutoScroll(e.GetPosition(_sceneTree));

            e.Handled = true;
        }

        private void OnSceneTreeDragLeave(object? sender, DragEventArgs e)
        {
            EndDrag();
        }

        private void OnSceneTreeDrop(object? sender, DragEventArgs e)
        {
            if (e.DataTransfer.Contains(AssetDragFormat))
            {
                var t = FindNodeViewModelInEventSource(e.Source);
                EndDrag();

                var vm = DataContext as MainWindowViewModel;
                if (vm != null)
                {
                    var assetData = e.DataTransfer.TryGetValue(AssetDragFormat);
                    if (!string.IsNullOrEmpty(assetData))
                    {
                        var parts = assetData.Split('|');
                        if (parts.Length == 2 && uint.TryParse(parts[0], out uint bucketIndex))
                        {
                            vm.AddAssetToScene(bucketIndex, new Name(parts[1]));
                        }
                    }
                }

                e.Handled = true;
                return;
            }

            if (!e.DataTransfer.Contains(NodeViewModelDragFormat))
                return;

            var dragged = e.DataTransfer.TryGetValue(NodeViewModelDragFormat);
            var target = FindNodeViewModelInEventSource(e.Source);

            EndDrag();

            if (dragged == null || target == null)
                return;

            var vm_node = DataContext as MainWindowViewModel;
            vm_node?.SceneHierarchy.ReparentNode(dragged, target);

            e.Handled = true;
        }

        private void EndDrag()
        {
            _dragPressedArgs?.Pointer.Capture(null);

            _isDragging = false;
            _dragCandidate = null;
            _dragPressedArgs = null;

            var vm = DataContext as MainWindowViewModel;
            vm?.SceneHierarchy.SetDropTarget(null);

            _autoScrollDelta = 0;
            _autoScrollTimer?.Stop();
        }

        private void OnSceneTreeSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            var mvm = DataContext as MainWindowViewModel;
            if (mvm == null)
                return;

            if (_suppressTreeSelectionHandling)
            {
                mvm.SceneHierarchy.SetSuppressSelectionNotifications(false);
                return;
            }

            var added = e.AddedItems.OfType<NodeViewModel>().ToList();
            var removed = e.RemovedItems.OfType<NodeViewModel>().ToList();

            // Un-suppress after SelectedItem binding fired (suppressed)
            mvm.SceneHierarchy.SetSuppressSelectionNotifications(false);

            mvm.HandleTreeSelectionChanged(added, removed);
        }

        private void UpdateAutoScroll(Point posRelativeToTree)
        {
            if (_sceneTree == null)
                return;

            _sceneTreeScrollViewer ??= _sceneTree.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();

            if (_sceneTreeScrollViewer == null)
                return;

            var treeHeight = _sceneTree.Bounds.Height;

            if (posRelativeToTree.Y < AutoScrollZone)
                _autoScrollDelta = -AutoScrollSpeed * (1.0 - posRelativeToTree.Y / AutoScrollZone);
            else if (posRelativeToTree.Y > treeHeight - AutoScrollZone)
                _autoScrollDelta = AutoScrollSpeed * (1.0 - (treeHeight - posRelativeToTree.Y) / AutoScrollZone);
            else
                _autoScrollDelta = 0;

            if (_autoScrollDelta != 0)
            {
                if (_autoScrollTimer == null)
                {
                    _autoScrollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(50) };
                    _autoScrollTimer.Tick += OnAutoScrollTick;
                }

                if (!_autoScrollTimer.IsEnabled)
                    _autoScrollTimer.Start();

                // Also step immediately on this DragOver rather than waiting for the next timer
                // tick, since DispatcherTimer ticks can be delayed while the OS-driven drag loop
                // is pumping messages.
                ApplyAutoScrollStep();
            }
            else
            {
                _autoScrollTimer?.Stop();
            }
        }

        private void OnAutoScrollTick(object? sender, EventArgs e)
        {
            if (_sceneTreeScrollViewer == null || _autoScrollDelta == 0)
            {
                _autoScrollTimer?.Stop();
                return;
            }

            ApplyAutoScrollStep();
        }

        private void ApplyAutoScrollStep()
        {
            if (_sceneTreeScrollViewer == null || _autoScrollDelta == 0)
                return;

            var offset = _sceneTreeScrollViewer.Offset;
            _sceneTreeScrollViewer.Offset = new Vector(offset.X, Math.Max(0, offset.Y + _autoScrollDelta));
        }

        private static NodeViewModel? FindNodeViewModelInEventSource(object? source)
        {
            // Walk up the visual tree from the event source to find a DataContext that is a NodeViewModel.
            var control = source as Control;
            while (control != null)
            {
                if (control.DataContext is NodeViewModel nvm)
                    return nvm;
                control = control.Parent as Control;
            }
            return null;
        }

        private void SetupContentBrowserDragDrop()
        {
            _contentBrowserAssetList = FindVisualChildByName<ListBox>("ContentBrowserAssetList");
            if (_contentBrowserAssetList == null)
                return;

            DragDrop.SetAllowDrop(_contentBrowserAssetList, true);

            _contentBrowserAssetList.AddHandler(InputElement.PointerPressedEvent, OnContentBrowserPointerPressed, RoutingStrategies.Tunnel);
            _contentBrowserAssetList.AddHandler(InputElement.PointerMovedEvent, OnContentBrowserPointerMoved, RoutingStrategies.Tunnel);
        }

        private void OnContentBrowserPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            var point = e.GetCurrentPoint(sender as Visual);

            if (point.Properties.IsLeftButtonPressed)
            {
                _assetDragCandidate = FindAssetViewModelInEventSource(e.Source);
                _assetDragPressedArgs = e;
                _assetDragStartPoint = e.GetPosition(sender as Visual);
                _isDraggingAsset = false;
            }
        }

        private async void OnContentBrowserPointerMoved(object? sender, PointerEventArgs e)
        {
            if (_assetDragCandidate == null || _isDraggingAsset)
                return;

            if (!e.GetCurrentPoint(sender as Visual).Properties.IsLeftButtonPressed)
            {
                _assetDragCandidate = null;
                _assetDragPressedArgs = null;
                return;
            }

            var pos = e.GetPosition(sender as Visual);
            var delta = pos - _assetDragStartPoint;

            if (Math.Abs(delta.X) < 5 && Math.Abs(delta.Y) < 5)
                return;

            if (_assetDragPressedArgs == null)
            {
                _assetDragCandidate = null;
                return;
            }

            _isDraggingAsset = true;
            var candidate = _assetDragCandidate;

            var data = new DataTransfer();
            data.Add(DataTransferItem.Create(AssetDragFormat, $"{candidate.Bucket?.BucketIndex ?? 0}|{candidate.AssetDesc.Name}"));

            try
            {
                await DragDrop.DoDragDropAsync(_assetDragPressedArgs, data, DragDropEffects.Copy);
            }
            catch (Exception ex) when (ex is System.Runtime.InteropServices.COMException)
            {
            }
            finally
            {
                _isDraggingAsset = false;
                _assetDragCandidate = null;
                _assetDragPressedArgs = null;
            }
        }

        private static AssetObjectViewModel? FindAssetViewModelInEventSource(object? source)
        {
            var control = source as Control;
            while (control != null)
            {
                if (control.DataContext is AssetObjectViewModel avm)
                    return avm;
                control = control.Parent as Control;
            }
            return null;
        }

        private void SetupViewportDropTarget()
        {
            _viewportDropTarget = FindVisualChildByName<Border>("ViewportDropTarget");
            if (_viewportDropTarget == null)
                return;

            DragDrop.SetAllowDrop(_viewportDropTarget, true);
            _viewportDropTarget.AddHandler(DragDrop.DragOverEvent, OnViewportDragOver);
            _viewportDropTarget.AddHandler(DragDrop.DropEvent, OnViewportDrop);
        }

        private void OnViewportDragOver(object? sender, DragEventArgs e)
        {
            if (e.DataTransfer.Contains(AssetDragFormat))
            {
                e.DragEffects = DragDropEffects.Copy;
                e.Handled = true;
            }
        }

        private void OnViewportDrop(object? sender, DragEventArgs e)
        {
            if (!e.DataTransfer.Contains(AssetDragFormat))
                return;

            var assetData = e.DataTransfer.TryGetValue(AssetDragFormat);
            if (string.IsNullOrEmpty(assetData))
                return;

            var parts = assetData.Split('|');
            if (parts.Length != 2 || !uint.TryParse(parts[0], out uint bucketIndex))
                return;

            // Calculate normalized drop position within the viewport
            var pos = e.GetPosition(_viewportDropTarget);
            double nx = Math.Clamp(pos.X / _viewportDropTarget.Bounds.Width, 0.0, 1.0);
            double ny = Math.Clamp(pos.Y / _viewportDropTarget.Bounds.Height, 0.0, 1.0);

            var vm = DataContext as MainWindowViewModel;
            vm?.AddAssetToSceneAtViewport(bucketIndex, new Name(parts[1]), (float)nx, (float)ny);

            e.Handled = true;
        }

        protected override void OnClosing(WindowClosingEventArgs e)
        {
            _isClosing = true;

            // Disable main thread loop until this is done
            // This should prevent MainThread::Update() from being triggered by avalonia
            // directly after clicking any of the messagebox buttons
            EngineManager.DisableMainLoop = true;

            try
            {
                MainWindowViewModel vm = DataContext as MainWindowViewModel;
                EditorProject? project = EngineManager.CurrentProject;

                void SaveProjectSynchronous()
                {
                    if (vm == null)
                    {
                        return;
                    }

                    bool shouldTimeout = project != null && project.IsSaved;

                    Task task = EngineManager.PostToSimThread(() => vm.SaveProject.Execute(null));
                    bool taskCompleted = true;

                    if (shouldTimeout)
                        taskCompleted = task.Wait(TimeSpan.FromSeconds(30));
                    else
                        task.Wait();

                    if (!taskCompleted)
                    {
                        Logger.Log(LogLevel.Error, "Failed to save project in a reasonable amount of time, so canceling exiting the editor process to prevent loss of data.");
                        e.Cancel = true;

                        return;
                    }
                }

                MessageBox.Info("Save changes?", "Closing will discard any unsaved changes. Do you want to save changes before exiting?")
                    .Button("Save", SaveProjectSynchronous)
                    .Button("Discard", () => { })
                    .Button("Cancel", () => e.Cancel = true)
                    .Show();

                base.OnClosing(e);

                EngineManager.DisableMainLoop = false;

                if (e.Cancel)
                {
                    // @TODO Implement windows-only hack here for removing the parent hwnd -- otherwise we get that nasty crash when closing the editor
                    // we'll need to remove the parent hwnd on close, add it back in here.
                }
                else
                {
                    EngineManager.Shutdown();
                }
            }
            catch (Exception)
            {
                EngineManager.DisableMainLoop = false;
            }
        }

        private void OnFrame(TimeSpan time)
        {
            if (!EngineManager.DisableMainLoop)
            {
                NativeBindings.Hyp_MainThreadUpdate();
            }

            ConsoleService.Instance.ProcessLogQueue();

            // Periodic maintenance: keep the native viewport out of the way of docking
            // overlays, and close orphaned floating panel windows.
            if (++_frameCounter % 10 == 0)
            {
                UpdateViewportNativeVisibility();
            }

            if (_frameCounter % 30 == 0)
            {
                CleanupOrphanedPanelWindow();
            }

            var topLevel = GetTopLevel(this);
            topLevel?.RequestAnimationFrame(OnFrame);
        }

        private bool _flyoutHandlerAttached;

        private void OnSceneFlyoutOpened(object? sender, EventArgs e)
        {
            if (_flyoutHandlerAttached
                || _sceneDropDown is not { } dropDown
                || dropDown.Flyout is not Flyout flyout
                || flyout.Content is not StackPanel panel)
            {
                return;
            }

            panel.AddHandler(
                InputElement.PointerReleasedEvent,
                OnSceneFlyoutPointerReleased,
                RoutingStrategies.Bubble,
                handledEventsToo: true);

            _flyoutHandlerAttached = true;
        }

        private void OnSceneFlyoutPointerReleased(object? sender, Avalonia.Input.PointerReleasedEventArgs e)
        {
            _sceneDropDown?.Flyout?.Hide();
        }

        // protected override void OnKeyDown(Avalonia.Input.KeyEventArgs e)
        // {
        //     base.OnKeyDown(e);

        //     var vm = DataContext as MainWindowViewModel;
        //     vm?.HandleKeyDown(e);
        // }
    }
}
