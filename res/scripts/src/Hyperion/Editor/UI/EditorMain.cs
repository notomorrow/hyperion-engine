
using System;
using System.IO;
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

        public class TestEditorDebugOverlay : EditorDebugOverlayBase
        {
            public TestEditorDebugOverlay()
            {
            }

            public override UIObject CreateUIObject(UIObject spawnParent)
            {
                return spawnParent.Spawn<UIImage>(new Name("TestEditorDebugOverlay"), new Vec2i(0, 0), new UIObjectSize(new Vec2i(100, 100), UIObjectSize.Pixel));
            }

            public override Name GetName()
            {
                return new Name("TestEditorDebugOverlay");
            }

            public override bool IsEnabled()
            {
                return true;
            }

            public override void Update()
            {
            }
        }

        public class TestEditorDebugOverlay2 : EditorDebugOverlayBase
        {
            public TestEditorDebugOverlay2()
            {
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

            public override void Update()
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
                editorSubsystem.AddDebugOverlay(new TestEditorDebugOverlay());
                editorSubsystem.AddDebugOverlay(new TestEditorDebugOverlay2());
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