using Libretro.Core.Abi;
using Libretro.Core.Environment;

namespace Libretro.NativeAot.Probe.Core;

internal static unsafe class ProbeRuntime
{
    private static RetroFrontendCallbacks _callbacks;
    private static ProbeCore? _core;
    private static int _failed;
    private static bool _supportsInputBitmasks;
    private static uint _messageInterfaceVersion;

    public static void SetEnvironment(delegate* unmanaged[Cdecl]<uint, void*, byte> callback)
    {
        _callbacks.Environment = callback;
        var environment = new RetroEnvironment(callback);
        _ = environment.SetSupportNoGame(true);
        _ = environment.SetCoreOptionsV2(ProbeEnvironmentData.CoreOptions);
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

        var environment = new RetroEnvironment(_callbacks.Environment);
        _ = environment.SetInputDescriptors(ProbeEnvironmentData.InputDescriptors);
        _supportsInputBitmasks = environment.SupportsInputBitmasks();
        _ = environment.GetSystemDirectory(out _);
        _ = environment.GetSaveDirectory(out _);
        _ = environment.GetContentDirectory(out _);
        _ = environment.GetCoreAssetsDirectory(out _);
        _ = environment.GetLanguage(out _);
        _ = environment.GetMessageInterfaceVersion(out _messageInterfaceVersion);
    }

    public static void Deinitialize()
    {
        _core?.UnloadContent();
        _core = null;
        _callbacks = default;
        _failed = 0;
        _supportsInputBitmasks = false;
        _messageInterfaceVersion = 0;
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
        var environment = new RetroEnvironment(_callbacks.Environment);
        if (_core is null || !environment.IsAvailable || _failed != 0)
        {
            return 0;
        }

        if (!environment.SetPixelFormat(RetroPixelFormat.Xrgb8888))
        {
            return 0;
        }

        if (!_core.LoadContent())
        {
            return 0;
        }

        ShowReadyMessage(environment);
        return 1;
    }

    public static void UnloadGame() => _core?.UnloadContent();

    public static void Reset() => _core?.Reset();

    public static void Run()
    {
        if (_failed != 0 || _core is null || !_core.IsContentLoaded)
        {
            return;
        }

        var allocatedBefore = GC.GetAllocatedBytesForCurrentThread();
        var environment = new RetroEnvironment(_callbacks.Environment);
        if (environment.GetVariableUpdate(out var updated) && updated)
        {
            _ = environment.GetVariable(ProbeEnvironmentData.CoreOptionKey, out _);
        }

        _ = environment.GetAudioVideoEnable(out var audioVideoEnable);
        _ = environment.GetFastForwarding(out _);
        _core?.Run(_callbacks, _supportsInputBitmasks, audioVideoEnable);
        if (GC.GetAllocatedBytesForCurrentThread() != allocatedBefore)
        {
            _failed = 1;
        }
    }

    public static void RecordFailure() => _failed = 1;

    private static void ShowReadyMessage(RetroEnvironment environment)
    {
        if (_messageInterfaceVersion >= 1)
        {
            var extendedMessage = ProbeEnvironmentData.ExtendedReadyMessage;
            if (environment.SetMessageExtended(&extendedMessage))
            {
                return;
            }
        }

        var legacyMessage = ProbeEnvironmentData.LegacyReadyMessage;
        _ = environment.SetMessage(&legacyMessage);
    }
}
