// Derived from the pinned libretro.h. See NOTICE.md in this directory.
namespace Libretro.Core.Abi;

public static class LibretroConstants
{
    public const uint ApiVersion = 1;
    public const int CoreOptionValuesMaximum = 128;
    public const uint EnvironmentExperimental = 0x10000;
}

public enum RetroEnvironmentCommand : uint
{
    SetMessage = 6,
    GetSystemDirectory = 9,
    SetPixelFormat = 10,
    SetInputDescriptors = 11,
    GetVariable = 15,
    GetVariableUpdate = 17,
    SetSupportNoGame = 18,
    GetLogInterface = 27,
    GetContentDirectory = 30,
    GetCoreAssetsDirectory = GetContentDirectory,
    GetSaveDirectory = 31,
    SetControllerInfo = 35,
    GetLanguage = 39,
    GetAudioVideoEnable = 47 | LibretroConstants.EnvironmentExperimental,
    GetFastForwarding = 49 | LibretroConstants.EnvironmentExperimental,
    GetInputBitmasks = 51 | LibretroConstants.EnvironmentExperimental,
    GetMessageInterfaceVersion = 59,
    SetMessageExtended = 60,
    SetCoreOptionsV2 = 67,
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

public enum RetroLanguage
{
    English = 0,
    Japanese = 1,
    French = 2,
    Spanish = 3,
    German = 4,
    Italian = 5,
    Dutch = 6,
    PortugueseBrazil = 7,
    PortuguesePortugal = 8,
    Russian = 9,
    Korean = 10,
    ChineseTraditional = 11,
    ChineseSimplified = 12,
    Esperanto = 13,
    Polish = 14,
    Vietnamese = 15,
    Arabic = 16,
    Greek = 17,
    Turkish = 18,
    Slovak = 19,
    Persian = 20,
    Hebrew = 21,
    Asturian = 22,
    Finnish = 23,
    Indonesian = 24,
    Swedish = 25,
    Ukrainian = 26,
    Czech = 27,
    CatalanValencia = 28,
    Catalan = 29,
    BritishEnglish = 30,
    Hungarian = 31,
    Belarusian = 32,
    Galician = 33,
    Norwegian = 34,
    Irish = 35,
    Thai = 36,
    Last = 37,
    Unknown = int.MaxValue,
}

[Flags]
public enum RetroAudioVideoEnableFlags
{
    None = 0,
    Video = 1 << 0,
    Audio = 1 << 1,
    FastSaveStates = 1 << 2,
    HardDisableAudio = 1 << 3,
    Unknown = int.MaxValue,
}

public enum RetroLogLevel
{
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Unknown = int.MaxValue,
}

public enum RetroMessageTarget
{
    All = 0,
    OnScreenDisplay = 1,
    Log = 2,
}

public enum RetroMessageType
{
    Notification = 0,
    NotificationAlternate = 1,
    Status = 2,
    Progress = 3,
}
