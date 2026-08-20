namespace Libretro.Core.Hosting;

public readonly struct LibretroSystemMetadata
{
    public LibretroSystemMetadata(
        string libraryName,
        string libraryVersion,
        string validExtensions = "",
        bool needFullPath = false,
        bool blockExtract = false)
    {
        LibraryName = libraryName;
        LibraryVersion = libraryVersion;
        ValidExtensions = validExtensions;
        NeedFullPath = needFullPath;
        BlockExtract = blockExtract;
    }

    public string LibraryName { get; }

    public string LibraryVersion { get; }

    public string ValidExtensions { get; }

    public bool NeedFullPath { get; }

    public bool BlockExtract { get; }
}
