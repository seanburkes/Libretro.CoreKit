using System.Runtime.InteropServices;
using Libretro.Core.Abi;

namespace Libretro.NativeAot.Chip8.Core;

internal static unsafe class Chip8EnvironmentData
{
    private static readonly byte* Key0Description = Allocate("CHIP-8 key 0\0"u8);
    private static readonly byte* Key1Description = Allocate("CHIP-8 key 1\0"u8);
    private static readonly byte* Key2Description = Allocate("CHIP-8 key 2\0"u8);
    private static readonly byte* Key3Description = Allocate("CHIP-8 key 3\0"u8);
    private static readonly byte* Key4Description = Allocate("CHIP-8 key 4\0"u8);
    private static readonly byte* Key5Description = Allocate("CHIP-8 key 5\0"u8);
    private static readonly byte* Key6Description = Allocate("CHIP-8 key 6\0"u8);
    private static readonly byte* Key7Description = Allocate("CHIP-8 key 7\0"u8);
    private static readonly byte* Key8Description = Allocate("CHIP-8 key 8\0"u8);
    private static readonly byte* Key9Description = Allocate("CHIP-8 key 9\0"u8);
    private static readonly byte* KeyADescription = Allocate("CHIP-8 key A\0"u8);
    private static readonly byte* KeyBDescription = Allocate("CHIP-8 key B\0"u8);
    private static readonly byte* KeyCDescription = Allocate("CHIP-8 key C\0"u8);
    private static readonly byte* KeyDDescription = Allocate("CHIP-8 key D\0"u8);
    private static readonly byte* KeyEDescription = Allocate("CHIP-8 key E\0"u8);
    private static readonly byte* KeyFDescription = Allocate("CHIP-8 key F\0"u8);
    private static readonly byte* ControllerDescription = Allocate("RetroPad\0"u8);
    private static readonly RetroInputDescriptor* Descriptors = CreateDescriptors();
    private static readonly RetroControllerInfo* Controllers = CreateControllers();

    public static RetroInputDescriptor* InputDescriptors => Descriptors;

    public static RetroControllerInfo* ControllerInfo => Controllers;

    private static RetroInputDescriptor* CreateDescriptors()
    {
        var descriptors = (RetroInputDescriptor*)NativeMemory.AllocZeroed(
            17,
            (nuint)sizeof(RetroInputDescriptor));
        descriptors[0] = CreateDescriptor(RetroJoypadId.B, Key0Description);
        descriptors[1] = CreateDescriptor(RetroJoypadId.Y, Key1Description);
        descriptors[2] = CreateDescriptor(RetroJoypadId.Select, KeyCDescription);
        descriptors[3] = CreateDescriptor(RetroJoypadId.Start, KeyDDescription);
        descriptors[4] = CreateDescriptor(RetroJoypadId.Up, Key2Description);
        descriptors[5] = CreateDescriptor(RetroJoypadId.Down, Key8Description);
        descriptors[6] = CreateDescriptor(RetroJoypadId.Left, Key4Description);
        descriptors[7] = CreateDescriptor(RetroJoypadId.Right, Key6Description);
        descriptors[8] = CreateDescriptor(RetroJoypadId.A, Key5Description);
        descriptors[9] = CreateDescriptor(RetroJoypadId.X, Key3Description);
        descriptors[10] = CreateDescriptor(RetroJoypadId.L, Key7Description);
        descriptors[11] = CreateDescriptor(RetroJoypadId.R, Key9Description);
        descriptors[12] = CreateDescriptor(RetroJoypadId.L2, KeyADescription);
        descriptors[13] = CreateDescriptor(RetroJoypadId.R2, KeyBDescription);
        descriptors[14] = CreateDescriptor(RetroJoypadId.L3, KeyEDescription);
        descriptors[15] = CreateDescriptor(RetroJoypadId.R3, KeyFDescription);
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
