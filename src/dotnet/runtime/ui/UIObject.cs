using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public abstract class UIObject : Script
    {
        [Hyperion.ScriptMethodStub]
        public virtual bool OnMouseDown()
        {
            return false;
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnMouseUp()
        {
            return false;
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnMouseHover()
        {
            return false;
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnMouseDrag()
        {
            return false;
        }

        [Hyperion.ScriptMethodStub]
        public virtual bool OnClick()
        {
            return false;
        }
    }
}