using System.Reflection;
using Libretro.Core.Abi;
using Libretro.Core.Hosting;

var host = new LibretroHost<PackageCore>(new PackageCore());
var assembly = typeof(ILibretroCore).Assembly;
var informationalVersion = assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion;

if (!LibretroAbi.IsSupportedLayout() ||
    host.State != LibretroHostState.Uninitialized ||
    host.HasFailed ||
    assembly.GetName().Version != new Version(0, 1, 0, 0) ||
    informationalVersion is null ||
    !informationalVersion.StartsWith("0.1.0-preview.2", StringComparison.Ordinal))
{
    return 1;
}

Console.WriteLine($"PASS: consumed {assembly.GetName().Name} {informationalVersion}");
return 0;

internal sealed class PackageCore : ILibretroCore
{
    public LibretroSystemMetadata SystemMetadata { get; } =
        new("Package consumer", "0.1", validExtensions: "");

    public LibretroCallbackRequirements RequiredFrameCallbacks =>
        LibretroCallbackRequirements.None;

    public int SerializedStateSize => 0;

    public void ConfigureEnvironment(LibretroEnvironmentContext context) { }

    public void Initialize(LibretroInitializationContext context) { }

    public bool LoadContent(LibretroLoadContext context) => true;

    public void GetSystemAvInfo(out RetroSystemAvInfo info) => info = default;

    public void Reset() { }

    public void SetControllerPortDevice(uint port, uint device) { }

    public void RunFrame(ref LibretroFrameContext context) { }

    public bool Serialize(Span<byte> destination) => destination.IsEmpty;

    public bool Unserialize(ReadOnlySpan<byte> source) => source.IsEmpty;

    public Memory<byte> GetMemory(RetroMemory region) => Memory<byte>.Empty;

    public void UnloadContent() { }

    public void Deinitialize() { }
}
