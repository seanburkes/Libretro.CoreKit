# Phase 5: Framework Hardening

**Status:** In progress. Compatibility pins are now executable policy rather
than scattered repository conventions. Craterboy integration is separate,
deferred future work and is not part of this phase.

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

## Remaining hardening work

- Produce release-ready core artifacts with checksums, licenses, `.info` files,
  and a provenance manifest containing the SDK, RID, header revision, source
  revision, and build options.
- Establish and test an intentional minimum Linux glibc baseline.
- Define public package versioning and compatibility guarantees for the
  reusable managed API.
- Keep the probe and CHIP-8 cores as regression consumers for framework changes.

The next slice should add deterministic artifact provenance for the probe and
CHIP-8 NativeAOT outputs. It does not need another emulator to justify itself.
