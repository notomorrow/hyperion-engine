using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Controls.Primitives;
using Avalonia.Interactivity;
using Avalonia.LogicalTree;
using Avalonia.Markup.Xaml;
using Hyperion.Editor.Services;
using Hyperion.Editor.ViewModels;
using Hyperion.Editor.Views;

namespace Hyperion.Editor
{
    public partial class App : Application
    {
        public static bool AlreadyInitialized { get; set; } = false;
        private static readonly bool IsStartScreenEnabled = true;

        public override void Initialize()
        {
            AvaloniaXamlLoader.Load(this);

            MenuItem.ClickEvent.AddClassHandler<MenuItem>(CloseFlyoutOnMenuItemClick, handledEventsToo: true);
        }

        private static void CloseFlyoutOnMenuItemClick(MenuItem item, RoutedEventArgs e)
        {
            if (item.HasSubMenu || item.Parent is MenuBase or MenuItem)
            {
                return;
            }

            if (item.FindLogicalAncestorOfType<Popup>() is not { } popup)
            {
                return;
            }

            if (popup.PlacementTarget is Button { Flyout: { } flyout })
            {
                flyout.Hide();
            }
            else if (popup.PlacementTarget is Control target && FlyoutBase.GetAttachedFlyout(target) is { } attachedFlyout)
            {
                attachedFlyout.Hide();
            }
            else
            {
                popup.Close();
            }
        }

        public override void OnFrameworkInitializationCompleted()
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                SplashWindow splashWindow = new();
                splashWindow.Show();

                StartEditor(desktop, splashWindow);
            }
            else
            {
                InitializeEngine();
            }

            base.OnFrameworkInitializationCompleted();
        }

        private static async void StartEditor(IClassicDesktopStyleApplicationLifetime desktop, SplashWindow splashWindow)
        {
            splashWindow.SetStatus("Initializing engine...");
            await splashWindow.WaitForNextFrameAsync();

            InitializeEngine();

            if (IsStartScreenEnabled && !RecentProjectsService.Instance.WillOpenLastProjectOnStartup)
            {
                ShowStartScreen(desktop, splashWindow);

                return;
            }

            splashWindow.SetStatus("Loading editor...");
            await splashWindow.WaitForNextFrameAsync();

            ShowMainWindow(desktop);

            splashWindow.Close();
        }

        private static void ShowStartScreen(IClassicDesktopStyleApplicationLifetime desktop, SplashWindow splashWindow)
        {
            StartScreenViewModel startScreenViewModel = new();
            StartScreenWindow startScreenWindow = new StartScreenWindow { DataContext = startScreenViewModel };

            bool mainWindowShown = false;

            void ContinueToEditor()
            {
                if (mainWindowShown)
                {
                    return;
                }

                mainWindowShown = true;

                ShowMainWindow(desktop);
            }

            startScreenViewModel.Finished += () =>
            {
                ContinueToEditor();
                startScreenWindow.Close();
            };

            startScreenWindow.Closing += (_, _) => ContinueToEditor();

            startScreenWindow.Show();
            splashWindow.Close();
            startScreenWindow.Activate();
        }

        private static void ShowMainWindow(IClassicDesktopStyleApplicationLifetime desktop)
        {
            MainWindow mainWindow = new MainWindow();
            desktop.MainWindow = mainWindow;
            mainWindow.Show();
            mainWindow.Activate();
        }

        private static void InitializeEngine()
        {
            if (!AlreadyInitialized)
            {
                EngineManager.Initialize();
            }

            EngineManager.InitializeEditor();

            // Initialize Console Service
            _ = Services.ConsoleService.Instance;
        }
    }
}
