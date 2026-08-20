using System.Buffers.Binary;
using System.Runtime.InteropServices;
using Libretro.Core.Abi;
using Libretro.Core.Environment;
using Libretro.Core.Hosting;
using Libretro.Core.Logging;

namespace Libretro.NativeAot.Probe.Core;

internal sealed unsafe class ProbeCore : ILibretroCore
{
    public const int Width = 160;
    public const int Height = 144;
    public const int AudioSampleRate = 48_000;
    public const int AudioFramesPerVideoFrame = 800;
    public const double FramesPerSecond = 60.0;
    public const int SaveRamSize = 64;
    public const int StateSize = 24 + SaveRamSize;

    private const double Tau = Math.PI * 2.0;
    private const uint StateMagic = 0x31534B43;

    private readonly uint[] _video = new uint[Width * Height];
    private readonly short[] _audio = new short[AudioFramesPerVideoFrame * 2];
    private readonly byte[] _saveRam = new byte[SaveRamSize];

    private uint _frameNumber;
    private double _tonePhase;
    private int _cursorX;
    private uint _messageInterfaceVersion;
    private RetroDevice _controllerDevice = RetroDevice.Joypad;
    private bool _toneEnabled = true;
    private bool _monochromePalette;

    public LibretroSystemMetadata SystemMetadata => new(
        "CoreKit NativeAOT Probe",
        "0.1.0-phase3");

    public LibretroCallbackRequirements RequiredFrameCallbacks =>
        LibretroCallbackRequirements.SoftwareCore;

    public void ConfigureEnvironment(LibretroEnvironmentContext context)
    {
        _ = context.Environment.SetSupportNoGame(true);
        _ = context.Environment.SetCoreOptionsV2(ProbeEnvironmentData.CoreOptions);
    }

    public void Initialize(LibretroInitializationContext context)
    {
        _ = context.Environment.SetInputDescriptors(ProbeEnvironmentData.InputDescriptors);
        _ = context.Environment.GetSystemDirectory(out _);
        _ = context.Environment.GetSaveDirectory(out _);
        _ = context.Environment.GetContentDirectory(out _);
        _ = context.Environment.GetCoreAssetsDirectory(out _);
        _ = context.Environment.GetLanguage(out _);
        _ = context.Environment.GetMessageInterfaceVersion(out _messageInterfaceVersion);
    }

    public bool LoadContent(LibretroLoadContext context)
    {
        if (!context.Content.IsContentless ||
            !context.Environment.SetPixelFormat(RetroPixelFormat.Xrgb8888))
        {
            return false;
        }

        _ = context.Environment.SetControllerInfo(ProbeEnvironmentData.ControllerInfo);
        UpdateCoreOptions(context.Environment, context.Logger);
        Array.Clear(_saveRam);
        ResetState();
        ShowReadyMessage(context);
        return true;
    }

    public void UnloadContent()
    {
        ResetState();
        Array.Clear(_saveRam);
    }

    public void Reset() => ResetState();

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
                FramesPerSecond = FramesPerSecond,
                SampleRate = AudioSampleRate,
            },
        };
    }

    public void RunFrame(ref LibretroFrameContext context)
    {
        if (context.CoreOptionsUpdated)
        {
            UpdateCoreOptions(context.Environment, context.Logger);
        }

        var input = _controllerDevice == RetroDevice.Joypad
            ? context.PollRetroPad()
            : (ushort)0;
        if (IsPressed(input, RetroJoypadId.Left))
        {
            _cursorX = Math.Max(0, _cursorX - 2);
        }

        if (IsPressed(input, RetroJoypadId.Right))
        {
            _cursorX = Math.Min(Width - 12, _cursorX + 2);
        }

        RenderFrame();
        GenerateAudio(IsPressed(input, RetroJoypadId.A));
        _ = context.SubmitAudio(_audio);
        _ = context.SubmitVideo(_video, Width, Height, Width * sizeof(uint));

        _saveRam[0]++;
        _frameNumber++;
    }

    public int SerializedStateSize => StateSize;

    public bool Serialize(Span<byte> destination)
    {
        if (destination.Length != StateSize)
        {
            return false;
        }

        BinaryPrimitives.WriteUInt32LittleEndian(destination, StateMagic);
        BinaryPrimitives.WriteUInt32LittleEndian(destination[4..], _frameNumber);
        BinaryPrimitives.WriteInt32LittleEndian(destination[8..], _cursorX);
        BinaryPrimitives.WriteInt64LittleEndian(
            destination[12..],
            BitConverter.DoubleToInt64Bits(_tonePhase));
        BinaryPrimitives.WriteUInt32LittleEndian(destination[20..], SaveRamSize);
        _saveRam.CopyTo(destination[24..]);
        return true;
    }

    public bool Unserialize(ReadOnlySpan<byte> source)
    {
        if (source.Length != StateSize ||
            BinaryPrimitives.ReadUInt32LittleEndian(source) != StateMagic ||
            BinaryPrimitives.ReadUInt32LittleEndian(source[20..]) != SaveRamSize)
        {
            return false;
        }

        var frameNumber = BinaryPrimitives.ReadUInt32LittleEndian(source[4..]);
        var cursorX = BinaryPrimitives.ReadInt32LittleEndian(source[8..]);
        var tonePhase = BitConverter.Int64BitsToDouble(
            BinaryPrimitives.ReadInt64LittleEndian(source[12..]));
        if (cursorX < 0 || cursorX > Width - 12 ||
            !double.IsFinite(tonePhase) || tonePhase < 0 || tonePhase >= Tau)
        {
            return false;
        }

        _frameNumber = frameNumber;
        _cursorX = cursorX;
        _tonePhase = tonePhase;
        source[24..].CopyTo(_saveRam);
        return true;
    }

    public Memory<byte> GetMemory(RetroMemory region) =>
        region == RetroMemory.SaveRam ? _saveRam : Memory<byte>.Empty;

    public void Deinitialize()
    {
        ResetState();
        Array.Clear(_saveRam);
        _messageInterfaceVersion = 0;
        _controllerDevice = RetroDevice.Joypad;
        _toneEnabled = true;
        _monochromePalette = false;
    }

    public void SetControllerPortDevice(uint port, uint device)
    {
        if (port == 0)
        {
            _controllerDevice = (RetroDevice)device;
        }
    }

    private void RenderFrame()
    {
        for (var y = 0; y < Height; y++)
        {
            for (var x = 0; x < Width; x++)
            {
                var animatedX = (x + (int)(_frameNumber % Width)) % Width;
                _video[(y * Width) + x] = _monochromePalette
                    ? (animatedX / 32) switch
                    {
                        0 => 0x00303030u,
                        1 => 0x00606060u,
                        2 => 0x00909090u,
                        3 => 0x00C0C0C0u,
                        _ => 0x00E0E0E0u,
                    }
                    : (animatedX / 32) switch
                {
                    0 => 0x00D94A4Au,
                    1 => 0x00E6A23Cu,
                    2 => 0x00E5D85Cu,
                    3 => 0x0046B96Bu,
                    _ => 0x004A78D0u,
                };
            }
        }

        for (var y = Height - 20; y < Height - 8; y++)
        {
            for (var x = _cursorX; x < _cursorX + 12; x++)
            {
                _video[(y * Width) + x] = 0x00FFFFFF;
            }
        }
    }

    private void GenerateAudio(bool actionPressed)
    {
        if (!_toneEnabled)
        {
            Array.Clear(_audio);
            return;
        }

        var toneAmplitude = 3_000;
        if (actionPressed)
        {
            toneAmplitude = 6_000;
        }

        var phaseStep = Tau * 440.0 / AudioSampleRate;
        for (var frame = 0; frame < AudioFramesPerVideoFrame; frame++)
        {
            var sample = (short)(Math.Sin(_tonePhase) * toneAmplitude);
            _audio[frame * 2] = sample;
            _audio[(frame * 2) + 1] = sample;

            _tonePhase += phaseStep;
            if (_tonePhase >= Tau)
            {
                _tonePhase -= Tau;
            }
        }
    }

    private static bool IsPressed(ushort input, RetroJoypadId id) =>
        (input & (1 << (int)id)) != 0;

    private void UpdateCoreOptions(RetroEnvironment environment, RetroLogger logger)
    {
        if (environment.GetVariable(ProbeEnvironmentData.ToneOptionKey, out var value))
        {
            var enabled = ReadOption(value, "on"u8, "off"u8, _toneEnabled);
            if (enabled != _toneEnabled)
            {
                _toneEnabled = enabled;
                _ = logger.Write(
                    RetroLogLevel.Info,
                    enabled
                        ? "CoreKit tone option enabled\n\0"u8
                        : "CoreKit tone option disabled\n\0"u8);
            }
        }

        if (environment.GetVariable(ProbeEnvironmentData.PaletteOptionKey, out value))
        {
            var monochrome = ReadOption(
                value,
                "monochrome"u8,
                "color"u8,
                _monochromePalette);
            if (monochrome != _monochromePalette)
            {
                _monochromePalette = monochrome;
                _ = logger.Write(
                    RetroLogLevel.Info,
                    monochrome
                        ? "CoreKit monochrome palette selected\n\0"u8
                        : "CoreKit color palette selected\n\0"u8);
            }
        }
    }

    private static bool ReadOption(
        byte* value,
        ReadOnlySpan<byte> enabledValue,
        ReadOnlySpan<byte> disabledValue,
        bool fallback)
    {
        if (value == null)
        {
            return fallback;
        }

        var option = MemoryMarshal.CreateReadOnlySpanFromNullTerminated(value);
        if (option.SequenceEqual(enabledValue))
        {
            return true;
        }

        return option.SequenceEqual(disabledValue) ? false : fallback;
    }

    private void ShowReadyMessage(LibretroLoadContext context)
    {
        if (_messageInterfaceVersion >= 1)
        {
            var extendedMessage = ProbeEnvironmentData.ExtendedReadyMessage;
            if (context.Environment.SetMessageExtended(&extendedMessage))
            {
                return;
            }
        }

        var legacyMessage = ProbeEnvironmentData.LegacyReadyMessage;
        _ = context.Environment.SetMessage(&legacyMessage);
    }

    private void ResetState()
    {
        _frameNumber = 0;
        _tonePhase = 0;
        _cursorX = (Width - 12) / 2;
        Array.Clear(_video);
        Array.Clear(_audio);
    }
}
