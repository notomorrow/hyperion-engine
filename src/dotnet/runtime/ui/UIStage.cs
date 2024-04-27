using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class UIStage
    {
        private IntPtr ptr;

        // This dictionary is used to map the generic type of the UI object to the corresponding method that creates the UI object
        private static readonly Dictionary<Type, Func<IntPtr, Name, Vec2i, Vec2i, bool, RefCountedPtr>> createUIObjectMethods = new Dictionary<Type, Func<IntPtr, Name, Vec2i, Vec2i, bool, RefCountedPtr>>
        {
            {
                typeof(UIButton), (IntPtr ptr, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIButton(ptr, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIText), (IntPtr ptr, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIText(ptr, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIPanel), (IntPtr ptr, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIPanel(ptr, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIImage), (IntPtr ptr, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIImage(ptr, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UITabView), (IntPtr ptr, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UITabView(ptr, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            },
            {
                typeof(UIGrid), (IntPtr ptr, Name name, Vec2i position, Vec2i size, bool attachToRoot) =>
                {
                    RefCountedPtr outPtr = new RefCountedPtr();
                    UIStage_CreateUIObject_UIGrid(ptr, ref name, ref position, ref size, attachToRoot, out outPtr);
                    return outPtr;
                }
            }
        };

        public UIStage(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public Scene Scene
        {
            get
            {
                ManagedHandle sceneHandle = new ManagedHandle();
                UIStage_GetScene(ptr, out sceneHandle);
                return new Scene(sceneHandle);
            }
        }

        public Vec2i SurfaceSize
        {
            get
            {
                Vec2i size = new Vec2i();
                UIStage_GetSurfaceSize(ptr, out size);
                return size;
            }
        }

        public T CreateUIObject<T>(Name name, Vec2i position, Vec2i size, bool attachToRoot) where T : UIObject, new()
        {
            if (!createUIObjectMethods.TryGetValue(typeof(T), out Func<IntPtr, Name, Vec2i, Vec2i, bool, RefCountedPtr> method))
            {
                throw new Exception("Failed to create UI object");
            }

            RefCountedPtr refCountedPtr = method(ptr, name, position, size, attachToRoot);

            if (!refCountedPtr.IsValid)
            {
                throw new Exception("Failed to create UI object");
            }

            return UIHelpers.MarshalUIObject(refCountedPtr) as T;
        }

        [DllImport("hyperion", EntryPoint = "UIStage_GetScene")]
        private static extern void UIStage_GetScene(IntPtr uiStagePtr, [Out] out ManagedHandle sceneHandle);

        [DllImport("hyperion", EntryPoint = "UIStage_GetSurfaceSize")]
        private static extern void UIStage_GetSurfaceSize(IntPtr uiStagePtr, [Out] out Vec2i size);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIButton")]
        private static extern void UIStage_CreateUIObject_UIButton(IntPtr uiStagePtr, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIText")]
        private static extern void UIStage_CreateUIObject_UIText(IntPtr uiStagePtr, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIPanel")]
        private static extern void UIStage_CreateUIObject_UIPanel(IntPtr uiStagePtr, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIImage")]
        private static extern void UIStage_CreateUIObject_UIImage(IntPtr uiStagePtr, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UITabView")]
        private static extern void UIStage_CreateUIObject_UITabView(IntPtr uiStagePtr, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);

        [DllImport("hyperion", EntryPoint = "UIStage_CreateUIObject_UIGrid")]
        private static extern void UIStage_CreateUIObject_UIGrid(IntPtr uiStagePtr, [MarshalAs(UnmanagedType.LPStruct)] ref Name name, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i position, [MarshalAs(UnmanagedType.LPStruct)] ref Vec2i size, bool attachToRoot, [Out] out RefCountedPtr outPtr);
    }
}