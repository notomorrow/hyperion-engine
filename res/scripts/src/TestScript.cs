
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    public class FizzBuzzTest
    {
        private Scene? scene = null;

        public void BeforeInit(ManagedHandle sceneHandle)
        {
            scene = new Scene(sceneHandle);
        }

        public async void Init(Entity entity)
        {
            Console.WriteLine("Blah, scene id = " + scene.ID);

            var node = scene.Root.FindChild("house");
            
            if (node != null)
            {
                return;
            }
            
            var assetBatch = new AssetBatch();
            assetBatch.Add("test_model", "models/house.obj");

            var assetMap = await assetBatch.Load();

            node = assetMap["test_model"].GetNode();
            node.Name = "house";

            scene.Root.AddChild(node);

            Console.WriteLine("Init Script " + entity.ID);
        }

        public void Update(float delta)
        {
        }
    }
}