
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    public class TestUIScript : Script
    {
        private Scene? scene = null;

        public void BeforeInit(ManagedHandle sceneHandle)
        {
            scene = new Scene(sceneHandle);
        }

        public void Init(Entity entity)
        {
        }

        public void ButtonClick()
        { 
            Console.WriteLine("DELETE THE WHOLE WORLD");
        }
    }
}