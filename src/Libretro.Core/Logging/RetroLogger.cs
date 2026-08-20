using System.Runtime.InteropServices;
using Libretro.Core.Abi;

namespace Libretro.Core.Logging;

public readonly unsafe partial struct RetroLogger
{
    private readonly void* _callback;

    internal RetroLogger(void* callback) => _callback = callback;

    public bool IsAvailable => _callback != null;

    public bool Write(RetroLogLevel level, ReadOnlySpan<byte> nullTerminatedUtf8)
    {
        if (_callback == null ||
            level < RetroLogLevel.Debug ||
            level > RetroLogLevel.Error ||
            nullTerminatedUtf8.IsEmpty ||
            nullTerminatedUtf8[^1] != 0 ||
            nullTerminatedUtf8[..^1].Contains((byte)0))
        {
            return false;
        }

        fixed (byte* message = nullTerminatedUtf8)
        {
            NativeMethods.LogMessage(_callback, level, message);
        }

        return true;
    }

    private static partial class NativeMethods
    {
        [LibraryImport("__Internal", EntryPoint = "libretro_core_log_message")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        internal static partial void LogMessage(void* callback, RetroLogLevel level, byte* message);
    }
}
