using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class UIPanel : UIObject
    {
        public UIPanel() : base()
        {
                
        }

        public UIPanel(RefCountedPtr refCountedPtr) : base(refCountedPtr)
        {
        }
    }
}