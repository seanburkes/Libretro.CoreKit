#!/usr/bin/env python3
"""Verify the initial Libretro.Core NuGet and symbol package contract."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import zipfile
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
PROJECT = "src/Libretro.Core/Libretro.Core.csproj"
NUSPEC_NAMESPACE = "http://schemas.microsoft.com/packaging/2012/06/nuspec.xsd"
PACKAGE_PROPERTIES = (
    "PackageId",
    "PackageVersion",
    "TargetFramework",
    "PackageLicenseExpression",
    "RepositoryUrl",
    "PackageReadmeFile",
    "IsPackable",
    "IncludeSymbols",
    "SymbolPackageFormat",
    "EnablePackageValidation",
    "PackageValidationBaselineVersion",
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


def properties() -> dict[str, str]:
    result = json.loads(
        run(
            "dotnet",
            "msbuild",
            PROJECT,
            "-nologo",
            "-v:q",
            f"-getProperty:{','.join(PACKAGE_PROPERTIES)}",
        )
    )["Properties"]
    expected = {
        "PackageId": "Libretro.Core",
        "TargetFramework": "net10.0",
        "PackageLicenseExpression": "MIT",
        "RepositoryUrl": "https://github.com/seanburkes/Libretro.CoreKit",
        "PackageReadmeFile": "README.md",
        "IsPackable": "true",
        "IncludeSymbols": "true",
        "SymbolPackageFormat": "snupkg",
        "EnablePackageValidation": "true",
        "PackageValidationBaselineVersion": "",
    }
    if any(result.get(name) != value for name, value in expected.items()):
        fail(f"Unexpected managed package properties: {result!r}")
    if re.fullmatch(r"0\.\d+\.\d+-preview\.\d+", result["PackageVersion"]) is None:
        fail(f"The Phase 5 package must use a 0.x preview version: {result['PackageVersion']}")
    return result


def metadata(archive: zipfile.ZipFile, nuspec: str) -> ElementTree.Element:
    document = ElementTree.fromstring(archive.read(nuspec))
    value = document.find(f"{{{NUSPEC_NAMESPACE}}}metadata")
    if value is None:
        fail(f"{nuspec} has no package metadata.")
    return value


def text(metadata: ElementTree.Element, name: str) -> str:
    value = metadata.findtext(f"{{{NUSPEC_NAMESPACE}}}{name}")
    if value is None:
        fail(f"NuGet metadata is missing {name}.")
    return value


def validate_main(path: Path, project: dict[str, str], revision: str) -> None:
    with zipfile.ZipFile(path, "r") as archive:
        names = archive.namelist()
        fixed = {
            "_rels/.rels",
            "Libretro.Core.nuspec",
            "lib/net10.0/Libretro.Core.dll",
            "native/libretro.h",
            "native/libretro_log_shim.c",
            "README.md",
            "[Content_Types].xml",
        }
        dynamic = [
            name
            for name in names
            if re.fullmatch(
                r"package/services/metadata/core-properties/[0-9a-f]{32}\.psmdcp",
                name,
            )
        ]
        if set(names) != fixed | set(dynamic) or len(dynamic) != 1:
            fail(f"Unexpected files in {path.name}: {names!r}")
        if archive.read("README.md") != (ROOT / "src/Libretro.Core/PACKAGE.md").read_bytes():
            fail("The packaged README does not match src/Libretro.Core/PACKAGE.md.")
        native_assets = {
            "native/libretro.h": ROOT / "eng/libretro/libretro.h",
            "native/libretro_log_shim.c": ROOT / "src/Libretro.Core/Native/libretro_log_shim.c",
        }
        for name, source in native_assets.items():
            if archive.read(name) != source.read_bytes():
                fail(f"The packaged {name} does not match {source.relative_to(ROOT)}.")

        package = metadata(archive, "Libretro.Core.nuspec")
        expected = {
            "id": project["PackageId"],
            "version": project["PackageVersion"],
            "authors": "Sean Burkes",
            "description": "Reusable, strongly typed .NET hosting primitives for NativeAOT libretro cores.",
            "readme": "README.md",
        }
        if any(text(package, name) != value for name, value in expected.items()):
            fail("The NuGet metadata does not match the managed package contract.")

        license_node = package.find(f"{{{NUSPEC_NAMESPACE}}}license")
        repository = package.find(f"{{{NUSPEC_NAMESPACE}}}repository")
        if license_node is None or license_node.attrib != {"type": "expression"}:
            fail("The package must use an SPDX license expression.")
        if license_node.text != project["PackageLicenseExpression"]:
            fail("The package license expression is incorrect.")
        if repository is None or repository.attrib != {
            "type": "git",
            "url": project["RepositoryUrl"],
            "commit": revision,
        }:
            fail("The package repository identity is incorrect.")

        dependencies = package.find(f"{{{NUSPEC_NAMESPACE}}}dependencies")
        groups = [] if dependencies is None else list(dependencies)
        if len(groups) != 1 or groups[0].attrib != {"targetFramework": "net10.0"}:
            fail("The package must contain one net10.0 dependency group.")
        if list(groups[0]):
            fail("Libretro.Core must not expose runtime package dependencies.")


def validate_symbols(path: Path, project: dict[str, str]) -> None:
    with zipfile.ZipFile(path, "r") as archive:
        names = archive.namelist()
        fixed = {
            "_rels/.rels",
            "Libretro.Core.nuspec",
            "lib/net10.0/Libretro.Core.pdb",
            "[Content_Types].xml",
        }
        dynamic = [
            name
            for name in names
            if re.fullmatch(
                r"package/services/metadata/core-properties/[0-9a-f]{32}\.psmdcp",
                name,
            )
        ]
        if set(names) != fixed | set(dynamic) or len(dynamic) != 1:
            fail(f"Unexpected files in {path.name}: {names!r}")
        package = metadata(archive, "Libretro.Core.nuspec")
        if text(package, "id") != project["PackageId"] or text(
            package, "version"
        ) != project["PackageVersion"]:
            fail("The symbol package identity does not match the main package.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("symbols", type=Path)
    arguments = parser.parse_args()

    try:
        project = properties()
        expected_stem = f"{project['PackageId']}.{project['PackageVersion']}"
        package = arguments.package.resolve()
        symbols = arguments.symbols.resolve()
        if package.name != f"{expected_stem}.nupkg" or not package.is_file():
            fail(f"Expected {expected_stem}.nupkg, got {package}.")
        if symbols.name != f"{expected_stem}.snupkg" or not symbols.is_file():
            fail(f"Expected {expected_stem}.snupkg, got {symbols}.")
        revision = run("git", "rev-parse", "HEAD")
        validate_main(package, project, revision)
        validate_symbols(symbols, project)
    except (
        ElementTree.ParseError,
        json.JSONDecodeError,
        KeyError,
        OSError,
        RuntimeError,
        subprocess.CalledProcessError,
        zipfile.BadZipFile,
    ) as error:
        print(f"managed package check failed: {error}", file=sys.stderr)
        return 1

    print(f"PASS: {project['PackageId']} {project['PackageVersion']} package contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
