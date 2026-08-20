using System.Runtime.InteropServices;
using Libretro.Core.Abi;
using Libretro.Core.Environment;
using Libretro.Core.Logging;

namespace Libretro.Core.Hosting;

public readonly struct LibretroEnvironmentContext
{
    internal LibretroEnvironmentContext(RetroEnvironment environment, RetroLogger logger)
    {
        Environment = environment;
        Logger = logger;
    }

    public RetroEnvironment Environment { get; }

    public RetroLogger Logger { get; }
}

public readonly struct LibretroInitializationContext
{
    internal LibretroInitializationContext(
        RetroEnvironment environment,
        RetroLogger logger,
        bool supportsInputBitmasks)
    {
        Environment = environment;
        Logger = logger;
        SupportsInputBitmasks = supportsInputBitmasks;
    }

    public RetroEnvironment Environment { get; }

    public RetroLogger Logger { get; }

    public bool SupportsInputBitmasks { get; }
}

public readonly ref struct LibretroContent
{
    internal unsafe LibretroContent(RetroGameInfo* game)
    {
        IsContentless = game == null;
        if (game == null)
        {
            Data = [];
            PathUtf8 = [];
            MetadataUtf8 = [];
            return;
        }

        Data = game->Data == null
            ? []
            : new ReadOnlySpan<byte>(game->Data, checked((int)game->Size));
        PathUtf8 = game->Path == null
            ? []
            : MemoryMarshal.CreateReadOnlySpanFromNullTerminated(game->Path);
        MetadataUtf8 = game->Metadata == null
            ? []
            : MemoryMarshal.CreateReadOnlySpanFromNullTerminated(game->Metadata);
    }

    public bool IsContentless { get; }

    public ReadOnlySpan<byte> Data { get; }

    public ReadOnlySpan<byte> PathUtf8 { get; }

    public ReadOnlySpan<byte> MetadataUtf8 { get; }
}

public readonly ref struct LibretroLoadContext
{
    internal LibretroLoadContext(
        LibretroContent content,
        RetroEnvironment environment,
        RetroLogger logger)
    {
        Content = content;
        Environment = environment;
        Logger = logger;
    }

    public LibretroContent Content { get; }

    public RetroEnvironment Environment { get; }

    public RetroLogger Logger { get; }
}

public unsafe ref struct LibretroFrameContext
{
    private readonly RetroFrontendCallbacks _callbacks;
    private readonly bool _supportsInputBitmasks;
    private bool _inputPolled;

    internal LibretroFrameContext(
        RetroFrontendCallbacks callbacks,
        RetroEnvironment environment,
        RetroLogger logger,
        bool supportsInputBitmasks,
        bool coreOptionsUpdated,
        RetroAudioVideoEnableFlags audioVideoEnable,
        bool fastForwarding)
    {
        _callbacks = callbacks;
        _supportsInputBitmasks = supportsInputBitmasks;
        _inputPolled = false;
        Environment = environment;
        Logger = logger;
        CoreOptionsUpdated = coreOptionsUpdated;
        AudioVideoEnable = audioVideoEnable;
        FastForwarding = fastForwarding;
    }

    public RetroEnvironment Environment { get; }

    public RetroLogger Logger { get; }

    public bool CoreOptionsUpdated { get; }

    public RetroAudioVideoEnableFlags AudioVideoEnable { get; }

    public bool FastForwarding { get; }

    public ushort PollRetroPad(uint port = 0)
    {
        if (!_inputPolled)
        {
            if (_callbacks.InputPoll != null)
            {
                _callbacks.InputPoll();
            }

            _inputPolled = true;
        }

        if (_callbacks.InputState == null)
        {
            return 0;
        }

        if (_supportsInputBitmasks)
        {
            return (ushort)_callbacks.InputState(
                port,
                (uint)RetroDevice.Joypad,
                0,
                (uint)RetroJoypadId.Mask);
        }

        ushort state = 0;
        for (var id = RetroJoypadId.B; id <= RetroJoypadId.R3; id++)
        {
            if (_callbacks.InputState(port, (uint)RetroDevice.Joypad, 0, (uint)id) != 0)
            {
                state |= (ushort)(1 << (int)id);
            }
        }

        return state;
    }

    public bool SubmitVideo(
        ReadOnlySpan<uint> xrgb8888,
        uint width,
        uint height,
        nuint pitchBytes)
    {
        if (width == 0 || height == 0)
        {
            return false;
        }

        var rowBytes = checked((nuint)width * sizeof(uint));
        var requiredBytes = checked((pitchBytes * (height - 1)) + rowBytes);
        var availableBytes = checked((nuint)xrgb8888.Length * sizeof(uint));
        if (pitchBytes < rowBytes || availableBytes < requiredBytes)
        {
            return false;
        }

        if ((AudioVideoEnable & RetroAudioVideoEnableFlags.Video) == 0)
        {
            return true;
        }

        if (_callbacks.VideoRefresh == null)
        {
            return false;
        }

        fixed (uint* data = xrgb8888)
        {
            _callbacks.VideoRefresh(data, width, height, pitchBytes);
        }

        return true;
    }

    public nuint SubmitAudio(ReadOnlySpan<short> interleavedStereo)
    {
        if ((interleavedStereo.Length & 1) != 0)
        {
            return 0;
        }

        var frames = (nuint)(interleavedStereo.Length / 2);
        if (frames == 0)
        {
            return 0;
        }

        if ((AudioVideoEnable & RetroAudioVideoEnableFlags.Audio) == 0)
        {
            return frames;
        }

        fixed (short* data = interleavedStereo)
        {
            if (_callbacks.AudioSampleBatch != null)
            {
                return _callbacks.AudioSampleBatch(data, frames);
            }

            if (_callbacks.AudioSample == null)
            {
                return 0;
            }

            for (nuint frame = 0; frame < frames; frame++)
            {
                _callbacks.AudioSample(data[frame * 2], data[(frame * 2) + 1]);
            }
        }

        return frames;
    }
}
