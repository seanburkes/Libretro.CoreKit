using Libretro.Core.Abi;

namespace Libretro.NativeAot.Probe.Core;

internal sealed unsafe class ProbeCore
{
    public const int Width = 160;
    public const int Height = 144;
    public const int AudioSampleRate = 48_000;
    public const int AudioFramesPerVideoFrame = 800;
    public const double FramesPerSecond = 60.0;

    private const double Tau = Math.PI * 2.0;

    private readonly uint[] _video = new uint[Width * Height];
    private readonly short[] _audio = new short[AudioFramesPerVideoFrame * 2];

    private CoreLifecycle _lifecycle = CoreLifecycle.Initialized;
    private uint _frameNumber;
    private double _tonePhase;
    private int _cursorX;

    public bool IsContentLoaded => _lifecycle == CoreLifecycle.ContentLoaded;

    public bool LoadContent()
    {
        if (_lifecycle != CoreLifecycle.Initialized)
        {
            return false;
        }

        ResetState();
        _lifecycle = CoreLifecycle.ContentLoaded;
        return true;
    }

    public void UnloadContent()
    {
        if (_lifecycle != CoreLifecycle.ContentLoaded)
        {
            return;
        }

        ResetState();
        _lifecycle = CoreLifecycle.Initialized;
    }

    public void Reset()
    {
        if (_lifecycle == CoreLifecycle.ContentLoaded)
        {
            ResetState();
        }
    }

    public void GetSystemAvInfo(RetroSystemAvInfo* info)
    {
        *info = new RetroSystemAvInfo
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

    public void Run(
        RetroFrontendCallbacks callbacks,
        bool supportsInputBitmasks,
        RetroAudioVideoEnableFlags audioVideoEnable)
    {
        if (_lifecycle != CoreLifecycle.ContentLoaded)
        {
            return;
        }

        var input = PollInput(callbacks, supportsInputBitmasks);
        RenderFrame();
        GenerateAudio(
            callbacks,
            IsPressed(input, RetroJoypadId.A),
            (audioVideoEnable & RetroAudioVideoEnableFlags.Audio) != 0);

        if ((audioVideoEnable & RetroAudioVideoEnableFlags.Video) != 0 &&
            callbacks.VideoRefresh != null)
        {
            fixed (uint* video = _video)
            {
                callbacks.VideoRefresh(video, Width, Height, Width * sizeof(uint));
            }
        }

        _frameNumber++;
    }

    private ushort PollInput(RetroFrontendCallbacks callbacks, bool supportsInputBitmasks)
    {
        if (callbacks.InputPoll != null)
        {
            callbacks.InputPoll();
        }

        if (callbacks.InputState == null)
        {
            return 0;
        }

        ushort input;
        if (supportsInputBitmasks)
        {
            input = (ushort)callbacks.InputState(
                0,
                (uint)RetroDevice.Joypad,
                0,
                (uint)RetroJoypadId.Mask);
        }
        else
        {
            input = 0;
            input |= ReadButton(callbacks, RetroJoypadId.Left);
            input |= ReadButton(callbacks, RetroJoypadId.Right);
            input |= ReadButton(callbacks, RetroJoypadId.A);
        }

        if (IsPressed(input, RetroJoypadId.Left))
        {
            _cursorX = Math.Max(0, _cursorX - 2);
        }

        if (IsPressed(input, RetroJoypadId.Right))
        {
            _cursorX = Math.Min(Width - 12, _cursorX + 2);
        }

        return input;
    }

    private void RenderFrame()
    {
        for (var y = 0; y < Height; y++)
        {
            for (var x = 0; x < Width; x++)
            {
                var animatedX = (x + (int)(_frameNumber % Width)) % Width;
                _video[(y * Width) + x] = (animatedX / 32) switch
                {
                    0 => 0x00D94A4A,
                    1 => 0x00E6A23C,
                    2 => 0x00E5D85C,
                    3 => 0x0046B96B,
                    _ => 0x004A78D0,
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

    private void GenerateAudio(
        RetroFrontendCallbacks callbacks,
        bool actionPressed,
        bool submitAudio)
    {
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

        fixed (short* audio = _audio)
        {
            if (!submitAudio)
            {
                return;
            }

            if (callbacks.AudioSampleBatch != null)
            {
                _ = callbacks.AudioSampleBatch(audio, AudioFramesPerVideoFrame);
                return;
            }

            if (callbacks.AudioSample != null)
            {
                for (var frame = 0; frame < AudioFramesPerVideoFrame; frame++)
                {
                    callbacks.AudioSample(audio[frame * 2], audio[(frame * 2) + 1]);
                }
            }
        }
    }

    private static ushort ReadButton(RetroFrontendCallbacks callbacks, RetroJoypadId id) =>
        callbacks.InputState(0, (uint)RetroDevice.Joypad, 0, (uint)id) != 0
            ? (ushort)(1 << (int)id)
            : (ushort)0;

    private static bool IsPressed(ushort input, RetroJoypadId id) =>
        (input & (1 << (int)id)) != 0;

    private void ResetState()
    {
        _frameNumber = 0;
        _tonePhase = 0;
        _cursorX = (Width - 12) / 2;
        Array.Clear(_video);
        Array.Clear(_audio);
    }
}
