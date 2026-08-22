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

mkdir -p "${output_dir}"
dotnet restore "${repo_root}/src/Libretro.Core/Libretro.Core.csproj" \
  --locked-mode \
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

echo "PASS: local package consumer restored, built, and ran"
