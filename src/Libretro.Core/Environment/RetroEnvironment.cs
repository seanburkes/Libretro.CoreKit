using Libretro.Core.Abi;

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

    private bool Invoke(RetroEnvironmentCommand command, void* data) =>
        _callback != null && _callback((uint)command, data) != 0;
}
