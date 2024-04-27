using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class UIHelpers
    {
        public static UIObject MarshalUIObject(RefCountedPtr rc)
        {
            if (!rc.IsValid)
            {
                return new UIObject();
            }

            UIObjectType type = UIObject_GetType(rc);

            switch (type) {
            case UIObjectType.Unknown: // Custom override type
                return new UIObject(rc);
            // case UIObjectType.Stage: // <-- TODO
            //     return new UIStage(rc);
            case UIObjectType.Button:
                return new UIButton(rc);
            case UIObjectType.Text:
                return new UIText(rc);
            case UIObjectType.Panel:
                return new UIPanel(rc);
            case UIObjectType.Image:
                return new UIImage(rc);
            case UIObjectType.TabView:
                return new UITabView(rc);
            case UIObjectType.Grid:
                return new UIGrid(rc);
            default:
                throw new Exception("Unknown UIObjectType!");
            }
        }

        [DllImport("hyperion", EntryPoint = "UIObject_GetType")]
        [return: MarshalAs(UnmanagedType.U4)]
        private static extern UIObjectType UIObject_GetType(RefCountedPtr rc);
    }
}