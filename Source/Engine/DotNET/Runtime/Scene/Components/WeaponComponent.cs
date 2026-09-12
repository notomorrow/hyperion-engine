using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="WeaponComponent")]
    [StructLayout(LayoutKind.Sequential, Size = 8)]
    public ref struct WeaponComponent : IComponent
    {
        public Handle<Weapon> Weapon;

        public WeaponComponent()
        {
        }

        public void Dispose()
        {
        }

        public static Class Class => Class.GetClass(typeof(WeaponComponent));

        public unsafe IntPtr NativeAddress
        {
            get
            {
                fixed (WeaponComponent* pThis = &this)
                {
                    return (IntPtr)pThis;
                }
            }
        }
    }
}
