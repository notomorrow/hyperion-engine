using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public struct Asset
    {
        public static readonly Asset Invalid = new Asset(IntPtr.Zero);

        private IntPtr ptr;

        public Asset(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public IntPtr Handle
        {
            get
            {
                return ptr;
            }
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public Node GetNode()
        {
            throw new NotImplementedException();
        }

        public Texture GetTexture()
        {
            throw new NotImplementedException();
        }

        internal void Destroy()
        {
            if (IsValid)
            {
                Asset_Destroy(ptr);
                ptr = IntPtr.Zero;
            }
        }

        [DllImport("hyperion", EntryPoint = "Asset_Destroy")]
        private static extern void Asset_Destroy(IntPtr assetPtr);
    }

    public class ManagedAsset<T> : IDisposable
    {
        private Asset asset;

        public ManagedAsset(Asset asset)
        {
            this.asset = asset;
        }

        public ManagedAsset(IntPtr assetPtr)
        {
            this.asset = new Asset(assetPtr);
        }

        ~ManagedAsset()
        {
            asset.Destroy();
        }

        public void Dispose()
        {
            asset.Destroy();
            GC.SuppressFinalize(this);
        }

        public Asset Asset
        {
            get
            {
                return asset;
            }
        }
    }
}