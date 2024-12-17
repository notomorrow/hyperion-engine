using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="InputHandlerBase")]
    public abstract class InputHandlerBase : HypObject
    {
        public InputHandlerBase()
        {
        }

        public abstract bool OnKeyDown(KeyboardEvent evt);
        public abstract bool OnKeyUp(KeyboardEvent evt);
        public abstract bool OnMouseDown(MouseEvent evt);
        public abstract bool OnMouseUp(MouseEvent evt);
        public abstract bool OnMouseMove(MouseEvent evt);
        public abstract bool OnMouseDrag(MouseEvent evt);
        public abstract bool OnClick(MouseEvent evt);
    }

    [HypClassBinding(Name="NullInputHandler")]
    public class NullInputHandler : InputHandlerBase
    {
        public NullInputHandler()
        {
        }

        public override bool OnKeyDown(KeyboardEvent evt)
        {
            return false;
        }

        public override bool OnKeyUp(KeyboardEvent evt)
        {
            return false;
        }

        public override bool OnMouseDown(MouseEvent evt)
        {
            return false;
        }

        public override bool OnMouseUp(MouseEvent evt)
        {
            return false;
        }

        public override bool OnMouseMove(MouseEvent evt)
        {
            return false;
        }

        public override bool OnMouseDrag(MouseEvent evt)
        {
            return false;
        }

        public override bool OnClick(MouseEvent evt)
        {
            return false;
        }
    }
}