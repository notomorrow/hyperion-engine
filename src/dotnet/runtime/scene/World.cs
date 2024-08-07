using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class World
    {
        private IntPtr ptr;

        public World(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public void AddScene(Scene scene)
        {
            World_AddScene(ptr, scene.Handle);
        }

        public uint ID
        {
            get
            {
                return World_GetID(ptr);
            }
        }

        public Subsystem? GetSubsystem(TypeID typeID)
        {
            IntPtr subsystemPtr = World_GetSubsystem(ptr, typeID);

            if (subsystemPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Subsystem(subsystemPtr);
        }

        [DllImport("hyperion", EntryPoint = "World_AddScene")]
        private static extern void World_AddScene(IntPtr worldPtr, ManagedHandle scene);

        [DllImport("hyperion", EntryPoint = "World_GetID")]
        private static extern uint World_GetID(IntPtr worldPtr);

        [DllImport("hyperion", EntryPoint = "World_GetSubsystem")]
        private static extern IntPtr World_GetSubsystem(IntPtr worldPtr, TypeID typeID);
    }
}