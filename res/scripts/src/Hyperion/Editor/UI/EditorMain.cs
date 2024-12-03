
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    // temp
    class TestDataSource : UIDataSource
    {

    }

    public class TestCustomEditorAction : IEditorAction
    {
        private Name name;
        private Action execute;
        private Action revert;

        public TestCustomEditorAction(Name name, Action execute, Action revert) : base()
        {
            this.name = name;
            this.execute = execute;
            this.revert = revert;
        }

        public override Name GetName()
        {
            return name;
        }

        public override void Execute()
        {
            Logger.Log(LogType.Info, "TestCustomEditorAction.Execute()");

            execute();
        }

        public override void Revert()
        {
            Logger.Log(LogType.Info, "TestCustomEditorAction.Revert()");

            revert();
        }
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
        public void UndoClicked()
        {
            // Temp, refactor this
            Scene mainScene = Scene.GetWorld().GetSceneByName(new Name("Scene_Main", weak: true));

            if (mainScene == null)
            {
                Logger.Log(LogType.Error, "Scene not found");

                return;
            }

            var editorSubsystem = mainScene.GetWorld().GetSubsystem<EditorSubsystem>();

            if (editorSubsystem == null)
            {
                Logger.Log(LogType.Error, "EditorSubsystem not found");

                return;
            }

            if (!editorSubsystem.GetActionStack().CanUndo())
            {
                Logger.Log(LogType.Info, "Cannot undo, stack is empty");

                return;
            }

            editorSubsystem.GetActionStack().Undo();
        }

        [UIEvent(AllowNested = true)]
        public void RedoClicked()
        {
            // Temp, refactor this
            Scene mainScene = Scene.GetWorld().GetSceneByName(new Name("Scene_Main", weak: true));

            if (mainScene == null)
            {
                Logger.Log(LogType.Error, "Scene not found");

                return;
            }

            var editorSubsystem = mainScene.GetWorld().GetSubsystem<EditorSubsystem>();

            if (editorSubsystem == null)
            {
                Logger.Log(LogType.Error, "EditorSubsystem not found");
                
                return;
            }

            if (!editorSubsystem.GetActionStack().CanRedo())
            {
                Logger.Log(LogType.Info, "Cannot redo, stack is empty");

                return;
            }

            editorSubsystem.GetActionStack().Redo();
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

            // node.SetEntity(entityManager.AddEntity());

            var editorSubsystem = mainScene.GetWorld().GetSubsystem<EditorSubsystem>();

            if (editorSubsystem == null)
            {
                Logger.Log(LogType.Error, "EditorSubsystem not found");
                return;
            }

            var node = new Node();
            node.SetName("New Node");

            editorSubsystem.GetActionStack().Push(new TestCustomEditorAction(
                new Name("AddNewNode"),
                () =>
                {
                    Logger.Log(LogType.Info, "TestCustomEditorAction.Execute()");

                    mainScene.GetRoot().AddChild(node);
                    editorSubsystem.SetFocusedNode(node);
                },
                () =>
                {
                    Logger.Log(LogType.Info, "TestCustomEditorAction.Revert()");

                    node.Remove();
                }
            ));

            // @TODO Focus on node in scene view
        }
    }
}