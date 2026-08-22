# Managed Package and Compatibility Policy

`Libretro.Core` is the reusable managed framework package. The current package
version is `0.1.0-preview.2`, targets `net10.0`, and has no runtime package
dependencies. It is a preview artifact, not a stable API or an automatic claim
of RetroArch support on every platform where a consumer can compile it.

Pull requests build the main `.nupkg` and portable-symbol `.snupkg`, validate
their contents, and compile and run a separate project that restores only from
the produced package directory. CI uploads those files as workflow evidence but
does not publish them to NuGet. External publication requires separate explicit
authorization.

## Version rules

Package versions follow SemVer 2.0:

- During `0.x`, an incompatible public API, target framework, or dependency
  contract change increments the minor component and documents the migration.
- Compatible additions and fixes increment the patch or prerelease component.
  They must not remove or change an API within the same `0.x` minor line.
- At `1.0.0` and later, breaking changes increment major, compatible additions
  increment minor, and compatible fixes increment patch.
- Prerelease suffixes describe maturity. A stable version is created only by an
  explicit promotion decision, never by silently dropping `-preview`.
- Assembly versions never decrease. Prereleases sharing a numeric version
  prefix may share its assembly version; changing the numeric prefix changes
  the assembly version produced by the SDK.

`CoreKitManagedPackageVersion` in `Directory.Build.props` is the repository's
single version input. A version change also updates `PACKAGE.md` installation
guidance and package release notes.

## Public API contract

The `Microsoft.CodeAnalysis.PublicApiAnalyzers` build dependency compares the
compiled surface with `PublicAPI.Shipped.txt` and `PublicAPI.Unshipped.txt`.
Adding, removing, or changing a public symbol without explicitly updating those
files fails the build. The analyzer is private build tooling and is not exposed
as a dependency of `Libretro.Core` consumers.

All current APIs remain in `PublicAPI.Unshipped.txt` because no package has been
published. The release that first publishes this package must move its complete
surface to `PublicAPI.Shipped.txt`. Later release work records only new APIs in
the unshipped file and promotes them when published.

SDK package validation is enabled now for package structure and compatible
framework checks. `PackageValidationBaselineVersion` deliberately remains unset
until a real package version exists on the configured feed. After the first
publication, compatible release work must compare against the latest published
version in the same compatibility line. Suppressing an API compatibility error
requires a documented compatibility decision; a suppression is not a version
policy.

## Release gate

Run from a clean checkout:

```sh
./eng/run-managed-package.sh
```

The complete gate runs on Linux x64 and requires the .NET NativeAOT
prerequisites, Clang, CMake, and a C11 compiler.

For local development only, set
`COREKIT_MANAGED_PACKAGE_ALLOW_DIRTY=1`. The gate performs a locked restore,
packs the Release assembly and portable symbols, verifies metadata and exact
package assets, proves that no runtime package dependency escaped into the
manifest, and restores/builds/runs the independent managed package consumer.

The same gate then restores a separate NativeAOT publishing project from the
produced package, compiles the packaged logging shim, publishes a Linux x64
shared core, and executes its full callback and logging lifecycle through the
independent C host. The test project owns its concrete `retro_*` exports and
platform linker settings, matching the boundary required of an external core.

The package contains the reusable `Libretro.Core` assembly and native logging
companion sources; concrete NativeAOT core publishing assemblies and their
`retro_*` entry points remain owned by each emulator core.
