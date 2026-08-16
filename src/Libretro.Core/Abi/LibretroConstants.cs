// Derived from the pinned libretro.h. See NOTICE.md in this directory.
namespace Libretro.Core.Abi;

public static class LibretroConstants
{
    public const uint ApiVersion = 1;
}

public enum RetroEnvironmentCommand : uint
{
    SetPixelFormat = 10,
    SetSupportNoGame = 18,
}

public enum RetroPixelFormat
{
    ZeroRgb1555 = 0,
    Xrgb8888 = 1,
    Rgb565 = 2,
    Unknown = int.MaxValue,
}

public enum RetroDevice : uint
{
    None = 0,
    Joypad = 1,
    Mouse = 2,
    Keyboard = 3,
    LightGun = 4,
    Analog = 5,
    Pointer = 6,
}

public enum RetroJoypadId : uint
{
    B = 0,
    Y = 1,
    Select = 2,
    Start = 3,
    Up = 4,
    Down = 5,
    Left = 6,
    Right = 7,
    A = 8,
    X = 9,
    L = 10,
    R = 11,
    L2 = 12,
    R2 = 13,
    L3 = 14,
    R3 = 15,
    Mask = 256,
}

public enum RetroRegion : uint
{
    Ntsc = 0,
    Pal = 1,
}

public enum RetroMemory : uint
{
    SaveRam = 0,
    Rtc = 1,
    SystemRam = 2,
    VideoRam = 3,
    Rom = 4,
}
