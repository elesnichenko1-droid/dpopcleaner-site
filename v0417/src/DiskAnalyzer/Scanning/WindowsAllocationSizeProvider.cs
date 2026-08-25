using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace DPop.DiskAnalyzer.Scanning
{
    public sealed class WindowsAllocationSizeProvider : IAllocationSizeProvider
    {
        private const uint InvalidFileSize = 0xFFFFFFFF;

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern uint GetCompressedFileSizeW(string lpFileName, out uint lpFileSizeHigh);

        public long? GetAllocatedBytes(string path)
        {
            uint high;
            Marshal.GetLastWin32Error();
            var low = GetCompressedFileSizeW(path, out high);
            if (low == InvalidFileSize)
            {
                var error = Marshal.GetLastWin32Error();
                if (error != 0)
                    return null;
            }

            return unchecked((long)(((ulong)high << 32) | low));
        }
    }
}
