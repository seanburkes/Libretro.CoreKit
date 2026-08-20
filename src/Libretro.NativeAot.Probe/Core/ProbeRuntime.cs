using Libretro.Core.Abi;
using Libretro.Core.Hosting;

namespace Libretro.NativeAot.Probe.Core;

internal static unsafe class ProbeRuntime
{
    private static readonly LibretroHost<ProbeCore> Host = new(new ProbeCore());

    public static void SetEnvironment(delegate* unmanaged[Cdecl]<uint, void*, byte> callback) =>
        Host.SetEnvironment(callback);

    public static void SetVideoRefresh(delegate* unmanaged[Cdecl]<void*, uint, uint, nuint, void> callback) =>
        Host.SetVideoRefresh(callback);

    public static void SetAudioSample(delegate* unmanaged[Cdecl]<short, short, void> callback) =>
        Host.SetAudioSample(callback);

    public static void SetAudioSampleBatch(delegate* unmanaged[Cdecl]<short*, nuint, nuint> callback) =>
        Host.SetAudioSampleBatch(callback);

    public static void SetInputPoll(delegate* unmanaged[Cdecl]<void> callback) =>
        Host.SetInputPoll(callback);

    public static void SetInputState(delegate* unmanaged[Cdecl]<uint, uint, uint, uint, short> callback) =>
        Host.SetInputState(callback);

    public static void Initialize() => Host.Initialize();

    public static void Deinitialize() => Host.Deinitialize();

    public static void GetSystemInfo(RetroSystemInfo* info) => ProbeMetadata.Fill(info);

    public static void GetSystemAvInfo(RetroSystemAvInfo* info) => Host.GetSystemAvInfo(info);

    public static byte LoadGame(RetroGameInfo* game) => Host.LoadGame(game);

    public static void UnloadGame() => Host.UnloadGame();

    public static void Reset() => Host.Reset();

    public static void Run() => Host.Run();

    public static void RecordFailure() => Host.RecordFailure();
}
