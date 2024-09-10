
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

        public void SimulateClicked()
        {
            if (Scene.GetWorld().GetGameState().Mode == GameStateMode.Simulating)
            {
                Logger.Log(LogType.Info, "Stop simulation");

                Scene.GetWorld().StopSimulating();
                return;
            }

            Logger.Log(LogType.Info, "Start simulation");

            Scene.GetWorld().StartSimulating();
        }

    }
}