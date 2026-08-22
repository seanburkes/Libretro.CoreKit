#!/usr/bin/env python3
"""Create and verify deterministic Linux libretro release bundles."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RID = "linux-x64"
CONFIGURATION = "Release"
ZIP_TIME = (1980, 1, 1, 0, 0, 0)
FILE_MODE = 0o100644
EXECUTABLE_MODE = 0o100755
REPOSITORY = "https://github.com/seanburkes/Libretro.CoreKit"
BUILD_PROPERTIES = (
    "TargetFramework",
    "PublishAot",
    "NativeLib",
    "SelfContained",
    "IsAotCompatible",
    "InvariantGlobalization",
    "OptimizationPreference",
    "StripSymbols",
    "Deterministic",
)


@dataclass(frozen=True)
class Core:
    id: str
    stem: str
    project: str

    @property
    def binary(self) -> str:
        return f"{self.stem}.so"

    @property
    def debug(self) -> str:
        return f"{self.binary}.dbg"

    @property
    def info(self) -> str:
        return f"{self.stem}.info"

    @property
    def provenance(self) -> str:
        return f"{self.stem}.provenance.json"

    @property
    def archive(self) -> str:
        return f"{self.stem}-{RID}.zip"


CORES = (
    Core(
        "probe",
        "corekit_probe_libretro",
        "src/Libretro.NativeAot.Probe/Libretro.NativeAot.Probe.csproj",
    ),
    Core(
        "chip8",
        "corekit_chip8_libretro",
        "src/Libretro.NativeAot.Chip8/Libretro.NativeAot.Chip8.csproj",
    ),
)


def fail(message: str) -> None:
    raise RuntimeError(message)


def run(*arguments: str) -> str:
    result = subprocess.run(
        arguments,
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.stdout.strip()


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def exact(pattern: str, value: str, description: str) -> str:
    match = re.fullmatch(pattern, value)
    if match is None:
        fail(f"Invalid {description}: {value!r}")
    return match.group(1) if match.lastindex else match.group(0)


def source_identity(allow_dirty: bool) -> dict[str, object]:
    dirty = bool(run("git", "status", "--porcelain", "--untracked-files=normal"))
    if dirty and not allow_dirty:
        fail(
            "Release artifacts require a clean source tree; "
            "use --allow-dirty only for local validation."
        )
    return {
        "dirty": dirty,
        "repository": REPOSITORY,
        "revision": exact(r"[0-9a-f]{40}", run("git", "rev-parse", "HEAD"), "source revision"),
        "tree": exact(r"[0-9a-f]{40}", run("git", "rev-parse", "HEAD^{tree}"), "source tree"),
    }


def compatibility_identity() -> dict[str, object]:
    version_lines = (ROOT / "eng/libretro/VERSION").read_text(encoding="utf-8").splitlines()
    if len(version_lines) != 2:
        fail("eng/libretro/VERSION must contain exactly two lines.")
    revision = exact(
        r"libretro-common ([0-9a-f]{40})",
        version_lines[0],
        "libretro revision",
    )
    source = exact(r"source (https://\S+)", version_lines[1], "libretro source")

    checksum_line = (ROOT / "eng/libretro/SHA256").read_text(encoding="utf-8").strip()
    expected_hash = exact(
        r"([0-9a-f]{64})  libretro\.h",
        checksum_line,
        "libretro header checksum",
    )
    header = (ROOT / "eng/libretro/libretro.h").read_bytes()
    if digest(header) != expected_hash:
        fail("The vendored libretro header does not match eng/libretro/SHA256.")
    api_match = re.search(
        r"^#define RETRO_API_VERSION\s+(\d+)$",
        header.decode("utf-8"),
        re.MULTILINE,
    )
    if api_match is None:
        fail("RETRO_API_VERSION is missing from the vendored libretro header.")

    retroarch = exact(
        r"[0-9a-f]{40}",
        (ROOT / "eng/retroarch/VERSION").read_text(encoding="utf-8").strip(),
        "RetroArch revision",
    )
    return {
        "libretroHeader": {
            "apiVersion": int(api_match.group(1)),
            "revision": revision,
            "sha256": expected_hash,
            "source": source,
        },
        "retroarchLifecycleRevision": retroarch,
    }


def dotnet_notices(sdk_version: str) -> tuple[Path, Path]:
    sdk_root = None
    for line in run("dotnet", "--list-sdks").splitlines():
        match = re.fullmatch(r"(\S+) \[(.+)]", line.strip())
        if match is not None and match.group(1) == sdk_version:
            sdk_root = Path(match.group(2)).parent
            break
    if sdk_root is None:
        fail(f"Could not locate the installed .NET SDK root for {sdk_version}.")

    files = {path.name.lower(): path for path in sdk_root.iterdir() if path.is_file()}
    license_path = files.get("license.txt")
    notices_path = files.get("thirdpartynotices.txt") or files.get("third-party-notices.txt")
    if license_path is None or notices_path is None:
        fail(f"The .NET SDK root {sdk_root} is missing license or notice material.")
    return license_path, notices_path


def build_identity(project: str) -> dict[str, object]:
    result = json.loads(
        run(
            "dotnet",
            "msbuild",
            project,
            "-nologo",
            "-v:q",
            f"-p:RuntimeIdentifier={RID}",
            f"-p:Configuration={CONFIGURATION}",
            "-p:StripSymbols=true",
            f"-getProperty:{','.join(BUILD_PROPERTIES)}",
            "-getItem:LinkerArg,DirectPInvoke",
        )
    )
    properties = result.get("Properties", {})
    required = {
        "PublishAot": "true",
        "NativeLib": "Shared",
        "SelfContained": "true",
        "StripSymbols": "true",
        "Deterministic": "true",
    }
    if any(properties.get(name) != value for name, value in required.items()):
        fail(f"Unexpected NativeAOT release properties for {project}: {properties!r}")

    items = result.get("Items", {})
    linker = sorted(item["Identity"] for item in items.get("LinkerArg", []))
    direct_pinvokes = sorted(item["Identity"] for item in items.get("DirectPInvoke", []))
    if linker != ["-Wl,-z,nodelete"] or direct_pinvokes != ["__Internal"]:
        fail(f"Unexpected Linux native linkage for {project}.")
    return {
        "directPInvokes": direct_pinvokes,
        "nativeLinkerArguments": linker,
        "properties": properties,
    }


def linux_compatibility(binary: Path) -> dict[str, object]:
    result = json.loads(
        run(
            sys.executable,
            "eng/check-glibc-baseline.py",
            "--json",
            str(binary),
        )
    )
    artifacts = result.get("artifacts", [])
    baseline = result.get("baseline")
    if len(artifacts) != 1 or not isinstance(baseline, dict):
        fail(f"Unexpected glibc baseline result for {binary}.")
    artifact = artifacts[0]
    return {
        "baseline": baseline,
        "maximumRequiredGlibcVersion": artifact["maximumRequiredGlibcVersion"],
        "requiredGlibcVersions": artifact["requiredGlibcVersions"],
    }


def write_archive(
    core: Core,
    publish_directory: Path,
    output_directory: Path,
    sdk_version: str,
    license_path: Path,
    notices_path: Path,
    compatibility: dict[str, object],
    source: dict[str, object],
) -> Path:
    inputs = (
        (core.binary, publish_directory / core.binary, "core", EXECUTABLE_MODE),
        (core.debug, publish_directory / core.debug, "debugSymbols", FILE_MODE),
        (core.info, ROOT / "eng/release/info" / core.info, "frontendInfo", FILE_MODE),
        ("LICENSE.txt", ROOT / "LICENSE", "projectLicense", FILE_MODE),
        (
            "LIBRETRO_ABI_NOTICE.md",
            ROOT / "src/Libretro.Core/Abi/NOTICE.md",
            "libretroAbiNotice",
            FILE_MODE,
        ),
        ("DOTNET_LICENSE.txt", license_path, "dotnetLicense", FILE_MODE),
        (
            "DOTNET_THIRD_PARTY_NOTICES.txt",
            notices_path,
            "dotnetThirdPartyNotices",
            FILE_MODE,
        ),
    )

    members: dict[str, tuple[bytes, int]] = {}
    files: list[dict[str, object]] = []
    for name, path, role, mode in inputs:
        if not path.is_file():
            fail(f"Release input is missing: {path}")
        data = path.read_bytes()
        members[name] = (data, mode)
        files.append({"path": name, "role": role, "sha256": digest(data), "size": len(data)})

    manifest = {
        "artifact": {
            "archive": core.archive,
            "configuration": CONFIGURATION,
            "core": core.id,
            "runtimeIdentifier": RID,
        },
        "build": build_identity(core.project),
        "compatibility": {
            **compatibility,
            "linuxRuntime": linux_compatibility(publish_directory / core.binary),
        },
        "files": sorted(files, key=lambda item: str(item["path"])),
        "schemaVersion": 1,
        "source": source,
        "toolchain": {"dotnetSdk": sdk_version},
    }
    manifest_data = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")
    members[core.provenance] = (manifest_data, FILE_MODE)

    archive_path = output_directory / core.archive
    temporary_path = archive_path.with_suffix(".zip.tmp")
    with zipfile.ZipFile(temporary_path, "w", compression=zipfile.ZIP_STORED) as archive:
        for name in sorted(members):
            data, mode = members[name]
            info = zipfile.ZipInfo(name, ZIP_TIME)
            info.compress_type = zipfile.ZIP_STORED
            info.create_system = 3
            info.external_attr = mode << 16
            archive.writestr(info, data)
    temporary_path.replace(archive_path)

    with zipfile.ZipFile(archive_path, "r") as archive:
        infos = archive.infolist()
        if [info.filename for info in infos] != sorted(members) or archive.comment:
            fail(f"Unexpected archive layout in {archive_path.name}.")
        for info in infos:
            _, expected_mode = members[info.filename]
            if (
                info.date_time != ZIP_TIME
                or info.compress_type != zipfile.ZIP_STORED
                or info.extra
                or info.external_attr >> 16 != expected_mode
            ):
                fail(f"Non-deterministic ZIP metadata for {info.filename}.")
        if json.loads(archive.read(core.provenance)) != manifest:
            fail(f"Provenance changed while writing {archive_path.name}.")
        for item in manifest["files"]:
            data = archive.read(item["path"])
            if digest(data) != item["sha256"] or len(data) != item["size"]:
                fail(f"Provenance mismatch for {item['path']}.")
    return archive_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rid", default=RID)
    parser.add_argument("--probe-dir", type=Path, required=True)
    parser.add_argument("--chip8-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allow-dirty", action="store_true")
    arguments = parser.parse_args()

    try:
        if arguments.rid != RID:
            fail(f"This release slice supports {RID}, not {arguments.rid}.")
        output = arguments.output.resolve()
        output.mkdir(parents=True, exist_ok=True)
        expected_outputs = sorted([core.archive for core in CORES] + ["SHA256SUMS"])
        existing_outputs = sorted(path.name for path in output.iterdir())
        if existing_outputs and existing_outputs != expected_outputs:
            fail(f"Release output directory is not clean: {existing_outputs!r}")

        sdk_version = exact(
            r"\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?",
            run("dotnet", "--version"),
            ".NET SDK version",
        )
        license_path, notices_path = dotnet_notices(sdk_version)
        compatibility = compatibility_identity()
        source = source_identity(arguments.allow_dirty)
        publish_directories = {
            "probe": arguments.probe_dir.resolve(),
            "chip8": arguments.chip8_dir.resolve(),
        }
        archives = [
            write_archive(
                core,
                publish_directories[core.id],
                output,
                sdk_version,
                license_path,
                notices_path,
                compatibility,
                source,
            )
            for core in CORES
        ]

        checksum_lines = [f"{digest(path.read_bytes())}  {path.name}" for path in sorted(archives)]
        (output / "SHA256SUMS").write_text(
            "\n".join(checksum_lines) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        if sorted(path.name for path in output.iterdir()) != expected_outputs:
            fail("Release output set is incomplete.")
    except (OSError, RuntimeError, subprocess.CalledProcessError, json.JSONDecodeError) as error:
        print(f"release packaging failed: {error}", file=sys.stderr)
        return 1

    print(f"PASS: deterministic {RID} release bundles")
    print("\n".join(checksum_lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
