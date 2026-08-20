using Libretro.Core.Abi;

namespace Libretro.Core.Hosting;

public interface ILibretroCore
{
    LibretroCallbackRequirements RequiredFrameCallbacks { get; }

    void ConfigureEnvironment(LibretroEnvironmentContext context);

    void Initialize(LibretroInitializationContext context);

    bool LoadContent(LibretroLoadContext context);

    void GetSystemAvInfo(out RetroSystemAvInfo info);

    void Reset();

    void RunFrame(ref LibretroFrameContext context);

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
