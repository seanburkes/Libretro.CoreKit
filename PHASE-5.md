# Phase 5: Framework Hardening

**Status:** Complete. Compatibility pins are executable policy, Linux x64
release bundles carry deterministic provenance and required companion material,
their glibc 2.34 compatibility floor is enforced, and the reusable managed API
has a preview package and compatibility contract. Craterboy integration is
separate, deferred future work and is not part of this phase.

## Slice 1: Compatibility pin policy

The repository now has one compatibility contract for:

- the minimum .NET SDK and allowed patch roll-forward behavior;
- the canonical `libretro-common` revision, source URL, vendored-header hash,
  API version, and core-option capacity;
- the pinned RetroArch lifecycle revision and required local command-poller
  patch; and
- the distinction between NativeAOT artifact evidence and actual frontend
  support.

`eng/check-compatibility-pins.py` validates those relationships without network
access and reports the exact SDK selected by `global.json`. A dedicated pull
request job runs the checker for every repository change.

The update and promotion procedure is documented in
[`docs/compatibility-policy.md`](docs/compatibility-policy.md). Patch-level SDK
updates still require the complete existing matrix. New SDK feature bands,
libretro headers, RetroArch baselines, and platform support claims require an
explicit compatibility decision plus the relevant native-host and frontend
lifecycle evidence.

## Slice 2: Deterministic release provenance

`eng/run-release.sh` publishes the probe and CHIP-8 cores for Linux x64 and
creates one verified release ZIP per core. Each archive contains the stripped
core artifact, separate native debug symbols, canonical `.info` metadata, the
project and libretro licenses, the .NET license and third-party notices, and a
JSON provenance manifest.

The manifest records the exact SDK, RID, configuration, evaluated NativeAOT
properties, Linux `NODELETE` linkage, header revision and checksum, pinned
RetroArch lifecycle revision, source revision and tree, and the size and SHA-256
of every packaged companion file. `SHA256SUMS` covers both complete archives.

Packaging uses fixed member order, timestamps, Unix modes, and storage. The gate
packages each publish output twice and requires byte-identical ZIPs and checksum
files. Clean source is mandatory in CI; the explicit local override records a
dirty provenance state instead of disguising it as a release.

The `Linux x64 release bundle` pull-request job executes each stripped core with
its independent C host, enforces the packaging contract, and uploads the three
resulting release files. The format and verification procedure are documented
in [`docs/release-artifacts.md`](docs/release-artifacts.md).

## Slice 3: Linux x64 glibc floor

Linux x64 release cores require glibc 2.34 or newer. The canonical baseline
records that version and a digest-pinned Red Hat UBI 9 runtime image.

The release gate rejects ELF artifacts that import a newer `GLIBC_*` symbol
version, then asks the glibc 2.34 loader to resolve every dependency and
relocation with `ldd -r` inside the pinned image. Both cores still execute
through their independent C hosts after this compatibility check, keeping the
native host as the ABI and lifecycle oracle.

Each release manifest records the baseline and the exact glibc symbol versions
required by its core artifact. The support boundary and update procedure are
documented in
[`docs/linux-glibc-baseline.md`](docs/linux-glibc-baseline.md).

## Slice 4: Managed package contract

`Libretro.Core` is packable as `0.1.0-preview.2` for `net10.0`, with complete
NuGet metadata, a package README, portable symbols, no runtime package
dependencies, and a locked analyzer dependency graph. CI builds and uploads the
package as evidence but does not publish it externally.

The public API analyzer records the complete current framework surface and
fails undeclared additions, removals, and signature changes. All entries remain
unshipped until an explicitly authorized first publication. SDK package
validation runs during pack; a published-package baseline is intentionally
deferred until such a package actually exists rather than citing an imaginary
release.

`eng/run-managed-package.sh` verifies package metadata and exact assets, then
restores, builds, and runs a separate consumer using only the produced package
directory. The version rules, promotion procedure, and compatibility guarantees
are documented in
[`docs/managed-package-policy.md`](docs/managed-package-policy.md).

The gate also publishes the probe behavior as a Linux x64 NativeAOT shared core
from a package-only framework reference. Its publishing assembly owns the
mandatory exports and Linux linker decisions, compiles the packaged logging
shim, and passes the independent C host's callback and logging lifecycle. This
closes the difference between consuming the managed API and actually building
an external native core from the package.

## Ongoing regression responsibility

The probe and CHIP-8 cores remain regression consumers for framework changes.
Their independent C hosts, Linux x64 RetroArch lifecycle, release bundle, glibc
floor, and managed package jobs remain required gates.

Phase 5 is complete and this repository is in maintenance mode. Future platform
work remains Windows x64 frontend lifecycle support followed by Android, but no
new implementation phase is active.
