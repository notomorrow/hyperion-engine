
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