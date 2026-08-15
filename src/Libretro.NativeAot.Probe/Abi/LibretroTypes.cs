using System.Runtime.InteropServices;

namespace Libretro.NativeAot.Probe.Abi;

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct RetroSystemInfo
{
    public byte* LibraryName;
    public byte* LibraryVersion;
    public byte* ValidExtensions;
    public byte NeedFullPath;
    public byte BlockExtract;
}
[StructLayout(LayoutKind.Sequential)]
internal struct RetroGameGeometry
{
    public uint BaseWidth;
    public uint BaseHeight;
    public uint MaxWidth;
    public uint MaxHeight;
    public float AspectRatio;
}

[StructLayout(LayoutKind.Sequential)]
internal struct RetroSystemTiming
{
    public double FramesPerSecond;
    public double SampleRate;
}

[StructLayout(LayoutKind.Sequential)]
internal struct RetroSystemAvInfo
{
    public RetroGameGeometry Geometry;
    public RetroSystemTiming Timing;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct RetroGameInfo
{
    public byte* Path;
    public void* Data;
    public nuint Size;
    public byte* Metadata;
}
