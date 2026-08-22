# Linux x64 Release Artifacts

Phase 5 produces commit-identifiable Linux x64 bundles for the NativeAOT probe
and CHIP-8 cores. These bundles are release inputs and CI evidence; the probe
remains a diagnostic core rather than an end-user emulator.

Run the release gate from a clean checkout:

```sh
./eng/run-release.sh
```

For local development only, a dirty-tree build can be inspected with
`COREKIT_RELEASE_ALLOW_DIRTY=1`. Its provenance records `dirty: true`; CI and
release builds reject that state.

## Outputs

The gate writes these files under `artifacts/release/linux-x64`:

- `corekit_probe_libretro-linux-x64.zip`;
- `corekit_chip8_libretro-linux-x64.zip`; and
- `SHA256SUMS`, covering both ZIP archives.

Each core archive contains:

- the stripped executable NativeAOT `.so` core artifact and its separate
  `.so.dbg` diagnostic symbols;
- its canonical RetroArch `.info` file;
- the Libretro.CoreKit MIT license;
- the libretro ABI notice and license;
- the .NET SDK license and third-party notices that accompany the runtime code
  included by NativeAOT; and
- a core-specific `*.provenance.json` manifest.

The provenance schema records the exact .NET SDK, RID, configuration, evaluated
NativeAOT properties, direct P/Invoke and linker settings, canonical libretro
header revision and checksum, pinned RetroArch lifecycle revision, Linux x64
glibc floor and required symbol versions, source revision and tree, dirty-tree
state, and the size and SHA-256 of every companion file other than the manifest
itself. The archive checksum covers the manifest.

## Determinism and verification

Archive members have a fixed order, timestamp, Unix mode, and storage method.
The release gate packages the same publish outputs twice and rejects any
byte-level difference in either archive or `SHA256SUMS`. This proves deterministic
packaging for the exact NativeAOT outputs; it does not pretend that different
SDKs or build environments must emit identical machine code. The provenance
hash identifies the binary that was actually packaged.

Verify downloaded archives with:

```sh
sha256sum -c SHA256SUMS
```

The `Linux x64 release bundle` pull-request job performs the clean-tree build,
executes both stripped cores through their independent C hosts, validates both
manifests and archive layouts, rejects glibc requirements newer than 2.34,
resolves every core relocation with the digest-pinned glibc 2.34 loader, repeats
the determinism check, and uploads all three files as one GitHub Actions
artifact. See [linux-glibc-baseline.md](linux-glibc-baseline.md) for the exact
support boundary and update procedure.

To install a bundle manually, place its `.so` file in RetroArch's core directory
and its `.info` file in the frontend's core-info directory. License and
provenance files remain companion distribution material.
