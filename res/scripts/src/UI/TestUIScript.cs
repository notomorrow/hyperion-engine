
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    public class TestUIScript : Script
    {
        private Scene? scene = null;

        public void BeforeInit(Scene scene)
        {
            this.scene = scene;
        }

        public void Init(Entity entity)
        {
            var editorSubsystem = Engine.Instance.World!.GetSubsystem(TypeID.FromString("EditorSubsystem"));

            if (editorSubsystem == null)
            {
                Console.WriteLine("EditorSubsystem not found");
                return;
            }
        }

    }
}