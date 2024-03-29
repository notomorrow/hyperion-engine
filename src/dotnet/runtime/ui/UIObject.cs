using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class UIObject : IDisposable
    {
        private RefCountedPtr refCountedPtr = RefCountedPtr.Null;

        public UIObject()
        {
            this.refCountedPtr = RefCountedPtr.Null;
        }

        public UIObject(RefCountedPtr refCountedPtr)
        {
            this.refCountedPtr = refCountedPtr;

            if (this.refCountedPtr.Valid)
            {
                this.refCountedPtr.IncRef();
            }
        }

        public void Dispose()
        {
            if (this.refCountedPtr.Valid)
            {
                refCountedPtr.DecRef();
            }
        }

        public RefCountedPtr RefCountedPtr
        {
            get
            {
                return refCountedPtr;
            }
        }

        public Vec2i Position
        {
            get
            {
                return UIObject_GetPosition(refCountedPtr);
            }
            set
            {
                UIObject_SetPosition(refCountedPtr, value);
            }
        }

        public Vec2i Size
        {
            get
            {
                return UIObject_GetSize(refCountedPtr);
            }
            set
            {
                UIObject_SetSize(refCountedPtr, value);
            }
        }

        [DllImport("libhyperion", EntryPoint = "UIObject_GetPosition")]
        private static extern Vec2i UIObject_GetPosition(RefCountedPtr ctrlBlock);

        [DllImport("libhyperion", EntryPoint = "UIObject_SetPosition")]
        private static extern void UIObject_SetPosition(RefCountedPtr ctrlBlock, Vec2i position);

        [DllImport("libhyperion", EntryPoint = "UIObject_GetSize")]
        private static extern Vec2i UIObject_GetSize(RefCountedPtr ctrlBlock);

        [DllImport("libhyperion", EntryPoint = "UIObject_SetSize")]
        private static extern void UIObject_SetSize(RefCountedPtr ctrlBlock, Vec2i size);
    }
}