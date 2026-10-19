using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="Error")]
    [StructLayout(LayoutKind.Sequential)]
    public struct Error
    {
        private unsafe byte* _message;

        private unsafe byte* _currentFunction;

        public Error()
        {
            unsafe
            {
                _message = null;
                _currentFunction = null;
            }
        }
    }
}