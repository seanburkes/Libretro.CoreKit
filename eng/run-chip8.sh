#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rid="${COREKIT_RID:-linux-x64}"
cycles="${COREKIT_CHIP8_CYCLES:-25}"
sanitizers="${COREKIT_CHIP8_SANITIZERS:-ON}"
publish_dir="${repo_root}/artifacts/phase-4/${rid}/core"
host_build_dir="${repo_root}/artifacts/phase-4/${rid}/native-host"

case "${rid}" in
  linux-*) core_name="corekit_chip8_libretro.so" ;;
  *) echo "The Phase 4 slice supports Linux RIDs, not ${rid}" >&2; exit 2 ;;
esac

(cd "${repo_root}/eng/libretro" && sha256sum -c SHA256)

dotnet publish \
  "${repo_root}/src/Libretro.NativeAot.Chip8/Libretro.NativeAot.Chip8.csproj" \
  --configuration Release \
  --runtime "${rid}" \
  --output "${publish_dir}"

core_path="${publish_dir}/${core_name}"
if [[ ! -f "${core_path}" ]]; then
  echo "Native CHIP-8 core was not published at ${core_path}" >&2
  exit 1
fi

cmake \
  -S "${repo_root}/tests/Libretro.Chip8.NativeHost" \
  -B "${host_build_dir}" \
  -DCORE_PATH="${core_path}" \
  -DCORE_CYCLES="${cycles}" \
  -DCORE_RESULT_PATH="${host_build_dir}/phase-4-result.json" \
  -DCORE_ENABLE_SANITIZERS="${sanitizers}" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${host_build_dir}" --config Release
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir "${host_build_dir}" --build-config Release --output-on-failure

if command -v nm >/dev/null 2>&1; then
  nm -D --defined-only "${core_path}" | awk '$3 ~ /^retro_/ { print $3 }' | sort
fi
