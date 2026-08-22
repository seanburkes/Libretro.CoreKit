#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "$#" -eq 0 ]]; then
  echo "Usage: $0 CORE_ARTIFACT..." >&2
  exit 2
fi

python3 "${repo_root}/eng/check-glibc-baseline.py" "$@"
baseline_image="$(python3 "${repo_root}/eng/check-glibc-baseline.py" --print-image)"
glibc_floor="$(python3 "${repo_root}/eng/check-glibc-baseline.py" --print-floor)"

if command -v docker >/dev/null 2>&1; then
  container=(docker)
elif command -v podman >/dev/null 2>&1; then
  container=(podman --storage-opt ignore_chown_errors=true)
else
  echo "The glibc floor gate requires Docker or Podman." >&2
  exit 1
fi

container_glibc="$(
  "${container[@]}" run --rm --network none --platform linux/amd64 \
    "${baseline_image}" /usr/bin/getconf GNU_LIBC_VERSION
)"
if [[ "${container_glibc}" != "glibc ${glibc_floor}" ]]; then
  echo "Baseline image reported ${container_glibc}, expected glibc ${glibc_floor}." >&2
  exit 1
fi

for core in "$@"; do
  core_path="$(realpath "${core}")"
  if ! relocation_output="$(
    "${container[@]}" run --rm --network none --platform linux/amd64 \
      --volume "${core_path}:/core.so:ro,z" \
      "${baseline_image}" /usr/bin/ldd -r /core.so 2>&1
  )"; then
    echo "${relocation_output}" >&2
    echo "The glibc ${glibc_floor} loader rejected $(basename "${core_path}")." >&2
    exit 1
  fi
  if [[ "${relocation_output}" == *"not found"* || "${relocation_output}" == *"undefined symbol"* ]]; then
    echo "${relocation_output}" >&2
    echo "The glibc ${glibc_floor} loader could not relocate $(basename "${core_path}")." >&2
    exit 1
  fi
  echo "PASS: glibc ${glibc_floor} relocated $(basename "${core_path}")"
done
