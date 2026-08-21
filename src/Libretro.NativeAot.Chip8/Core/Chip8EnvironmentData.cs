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
    private static readonly byte* EmulationCategoryKey = Allocate("emulation\0"u8);
    private static readonly byte* EmulationCategoryDescription = Allocate("Emulation\0"u8);
    private static readonly byte* EmulationCategoryInformation = Allocate("CHIP-8 interpreter compatibility settings.\0"u8);
    private static readonly byte* VideoCategoryKey = Allocate("video\0"u8);
    private static readonly byte* VideoCategoryDescription = Allocate("Video\0"u8);
    private static readonly byte* VideoCategoryInformation = Allocate("CHIP-8 display compatibility settings.\0"u8);
    private static readonly byte* ShiftSourceKeyValue = Allocate("corekit_chip8_shift_source\0"u8);
    private static readonly byte* ShiftSourceDescription = Allocate("Shift source register\0"u8);
    private static readonly byte* ShiftSourceInformation = Allocate("Selects the source operand for 8xy6 and 8xyE.\0"u8);
    private static readonly byte* LogicVfKeyValue = Allocate("corekit_chip8_logic_vf\0"u8);
    private static readonly byte* LogicVfDescription = Allocate("Logic-operation VF\0"u8);
    private static readonly byte* LogicVfInformation = Allocate("Controls whether 8xy1, 8xy2, and 8xy3 preserve or clear VF.\0"u8);
    private static readonly byte* MemoryIndexKeyValue = Allocate("corekit_chip8_memory_index\0"u8);
    private static readonly byte* MemoryIndexDescription = Allocate("Load/store index register\0"u8);
    private static readonly byte* MemoryIndexInformation = Allocate("Controls whether Fx55 and Fx65 leave I unchanged or increment it by X + 1.\0"u8);
    private static readonly byte* JumpOffsetKeyValue = Allocate("corekit_chip8_jump_offset\0"u8);
    private static readonly byte* JumpOffsetDescription = Allocate("Jump offset register\0"u8);
    private static readonly byte* JumpOffsetInformation = Allocate("Selects NNN + V0 or XNN + VX for Bnnn.\0"u8);
    private static readonly byte* IndexOverflowKeyValue = Allocate("corekit_chip8_index_overflow\0"u8);
    private static readonly byte* IndexOverflowDescription = Allocate("Index-overflow VF\0"u8);
    private static readonly byte* IndexOverflowInformation = Allocate("Controls whether Fx1E preserves VF or sets it on 0xFFF overflow.\0"u8);
    private static readonly byte* SpriteEdgesKeyValue = Allocate("corekit_chip8_sprite_edges\0"u8);
    private static readonly byte* SpriteEdgesDescription = Allocate("Sprite edge behavior\0"u8);
    private static readonly byte* SpriteEdgesInformation = Allocate("Wraps or clips DxyN sprites at display edges.\0"u8);
    private static readonly byte* OptionV0 = Allocate("v0\0"u8);
    private static readonly byte* OptionVx = Allocate("vx\0"u8);
    private static readonly byte* OptionVy = Allocate("vy\0"u8);
    private static readonly byte* OptionPreserve = Allocate("preserve\0"u8);
    private static readonly byte* OptionClear = Allocate("clear\0"u8);
    private static readonly byte* OptionUnchanged = Allocate("unchanged\0"u8);
    private static readonly byte* OptionIncrement = Allocate("increment\0"u8);
    private static readonly byte* OptionSet = Allocate("set\0"u8);
    private static readonly byte* OptionWrap = Allocate("wrap\0"u8);
    private static readonly byte* OptionClip = Allocate("clip\0"u8);
    private static readonly byte* LabelV0 = Allocate("V0\0"u8);
    private static readonly byte* LabelVx = Allocate("Vx\0"u8);
    private static readonly byte* LabelVy = Allocate("Vy\0"u8);
    private static readonly byte* LabelPreserve = Allocate("Preserve\0"u8);
    private static readonly byte* LabelClear = Allocate("Clear\0"u8);
    private static readonly byte* LabelUnchanged = Allocate("Unchanged\0"u8);
    private static readonly byte* LabelIncrement = Allocate("Increment\0"u8);
    private static readonly byte* LabelSet = Allocate("Set\0"u8);
    private static readonly byte* LabelWrap = Allocate("Wrap\0"u8);
    private static readonly byte* LabelClip = Allocate("Clip\0"u8);
    private static readonly RetroInputDescriptor* Descriptors = CreateDescriptors();
    private static readonly RetroControllerInfo* Controllers = CreateControllers();
    private static readonly RetroCoreOptionsV2* Options = CreateOptions();

    public static RetroInputDescriptor* InputDescriptors => Descriptors;

    public static RetroControllerInfo* ControllerInfo => Controllers;

    public static RetroCoreOptionsV2* CoreOptions => Options;

    public static byte* ShiftSourceOptionKey => ShiftSourceKeyValue;

    public static byte* LogicVfOptionKey => LogicVfKeyValue;

    public static byte* MemoryIndexOptionKey => MemoryIndexKeyValue;

    public static byte* JumpOffsetOptionKey => JumpOffsetKeyValue;

    public static byte* IndexOverflowOptionKey => IndexOverflowKeyValue;

    public static byte* SpriteEdgesOptionKey => SpriteEdgesKeyValue;

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

    private static RetroCoreOptionsV2* CreateOptions()
    {
        var categories = (RetroCoreOptionV2Category*)NativeMemory.AllocZeroed(
            3,
            (nuint)sizeof(RetroCoreOptionV2Category));
        categories[0] = new RetroCoreOptionV2Category
        {
            Key = EmulationCategoryKey,
            Description = EmulationCategoryDescription,
            Information = EmulationCategoryInformation,
        };
        categories[1] = new RetroCoreOptionV2Category
        {
            Key = VideoCategoryKey,
            Description = VideoCategoryDescription,
            Information = VideoCategoryInformation,
        };

        var definitions = (RetroCoreOptionV2Definition*)NativeMemory.AllocZeroed(
            7,
            (nuint)sizeof(RetroCoreOptionV2Definition));
        InitializeOption(
            ref definitions[0],
            ShiftSourceKeyValue,
            ShiftSourceDescription,
            ShiftSourceInformation,
            EmulationCategoryKey,
            OptionVx,
            LabelVx,
            OptionVy,
            LabelVy);
        InitializeOption(
            ref definitions[1],
            LogicVfKeyValue,
            LogicVfDescription,
            LogicVfInformation,
            EmulationCategoryKey,
            OptionPreserve,
            LabelPreserve,
            OptionClear,
            LabelClear);
        InitializeOption(
            ref definitions[2],
            MemoryIndexKeyValue,
            MemoryIndexDescription,
            MemoryIndexInformation,
            EmulationCategoryKey,
            OptionUnchanged,
            LabelUnchanged,
            OptionIncrement,
            LabelIncrement);
        InitializeOption(
            ref definitions[3],
            JumpOffsetKeyValue,
            JumpOffsetDescription,
            JumpOffsetInformation,
            EmulationCategoryKey,
            OptionV0,
            LabelV0,
            OptionVx,
            LabelVx);
        InitializeOption(
            ref definitions[4],
            IndexOverflowKeyValue,
            IndexOverflowDescription,
            IndexOverflowInformation,
            EmulationCategoryKey,
            OptionPreserve,
            LabelPreserve,
            OptionSet,
            LabelSet);
        InitializeOption(
            ref definitions[5],
            SpriteEdgesKeyValue,
            SpriteEdgesDescription,
            SpriteEdgesInformation,
            VideoCategoryKey,
            OptionWrap,
            LabelWrap,
            OptionClip,
            LabelClip);

        var options = (RetroCoreOptionsV2*)NativeMemory.Alloc((nuint)sizeof(RetroCoreOptionsV2));
        *options = new RetroCoreOptionsV2
        {
            Categories = categories,
            Definitions = definitions,
        };
        return options;
    }

    private static void InitializeOption(
        ref RetroCoreOptionV2Definition definition,
        byte* key,
        byte* description,
        byte* information,
        byte* categoryKey,
        byte* defaultValue,
        byte* defaultLabel,
        byte* alternateValue,
        byte* alternateLabel)
    {
        definition.Key = key;
        definition.Description = description;
        definition.Information = information;
        definition.CategoryKey = categoryKey;
        definition.Values[0] = new RetroCoreOptionValue
        {
            Value = defaultValue,
            Label = defaultLabel,
        };
        definition.Values[1] = new RetroCoreOptionValue
        {
            Value = alternateValue,
            Label = alternateLabel,
        };
        definition.DefaultValue = defaultValue;
    }

    private static byte* Allocate(ReadOnlySpan<byte> value)
    {
        var memory = (byte*)NativeMemory.Alloc((nuint)value.Length);
        value.CopyTo(new Span<byte>(memory, value.Length));
        return memory;
    }
}
