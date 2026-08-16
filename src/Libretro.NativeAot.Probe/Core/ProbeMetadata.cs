using System.Runtime.InteropServices;
using Libretro.Core.Abi;

namespace Libretro.NativeAot.Probe.Core;

internal static unsafe class ProbeMetadata
{
    private static readonly byte* LibraryName = Allocate("CoreKit NativeAOT Probe\0"u8);
    private static readonly byte* LibraryVersion = Allocate("0.1.0-phase1\0"u8);
    private static readonly byte* ValidExtensions = Allocate("\0"u8);

    public static void Fill(RetroSystemInfo* info)
    {
        *info = new RetroSystemInfo
        {
            LibraryName = LibraryName,
            LibraryVersion = LibraryVersion,
            ValidExtensions = ValidExtensions,
            NeedFullPath = 0,
            BlockExtract = 0,
        };
    }

    private static byte* Allocate(ReadOnlySpan<byte> value)
    {
        var memory = (byte*)NativeMemory.Alloc((nuint)value.Length);
        value.CopyTo(new Span<byte>(memory, value.Length));
        return memory;
    }
}
