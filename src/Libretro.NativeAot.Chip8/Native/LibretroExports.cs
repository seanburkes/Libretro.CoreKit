using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Libretro.Core.Abi;
using Libretro.NativeAot.Chip8.Core;

namespace Libretro.NativeAot.Chip8.Native;

internal static unsafe class LibretroExports
{
    [UnmanagedCallersOnly(EntryPoint = "retro_set_environment", CallConvs = [typeof(CallConvCdecl)])]
    public static void SetEnvironment(delegate* unmanaged[Cdecl]<uint, void*, byte> callback)
    {
        try { Chip8Runtime.SetEnvironment(callback); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_set_video_refresh", CallConvs = [typeof(CallConvCdecl)])]
    public static void SetVideoRefresh(delegate* unmanaged[Cdecl]<void*, uint, uint, nuint, void> callback)
    {
        try { Chip8Runtime.SetVideoRefresh(callback); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_set_audio_sample", CallConvs = [typeof(CallConvCdecl)])]
    public static void SetAudioSample(delegate* unmanaged[Cdecl]<short, short, void> callback)
    {
        try { Chip8Runtime.SetAudioSample(callback); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_set_audio_sample_batch", CallConvs = [typeof(CallConvCdecl)])]
    public static void SetAudioSampleBatch(delegate* unmanaged[Cdecl]<short*, nuint, nuint> callback)
    {
        try { Chip8Runtime.SetAudioSampleBatch(callback); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_set_input_poll", CallConvs = [typeof(CallConvCdecl)])]
    public static void SetInputPoll(delegate* unmanaged[Cdecl]<void> callback)
    {
        try { Chip8Runtime.SetInputPoll(callback); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_set_input_state", CallConvs = [typeof(CallConvCdecl)])]
    public static void SetInputState(delegate* unmanaged[Cdecl]<uint, uint, uint, uint, short> callback)
    {
        try { Chip8Runtime.SetInputState(callback); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_init", CallConvs = [typeof(CallConvCdecl)])]
    public static void Init()
    {
        try { Chip8Runtime.Initialize(); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_deinit", CallConvs = [typeof(CallConvCdecl)])]
    public static void Deinit()
    {
        try { Chip8Runtime.Deinitialize(); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_api_version", CallConvs = [typeof(CallConvCdecl)])]
    public static uint ApiVersion() => LibretroConstants.ApiVersion;

    [UnmanagedCallersOnly(EntryPoint = "retro_get_system_info", CallConvs = [typeof(CallConvCdecl)])]
    public static void GetSystemInfo(RetroSystemInfo* info)
    {
        if (info == null) { return; }
        try { Chip8Runtime.GetSystemInfo(info); }
        catch { *info = default; Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_get_system_av_info", CallConvs = [typeof(CallConvCdecl)])]
    public static void GetSystemAvInfo(RetroSystemAvInfo* info)
    {
        if (info == null) { return; }
        try { Chip8Runtime.GetSystemAvInfo(info); }
        catch { *info = default; Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_set_controller_port_device", CallConvs = [typeof(CallConvCdecl)])]
    public static void SetControllerPortDevice(uint port, uint device)
    {
        try { Chip8Runtime.SetControllerPortDevice(port, device); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_reset", CallConvs = [typeof(CallConvCdecl)])]
    public static void Reset()
    {
        try { Chip8Runtime.Reset(); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_run", CallConvs = [typeof(CallConvCdecl)])]
    public static void Run()
    {
        try { Chip8Runtime.Run(); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_serialize_size", CallConvs = [typeof(CallConvCdecl)])]
    public static nuint SerializeSize()
    {
        try { return Chip8Runtime.SerializeSize(); }
        catch { Chip8Runtime.RecordFailure(); return 0; }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_serialize", CallConvs = [typeof(CallConvCdecl)])]
    public static byte Serialize(void* data, nuint size)
    {
        try { return Chip8Runtime.Serialize(data, size); }
        catch { Chip8Runtime.RecordFailure(); return 0; }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_unserialize", CallConvs = [typeof(CallConvCdecl)])]
    public static byte Unserialize(void* data, nuint size)
    {
        try { return Chip8Runtime.Unserialize(data, size); }
        catch { Chip8Runtime.RecordFailure(); return 0; }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_cheat_reset", CallConvs = [typeof(CallConvCdecl)])]
    public static void CheatReset()
    {
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_cheat_set", CallConvs = [typeof(CallConvCdecl)])]
    public static void CheatSet(uint index, byte enabled, byte* code)
    {
        _ = index;
        _ = enabled;
        _ = code;
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_load_game", CallConvs = [typeof(CallConvCdecl)])]
    public static byte LoadGame(RetroGameInfo* game)
    {
        try { return Chip8Runtime.LoadGame(game); }
        catch { Chip8Runtime.RecordFailure(); return 0; }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_load_game_special", CallConvs = [typeof(CallConvCdecl)])]
    public static byte LoadGameSpecial(uint gameType, RetroGameInfo* info, nuint count)
    {
        _ = gameType;
        _ = info;
        _ = count;
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_unload_game", CallConvs = [typeof(CallConvCdecl)])]
    public static void UnloadGame()
    {
        try { Chip8Runtime.UnloadGame(); }
        catch { Chip8Runtime.RecordFailure(); }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_get_region", CallConvs = [typeof(CallConvCdecl)])]
    public static uint GetRegion() => (uint)RetroRegion.Ntsc;

    [UnmanagedCallersOnly(EntryPoint = "retro_get_memory_data", CallConvs = [typeof(CallConvCdecl)])]
    public static void* GetMemoryData(uint id)
    {
        try { return Chip8Runtime.GetMemoryData(id); }
        catch { Chip8Runtime.RecordFailure(); return null; }
    }

    [UnmanagedCallersOnly(EntryPoint = "retro_get_memory_size", CallConvs = [typeof(CallConvCdecl)])]
    public static nuint GetMemorySize(uint id)
    {
        try { return Chip8Runtime.GetMemorySize(id); }
        catch { Chip8Runtime.RecordFailure(); return 0; }
    }
}
