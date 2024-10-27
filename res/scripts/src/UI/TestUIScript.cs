
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    // temp
    class TestDataSource : UIDataSource
    {

    }

    public class TestUIScript : UIEventHandler
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


            // temp testing
            var testDataSource = new TestDataSource();

            var testUIButton = new UIButton();
            testUIButton.SetName(new Name("Test Button"));
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

            Logger.Log(LogType.Info, "Scene name - {0}", mainScene.GetName());

            var node = mainScene.GetRoot().AddChild(new Node());
            node.SetName("New Node");

            // testing
            EntityManager entityManager = mainScene.GetEntityManager();
            ref TransformComponent transformComponent = ref entityManager.GetComponent<TransformComponent>(node.GetEntity());
            Logger.Log(LogType.Info, "TransformComponent - {0}", transformComponent.Transform);

            var editorSubsystem = mainScene.GetWorld().GetSubsystem<EditorSubsystem>();

            if (editorSubsystem == null)
            {
                Logger.Log(LogType.Error, "EditorSubsystem not found");
                return;
            }

            // @TODO Focus on node in scene view
        }
    }
}