using System.Runtime.InteropServices;
using Libretro.Core.Abi;

namespace Libretro.NativeAot.Chip8.Core;

internal static unsafe class Chip8EnvironmentData
{
    private static readonly byte* UpDescription = Allocate("CHIP-8 key 2\0"u8);
    private static readonly byte* DownDescription = Allocate("CHIP-8 key 8\0"u8);
    private static readonly byte* LeftDescription = Allocate("CHIP-8 key 4\0"u8);
    private static readonly byte* RightDescription = Allocate("CHIP-8 key 6\0"u8);
    private static readonly byte* ActionDescription = Allocate("CHIP-8 key 5\0"u8);
    private static readonly byte* AlternateDescription = Allocate("CHIP-8 key 0\0"u8);
    private static readonly byte* ControllerDescription = Allocate("RetroPad\0"u8);
    private static readonly RetroInputDescriptor* Descriptors = CreateDescriptors();
    private static readonly RetroControllerInfo* Controllers = CreateControllers();

    public static RetroInputDescriptor* InputDescriptors => Descriptors;

    public static RetroControllerInfo* ControllerInfo => Controllers;

    private static RetroInputDescriptor* CreateDescriptors()
    {
        var descriptors = (RetroInputDescriptor*)NativeMemory.AllocZeroed(
            7,
            (nuint)sizeof(RetroInputDescriptor));
        descriptors[0] = CreateDescriptor(RetroJoypadId.Up, UpDescription);
        descriptors[1] = CreateDescriptor(RetroJoypadId.Down, DownDescription);
        descriptors[2] = CreateDescriptor(RetroJoypadId.Left, LeftDescription);
        descriptors[3] = CreateDescriptor(RetroJoypadId.Right, RightDescription);
        descriptors[4] = CreateDescriptor(RetroJoypadId.A, ActionDescription);
        descriptors[5] = CreateDescriptor(RetroJoypadId.B, AlternateDescription);
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

    private static byte* Allocate(ReadOnlySpan<byte> value)
    {
        var memory = (byte*)NativeMemory.Alloc((nuint)value.Length);
        value.CopyTo(new Span<byte>(memory, value.Length));
        return memory;
    }
}
