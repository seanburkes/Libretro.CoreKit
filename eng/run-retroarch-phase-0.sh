#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cycles="${COREKIT_RETROARCH_CYCLES:-50}"
rss_limit="${COREKIT_RETROARCH_RSS_LIMIT_MIB:-16}"
ra_commit="$(tr -d '[:space:]' < "${repo_root}/eng/retroarch/VERSION")"
ra_patch="${repo_root}/eng/retroarch/0001-netcmd-return-after-reinit.patch"
ra_root="${repo_root}/artifacts/retroarch/${ra_commit}"
ra_source="${ra_root}/source"
ra_binary="${RETROARCH_BINARY:-${ra_source}/retroarch}"
core_path="${repo_root}/artifacts/phase-0/linux-x64/core/corekit_probe_libretro.so"
control_path="${repo_root}/artifacts/phase-0/linux-x64/control/control_libretro.so"
profile_root="${ra_root}/profile"
installed_core="${profile_root}/cores/corekit_probe_libretro.so"
installed_control="${profile_root}/cores/control_libretro.so"
log_path="${ra_root}/lifecycle.log"
summary_path="${ra_root}/lifecycle-summary.log"
state_path="${profile_root}/config/retroarch/states/CoreKit NativeAOT Probe/CoreKit NativeAOT Probe.state"
save_path="${profile_root}/config/retroarch/saves/CoreKit NativeAOT Probe/CoreKit NativeAOT Probe.srm"
config_path="${COREKIT_RETROARCH_CONFIG:-${repo_root}/tests/RetroArch/stress.cfg}"

if [[ -z "${RETROARCH_BINARY:-}" ]]; then
  mkdir -p "${ra_root}"
  if [[ ! -d "${ra_source}/.git" ]]; then
    git clone --filter=blob:none --no-checkout \
      https://github.com/libretro/RetroArch.git "${ra_source}"
  fi
  git -C "${ra_source}" fetch --depth=1 origin "${ra_commit}"
  git -C "${ra_source}" checkout --detach "${ra_commit}"
  if git -C "${ra_source}" apply --check "${ra_patch}" 2>/dev/null; then
    git -C "${ra_source}" apply "${ra_patch}"
  elif ! git -C "${ra_source}" apply --reverse --check "${ra_patch}" 2>/dev/null; then
    echo "RetroArch command-interface patch does not apply cleanly" >&2
    exit 1
  fi
  if [[ ! -x "${ra_binary}" ]] || \
      [[ "${ra_source}/command.c" -nt "${ra_binary}" ]]; then
    (
      cd "${ra_source}"
      ./configure --enable-command --disable-qt --disable-ffmpeg \
        --disable-mpv --disable-discord --disable-caca --disable-sixel
      make -j"$(getconf _NPROCESSORS_ONLN)"
    )
  fi
fi

if [[ ! -x "${ra_binary}" ]]; then
  echo "RetroArch binary is not executable: ${ra_binary}" >&2
  exit 1
fi

COREKIT_STRESS_CYCLES="${COREKIT_NATIVE_CYCLES:-25}" \
  "${repo_root}/eng/run-phase-0a.sh"

mkdir -p "$(dirname "${control_path}")" "${profile_root}/config" \
  "${profile_root}/cache" "${profile_root}/data" \
  "${profile_root}/cores" "${profile_root}/info"
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -fPIC -shared \
  -I "${repo_root}/eng/libretro" \
  "${repo_root}/tests/RetroArch/conventional_core.c" \
  -o "${control_path}"
cp "${core_path}" "${installed_core}"
cp "${control_path}" "${installed_control}"
cp "${repo_root}/tests/RetroArch/corekit_probe_libretro.info" \
  "${profile_root}/info/corekit_probe_libretro.info"

XDG_CONFIG_HOME="${profile_root}/config" \
XDG_CACHE_HOME="${profile_root}/cache" \
XDG_DATA_HOME="${profile_root}/data" \
SDL_AUDIODRIVER=dummy \
SDL_VIDEODRIVER=x11 \
python3 "${repo_root}/tests/RetroArch/lifecycle.py" \
  --binary "${ra_binary}" \
  --config "${config_path}" \
  --managed-core "${installed_core}" \
  --control-core "${installed_control}" \
  --cycles "${cycles}" \
  --rss-limit-mib "${rss_limit}" \
  --state "${state_path}" \
  --save "${save_path}" \
  --log "${log_path}" | tee "${summary_path}"
