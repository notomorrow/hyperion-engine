using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "WeaponType")]
    public enum WeaponType : byte
    {
        Melee,
        Ranged,
        Magic,

        Max
    }

    [ClassBinding(Name = "Weapon")]
    public class Weapon : AssetObject
    {
        public Weapon()
        {
        }

        public WeaponType WeaponType => this.GetWeaponType();   // Extension Method

        public Mesh? Mesh => this.GetMesh();                    // Extension Method

        public Material? Material => this.GetMaterial();        // Extension Method
    }
}