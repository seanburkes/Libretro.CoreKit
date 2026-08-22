#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rid="${COREKIT_RELEASE_RID:-linux-x64}"
release_dir="${COREKIT_RELEASE_OUTPUT:-${repo_root}/artifacts/release/${rid}}"
work_root="$(mktemp -d "${TMPDIR:-/tmp}/corekit-release.XXXXXX")"

cleanup() {
  rm -rf -- "${work_root}"
}
trap cleanup EXIT

case "${rid}" in
  linux-x64) ;;
  *) echo "The Phase 5 release slice supports linux-x64, not ${rid}" >&2; exit 2 ;;
esac

probe_dir="${work_root}/probe"
chip8_dir="${work_root}/chip8"
repeat_dir="${work_root}/repeat"
probe_host_dir="${work_root}/probe-host"
chip8_host_dir="${work_root}/chip8-host"

python3 "${repo_root}/eng/check-compatibility-pins.py"

dotnet publish \
  "${repo_root}/src/Libretro.NativeAot.Probe/Libretro.NativeAot.Probe.csproj" \
  --configuration Release \
  --runtime "${rid}" \
  --output "${probe_dir}" \
  -p:StripSymbols=true \
  --disable-build-servers

cmake \
  -S "${repo_root}/tests/Libretro.NativeHost" \
  -B "${probe_host_dir}" \
  -DCORE_PATH="${probe_dir}/corekit_probe_libretro.so" \
  -DCORE_CYCLES=1 \
  -DCORE_MAX_RSS_GROWTH_MIB=16 \
  -DCORE_RESULT_PATH="${probe_host_dir}/release-result.json" \
  -DCORE_ENABLE_SANITIZERS=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${probe_host_dir}" --config Release
ctest --test-dir "${probe_host_dir}" --build-config Release --output-on-failure

dotnet publish \
  "${repo_root}/src/Libretro.NativeAot.Chip8/Libretro.NativeAot.Chip8.csproj" \
  --configuration Release \
  --runtime "${rid}" \
  --output "${chip8_dir}" \
  -p:StripSymbols=true \
  --disable-build-servers

cmake \
  -S "${repo_root}/tests/Libretro.Chip8.NativeHost" \
  -B "${chip8_host_dir}" \
  -DCORE_PATH="${chip8_dir}/corekit_chip8_libretro.so" \
  -DCORE_CYCLES=1 \
  -DCORE_RESULT_PATH="${chip8_host_dir}/release-result.json" \
  -DCORE_ENABLE_SANITIZERS=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${chip8_host_dir}" --config Release
ctest --test-dir "${chip8_host_dir}" --build-config Release --output-on-failure

package_arguments=(
  --rid "${rid}"
  --probe-dir "${probe_dir}"
  --chip8-dir "${chip8_dir}"
)
if [[ "${COREKIT_RELEASE_ALLOW_DIRTY:-0}" == "1" ]]; then
  package_arguments+=(--allow-dirty)
fi

python3 "${repo_root}/eng/package-release.py" \
  "${package_arguments[@]}" \
  --output "${release_dir}"
python3 "${repo_root}/eng/package-release.py" \
  "${package_arguments[@]}" \
  --output "${repeat_dir}"

for artifact in \
  corekit_probe_libretro-linux-x64.zip \
  corekit_chip8_libretro-linux-x64.zip \
  SHA256SUMS; do
  if ! cmp -s "${release_dir}/${artifact}" "${repeat_dir}/${artifact}"; then
    echo "Release output is not reproducible: ${artifact}" >&2
    exit 1
  fi
done

(cd "${release_dir}" && sha256sum -c SHA256SUMS)
echo "PASS: repeated packaging produced byte-identical release files"
