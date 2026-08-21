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
    public const int StateSize = 72 + (Width * Height) + MemorySize;

    private const int InstructionsPerFrame = 12;
    private const int AudioFramesPerVideoFrame = 800;
    private const uint AudioSampleRate = 48_000;
    private const uint ToneFrequency = 440;
    private const int ToneAmplitude = 6_000;
    private const int FontStart = 0x50;
    private const int RegisterOffset = 24;
    private const int StackOffset = 40;
    private const int DisplayOffset = 72;
    private const int MemoryOffset = DisplayOffset + (Width * Height);
    private const uint InitialRandomState = 0xC0DEF00D;
    private const uint StateMagic = 0x33533843;
    private const ushort StateVersion = 3;

    private static ReadOnlySpan<byte> FontData =>
    [
        0xF0, 0x90, 0x90, 0x90, 0xF0,
        0x20, 0x60, 0x20, 0x20, 0x70,
        0xF0, 0x10, 0xF0, 0x80, 0xF0,
        0xF0, 0x10, 0xF0, 0x10, 0xF0,
        0x90, 0x90, 0xF0, 0x10, 0x10,
        0xF0, 0x80, 0xF0, 0x10, 0xF0,
        0xF0, 0x80, 0xF0, 0x90, 0xF0,
        0xF0, 0x10, 0x20, 0x40, 0x40,
        0xF0, 0x90, 0xF0, 0x90, 0xF0,
        0xF0, 0x90, 0xF0, 0x10, 0xF0,
        0xF0, 0x90, 0xF0, 0x90, 0x90,
        0xE0, 0x90, 0xE0, 0x90, 0xE0,
        0xF0, 0x80, 0x80, 0x80, 0xF0,
        0xE0, 0x90, 0x90, 0x90, 0xE0,
        0xF0, 0x80, 0xF0, 0x80, 0xF0,
        0xF0, 0x80, 0xF0, 0x80, 0x80,
    ];

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
    private uint _randomState;
    private uint _audioPhase;
    private byte _delayTimer;
    private byte _soundTimer;
    private byte _stackPointer;
    private bool _halted;
    private RetroDevice _controllerDevice = RetroDevice.Joypad;

    public LibretroSystemMetadata SystemMetadata => new(
        "CoreKit CHIP-8",
        "0.3.0-phase4",
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
                SampleRate = AudioSampleRate,
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

        GenerateAudio(_soundTimer != 0);

        if (_delayTimer != 0)
        {
            _delayTimer--;
        }

        if (_soundTimer != 0)
        {
            _soundTimer--;
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
        BinaryPrimitives.WriteUInt32LittleEndian(destination[10..], _randomState);
        destination[14] = _delayTimer;
        destination[15] = _soundTimer;
        destination[16] = _stackPointer;
        destination[17] = _halted ? (byte)1 : (byte)0;
        destination[18] = 0;
        destination[19] = 0;
        BinaryPrimitives.WriteUInt32LittleEndian(destination[20..], _audioPhase);
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
        var randomState = BinaryPrimitives.ReadUInt32LittleEndian(source[10..]);
        var audioPhase = BinaryPrimitives.ReadUInt32LittleEndian(source[20..]);
        var stackPointer = source[16];
        var halted = source[17];
        if ((programCounter & 1) != 0 || programCounter > MemorySize ||
            (programCounter > MemorySize - 2 && halted == 0) ||
            indexRegister >= MemorySize || randomState == 0 || audioPhase >= AudioSampleRate ||
            stackPointer > _stack.Length || halted > 1 ||
            source[18] != 0 || source[19] != 0)
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
        _randomState = randomState;
        _audioPhase = audioPhase;
        _delayTimer = source[14];
        _soundTimer = source[15];
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
        var otherRegister = (opcode >> 4) & 0x0F;
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
            case 0x5000 when (opcode & 0x000F) == 0:
                if (_registers[register] == _registers[otherRegister])
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
            case 0x8000:
                return ExecuteArithmetic(register, otherRegister, opcode & 0x000F);
            case 0x9000 when (opcode & 0x000F) == 0:
                if (_registers[register] != _registers[otherRegister])
                {
                    _programCounter += 2;
                }

                return true;
            case 0xA000:
                _indexRegister = address;
                return true;
            case 0xB000:
                var jumpAddress = address + _registers[0];
                if (jumpAddress > MemorySize)
                {
                    return false;
                }

                _programCounter = (ushort)jumpAddress;
                return true;
            case 0xC000:
                _registers[register] = (byte)(NextRandomByte() & value);
                return true;
            case 0xD000:
                return DrawSprite(
                    _registers[register],
                    _registers[otherRegister],
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
            case 0xF000 when value == 0x07:
                _registers[register] = _delayTimer;
                return true;
            case 0xF000 when value == 0x0A:
                if (!TryGetPressedKey(out var key))
                {
                    _programCounter -= 2;
                    return true;
                }

                _registers[register] = key;
                return true;
            case 0xF000 when value == 0x15:
                _delayTimer = _registers[register];
                return true;
            case 0xF000 when value == 0x18:
                _soundTimer = _registers[register];
                return true;
            case 0xF000 when value == 0x1E:
                var indexRegister = _indexRegister + _registers[register];
                if (indexRegister >= MemorySize)
                {
                    return false;
                }

                _indexRegister = (ushort)indexRegister;
                return true;
            case 0xF000 when value == 0x29:
                if (_registers[register] > 0x0F)
                {
                    return false;
                }

                _indexRegister = (ushort)(FontStart + (_registers[register] * 5));
                return true;
            case 0xF000 when value == 0x33:
                if (_indexRegister > MemorySize - 3)
                {
                    return false;
                }

                var decimalValue = _registers[register];
                _memory[_indexRegister] = (byte)(decimalValue / 100);
                _memory[_indexRegister + 1] = (byte)((decimalValue / 10) % 10);
                _memory[_indexRegister + 2] = (byte)(decimalValue % 10);
                return true;
            case 0xF000 when value == 0x55:
                return StoreRegisters(register);
            case 0xF000 when value == 0x65:
                return LoadRegisters(register);
            default:
                return false;
        }
    }

    private bool ExecuteArithmetic(int register, int otherRegister, int operation)
    {
        var left = _registers[register];
        var right = _registers[otherRegister];
        switch (operation)
        {
            case 0x0:
                _registers[register] = right;
                return true;
            case 0x1:
                _registers[register] = (byte)(left | right);
                return true;
            case 0x2:
                _registers[register] = (byte)(left & right);
                return true;
            case 0x3:
                _registers[register] = (byte)(left ^ right);
                return true;
            case 0x4:
                var sum = left + right;
                _registers[register] = (byte)sum;
                _registers[0xF] = sum > byte.MaxValue ? (byte)1 : (byte)0;
                return true;
            case 0x5:
                _registers[register] = (byte)(left - right);
                _registers[0xF] = left >= right ? (byte)1 : (byte)0;
                return true;
            case 0x6:
                _registers[register] = (byte)(left >> 1);
                _registers[0xF] = (byte)(left & 1);
                return true;
            case 0x7:
                _registers[register] = (byte)(right - left);
                _registers[0xF] = right >= left ? (byte)1 : (byte)0;
                return true;
            case 0xE:
                _registers[register] = (byte)(left << 1);
                _registers[0xF] = (byte)(left >> 7);
                return true;
            default:
                return false;
        }
    }

    private bool StoreRegisters(int lastRegister)
    {
        if (_indexRegister + lastRegister >= MemorySize)
        {
            return false;
        }

        _registers.AsSpan(0, lastRegister + 1).CopyTo(_memory.AsSpan(_indexRegister));
        return true;
    }

    private bool LoadRegisters(int lastRegister)
    {
        if (_indexRegister + lastRegister >= MemorySize)
        {
            return false;
        }

        _memory.AsSpan(_indexRegister, lastRegister + 1)
            .CopyTo(_registers.AsSpan(0, lastRegister + 1));
        return true;
    }

    private byte NextRandomByte()
    {
        _randomState ^= _randomState << 13;
        _randomState ^= _randomState >> 17;
        _randomState ^= _randomState << 5;
        return (byte)_randomState;
    }

    private void GenerateAudio(bool soundActive)
    {
        if (!soundActive)
        {
            Array.Clear(_audio);
            return;
        }

        for (var frame = 0; frame < AudioFramesPerVideoFrame; frame++)
        {
            var sample = (short)(_audioPhase < AudioSampleRate / 2
                ? ToneAmplitude
                : -ToneAmplitude);
            _audio[frame * 2] = sample;
            _audio[(frame * 2) + 1] = sample;

            _audioPhase += ToneFrequency;
            if (_audioPhase >= AudioSampleRate)
            {
                _audioPhase -= AudioSampleRate;
            }
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

    private bool TryGetPressedKey(out byte key)
    {
        for (byte candidate = 0; candidate < _keys.Length; candidate++)
        {
            if (_keys[candidate])
            {
                key = candidate;
                return true;
            }
        }

        key = 0;
        return false;
    }

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
        FontData.CopyTo(_memory.AsSpan(FontStart));
        if (_contentLength != 0)
        {
            _content.AsSpan(0, _contentLength).CopyTo(_memory.AsSpan(ProgramStart));
        }

        _programCounter = ProgramStart;
        _indexRegister = 0;
        _randomState = InitialRandomState;
        _audioPhase = 0;
        _delayTimer = 0;
        _soundTimer = 0;
        _stackPointer = 0;
        _halted = false;
    }
}
