# Compatibility Update Policy

Libretro.CoreKit pins the .NET SDK feature band, canonical `libretro.h`, and
the RetroArch lifecycle baseline. Updating one of these inputs is a compatibility
change, not routine dependency housekeeping.

Run `python3 eng/check-compatibility-pins.py` before every compatibility update.
The pull-request gate runs the same check.

## .NET SDK

`global.json` pins the minimum SDK and uses `latestPatch` within that feature
band. CI may select a newer patch in the same band, but records the exact SDK in
the compatibility-check output and lifecycle evidence.

- A patch update must pass the Release build, both independent C hosts, all
  NativeAOT artifact jobs, and the Linux x64 RetroArch lifecycle gate.
- A new feature band, minor version, or major version requires an explicit
  compatibility decision. Revalidate NativeAOT exports, process-lifetime module
  residency, logical teardown, bounded memory growth, and normal frontend exit.
- Do not widen `rollForward` merely to make an unavailable SDK appear supported.

## Canonical libretro header

`eng/libretro/VERSION` records the full `libretro-common` revision and source
URL. `eng/libretro/SHA256` authenticates the vendored header used by both C
hosts.

To update the header:

1. Replace `eng/libretro/libretro.h` from one reviewed upstream commit.
2. Update `VERSION` and `SHA256` together.
3. Review the header diff and update only ABI declarations or environment
   commands exercised by a current vertical slice.
4. Extend the managed layout guard and independent C-host assertions for every
   newly bound type or constant.
5. Run the complete NativeAOT matrix and Linux x64 RetroArch lifecycle gate.

The checker also compares the canonical API version and core-option capacity
with their managed constants. A checksum update without the corresponding
review and ABI evidence is not an upgrade process; it is a checksum-shaped
rubber stamp.

## RetroArch baseline

`eng/retroarch/VERSION` pins the blocking Linux x64 lifecycle revision. The
source build carries the narrow command-poller repair in
`eng/retroarch/0001-netcmd-return-after-reinit.patch`.

To update the baseline:

1. Update the full commit hash and confirm the lifecycle patch still applies or
   remove it only when the upstream revision contains an equivalent fix.
2. Run the conventional C control core when diagnosing lifecycle behavior so a
   frontend regression is not misattributed to NativeAOT.
3. Pass content load, reset, state/save persistence, logical teardown, core
   switching, process reopen, bounded-memory, and normal-exit scenarios.
4. Record the new revision and measured evidence in the relevant phase result.

Testing a current stable RetroArch release is useful as a non-blocking signal;
it does not replace the pinned reproducible gate.

## Linux x64 glibc baseline

`eng/linux-x64-baseline.json` declares glibc 2.34 and pins the UBI 9 image used
as the oldest runtime loader gate. `eng/check-glibc-baseline.py` also enforces
the maximum imported `GLIBC_*` symbol version before packaging.

A glibc floor or image update must pass the complete release gate, both
independent C hosts, and the Linux x64 RetroArch lifecycle gate. Raising the
floor additionally requires an explicit support decision and release note;
changing a container tag or digest is not dependency housekeeping when it is
part of the compatibility evidence.

## Support claims

Linux x64 remains the only RetroArch-supported target. NativeAOT artifacts and
independent C-host results on other targets prove ABI/artifact compatibility,
not frontend support. Promote another platform only after it has equivalent
automated RetroArch lifecycle evidence and an explicit compatibility decision.
