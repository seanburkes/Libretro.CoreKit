// Derived from the pinned libretro.h. See NOTICE.md in this directory.
using System.Runtime.CompilerServices;
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

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroInputDescriptor
{
    public uint Port;
    public uint Device;
    public uint Index;
    public uint Id;
    public byte* Description;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroVariable
{
    public byte* Key;
    public byte* Value;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroMessage
{
    public byte* Text;
    public uint Frames;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroMessageExtended
{
    public byte* Text;
    public uint DurationMilliseconds;
    public uint Priority;
    public RetroLogLevel Level;
    public RetroMessageTarget Target;
    public RetroMessageType Type;
    public sbyte Progress;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroCoreOptionValue
{
    public byte* Value;
    public byte* Label;
}

[InlineArray(LibretroConstants.CoreOptionValuesMaximum)]
public struct RetroCoreOptionValues
{
    private RetroCoreOptionValue _element0;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroCoreOptionV2Category
{
    public byte* Key;
    public byte* Description;
    public byte* Information;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroCoreOptionV2Definition
{
    public byte* Key;
    public byte* Description;
    public byte* CategorizedDescription;
    public byte* Information;
    public byte* CategorizedInformation;
    public byte* CategoryKey;
    public RetroCoreOptionValues Values;
    public byte* DefaultValue;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct RetroCoreOptionsV2
{
    public RetroCoreOptionV2Category* Categories;
    public RetroCoreOptionV2Definition* Definitions;
}
