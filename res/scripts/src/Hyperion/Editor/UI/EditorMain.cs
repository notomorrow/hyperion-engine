
using System;
using System.IO;
using System.Linq;
using Hyperion;

namespace Hyperion
{
    namespace Editor
    {
        public class TestEditorTask : LongRunningEditorTask
        {
            public TestEditorTask()
            {
            }

            public override void Cancel()
            {
                Logger.Log(LogType.Info, "Cancel task");
            }

            public override bool IsCompleted()
            {
                return false;
            }

            public override void Process()
            {
                Logger.Log(LogType.Info, "Process task! testing");
            }
        }

        public class FPSCounterDebugOverlay : EditorDebugOverlayBase
        {
            private static readonly List<KeyValuePair<int, Color>> fpsColors = new List<KeyValuePair<int, Color>>
            {
                new KeyValuePair<int, Color>(30, new Color(1.0f, 0.0f, 0.0f, 1.0f)),
                new KeyValuePair<int, Color>(60, new Color(1.0f, 1.0f, 0.0f, 1.0f)),
                new KeyValuePair<int, Color>(int.MaxValue, new Color(0.0f, 1.0f, 0.0f, 1.0f))
            };

            private UIText? tpsTextElement;
            private UIText? memoryUsageTextElement;
            private UIText? fpsTextElement;
            private UIText? countersTextElement;
            private float deltaAccumGame = 0.0f;
            private int numTicksGame = 0;
            private World world;

            public FPSCounterDebugOverlay(World world)
            {
                this.world = world;
            }

            public override UIObject CreateUIObject(UIObject spawnParent)
            {
                UIListView panel = spawnParent.Spawn<UIListView>(new Name("FPSCounterDebugOverlay_Panel"), new Vec2i(0, 0), new UIObjectSize(100, UIObjectSize.Percent, 0, UIObjectSize.Auto));
                panel.SetBackgroundColor(new Color(0.0f, 0.0f, 0.0f, 0.0f));

                UIListView renderListView = spawnParent.Spawn<UIListView>(new Name("FPSCounterDebugOverlay_RenderListView"), new Vec2i(0, 0), new UIObjectSize(100, UIObjectSize.Percent, 15, UIObjectSize.Pixel));
                renderListView.SetBackgroundColor(new Color(0.0f, 0.0f, 0.0f, 0.0f));
                renderListView.SetOrientation(UIListViewOrientation.Horizontal);

                UIText fpsTextElement = spawnParent.Spawn<UIText>(new Name("FPSCounterDebugOverlay_FPS"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
                fpsTextElement.SetText("Render:");
                fpsTextElement.SetTextSize(12);
                fpsTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                fpsTextElement.SetPadding(new Vec2i(5, 5));
                renderListView.AddChildUIObject(fpsTextElement);
                this.fpsTextElement = fpsTextElement;

                UIText countersTextElement = spawnParent.Spawn<UIText>(new Name("FPSCounterDebugOverlay_Counters"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
                countersTextElement.SetText("draw calls: 0, Tris: 0");
                countersTextElement.SetTextSize(12);
                countersTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                countersTextElement.SetPadding(new Vec2i(5, 5));
                renderListView.AddChildUIObject(countersTextElement);
                this.countersTextElement = countersTextElement;

                panel.AddChildUIObject(renderListView);

                UIListView gameListView = spawnParent.Spawn<UIListView>(new Name("FPSCounterDebugOverlay_GameListView"), new Vec2i(0, 0), new UIObjectSize(100, UIObjectSize.Percent, 0, UIObjectSize.Auto));
                gameListView.SetBackgroundColor(new Color(0.0f, 0.0f, 0.0f, 0.0f));
                gameListView.SetOrientation(UIListViewOrientation.Horizontal);

                UIText tpsTextElement = spawnParent.Spawn<UIText>(new Name("FPSCounterDebugOverlay_TPS"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
                tpsTextElement.SetText("Game:");
                tpsTextElement.SetTextSize(12);
                tpsTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                tpsTextElement.SetPadding(new Vec2i(5, 5));
                gameListView.AddChildUIObject(tpsTextElement);
                this.tpsTextElement = tpsTextElement;

                UIText memoryUsageTextElement = spawnParent.Spawn<UIText>(new Name("FPSCounterDebugOverlay_MemoryUsage"), new Vec2i(0, 0), new UIObjectSize(UIObjectSize.Auto));
                memoryUsageTextElement.SetText(".NET Memory Usage: 0 MB");
                memoryUsageTextElement.SetTextSize(12);
                memoryUsageTextElement.SetTextColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
                memoryUsageTextElement.SetPadding(new Vec2i(5, 5));
                gameListView.AddChildUIObject(memoryUsageTextElement);
                this.memoryUsageTextElement = memoryUsageTextElement;

                panel.AddChildUIObject(gameListView);

                return panel;
            }

            public override Name GetName()
            {
                return new Name("FPSCounterDebugOverlay");
            }

            public override bool IsEnabled()
            {
                return true;
            }

            public override void Update(float delta)
            {
                deltaAccumGame += delta;
                numTicksGame++;

                if (deltaAccumGame >= 1.0f)
                {
                    if (tpsTextElement != null)
                    {
                        int ticksPerSecondGame = (int)(1.0f / (deltaAccumGame / (float)numTicksGame));

                        ((UIText)tpsTextElement).SetText(string.Format("Game: {0} ticks/sec", ticksPerSecondGame));
                        tpsTextElement.SetTextColor(GetFPSColor(ticksPerSecondGame));
                    }

                    if (memoryUsageTextElement != null)
                    {
                        long memoryUsage = GC.GetTotalMemory(false) / 1024 / 1024;

                        ((UIText)memoryUsageTextElement).SetText(string.Format(".NET Memory Usage: {0} MB", memoryUsage));
                    }

                    deltaAccumGame = 0.0f;
                    numTicksGame = 0;
                }

                var renderStats = world.GetRenderStats();

                if (fpsTextElement != null)
                {
                    ((UIText)fpsTextElement).SetText(string.Format("Render: {0} frames/sec, {1:0.00} ms/frame (avg: {2:0.00}, min: {3:0.00}, max: {4:0.00})",
                        (int)renderStats.framesPerSecond, renderStats.millisecondsPerFrame,
                        renderStats.millisecondsPerFrameAvg, renderStats.millisecondsPerFrameMin, renderStats.millisecondsPerFrameMax));
                    fpsTextElement.SetTextColor(GetFPSColor((int)renderStats.framesPerSecond));
                }

                if (countersTextElement != null)
                {
                    ((UIText)countersTextElement).SetText(string.Format("draw calls: {0}, Tris: {1}",
                        renderStats.counts.drawCalls, renderStats.counts.triangles));
                }
            }

            private static Color GetFPSColor(int fps)
            {
                for (int i = 0; i < fpsColors.Count; i++)
                {
                    if (fps <= fpsColors[i].Key)
                    {
                        return fpsColors[i].Value;
                    }
                }

                return fpsColors[fpsColors.Count - 1].Value;
            }
        }

        public class TestEditorDebugOverlay2 : EditorDebugOverlayBase
        {
            public TestEditorDebugOverlay2()
            {
            }

            ~TestEditorDebugOverlay2()
            {
                Logger.Log(LogType.Debug, "TestEditorDebugOverlay2 destructed");
            }

            public override UIObject CreateUIObject(UIObject spawnParent)
            {
                var image = spawnParent.Spawn<UIImage>(new Name("TestEditorDebugOverlay2"), new Vec2i(0, 0), new UIObjectSize(new Vec2i(100, 50), UIObjectSize.Pixel));
                image.SetBackgroundColor(new Color(1.0f, 0.0f, 0.0f, 1.0f));
                return image;
            }

            public override Name GetName()
            {
                return new Name("TestEditorDebugOverlay2");
            }

            public override bool IsEnabled()
            {
                return true;
            }

            public override void Update(float delta)
            {
            }
        }

        public class EditorMain : UIEventHandler
        {
            FirstPersonCameraController? firstPersonCameraController;

            public override void Init(Entity entity)
            {
                base.Init(entity);

                // // test
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();
                editorSubsystem.AddDebugOverlay(new FPSCounterDebugOverlay(World));
                editorSubsystem.AddDebugOverlay(new TestEditorDebugOverlay2());
            }

            public override void Destroy()
            {
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                editorSubsystem.RemoveDebugOverlay(new Name("TestEditorDebugOverlay", weak: true));
                editorSubsystem.RemoveDebugOverlay(new Name("TestEditorDebugOverlay2", weak: true));
            }

            public override void OnPlayStart()
            {
                Logger.Log(LogType.Info, "OnPlayStart");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                firstPersonCameraController = new FirstPersonCameraController();
                editorSubsystem.GetScene().GetCamera().AddCameraController(firstPersonCameraController);

                firstPersonCameraController.SetMode(FirstPersonCameraControllerMode.MouseLocked);
            }

            public override void OnPlayStop()
            {
                Logger.Log(LogType.Info, "OnPlayStop");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (!editorSubsystem.GetScene().GetCamera().RemoveCameraController(firstPersonCameraController))
                {
                    Logger.Log(LogType.Error, "Failed to remove camera controller!");

                    return;
                }

                firstPersonCameraController = null;

                Logger.Log(LogType.Info, "Camera controller removed");
            }

            public UIEventHandlerResult SaveClicked()
            {
                Logger.Log(LogType.Info, "Save clicked");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                EditorProject currentProject = editorSubsystem.GetCurrentProject();

                if (currentProject == null)
                {
                    Logger.Log(LogType.Error, "No project loaded; cannot save");

                    return UIEventHandlerResult.Error;
                }

                Result saveResult = currentProject.Save();

                if (!saveResult.IsValid)
                {
                    Logger.Log(LogType.Error, "Failed to save project");

                    return UIEventHandlerResult.Error;
                }

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult SimulateClicked()
            {
                // Test: Force GC
                GC.Collect();

                World world = Scene.GetWorld();
                
                if (world.GetGameState().Mode == GameStateMode.Simulating)
                {
                    Logger.Log(LogType.Info, "Stop simulation");

                    world.StopSimulating();
                    return UIEventHandlerResult.Ok;
                }

                Logger.Log(LogType.Info, "Start simulation");

                world.StartSimulating();

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult UndoClicked()
            {
                Logger.Log(LogType.Info, "Undo clicked");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Error;
                }

                if (!editorSubsystem.GetActionStack().CanUndo())
                {
                    Logger.Log(LogType.Info, "Cannot undo, stack is empty");

                    return UIEventHandlerResult.Ok;
                }

                editorSubsystem.GetActionStack().Undo();

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult RedoClicked()
            {
                Logger.Log(LogType.Info, "Redo clicked");

                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");
                    
                    return UIEventHandlerResult.Error;
                }

                if (!editorSubsystem.GetActionStack().CanRedo())
                {
                    Logger.Log(LogType.Info, "Cannot redo, stack is empty");

                    return UIEventHandlerResult.Ok;
                }

                editorSubsystem.GetActionStack().Redo();

                return UIEventHandlerResult.Ok;
            }

            public UIEventHandlerResult AddNodeClicked()
            {
                var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

                if (editorSubsystem == null)
                {
                    Logger.Log(LogType.Error, "EditorSubsystem not found");

                    return UIEventHandlerResult.Ok;
                }

                var node = new Node();
                node.SetName("New Node");

                editorSubsystem.GetActionStack().Push(new EditorAction(
                    new Name("AddNewNode"),
                    () =>
                    {
                        editorSubsystem.GetScene().GetRoot().AddChild(node);
                        editorSubsystem.SetFocusedNode(node);
                    },
                    () =>
                    {
                        node.Remove();

                        if (editorSubsystem.GetFocusedNode() == node)
                        {
                            editorSubsystem.SetFocusedNode(null);
                        }
                    }
                ));

                // @TODO Focus on node in scene view

                return UIEventHandlerResult.Ok;
            }
        }
    }
}