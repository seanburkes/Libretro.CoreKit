// Derived from the pinned libretro.h. See NOTICE.md in this directory.
using System.Runtime.InteropServices;

namespace Libretro.Core.Abi;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroSystemInfo
{
    public byte* LibraryName;
    public byte* LibraryVersion;
    public byte* ValidExtensions;
    public byte NeedFullPath;
    public byte BlockExtract;
}

[StructLayout(LayoutKind.Sequential)]
public struct RetroGameGeometry
{
    public uint BaseWidth;
    public uint BaseHeight;
    public uint MaxWidth;
    public uint MaxHeight;
    public float AspectRatio;
}

[StructLayout(LayoutKind.Sequential)]
public struct RetroSystemTiming
{
    public double FramesPerSecond;
    public double SampleRate;
}

[StructLayout(LayoutKind.Sequential)]
public struct RetroSystemAvInfo
{
    public RetroGameGeometry Geometry;
    public RetroSystemTiming Timing;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroGameInfo
{
    public byte* Path;
    public void* Data;
    public nuint Size;
    public byte* Metadata;
}
