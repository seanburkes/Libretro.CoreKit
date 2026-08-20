using System.Runtime.InteropServices;
using Libretro.Core.Abi;

namespace Libretro.NativeAot.Probe.Core;

internal static unsafe class ProbeEnvironmentData
{
    private static readonly byte* LeftDescription = Allocate("Move left\0"u8);
    private static readonly byte* RightDescription = Allocate("Move right\0"u8);
    private static readonly byte* ActionDescription = Allocate("Increase tone\0"u8);
    private static readonly byte* CategoryKey = Allocate("audio\0"u8);
    private static readonly byte* CategoryDescription = Allocate("Audio\0"u8);
    private static readonly byte* CategoryInformation = Allocate("Probe audio settings.\0"u8);
    private static readonly byte* OptionKey = Allocate("corekit_probe_tone\0"u8);
    private static readonly byte* OptionDescription = Allocate("Probe tone\0"u8);
    private static readonly byte* OptionInformation = Allocate("Enables the generated probe tone.\0"u8);
    private static readonly byte* OptionOff = Allocate("off\0"u8);
    private static readonly byte* OptionOn = Allocate("on\0"u8);
    private static readonly byte* OptionOffLabel = Allocate("Disabled\0"u8);
    private static readonly byte* OptionOnLabel = Allocate("Enabled\0"u8);
    private static readonly byte* ReadyMessage = Allocate("CoreKit probe ready\0"u8);
    private static readonly byte* ControllerDescription = Allocate("RetroPad\0"u8);
    private static readonly RetroInputDescriptor* Descriptors = CreateDescriptors();
    private static readonly RetroControllerInfo* Controllers = CreateControllers();
    private static readonly RetroCoreOptionsV2* Options = CreateOptions();

    public static RetroInputDescriptor* InputDescriptors => Descriptors;

    public static RetroControllerInfo* ControllerInfo => Controllers;

    public static RetroCoreOptionsV2* CoreOptions => Options;

    public static byte* CoreOptionKey => OptionKey;

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
            2,
            (nuint)sizeof(RetroCoreOptionV2Category));
        categories[0] = new RetroCoreOptionV2Category
        {
            Key = CategoryKey,
            Description = CategoryDescription,
            Information = CategoryInformation,
        };

        var definitions = (RetroCoreOptionV2Definition*)NativeMemory.AllocZeroed(
            2,
            (nuint)sizeof(RetroCoreOptionV2Definition));
        definitions[0].Key = OptionKey;
        definitions[0].Description = OptionDescription;
        definitions[0].Information = OptionInformation;
        definitions[0].CategoryKey = CategoryKey;
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
