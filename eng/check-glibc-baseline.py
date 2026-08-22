#!/usr/bin/env python3
"""Verify Linux x64 core artifacts against the declared glibc floor."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BASELINE_PATH = ROOT / "eng/linux-x64-baseline.json"
VERSION_PATTERN = re.compile(r"GLIBC_(\d+(?:\.\d+)+)")


def fail(message: str) -> None:
    raise RuntimeError(message)


def version_key(value: str) -> tuple[int, ...]:
    if re.fullmatch(r"\d+(?:\.\d+)+", value) is None:
        fail(f"Invalid glibc version: {value!r}")
    return tuple(int(part) for part in value.split("."))


def load_baseline() -> dict[str, object]:
    baseline = json.loads(BASELINE_PATH.read_text(encoding="utf-8"))
    expected_keys = {
        "architecture",
        "distribution",
        "glibcVersion",
        "runtimeIdentifier",
        "runtimeImage",
        "schemaVersion",
    }
    if set(baseline) != expected_keys:
        fail(f"Unexpected Linux baseline fields: {sorted(baseline)!r}")
    if baseline["schemaVersion"] != 1:
        fail("Unsupported Linux baseline schema version.")
    if baseline["architecture"] != "x86_64" or baseline["runtimeIdentifier"] != "linux-x64":
        fail("The Phase 5 baseline must describe Linux x64.")
    version_key(str(baseline["glibcVersion"]))
    if re.fullmatch(r"[^@\s]+@sha256:[0-9a-f]{64}", str(baseline["runtimeImage"])) is None:
        fail("The Linux baseline runtime image must use an exact SHA-256 digest.")
    return baseline


def readelf(*arguments: str) -> str:
    result = subprocess.run(
        ("readelf", *arguments),
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.stdout


def inspect(path: Path, floor: str) -> dict[str, object]:
    artifact = path.resolve()
    if not artifact.is_file():
        fail(f"Core artifact does not exist: {path}")

    header = readelf("--file-header", str(artifact))
    required_header = (
        "Class:                             ELF64",
        "Data:                              2's complement, little endian",
        "Type:                              DYN (Shared object file)",
        "Machine:                           Advanced Micro Devices X86-64",
    )
    missing = [value.strip() for value in required_header if value not in header]
    if missing:
        fail(f"{artifact.name} is not the expected Linux x64 shared object: {missing!r}")

    versions = sorted(
        set(VERSION_PATTERN.findall(readelf("--version-info", str(artifact)))),
        key=version_key,
    )
    if not versions:
        fail(f"{artifact.name} has no versioned glibc requirements.")
    maximum = versions[-1]
    if version_key(maximum) > version_key(floor):
        fail(
            f"{artifact.name} requires GLIBC_{maximum}, newer than the "
            f"declared GLIBC_{floor} floor."
        )
    return {
        "file": artifact.name,
        "maximumRequiredGlibcVersion": maximum,
        "requiredGlibcVersions": versions,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifacts", nargs="*", type=Path)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--print-floor", action="store_true")
    parser.add_argument("--print-image", action="store_true")
    arguments = parser.parse_args()

    try:
        baseline = load_baseline()
        if arguments.print_floor or arguments.print_image:
            if arguments.artifacts or arguments.json or arguments.print_floor == arguments.print_image:
                fail("Select exactly one baseline value without core artifacts.")
            key = "glibcVersion" if arguments.print_floor else "runtimeImage"
            print(baseline[key])
            return 0
        if not arguments.artifacts:
            fail("At least one Linux x64 core artifact is required.")

        inspections = [
            inspect(path, str(baseline["glibcVersion"])) for path in arguments.artifacts
        ]
        result = {"artifacts": inspections, "baseline": baseline}
        if arguments.json:
            print(json.dumps(result, sort_keys=True))
        else:
            for artifact in inspections:
                print(
                    f"PASS: {artifact['file']} requires at most "
                    f"GLIBC_{artifact['maximumRequiredGlibcVersion']} "
                    f"(declared floor GLIBC_{baseline['glibcVersion']})"
                )
    except (OSError, RuntimeError, subprocess.CalledProcessError, json.JSONDecodeError) as error:
        print(f"glibc baseline check failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
