using System;
using System.IO;
using System.Runtime.InteropServices;
using Hyperion;

namespace Hyperion
{
    [HypClassBinding(Name = "AssetObjectFlags")]
    [Flags]
    public enum AssetObjectFlags : uint
    {
        None = 0x0,
        Persistent = 0x1
    }

    [HypClassBinding(Name = "AssetObject")]
    public class AssetObject : HypObject
    {
        public AssetObject()
        {
        }
    }
}