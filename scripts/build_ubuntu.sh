#!/usr/bin/env bash
# Build with the Qt SDK already installed on the Ubuntu development machine.
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: bash scripts/build_ubuntu.sh <Qt-kit-directory> [build-directory]

Example: bash scripts/build_ubuntu.sh "$HOME/Qt/6.12.0/gcc_64"
Builds Debug with Qt Quick and Network; optional audio stays disabled.
Uses the existing compiler and CMake on PATH. Installs/downloads nothing.
SUNO_BUILD_JOBS controls parallel compilation (default: 2).
For an ARM64 cross-build use README_UBUNTU.md with your board's SDK.
USAGE
}

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    usage
    exit 0
fi
if (( $# < 1 || $# > 2 )); then
    usage >&2
    exit 2
fi
if [[ $(uname -s) != Linux ]]; then
    echo 'Run this helper on Ubuntu/Linux in the approved Qt environment.' >&2
    exit 2
fi
if [[ -n ${SDKTARGETSYSROOT:-} || -n ${OECORE_TARGET_SYSROOT:-} || -n ${CMAKE_TOOLCHAIN_FILE:-} ]]; then
    echo 'A cross-build environment is active. Use a fresh native terminal or the SDK instructions in README_UBUNTU.md.' >&2
    exit 2
fi
command -v cmake >/dev/null || { echo 'CMake 3.21+ must be available on PATH.' >&2; exit 2; }
[[ -d $1 ]] || { echo "Qt kit directory does not exist: $1" >&2; exit 2; }
qt_prefix=$(cd -- "$1" && pwd -P)
source_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
build_dir=${2:-"$source_root/build/ubuntu-debug"}
jobs=${SUNO_BUILD_JOBS:-2}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || { echo 'SUNO_BUILD_JOBS must be a positive integer.' >&2; exit 2; }

cmake -S "$source_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DQt6_ROOT="$qt_prefix" \
    -DCMAKE_PREFIX_PATH="$qt_prefix" \
    -DSUNO_LANXI_AUDIO=OFF
cmake --build "$build_dir" --parallel "$jobs"
printf '\nBuild complete. Desktop preview:\n  %q --windowed\n' "$build_dir/appFRDM_IMX95_QT_DSW"
