using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="UUID")]
    [StructLayout(LayoutKind.Explicit, Size = 16, Pack = 8)]
    public struct UUID : IEquatable<UUID>
    {
        public static readonly UUID Invalid = new UUID(0, 0);

        [FieldOffset(0)]
        private ulong data0;

        [FieldOffset(8)]
        private ulong data1;

        public UUID()
        {
            data0 = 0;
            data1 = 0;
        }

        public UUID(ulong data0, ulong data1)
        {
            this.data0 = data0;
            this.data1 = data1;
        }
        
        public Guid ToGuid()
        {
            return new Guid((uint)data0, (ushort)(data0 >> 32), (ushort)(data0 >> 48), (byte)data1, (byte)(data1 >> 8), (byte)(data1 >> 16), (byte)(data1 >> 24), (byte)(data1 >> 32), (byte)(data1 >> 40), (byte)(data1 >> 48), (byte)(data1 >> 56));
        }

        public bool Equals(UUID other) => data0 == other.data0 && data1 == other.data1;

        public override bool Equals(object? obj) => obj is UUID other && Equals(other);

        public override string ToString()
        {
            string h0 = data0.ToString("x16");
            string h1 = data1.ToString("x16");

            return $"{h0.Substring(0, 8)}-{h0.Substring(8, 4)}-{h0.Substring(12, 4)}-{h1.Substring(0, 4)}-{h1.Substring(4, 12)}";
        }

        public static UUID Parse(string s)
        {
            if (TryParse(s, out UUID result))
            {
                return result;
            }

            throw new FormatException($"Invalid UUID string: '{s}'");
        }

        public static bool TryParse(string? s, out UUID result)
        {
            result = Invalid;

            if (string.IsNullOrEmpty(s))
            {
                return false;
            }

            string hex = s!.Replace("-", string.Empty);

            if (hex.Length != 32)
            {
                return false;
            }

            try
            {
                ulong data0 = Convert.ToUInt64(hex.Substring(0, 16), 16);
                ulong data1 = Convert.ToUInt64(hex.Substring(16, 16), 16);

                result = new UUID(data0, data1);

                return true;
            }
            catch
            {
                return false;
            }
        }

        // Fully qualified: Hyperion.HashCode is the interop wrapper for the core library's type, and it shadows System.HashCode here.
        public override int GetHashCode() => System.HashCode.Combine(data0, data1);

        public static bool operator ==(UUID left, UUID right) => left.Equals(right);

        public static bool operator !=(UUID left, UUID right) => !left.Equals(right);
    }
}