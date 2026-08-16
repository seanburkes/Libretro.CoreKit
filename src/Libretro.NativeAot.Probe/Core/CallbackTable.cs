using Libretro.Core.Environment;

namespace Libretro.NativeAot.Probe.Core;

internal unsafe struct CallbackTable
{
    public RetroEnvironment Environment;
    public delegate* unmanaged[Cdecl]<void*, uint, uint, nuint, void> VideoRefresh;
    public delegate* unmanaged[Cdecl]<short, short, void> AudioSample;
    public delegate* unmanaged[Cdecl]<short*, nuint, nuint> AudioSampleBatch;
    public delegate* unmanaged[Cdecl]<void> InputPoll;
    public delegate* unmanaged[Cdecl]<uint, uint, uint, uint, short> InputState;
}
