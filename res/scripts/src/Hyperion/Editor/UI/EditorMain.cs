
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    // temp
    class TestDataSource : UIDataSource
    {

    }

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

    public class EditorMain : UIEventHandler
    {
        public override void Init(Entity entity)
        {
            base.Init(entity);
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


            // testing
            // Temp, refactor this
            Scene mainScene = Scene.GetWorld().GetSceneByName(new Name("Scene_Main", weak: true));

            if (mainScene == null)
            {
                Logger.Log(LogType.Error, "Scene not found");

                return UIEventHandlerResult.Error;
            }

            var editorSubsystem = mainScene.GetWorld().GetSubsystem<EditorSubsystem>();

            if (editorSubsystem == null)
            {
                Logger.Log(LogType.Error, "EditorSubsystem not found");

                return UIEventHandlerResult.Error;
            }

            editorSubsystem.AddTask(new TestEditorTask());

            return UIEventHandlerResult.Ok;
        }

        public UIEventHandlerResult UndoClicked()
        {
            Logger.Log(LogType.Info, "Undo clicked");

            // Temp, refactor this
            Scene mainScene = Scene.GetWorld().GetSceneByName(new Name("Scene_Main", weak: true));

            if (mainScene == null)
            {
                Logger.Log(LogType.Error, "Scene not found");

                return UIEventHandlerResult.Error;
            }

            var editorSubsystem = mainScene.GetWorld().GetSubsystem<EditorSubsystem>();

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

            // Temp, refactor this
            Scene mainScene = Scene.GetWorld().GetSceneByName(new Name("Scene_Main", weak: true));

            if (mainScene == null)
            {
                Logger.Log(LogType.Error, "Scene not found");

                return UIEventHandlerResult.Error;
            }

            var editorSubsystem = mainScene.GetWorld().GetSubsystem<EditorSubsystem>();

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
            // temp; testing
            Scene mainScene = Scene.GetWorld().GetSceneByName(new Name("Scene_Main", weak: true));

            if (mainScene == null)
            {
                Logger.Log(LogType.Error, "Scene not found");

                return UIEventHandlerResult.Ok;
            }

            EntityManager entityManager = mainScene.GetEntityManager();

            var editorSubsystem = mainScene.GetWorld().GetSubsystem<EditorSubsystem>();

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
                    mainScene.GetRoot().AddChild(node);
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