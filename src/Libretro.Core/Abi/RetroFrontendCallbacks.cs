// Derived from the pinned libretro.h. See NOTICE.md in this directory.
using System.Runtime.InteropServices;

namespace Libretro.Core.Abi;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroFrontendCallbacks
{
    public delegate* unmanaged[Cdecl]<uint, void*, byte> Environment;
    public delegate* unmanaged[Cdecl]<void*, uint, uint, nuint, void> VideoRefresh;
    public delegate* unmanaged[Cdecl]<short, short, void> AudioSample;
    public delegate* unmanaged[Cdecl]<short*, nuint, nuint> AudioSampleBatch;
    public delegate* unmanaged[Cdecl]<void> InputPoll;
    public delegate* unmanaged[Cdecl]<uint, uint, uint, uint, short> InputState;
}
