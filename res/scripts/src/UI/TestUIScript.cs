
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    public class TestUIScript : Script
    {
        public override void Init(Entity entity)
        {
            Logger.Log(LogType.Info, "Init for TestUIScript");
            // var editorSubsystem = Engine.Instance.World!.GetSubsystem(TypeID.FromString("EditorSubsystem"));

            // if (editorSubsystem == null)
            // {
            //     Console.WriteLine("EditorSubsystem not found");
            //     return;
            // }
        }

        public async void SimulateClicked()
        {
            World world = Scene.GetWorld();
            
            if (world.GetGameState().Mode == GameStateMode.Simulating)
            {
                Logger.Log(LogType.Info, "Stop simulation");

                world.StopSimulating();
                return;
            }

            Logger.Log(LogType.Info, "Start simulation");

            TaskBatch taskBatch = new TaskBatch();
            taskBatch.AddTask(() =>
            {
                Console.WriteLine("Task 1 complete");
            });
            taskBatch.AddTask(() =>
            {
                Console.WriteLine("Task 2 complete");
            });
            await taskBatch.Execute();

            world.StartSimulating();
        }

        public void AddNodeClicked()
        {
            Scene.GetRoot().AddChild(new Node());
        }
    }
}