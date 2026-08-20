using System.Runtime.InteropServices;
using Libretro.Core.Abi;

namespace Libretro.NativeAot.Probe.Core;

internal static unsafe class ProbeEnvironmentData
{
    private static readonly byte* LeftDescription = Allocate("Move left\0"u8);
    private static readonly byte* RightDescription = Allocate("Move right\0"u8);
    private static readonly byte* ActionDescription = Allocate("Increase tone\0"u8);
    private static readonly byte* AudioCategoryKey = Allocate("audio\0"u8);
    private static readonly byte* AudioCategoryDescription = Allocate("Audio\0"u8);
    private static readonly byte* AudioCategoryInformation = Allocate("Probe audio settings.\0"u8);
    private static readonly byte* VideoCategoryKey = Allocate("video\0"u8);
    private static readonly byte* VideoCategoryDescription = Allocate("Video\0"u8);
    private static readonly byte* VideoCategoryInformation = Allocate("Probe video settings.\0"u8);
    private static readonly byte* ToneOptionKeyValue = Allocate("corekit_probe_tone\0"u8);
    private static readonly byte* ToneOptionDescription = Allocate("Probe tone\0"u8);
    private static readonly byte* ToneOptionInformation = Allocate("Enables the generated probe tone.\0"u8);
    private static readonly byte* OptionOff = Allocate("off\0"u8);
    private static readonly byte* OptionOn = Allocate("on\0"u8);
    private static readonly byte* OptionOffLabel = Allocate("Disabled\0"u8);
    private static readonly byte* OptionOnLabel = Allocate("Enabled\0"u8);
    private static readonly byte* PaletteOptionKeyValue = Allocate("corekit_probe_palette\0"u8);
    private static readonly byte* PaletteOptionDescription = Allocate("Probe palette\0"u8);
    private static readonly byte* PaletteOptionInformation = Allocate("Selects the generated test-pattern palette.\0"u8);
    private static readonly byte* PaletteColor = Allocate("color\0"u8);
    private static readonly byte* PaletteMonochrome = Allocate("monochrome\0"u8);
    private static readonly byte* PaletteColorLabel = Allocate("Color bars\0"u8);
    private static readonly byte* PaletteMonochromeLabel = Allocate("Monochrome\0"u8);
    private static readonly byte* ReadyMessage = Allocate("CoreKit probe ready\0"u8);
    private static readonly byte* ControllerDescription = Allocate("RetroPad\0"u8);
    private static readonly RetroInputDescriptor* Descriptors = CreateDescriptors();
    private static readonly RetroControllerInfo* Controllers = CreateControllers();
    private static readonly RetroCoreOptionsV2* Options = CreateOptions();

    public static RetroInputDescriptor* InputDescriptors => Descriptors;

    public static RetroControllerInfo* ControllerInfo => Controllers;

    public static RetroCoreOptionsV2* CoreOptions => Options;

    public static byte* ToneOptionKey => ToneOptionKeyValue;

    public static byte* PaletteOptionKey => PaletteOptionKeyValue;

    public static RetroMessage LegacyReadyMessage => new()
    {
        Text = ReadyMessage,
        Frames = 180,
    };

    public static RetroMessageExtended ExtendedReadyMessage => new()
    {
        Text = ReadyMessage,
        DurationMilliseconds = 3_000,
        Priority = 1,
        Level = RetroLogLevel.Info,
        Target = RetroMessageTarget.All,
        Type = RetroMessageType.Notification,
        Progress = -1,
    };

    private static RetroInputDescriptor* CreateDescriptors()
    {
        var descriptors = (RetroInputDescriptor*)NativeMemory.AllocZeroed(
            4,
            (nuint)sizeof(RetroInputDescriptor));
        descriptors[0] = CreateDescriptor(RetroJoypadId.Left, LeftDescription);
        descriptors[1] = CreateDescriptor(RetroJoypadId.Right, RightDescription);
        descriptors[2] = CreateDescriptor(RetroJoypadId.A, ActionDescription);
        return descriptors;
    }

    private static RetroInputDescriptor CreateDescriptor(RetroJoypadId id, byte* description) =>
        new()
        {
            Port = 0,
            Device = (uint)RetroDevice.Joypad,
            Index = 0,
            Id = (uint)id,
            Description = description,
        };

    private static RetroControllerInfo* CreateControllers()
    {
        var descriptions = (RetroControllerDescription*)NativeMemory.Alloc(
            (nuint)sizeof(RetroControllerDescription));
        descriptions[0] = new RetroControllerDescription
        {
            Description = ControllerDescription,
            Id = (uint)RetroDevice.Joypad,
        };

        var controllers = (RetroControllerInfo*)NativeMemory.AllocZeroed(
            2,
            (nuint)sizeof(RetroControllerInfo));
        controllers[0] = new RetroControllerInfo
        {
            Types = descriptions,
            NumTypes = 1,
        };
        return controllers;
    }

    private static RetroCoreOptionsV2* CreateOptions()
    {
        var categories = (RetroCoreOptionV2Category*)NativeMemory.AllocZeroed(
            3,
            (nuint)sizeof(RetroCoreOptionV2Category));
        categories[0] = new RetroCoreOptionV2Category
        {
            Key = AudioCategoryKey,
            Description = AudioCategoryDescription,
            Information = AudioCategoryInformation,
        };
        categories[1] = new RetroCoreOptionV2Category
        {
            Key = VideoCategoryKey,
            Description = VideoCategoryDescription,
            Information = VideoCategoryInformation,
        };

        var definitions = (RetroCoreOptionV2Definition*)NativeMemory.AllocZeroed(
            3,
            (nuint)sizeof(RetroCoreOptionV2Definition));
        definitions[0].Key = ToneOptionKeyValue;
        definitions[0].Description = ToneOptionDescription;
        definitions[0].Information = ToneOptionInformation;
        definitions[0].CategoryKey = AudioCategoryKey;
        definitions[0].Values[0] = new RetroCoreOptionValue
        {
            Value = OptionOff,
            Label = OptionOffLabel,
        };
        definitions[0].Values[1] = new RetroCoreOptionValue
        {
            Value = OptionOn,
            Label = OptionOnLabel,
        };
        definitions[0].DefaultValue = OptionOn;

        definitions[1].Key = PaletteOptionKeyValue;
        definitions[1].Description = PaletteOptionDescription;
        definitions[1].Information = PaletteOptionInformation;
        definitions[1].CategoryKey = VideoCategoryKey;
        definitions[1].Values[0] = new RetroCoreOptionValue
        {
            Value = PaletteColor,
            Label = PaletteColorLabel,
        };
        definitions[1].Values[1] = new RetroCoreOptionValue
        {
            Value = PaletteMonochrome,
            Label = PaletteMonochromeLabel,
        };
        definitions[1].DefaultValue = PaletteColor;

        var options = (RetroCoreOptionsV2*)NativeMemory.Alloc((nuint)sizeof(RetroCoreOptionsV2));
        *options = new RetroCoreOptionsV2
        {
            Categories = categories,
            Definitions = definitions,
        };
        return options;
    }

    private static byte* Allocate(ReadOnlySpan<byte> value)
    {
        var memory = (byte*)NativeMemory.Alloc((nuint)value.Length);
        value.CopyTo(new Span<byte>(memory, value.Length));
        return memory;
    }
}
