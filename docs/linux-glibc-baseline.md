# Linux x64 glibc Baseline

Linux x64 release cores require glibc 2.34 or newer. This is a binary
compatibility floor, not a promise that every distribution carrying that libc
version has supported RetroArch packaging. Linux x64 remains the only claimed
frontend platform, with Ubuntu 22.04 as the pinned build runner.

The floor is recorded in `eng/linux-x64-baseline.json`. Its runtime image is a
digest-pinned Red Hat UBI 9 minimal image whose `getconf GNU_LIBC_VERSION`
reports glibc 2.34.

`eng/check-glibc-baseline.py` reads each release ELF's version requirements with
`readelf` and rejects any `GLIBC_*` requirement newer than 2.34. The release
gate then runs `ldd -r` inside the pinned image with networking disabled. That
uses the baseline dynamic loader to resolve the core artifact and all of its
relocations. The independent C hosts continue to execute initialization,
content load, frame execution, reset, teardown, and repeated loader lifecycles;
the container check does not duplicate the ABI oracle.

Run the focused check with Docker or Podman:

```sh
./eng/run-glibc-baseline.sh path/to/corekit_probe_libretro.so \
  path/to/corekit_chip8_libretro.so
```

The complete `./eng/run-release.sh` gate runs it automatically. Release
provenance records the baseline image and glibc version plus every required
glibc symbol version observed in each packaged core.

Changing the floor or image digest is a compatibility change. Review why the
new floor is acceptable, rerun both independent C hosts and the baseline
relocation gate, regenerate release provenance, and pass the Linux x64
RetroArch lifecycle gate before merging it.
