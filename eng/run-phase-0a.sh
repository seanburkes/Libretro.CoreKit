#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rid="${COREKIT_RID:-linux-x64}"
cycles="${COREKIT_STRESS_CYCLES:-25}"
publish_dir="${repo_root}/artifacts/phase-0/${rid}/core"
host_build_dir="${repo_root}/artifacts/phase-0/${rid}/native-host"

case "${rid}" in
  linux-*) core_name="corekit_probe_libretro.so" ;;
  *) echo "Stage 0A currently supports Linux RIDs, not ${rid}" >&2; exit 2 ;;
esac

(cd "${repo_root}/eng/libretro" && sha256sum -c SHA256)

dotnet publish \
  "${repo_root}/src/Libretro.NativeAot.Probe/Libretro.NativeAot.Probe.csproj" \
  --configuration Release \
  --runtime "${rid}" \
  --output "${publish_dir}"

core_path="${publish_dir}/${core_name}"
if [[ ! -f "${core_path}" ]]; then
  echo "Native core was not published at ${core_path}" >&2
  exit 1
fi

cmake \
  -S "${repo_root}/tests/Libretro.NativeHost" \
  -B "${host_build_dir}" \
  -DCORE_PATH="${core_path}" \
  -DCORE_CYCLES="${cycles}" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${host_build_dir}" --config Release
ctest --test-dir "${host_build_dir}" --build-config Release --output-on-failure

if command -v nm >/dev/null 2>&1; then
  nm -D --defined-only "${core_path}" | awk '$3 ~ /^retro_/ { print $3 }' | sort
fi
