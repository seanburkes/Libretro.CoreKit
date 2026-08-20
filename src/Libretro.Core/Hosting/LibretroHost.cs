using System.Buffers;
using Libretro.Core.Abi;
using Libretro.Core.Environment;
using Libretro.Core.Logging;

namespace Libretro.Core.Hosting;

public sealed unsafe class LibretroHost<TCore>
    where TCore : class, ILibretroCore
{
    private readonly TCore _core;
    private RetroFrontendCallbacks _callbacks;
    private RetroLogger _logger;
    private LibretroHostState _state;
    private bool _supportsInputBitmasks;
    private bool _failed;
    private int _serializedStateSize;
    private PinnedMemoryRegion _saveRam;
    private PinnedMemoryRegion _rtc;
    private PinnedMemoryRegion _systemRam;
    private PinnedMemoryRegion _videoRam;

    public LibretroHost(TCore core) =>
        _core = core ?? throw new ArgumentNullException(nameof(core));

    public LibretroHostState State => _state;

    public bool HasFailed => _failed;

    public void SetEnvironment(delegate* unmanaged[Cdecl]<uint, void*, byte> callback)
    {
        if (_state != LibretroHostState.Uninitialized)
        {
            return;
        }

        _callbacks.Environment = callback;
        var environment = new RetroEnvironment(callback);
        _ = environment.GetLogInterface(out _logger);

        try
        {
            _core.ConfigureEnvironment(new LibretroEnvironmentContext(environment, _logger));
            _ = _logger.Write(RetroLogLevel.Debug, "CoreKit environment configured\n\0"u8);
        }
        catch
        {
            _failed = true;
        }
    }

    public void SetVideoRefresh(delegate* unmanaged[Cdecl]<void*, uint, uint, nuint, void> callback) =>
        _callbacks.VideoRefresh = callback;

    public void SetAudioSample(delegate* unmanaged[Cdecl]<short, short, void> callback) =>
        _callbacks.AudioSample = callback;

    public void SetAudioSampleBatch(delegate* unmanaged[Cdecl]<short*, nuint, nuint> callback) =>
        _callbacks.AudioSampleBatch = callback;

    public void SetInputPoll(delegate* unmanaged[Cdecl]<void> callback) =>
        _callbacks.InputPoll = callback;

    public void SetInputState(delegate* unmanaged[Cdecl]<uint, uint, uint, uint, short> callback) =>
        _callbacks.InputState = callback;

    public void Initialize()
    {
        if (_state != LibretroHostState.Uninitialized || _failed ||
            !HasRequiredCallback(LibretroCallbackRequirements.Environment) ||
            !HasValidCallbackRequirements())
        {
            return;
        }

        if (!LibretroAbi.IsSupportedLayout())
        {
            _failed = true;
            return;
        }

        try
        {
            var environment = new RetroEnvironment(_callbacks.Environment);
            _supportsInputBitmasks = environment.SupportsInputBitmasks();
            _core.Initialize(new LibretroInitializationContext(
                environment,
                _logger,
                _supportsInputBitmasks));
            _state = LibretroHostState.Initialized;
            _ = _logger.Write(RetroLogLevel.Info, "CoreKit host initialized\n\0"u8);
        }
        catch
        {
            try
            {
                _core.Deinitialize();
            }
            catch
            {
                // Preserve the original initialization failure.
            }

            _failed = true;
        }
    }

    public byte LoadGame(RetroGameInfo* game)
    {
        if (_state != LibretroHostState.Initialized || _failed)
        {
            return 0;
        }

        if (game != null &&
            ((game->Data == null && game->Size != 0) || game->Size > int.MaxValue))
        {
            return 0;
        }

        try
        {
            var context = new LibretroLoadContext(
                new LibretroContent(game),
                new RetroEnvironment(_callbacks.Environment),
                _logger);
            if (!_core.LoadContent(context))
            {
                return 0;
            }

            var serializedStateSize = _core.SerializedStateSize;
            if (serializedStateSize < 0)
            {
                throw new InvalidOperationException("Serialized state size cannot be negative.");
            }

            PinMemoryRegions();
            _serializedStateSize = serializedStateSize;
            _state = LibretroHostState.ContentLoaded;
            _ = _logger.Write(RetroLogLevel.Info, "CoreKit content loaded\n\0"u8);
            return 1;
        }
        catch
        {
            ReleaseContentResources();
            try
            {
                _core.UnloadContent();
            }
            catch
            {
                // Preserve the original content-load failure.
            }

            _failed = true;
            return 0;
        }
    }

    public void GetSystemAvInfo(RetroSystemAvInfo* info)
    {
        if (info == null)
        {
            return;
        }

        *info = default;
        if (_state != LibretroHostState.ContentLoaded || _failed)
        {
            return;
        }

        try
        {
            _core.GetSystemAvInfo(out *info);
        }
        catch
        {
            *info = default;
            _failed = true;
        }
    }

    public void Reset()
    {
        if (_state != LibretroHostState.ContentLoaded || _failed)
        {
            return;
        }

        try
        {
            _core.Reset();
        }
        catch
        {
            _failed = true;
        }
    }

    public void Run()
    {
        if (_state != LibretroHostState.ContentLoaded || _failed || !HasRequiredCallbacks())
        {
            return;
        }

        var allocatedBefore = GC.GetAllocatedBytesForCurrentThread();
        try
        {
            var environment = new RetroEnvironment(_callbacks.Environment);
            _ = environment.GetVariableUpdate(out var coreOptionsUpdated);
            _ = environment.GetAudioVideoEnable(out var audioVideoEnable);
            _ = environment.GetFastForwarding(out var fastForwarding);
            var context = new LibretroFrameContext(
                _callbacks,
                environment,
                _logger,
                _supportsInputBitmasks,
                coreOptionsUpdated,
                audioVideoEnable,
                fastForwarding);
            _core.RunFrame(ref context);
        }
        catch
        {
            _failed = true;
        }

        if (GC.GetAllocatedBytesForCurrentThread() != allocatedBefore)
        {
            _failed = true;
        }
    }

    public void UnloadGame()
    {
        if (_state != LibretroHostState.ContentLoaded)
        {
            return;
        }

        try
        {
            _core.UnloadContent();
            _ = _logger.Write(RetroLogLevel.Info, "CoreKit content unloaded\n\0"u8);
        }
        catch
        {
            _failed = true;
        }
        finally
        {
            ReleaseContentResources();
            _state = LibretroHostState.Initialized;
        }
    }

    public nuint SerializeSize() =>
        _state == LibretroHostState.ContentLoaded && !_failed
            ? (nuint)_serializedStateSize
            : 0;

    public byte Serialize(void* data, nuint size)
    {
        if (_state != LibretroHostState.ContentLoaded || _failed || data == null)
        {
            return 0;
        }

        try
        {
            var expectedSize = _serializedStateSize;
            if (expectedSize <= 0 || size != (nuint)expectedSize)
            {
                return 0;
            }

            return _core.Serialize(new Span<byte>(data, expectedSize)) ? (byte)1 : (byte)0;
        }
        catch
        {
            _failed = true;
            return 0;
        }
    }

    public byte Unserialize(void* data, nuint size)
    {
        if (_state != LibretroHostState.ContentLoaded || _failed || data == null)
        {
            return 0;
        }

        try
        {
            var expectedSize = _serializedStateSize;
            if (expectedSize <= 0 || size != (nuint)expectedSize)
            {
                return 0;
            }

            return _core.Unserialize(new ReadOnlySpan<byte>(data, expectedSize))
                ? (byte)1
                : (byte)0;
        }
        catch
        {
            _failed = true;
            return 0;
        }
    }

    public void* GetMemoryData(uint id)
    {
        if (_state != LibretroHostState.ContentLoaded)
        {
            return null;
        }

        return id switch
        {
            (uint)RetroMemory.SaveRam => _saveRam.Data,
            (uint)RetroMemory.Rtc => _rtc.Data,
            (uint)RetroMemory.SystemRam => _systemRam.Data,
            (uint)RetroMemory.VideoRam => _videoRam.Data,
            _ => null,
        };
    }

    public nuint GetMemorySize(uint id)
    {
        if (_state != LibretroHostState.ContentLoaded)
        {
            return 0;
        }

        return id switch
        {
            (uint)RetroMemory.SaveRam => _saveRam.Size,
            (uint)RetroMemory.Rtc => _rtc.Size,
            (uint)RetroMemory.SystemRam => _systemRam.Size,
            (uint)RetroMemory.VideoRam => _videoRam.Size,
            _ => 0,
        };
    }

    public void Deinitialize()
    {
        if (_state == LibretroHostState.ContentLoaded)
        {
            try
            {
                _core.UnloadContent();
            }
            catch
            {
                // Continue logical teardown.
            }
            finally
            {
                ReleaseContentResources();
            }
        }

        if (_state != LibretroHostState.Uninitialized)
        {
            try
            {
                _core.Deinitialize();
            }
            catch
            {
                // Continue logical teardown.
            }
        }

        try
        {
            _ = _logger.Write(RetroLogLevel.Info, "CoreKit host deinitialized\n\0"u8);
        }
        catch
        {
            // Logging must never prevent logical teardown.
        }
        finally
        {
            _callbacks = default;
            _logger = default;
            _state = LibretroHostState.Uninitialized;
            _supportsInputBitmasks = false;
            _failed = false;
        }
    }

    public void RecordFailure() => _failed = true;

    private void PinMemoryRegions()
    {
        _saveRam.Pin(_core.GetMemory(RetroMemory.SaveRam));
        _rtc.Pin(_core.GetMemory(RetroMemory.Rtc));
        _systemRam.Pin(_core.GetMemory(RetroMemory.SystemRam));
        _videoRam.Pin(_core.GetMemory(RetroMemory.VideoRam));
    }

    private void ReleaseContentResources()
    {
        _saveRam.Release();
        _rtc.Release();
        _systemRam.Release();
        _videoRam.Release();
        _serializedStateSize = 0;
    }

    private bool HasRequiredCallbacks()
    {
        var requirements = _core.RequiredFrameCallbacks;
        return (requirements & ~LibretroCallbackRequirements.SoftwareCore) == 0 &&
            HasRequiredCallback(requirements & LibretroCallbackRequirements.Environment) &&
            HasRequiredCallback(requirements & LibretroCallbackRequirements.Video) &&
            HasRequiredCallback(requirements & LibretroCallbackRequirements.Audio) &&
            HasRequiredCallback(requirements & LibretroCallbackRequirements.InputPoll) &&
            HasRequiredCallback(requirements & LibretroCallbackRequirements.InputState);
    }

    private bool HasValidCallbackRequirements() =>
        (_core.RequiredFrameCallbacks & ~LibretroCallbackRequirements.SoftwareCore) == 0;

    private bool HasRequiredCallback(LibretroCallbackRequirements requirement) =>
        requirement switch
        {
            LibretroCallbackRequirements.None => true,
            LibretroCallbackRequirements.Environment => _callbacks.Environment != null,
            LibretroCallbackRequirements.Video => _callbacks.VideoRefresh != null,
            LibretroCallbackRequirements.Audio =>
                _callbacks.AudioSampleBatch != null || _callbacks.AudioSample != null,
            LibretroCallbackRequirements.InputPoll => _callbacks.InputPoll != null,
            LibretroCallbackRequirements.InputState => _callbacks.InputState != null,
            _ => false,
        };

    private struct PinnedMemoryRegion
    {
        private MemoryHandle _handle;

        public void* Data { get; private set; }

        public nuint Size { get; private set; }

        public void Pin(Memory<byte> memory)
        {
            if (memory.IsEmpty)
            {
                return;
            }

            _handle = memory.Pin();
            if (_handle.Pointer == null)
            {
                _handle.Dispose();
                this = default;
                throw new InvalidOperationException("A non-empty memory region returned a null pointer.");
            }

            Data = _handle.Pointer;
            Size = (nuint)memory.Length;
        }

        public void Release()
        {
            _handle.Dispose();
            this = default;
        }
    }
}
