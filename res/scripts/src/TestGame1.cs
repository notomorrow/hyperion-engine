
using System;
using System.IO;
using Hyperion;

namespace FooBar
{
    public class TestGame1 : Game
    {
        
        public override void Init()
        {
            Console.WriteLine("Init TestGame1");
        }
        
        public override void Update(float deltaTime)
        {
            Console.WriteLine("Update TestGame1");
        }
    }
}