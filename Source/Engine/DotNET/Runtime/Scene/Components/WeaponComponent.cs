using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="WeaponComponent")]
    [StructLayout(LayoutKind.Sequential)]
    public ref struct WeaponComponent : IComponent
    {
        public Handle<Weapon> Weapon;

        public WeaponComponent()
        {
        }

        public void Dispose()
        {
            Weapon.Dispose();
        }
    }
}
