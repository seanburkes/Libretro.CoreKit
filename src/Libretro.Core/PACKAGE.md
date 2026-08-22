# Libretro.Core

`Libretro.Core` provides the reusable managed ABI, callback wrappers, lifecycle
host, and core contract used to build .NET NativeAOT libretro cores.

```sh
dotnet add package Libretro.Core --version 0.1.0-preview.1
```

Implement `ILibretroCore`, bind it to `LibretroHost<TCore>`, and place the
mandatory `UnmanagedCallersOnly` `retro_*` entry points in the concrete
NativeAOT publishing assembly. NativeAOT does not export those methods from a
referenced package.

The package currently targets `net10.0` and is a preview. Linux x64 is the only
platform with complete RetroArch lifecycle support in the project; consuming
this managed package does not itself establish frontend support for another
platform.

See the [repository](https://github.com/seanburkes/Libretro.CoreKit) for the
reference cores, native-host gates, release policy, and current limitations.
