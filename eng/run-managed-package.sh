#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="${COREKIT_MANAGED_PACKAGE_OUTPUT:-${repo_root}/artifacts/managed-package}"
work_root="$(mktemp -d "${TMPDIR:-/tmp}/corekit-managed-package.XXXXXX")"

cleanup() {
  rm -rf -- "${work_root}"
}
trap cleanup EXIT

if [[ "${COREKIT_MANAGED_PACKAGE_ALLOW_DIRTY:-0}" != "1" ]] &&
   [[ -n "$(git -C "${repo_root}" status --porcelain --untracked-files=normal)" ]]; then
  echo "Managed packages require a clean source tree; use COREKIT_MANAGED_PACKAGE_ALLOW_DIRTY=1 only for local validation." >&2
  exit 1
fi

package_version="$(
  dotnet msbuild "${repo_root}/src/Libretro.Core/Libretro.Core.csproj" \
    -nologo -v:q -getProperty:PackageVersion
)"
package="${output_dir}/Libretro.Core.${package_version}.nupkg"
symbols="${output_dir}/Libretro.Core.${package_version}.snupkg"
framework_packages="${work_root}/framework-packages"
lock_file="${repo_root}/eng/managed-package.packages.lock.json"

mkdir -p "${output_dir}"
dotnet restore "${repo_root}/src/Libretro.Core/Libretro.Core.csproj" \
  --locked-mode \
  --packages "${framework_packages}" \
  -p:DisableImplicitLibraryPacksFolder=true \
  -p:RestorePackagesWithLockFile=true \
  -p:NuGetLockFilePath="${lock_file}" \
  --disable-build-servers
dotnet pack "${repo_root}/src/Libretro.Core/Libretro.Core.csproj" \
  --configuration Release \
  --no-restore \
  --output "${output_dir}" \
  --disable-build-servers \
  -m:1

python3 "${repo_root}/eng/check-managed-package.py" "${package}" "${symbols}"

consumer="${repo_root}/tests/Libretro.Core.PackageConsumer/Libretro.Core.PackageConsumer.csproj"
consumer_obj="${work_root}/consumer-obj/"
dotnet restore "${consumer}" \
  --source "${output_dir}" \
  --packages "${work_root}/packages" \
  --disable-build-servers \
  -p:BaseIntermediateOutputPath="${consumer_obj}"
dotnet build "${consumer}" \
  --configuration Release \
  --no-restore \
  --output "${work_root}/consumer" \
  --disable-build-servers \
  -m:1 \
  -p:BaseIntermediateOutputPath="${consumer_obj}"
dotnet "${work_root}/consumer/Libretro.Core.PackageConsumer.dll"

native_consumer="${repo_root}/tests/Libretro.Core.NativePackageConsumer/Libretro.Core.NativePackageConsumer.csproj"
native_packages="${work_root}/native-packages"
native_publish="${work_root}/native-consumer"
native_host="${work_root}/native-host"
dotnet publish "${native_consumer}" \
  --configuration Release \
  --runtime linux-x64 \
  --source "${output_dir}" \
  --source https://api.nuget.org/v3/index.json \
  --packages "${native_packages}" \
  --output "${native_publish}" \
  --disable-build-servers \
  -m:1 \
  -p:DisableImplicitLibraryPacksFolder=true

restored_package="${native_packages}/libretro.core/${package_version}/libretro.core.${package_version}.nupkg"
if ! cmp -s "${package}" "${restored_package}"; then
  echo "The native consumer did not restore the produced Libretro.Core package." >&2
  exit 1
fi

native_core="${native_publish}/corekit_package_consumer_libretro.so"
if [[ ! -f "${native_core}" ]]; then
  echo "The package-backed NativeAOT core was not published at ${native_core}." >&2
  exit 1
fi

cmake \
  -S "${repo_root}/tests/Libretro.NativeHost" \
  -B "${native_host}" \
  -DCORE_PATH="${native_core}" \
  -DCORE_CYCLES=1 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${native_host}" --config Release
ctest \
  --test-dir "${native_host}" \
  --build-config Release \
  --output-on-failure

echo "PASS: managed and NativeAOT consumers used the produced package"
