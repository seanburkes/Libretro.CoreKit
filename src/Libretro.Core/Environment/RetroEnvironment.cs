using Libretro.Core.Abi;
using Libretro.Core.Logging;

namespace Libretro.Core.Environment;

public readonly unsafe struct RetroEnvironment
{
    private readonly delegate* unmanaged[Cdecl]<uint, void*, byte> _callback;

    public RetroEnvironment(delegate* unmanaged[Cdecl]<uint, void*, byte> callback) =>
        _callback = callback;

    public bool IsAvailable => _callback != null;

    public bool SetPixelFormat(RetroPixelFormat format)
    {
        var value = (int)format;
        return Invoke(RetroEnvironmentCommand.SetPixelFormat, &value);
    }

    public bool SetSupportNoGame(bool supported)
    {
        byte value = supported ? (byte)1 : (byte)0;
        return Invoke(RetroEnvironmentCommand.SetSupportNoGame, &value);
    }

    public bool SetInputDescriptors(RetroInputDescriptor* descriptors) =>
        descriptors != null && Invoke(RetroEnvironmentCommand.SetInputDescriptors, descriptors);

    public bool SupportsInputBitmasks() =>
        Invoke(RetroEnvironmentCommand.GetInputBitmasks, null);

    public bool SetCoreOptionsV2(RetroCoreOptionsV2* options) =>
        Invoke(RetroEnvironmentCommand.SetCoreOptionsV2, options);

    public bool GetLogInterface(out RetroLogger logger)
    {
        RetroLogCallback value = default;
        var available = Invoke(RetroEnvironmentCommand.GetLogInterface, &value);
        logger = available && value.Log != null ? new RetroLogger(value.Log) : default;
        return logger.IsAvailable;
    }

    public bool GetVariableUpdate(out bool updated)
    {
        byte value = 0;
        var available = Invoke(RetroEnvironmentCommand.GetVariableUpdate, &value);
        updated = available && value != 0;
        return available;
    }

    public bool GetVariable(byte* key, out byte* value)
    {
        value = null;
        if (key == null)
        {
            return false;
        }

        var variable = new RetroVariable { Key = key };
        var available = Invoke(RetroEnvironmentCommand.GetVariable, &variable);
        value = available ? variable.Value : null;
        return available;
    }

    public bool GetSystemDirectory(out byte* path) =>
        GetDirectory(RetroEnvironmentCommand.GetSystemDirectory, out path);

    public bool GetSaveDirectory(out byte* path) =>
        GetDirectory(RetroEnvironmentCommand.GetSaveDirectory, out path);

    public bool GetContentDirectory(out byte* path) =>
        GetDirectory(RetroEnvironmentCommand.GetContentDirectory, out path);

    public bool GetCoreAssetsDirectory(out byte* path) =>
        GetDirectory(RetroEnvironmentCommand.GetCoreAssetsDirectory, out path);

    public bool GetLanguage(out RetroLanguage language)
    {
        var value = RetroLanguage.English;
        var available = Invoke(RetroEnvironmentCommand.GetLanguage, &value);
        language = available ? value : RetroLanguage.English;
        return available;
    }

    public bool GetAudioVideoEnable(out RetroAudioVideoEnableFlags flags)
    {
        var value = RetroAudioVideoEnableFlags.Video | RetroAudioVideoEnableFlags.Audio;
        var available = Invoke(RetroEnvironmentCommand.GetAudioVideoEnable, &value);
        flags = available ? value : RetroAudioVideoEnableFlags.Video | RetroAudioVideoEnableFlags.Audio;
        return available;
    }

    public bool GetFastForwarding(out bool fastForwarding)
    {
        byte value = 0;
        var available = Invoke(RetroEnvironmentCommand.GetFastForwarding, &value);
        fastForwarding = available && value != 0;
        return available;
    }

    public bool GetMessageInterfaceVersion(out uint version)
    {
        uint value = 0;
        var available = Invoke(RetroEnvironmentCommand.GetMessageInterfaceVersion, &value);
        version = available ? value : 0;
        return available;
    }

    public bool SetMessage(RetroMessage* message) =>
        message != null && Invoke(RetroEnvironmentCommand.SetMessage, message);

    public bool SetMessageExtended(RetroMessageExtended* message) =>
        message != null && Invoke(RetroEnvironmentCommand.SetMessageExtended, message);

    private bool GetDirectory(RetroEnvironmentCommand command, out byte* path)
    {
        byte* value = null;
        var available = Invoke(command, &value);
        path = available ? value : null;
        return available;
    }

    private bool Invoke(RetroEnvironmentCommand command, void* data) =>
        _callback != null && _callback((uint)command, data) != 0;
}
