using System;
using System.Runtime.InteropServices;

namespace Hyperion
{   
    [ClassBinding(Name="UIComponent")]
    [StructLayout(LayoutKind.Sequential)]
    public ref struct UIComponent : IComponent
    {
        private WeakHandle<UIObject> _uiObject;

        public UIComponent()
        {
        }

        public void Dispose()
        {
            _uiObject.Dispose();
        }

        public UIObject? UIObject => _uiObject.Lock().GetValue();
    }
}
