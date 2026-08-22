# Libretro.Core

`Libretro.Core` provides the reusable managed ABI, callback wrappers, lifecycle
host, and core contract used to build .NET NativeAOT libretro cores.

```sh
dotnet add package Libretro.Core --version 0.1.0-preview.2
```

Implement `ILibretroCore`, bind it to `LibretroHost<TCore>`, and place the
mandatory `UnmanagedCallersOnly` `retro_*` entry points in the concrete
NativeAOT publishing assembly. NativeAOT does not export those methods from a
referenced package.

On Linux, the package supplies the pinned `native/libretro.h` and audited
`native/libretro_log_shim.c` companion sources. Set `GeneratePathProperty` on
the package reference, compile the shim with the packaged `native` directory as
an include path through `$(PkgLibretro_Core)`, add its object file as a
`NativeLibrary`, and direct-bind `__Internal`. The publishing assembly still
owns its exports, NativeAOT settings, `NODELETE` decision, and other platform
linker behavior. See the repository's `Libretro.Core.NativePackageConsumer`
project for the complete Linux x64 example.

The package currently targets `net10.0` and is a preview. Linux x64 is the only
platform with complete RetroArch lifecycle support in the project; consuming
this managed package does not itself establish frontend support for another
platform.

See the [repository](https://github.com/seanburkes/Libretro.CoreKit) for the
reference cores, native-host gates, release policy, and current limitations.
