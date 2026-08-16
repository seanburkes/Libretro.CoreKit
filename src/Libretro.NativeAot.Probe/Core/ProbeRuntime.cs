using Libretro.Core.Abi;
using Libretro.Core.Environment;

namespace Libretro.NativeAot.Probe.Core;

internal static unsafe class ProbeRuntime
{
    private static CallbackTable _callbacks;
    private static ProbeCore? _core;
    private static int _failed;

    public static void SetEnvironment(delegate* unmanaged[Cdecl]<uint, void*, byte> callback)
    {
        _callbacks.Environment = new RetroEnvironment(callback);
        _ = _callbacks.Environment.SetSupportNoGame(true);
    }

    public static void SetVideoRefresh(delegate* unmanaged[Cdecl]<void*, uint, uint, nuint, void> callback) =>
        _callbacks.VideoRefresh = callback;

    public static void SetAudioSample(delegate* unmanaged[Cdecl]<short, short, void> callback) =>
        _callbacks.AudioSample = callback;

    public static void SetAudioSampleBatch(delegate* unmanaged[Cdecl]<short*, nuint, nuint> callback) =>
        _callbacks.AudioSampleBatch = callback;

    public static void SetInputPoll(delegate* unmanaged[Cdecl]<void> callback) =>
        _callbacks.InputPoll = callback;

    public static void SetInputState(delegate* unmanaged[Cdecl]<uint, uint, uint, uint, short> callback) =>
        _callbacks.InputState = callback;

    public static void Initialize()
    {
        if (!LibretroAbi.IsSupportedLayout())
        {
            _failed = 1;
            return;
        }

        _core ??= new ProbeCore();
        _failed = 0;
    }

    public static void Deinitialize()
    {
        _core?.UnloadContent();
        _core = null;
        _callbacks = default;
        _failed = 0;
    }

    public static void GetSystemInfo(RetroSystemInfo* info) => ProbeMetadata.Fill(info);

    public static void GetSystemAvInfo(RetroSystemAvInfo* info)
    {
        if (_core is null || !_core.IsContentLoaded)
        {
            *info = default;
            return;
        }

        _core.GetSystemAvInfo(info);
    }

    public static byte LoadGame()
    {
        if (_core is null || !_callbacks.Environment.IsAvailable || _failed != 0)
        {
            return 0;
        }

        if (!_callbacks.Environment.SetPixelFormat(RetroPixelFormat.Xrgb8888))
        {
            return 0;
        }

        return _core.LoadContent() ? (byte)1 : (byte)0;
    }

    public static void UnloadGame() => _core?.UnloadContent();

    public static void Reset() => _core?.Reset();

    public static void Run()
    {
        if (_failed != 0)
        {
            return;
        }

        var allocatedBefore = GC.GetAllocatedBytesForCurrentThread();
        _core?.Run(_callbacks);
        if (GC.GetAllocatedBytesForCurrentThread() != allocatedBefore)
        {
            _failed = 1;
        }
    }

    public static void RecordFailure() => _failed = 1;
}
