using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // Maps to core/utilities/Result.hpp
    [ClassBinding(Name="Result")]
    [StructLayout(LayoutKind.Sequential)]
    public struct Result
    {
        public static Result OK => new Result();

        private PimplPtr _error;

        public Result()
        {
        }

        public bool IsValid
        {
            get
            {
                return _error.ptr == IntPtr.Zero;
            }
        }
    }
}