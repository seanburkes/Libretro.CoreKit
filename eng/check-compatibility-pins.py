#!/usr/bin/env python3
"""Validate the repository's local SDK, ABI, and frontend compatibility pins."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
HEX_COMMIT = re.compile(r"[0-9a-f]{40}")
SDK_VERSION = re.compile(r"(?P<major>\d+)\.(?P<minor>\d+)\.(?P<patch>\d+)(?:[-+].*)?")


def fail(message: str) -> None:
    raise ValueError(message)


def require_match(pattern: str, text: str, description: str) -> re.Match[str]:
    match = re.search(pattern, text, re.MULTILINE)
    if match is None:
        fail(f"Could not read {description}.")
    return match


def parse_sdk_version(value: str, description: str) -> tuple[int, int, int]:
    match = SDK_VERSION.fullmatch(value.strip())
    if match is None:
        fail(f"{description} is not a supported SDK version: {value!r}")
    return tuple(int(match.group(name)) for name in ("major", "minor", "patch"))


def validate_sdk() -> tuple[str, str]:
    global_json = json.loads((REPOSITORY_ROOT / "global.json").read_text(encoding="utf-8"))
    sdk = global_json.get("sdk")
    if not isinstance(sdk, dict):
        fail("global.json does not contain an sdk object.")

    baseline_text = sdk.get("version")
    if not isinstance(baseline_text, str):
        fail("global.json does not contain an sdk.version string.")
    baseline = parse_sdk_version(baseline_text, "global.json sdk.version")
    if sdk.get("rollForward") != "latestPatch":
        fail("global.json sdk.rollForward must remain latestPatch.")

    selected_text = subprocess.run(
        ["dotnet", "--version"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    selected = parse_sdk_version(selected_text, "selected dotnet SDK")
    if selected[:2] != baseline[:2] or selected[2] // 100 != baseline[2] // 100:
        fail(
            "The selected dotnet SDK is outside the pinned feature band: "
            f"baseline {baseline_text}, selected {selected_text}."
        )
    if selected < baseline:
        fail(f"The selected dotnet SDK {selected_text} is older than {baseline_text}.")

    return baseline_text, selected_text


def validate_libretro() -> str:
    version_lines = (
        REPOSITORY_ROOT / "eng/libretro/VERSION"
    ).read_text(encoding="utf-8").splitlines()
    if len(version_lines) != 2:
        fail("eng/libretro/VERSION must contain exactly a revision and source line.")

    revision_match = re.fullmatch(r"libretro-common ([0-9a-f]{40})", version_lines[0])
    if revision_match is None:
        fail("eng/libretro/VERSION has an invalid revision line.")
    revision = revision_match.group(1)
    expected_source = (
        "source https://github.com/libretro/libretro-common/blob/"
        f"{revision}/include/libretro.h"
    )
    if version_lines[1] != expected_source:
        fail("eng/libretro/VERSION source does not match its pinned revision.")

    header_path = REPOSITORY_ROOT / "eng/libretro/libretro.h"
    checksum_line = (
        REPOSITORY_ROOT / "eng/libretro/SHA256"
    ).read_text(encoding="utf-8").strip()
    checksum_match = re.fullmatch(r"([0-9a-f]{64})  libretro\.h", checksum_line)
    if checksum_match is None:
        fail("eng/libretro/SHA256 has an invalid checksum line.")
    actual_checksum = hashlib.sha256(header_path.read_bytes()).hexdigest()
    if actual_checksum != checksum_match.group(1):
        fail("The vendored libretro.h does not match eng/libretro/SHA256.")

    header = header_path.read_text(encoding="utf-8")
    managed = (
        REPOSITORY_ROOT / "src/Libretro.Core/Abi/LibretroConstants.cs"
    ).read_text(encoding="utf-8")
    header_api = int(
        require_match(
            r"^#define\s+RETRO_API_VERSION\s+(\d+)\s*$",
            header,
            "RETRO_API_VERSION from libretro.h",
        ).group(1)
    )
    managed_api = int(
        require_match(
            r"public const uint ApiVersion = (\d+);",
            managed,
            "LibretroConstants.ApiVersion",
        ).group(1)
    )
    header_option_values = int(
        require_match(
            r"^#define\s+RETRO_NUM_CORE_OPTION_VALUES_MAX\s+(\d+)\s*$",
            header,
            "RETRO_NUM_CORE_OPTION_VALUES_MAX from libretro.h",
        ).group(1)
    )
    managed_option_values = int(
        require_match(
            r"public const int CoreOptionValuesMaximum = (\d+);",
            managed,
            "LibretroConstants.CoreOptionValuesMaximum",
        ).group(1)
    )
    if header_api != managed_api:
        fail(f"Managed API version {managed_api} does not match header value {header_api}.")
    if header_option_values != managed_option_values:
        fail(
            "Managed core-option value capacity "
            f"{managed_option_values} does not match header value {header_option_values}."
        )

    return revision


def validate_retroarch() -> str:
    revision = (
        REPOSITORY_ROOT / "eng/retroarch/VERSION"
    ).read_text(encoding="utf-8").strip()
    if HEX_COMMIT.fullmatch(revision) is None:
        fail("eng/retroarch/VERSION must contain one full lowercase commit hash.")

    patch = REPOSITORY_ROOT / "eng/retroarch/0001-netcmd-return-after-reinit.patch"
    patch_text = patch.read_text(encoding="utf-8")
    if "diff --git a/command.c b/command.c" not in patch_text or not patch_text.endswith("\n"):
        fail("The pinned RetroArch lifecycle patch is missing or malformed.")

    return revision


def main() -> int:
    try:
        sdk_baseline, selected_sdk = validate_sdk()
        libretro_revision = validate_libretro()
        retroarch_revision = validate_retroarch()
    except (OSError, ValueError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        print(f"compatibility pin validation failed: {error}", file=sys.stderr)
        return 1

    print("PASS: compatibility pins")
    print(f".NET SDK: baseline {sdk_baseline}, selected {selected_sdk}")
    print(f"libretro-common: {libretro_revision}")
    print(f"RetroArch: {retroarch_revision}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
