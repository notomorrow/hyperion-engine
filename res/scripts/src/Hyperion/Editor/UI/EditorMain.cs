
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    // temp
    class TestDataSource : UIDataSource
    {

    }

    public class EditorMain : UIEventHandler
    {
        public override void Init(Entity entity)
        {
            base.Init(entity);
        }

        [UIEvent(AllowNested = true)]
        public async void SimulateClicked()
        {
            // Test: Force GC
            GC.Collect();

            World world = Scene.GetWorld();
            
            if (world.GetGameState().Mode == GameStateMode.Simulating)
            {
                Logger.Log(LogType.Info, "Stop simulation");

                world.StopSimulating();
                return;
            }

            Logger.Log(LogType.Info, "Start simulation");

            world.StartSimulating();


            // // temp testing
            // var testDataSource = new TestDataSource();

            var testUIButton = new UIButton();
            testUIButton.SetName(new Name("Test Button"));
        }

        [UIEvent(AllowNested = true)]
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

        [UIEvent(AllowNested = true)]
        public UIEventHandlerResult RedoClicked()
        {
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

        [UIEvent(AllowNested = true)]
        public void AddNodeClicked()
        {
            // temp; testing
            Scene mainScene = Scene.GetWorld().GetSceneByName(new Name("Scene_Main", weak: true));

            if (mainScene == null)
            {
                Logger.Log(LogType.Error, "Scene not found");

                return;
            }

            EntityManager entityManager = mainScene.GetEntityManager();

            var editorSubsystem = mainScene.GetWorld().GetSubsystem<EditorSubsystem>();

            if (editorSubsystem == null)
            {
                Logger.Log(LogType.Error, "EditorSubsystem not found");
                return;
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
                }
            ));

            // @TODO Focus on node in scene view
        }
    }
}