using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="EditorDebugOverlayBase")]
    public abstract class EditorDebugOverlayBase : HypObject
    {
        public EditorDebugOverlayBase()
        {
        }

        // 0 = top-left, 1 = bottom-left, 2 = top-right, 3 = bottom-right
        public virtual int GetPlacement()
        {
            return InvokeNativeMethod<int>(new Name("GetPlacement_Impl", weak: true));
        }

        public abstract void Update(float delta);

        public abstract UIObject CreateUIObject(UIObject spawnParent);
        public abstract Name GetName();
        public abstract bool IsEnabled();
    }
}