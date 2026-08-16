namespace Libretro.NativeAot.Probe.Abi;

internal static unsafe class AbiLayout
{
    public static bool IsValid()
    {
        if (sizeof(nuint) != 8)
        {
            return false;
        }

        RetroSystemInfo systemInfo = default;
        RetroGameGeometry geometry = default;
        RetroSystemTiming timing = default;
        RetroSystemAvInfo avInfo = default;
        RetroGameInfo gameInfo = default;

        return sizeof(RetroSystemInfo) == 32 &&
            Offset(&systemInfo, &systemInfo.LibraryName) == 0 &&
            Offset(&systemInfo, &systemInfo.LibraryVersion) == 8 &&
            Offset(&systemInfo, &systemInfo.ValidExtensions) == 16 &&
            Offset(&systemInfo, &systemInfo.NeedFullPath) == 24 &&
            Offset(&systemInfo, &systemInfo.BlockExtract) == 25 &&
            sizeof(RetroGameGeometry) == 20 &&
            Offset(&geometry, &geometry.BaseWidth) == 0 &&
            Offset(&geometry, &geometry.BaseHeight) == 4 &&
            Offset(&geometry, &geometry.MaxWidth) == 8 &&
            Offset(&geometry, &geometry.MaxHeight) == 12 &&
            Offset(&geometry, &geometry.AspectRatio) == 16 &&
            sizeof(RetroSystemTiming) == 16 &&
            Offset(&timing, &timing.FramesPerSecond) == 0 &&
            Offset(&timing, &timing.SampleRate) == 8 &&
            sizeof(RetroSystemAvInfo) == 40 &&
            Offset(&avInfo, &avInfo.Geometry) == 0 &&
            Offset(&avInfo, &avInfo.Timing) == 24 &&
            sizeof(RetroGameInfo) == 32 &&
            Offset(&gameInfo, &gameInfo.Path) == 0 &&
            Offset(&gameInfo, &gameInfo.Data) == 8 &&
            Offset(&gameInfo, &gameInfo.Size) == 16 &&
            Offset(&gameInfo, &gameInfo.Metadata) == 24;
    }

    private static nuint Offset(void* value, void* field) =>
        (nuint)((byte*)field - (byte*)value);
}
