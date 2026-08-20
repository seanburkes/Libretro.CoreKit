using System.Buffers.Binary;
using Libretro.Core.Abi;
using Libretro.Core.Hosting;

namespace Libretro.NativeAot.Chip8.Core;

internal sealed unsafe class Chip8Core : ILibretroCore
{
    public const int Width = 64;
    public const int Height = 32;
    public const int MemorySize = 4_096;
    public const int ProgramStart = 0x200;
    public const int MaximumProgramSize = MemorySize - ProgramStart;
    public const int StateSize = 60 + (Width * Height) + MemorySize;

    private const int InstructionsPerFrame = 12;
    private const int AudioFramesPerVideoFrame = 800;
    private const int RegisterOffset = 12;
    private const int StackOffset = 28;
    private const int DisplayOffset = 60;
    private const int MemoryOffset = DisplayOffset + (Width * Height);
    private const uint StateMagic = 0x31533843;
    private const ushort StateVersion = 1;

    private readonly byte[] _content = new byte[MaximumProgramSize];
    private readonly byte[] _memory = new byte[MemorySize];
    private readonly byte[] _registers = new byte[16];
    private readonly ushort[] _stack = new ushort[16];
    private readonly byte[] _display = new byte[Width * Height];
    private readonly bool[] _keys = new bool[16];
    private readonly uint[] _video = new uint[Width * Height];
    private readonly short[] _audio = new short[AudioFramesPerVideoFrame * 2];

    private int _contentLength;
    private ushort _programCounter;
    private ushort _indexRegister;
    private byte _stackPointer;
    private bool _halted;
    private RetroDevice _controllerDevice = RetroDevice.Joypad;

    public LibretroSystemMetadata SystemMetadata => new(
        "CoreKit CHIP-8",
        "0.1.0-phase4",
        "ch8");

    public LibretroCallbackRequirements RequiredFrameCallbacks =>
        LibretroCallbackRequirements.SoftwareCore;

    public void ConfigureEnvironment(LibretroEnvironmentContext context)
    {
        _ = context;
    }

    public void Initialize(LibretroInitializationContext context)
    {
        _ = context.Environment.SetInputDescriptors(Chip8EnvironmentData.InputDescriptors);
    }

    public bool LoadContent(LibretroLoadContext context)
    {
        var content = context.Content.Data;
        if (context.Content.IsContentless || content.Length < 2 ||
            content.Length > MaximumProgramSize ||
            !context.Environment.SetPixelFormat(RetroPixelFormat.Xrgb8888))
        {
            return false;
        }

        _ = context.Environment.SetControllerInfo(Chip8EnvironmentData.ControllerInfo);
        Array.Clear(_content);
        content.CopyTo(_content);
        _contentLength = content.Length;
        ResetMachine();
        _ = context.Logger.Write(RetroLogLevel.Info, "CoreKit CHIP-8 content accepted\n\0"u8);
        return true;
    }

    public void GetSystemAvInfo(out RetroSystemAvInfo info)
    {
        info = new RetroSystemAvInfo
        {
            Geometry = new RetroGameGeometry
            {
                BaseWidth = Width,
                BaseHeight = Height,
                MaxWidth = Width,
                MaxHeight = Height,
                AspectRatio = (float)Width / Height,
            },
            Timing = new RetroSystemTiming
            {
                FramesPerSecond = 60.0,
                SampleRate = 48_000.0,
            },
        };
    }

    public void Reset() => ResetMachine();

    public void SetControllerPortDevice(uint port, uint device)
    {
        if (port == 0)
        {
            _controllerDevice = (RetroDevice)device;
        }
    }

    public void RunFrame(ref LibretroFrameContext context)
    {
        UpdateKeys(_controllerDevice == RetroDevice.Joypad
            ? context.PollRetroPad()
            : (ushort)0);

        for (var instruction = 0; instruction < InstructionsPerFrame && !_halted; instruction++)
        {
            _halted = !ExecuteInstruction();
        }

        for (var pixel = 0; pixel < _display.Length; pixel++)
        {
            _video[pixel] = _display[pixel] == 0 ? 0x00000000u : 0x00FFFFFFu;
        }

        _ = context.SubmitAudio(_audio);
        _ = context.SubmitVideo(_video, Width, Height, Width * sizeof(uint));
    }

    public int SerializedStateSize => StateSize;

    public bool Serialize(Span<byte> destination)
    {
        if (destination.Length != StateSize)
        {
            return false;
        }

        BinaryPrimitives.WriteUInt32LittleEndian(destination, StateMagic);
        BinaryPrimitives.WriteUInt16LittleEndian(destination[4..], StateVersion);
        BinaryPrimitives.WriteUInt16LittleEndian(destination[6..], _programCounter);
        BinaryPrimitives.WriteUInt16LittleEndian(destination[8..], _indexRegister);
        destination[10] = _stackPointer;
        destination[11] = _halted ? (byte)1 : (byte)0;
        _registers.CopyTo(destination[RegisterOffset..]);
        for (var index = 0; index < _stack.Length; index++)
        {
            BinaryPrimitives.WriteUInt16LittleEndian(
                destination[(StackOffset + (index * 2))..],
                _stack[index]);
        }

        _display.CopyTo(destination[DisplayOffset..]);
        _memory.CopyTo(destination[MemoryOffset..]);
        return true;
    }

    public bool Unserialize(ReadOnlySpan<byte> source)
    {
        if (source.Length != StateSize ||
            BinaryPrimitives.ReadUInt32LittleEndian(source) != StateMagic ||
            BinaryPrimitives.ReadUInt16LittleEndian(source[4..]) != StateVersion)
        {
            return false;
        }

        var programCounter = BinaryPrimitives.ReadUInt16LittleEndian(source[6..]);
        var indexRegister = BinaryPrimitives.ReadUInt16LittleEndian(source[8..]);
        var stackPointer = source[10];
        var halted = source[11];
        if ((programCounter & 1) != 0 || programCounter > MemorySize ||
            (programCounter > MemorySize - 2 && halted == 0) ||
            indexRegister >= MemorySize || stackPointer > _stack.Length || halted > 1)
        {
            return false;
        }

        for (var index = 0; index < stackPointer; index++)
        {
            var returnAddress = BinaryPrimitives.ReadUInt16LittleEndian(
                source[(StackOffset + (index * 2))..]);
            if ((returnAddress & 1) != 0 || returnAddress > MemorySize - 2)
            {
                return false;
            }
        }

        for (var pixel = 0; pixel < _display.Length; pixel++)
        {
            if (source[DisplayOffset + pixel] > 1)
            {
                return false;
            }
        }

        _programCounter = programCounter;
        _indexRegister = indexRegister;
        _stackPointer = stackPointer;
        _halted = halted != 0;
        source.Slice(RegisterOffset, _registers.Length).CopyTo(_registers);
        for (var index = 0; index < _stack.Length; index++)
        {
            _stack[index] = BinaryPrimitives.ReadUInt16LittleEndian(
                source[(StackOffset + (index * 2))..]);
        }

        source.Slice(DisplayOffset, _display.Length).CopyTo(_display);
        source.Slice(MemoryOffset, _memory.Length).CopyTo(_memory);
        return true;
    }

    public Memory<byte> GetMemory(RetroMemory region) =>
        region == RetroMemory.SystemRam ? _memory : Memory<byte>.Empty;

    public void UnloadContent()
    {
        _contentLength = 0;
        Array.Clear(_content);
        ResetMachine();
    }

    public void Deinitialize()
    {
        UnloadContent();
        _controllerDevice = RetroDevice.Joypad;
    }

    private bool ExecuteInstruction()
    {
        if (_programCounter > MemorySize - 2)
        {
            return false;
        }

        var opcode = (ushort)((_memory[_programCounter] << 8) | _memory[_programCounter + 1]);
        _programCounter += 2;
        var register = (opcode >> 8) & 0x0F;
        var value = (byte)opcode;
        var address = (ushort)(opcode & 0x0FFF);

        switch (opcode & 0xF000)
        {
            case 0x0000 when opcode == 0x00E0:
                Array.Clear(_display);
                return true;
            case 0x0000 when opcode == 0x00EE:
                if (_stackPointer == 0)
                {
                    return false;
                }

                _programCounter = _stack[--_stackPointer];
                return true;
            case 0x1000:
                _programCounter = address;
                return true;
            case 0x2000:
                if (_stackPointer >= _stack.Length)
                {
                    return false;
                }

                _stack[_stackPointer++] = _programCounter;
                _programCounter = address;
                return true;
            case 0x3000:
                if (_registers[register] == value)
                {
                    _programCounter += 2;
                }

                return true;
            case 0x4000:
                if (_registers[register] != value)
                {
                    _programCounter += 2;
                }

                return true;
            case 0x6000:
                _registers[register] = value;
                return true;
            case 0x7000:
                _registers[register] += value;
                return true;
            case 0xA000:
                _indexRegister = address;
                return true;
            case 0xD000:
                return DrawSprite(
                    _registers[register],
                    _registers[(opcode >> 4) & 0x0F],
                    opcode & 0x000F);
            case 0xE000 when value == 0x9E:
                if (IsKeyPressed(_registers[register]))
                {
                    _programCounter += 2;
                }

                return true;
            case 0xE000 when value == 0xA1:
                if (!IsKeyPressed(_registers[register]))
                {
                    _programCounter += 2;
                }

                return true;
            default:
                return false;
        }
    }

    private bool DrawSprite(int originX, int originY, int height)
    {
        if (height == 0 || _indexRegister + height > MemorySize)
        {
            return false;
        }

        _registers[0xF] = 0;
        for (var row = 0; row < height; row++)
        {
            var sprite = _memory[_indexRegister + row];
            for (var column = 0; column < 8; column++)
            {
                if ((sprite & (0x80 >> column)) == 0)
                {
                    continue;
                }

                var x = (originX + column) % Width;
                var y = (originY + row) % Height;
                var pixel = (y * Width) + x;
                if (_display[pixel] != 0)
                {
                    _registers[0xF] = 1;
                }

                _display[pixel] ^= 1;
            }
        }

        return true;
    }

    private void UpdateKeys(ushort input)
    {
        Array.Clear(_keys);
        _keys[0x0] = IsPressed(input, RetroJoypadId.B);
        _keys[0x2] = IsPressed(input, RetroJoypadId.Up);
        _keys[0x4] = IsPressed(input, RetroJoypadId.Left);
        _keys[0x5] = IsPressed(input, RetroJoypadId.A);
        _keys[0x6] = IsPressed(input, RetroJoypadId.Right);
        _keys[0x8] = IsPressed(input, RetroJoypadId.Down);
    }

    private bool IsKeyPressed(int key) => key < _keys.Length && _keys[key];

    private static bool IsPressed(ushort input, RetroJoypadId id) =>
        (input & (1 << (int)id)) != 0;

    private void ResetMachine()
    {
        Array.Clear(_memory);
        Array.Clear(_registers);
        Array.Clear(_stack);
        Array.Clear(_display);
        Array.Clear(_keys);
        Array.Clear(_video);
        Array.Clear(_audio);
        if (_contentLength != 0)
        {
            _content.AsSpan(0, _contentLength).CopyTo(_memory.AsSpan(ProgramStart));
        }

        _programCounter = ProgramStart;
        _indexRegister = 0;
        _stackPointer = 0;
        _halted = false;
    }
}
