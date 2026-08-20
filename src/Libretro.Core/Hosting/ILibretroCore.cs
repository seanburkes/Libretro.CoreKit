using Libretro.Core.Abi;

namespace Libretro.Core.Hosting;

public interface ILibretroCore
{
    LibretroSystemMetadata SystemMetadata { get; }

    LibretroCallbackRequirements RequiredFrameCallbacks { get; }

    void ConfigureEnvironment(LibretroEnvironmentContext context);

    void Initialize(LibretroInitializationContext context);

    bool LoadContent(LibretroLoadContext context);

    void GetSystemAvInfo(out RetroSystemAvInfo info);

    void Reset();

    void SetControllerPortDevice(uint port, uint device);

    void RunFrame(ref LibretroFrameContext context);

    int SerializedStateSize { get; }

    bool Serialize(Span<byte> destination);

    bool Unserialize(ReadOnlySpan<byte> source);

    Memory<byte> GetMemory(RetroMemory region);

    void UnloadContent();

    void Deinitialize();
}

[Flags]
public enum LibretroCallbackRequirements
{
    None = 0,
    Environment = 1 << 0,
    Video = 1 << 1,
    Audio = 1 << 2,
    InputPoll = 1 << 3,
    InputState = 1 << 4,
    SoftwareCore = Environment | Video | Audio | InputPoll | InputState,
}

public enum LibretroHostState
{
    Uninitialized,
    Initialized,
    ContentLoaded,
}
