#!/usr/bin/env bash
#
# cov_build.sh - Coverity-friendly build of networkmanager with the Gnome
# libnm proxy.
#
# This is the second half of the original cov_build.sh: it builds only the
# networkmanager component. The Thunder ecosystem and mock dependency files
# it relies on are produced by build_dependencies.sh, which must be run first
# from the same workspace.
#
# Usage:
#   ./build_dependencies.sh
#   ./cov_build.sh
#
# Override any of the environment variables below on the command line, e.g.:
#   THUNDER_REF=R4.4.3 ./cov_build.sh
#

# Re-exec under bash if started with a non-bash shell (e.g. `sh cov_build.sh`,
# where sh is dash). This script relies on bash features such as `pipefail`
# and ${BASH_SOURCE[0]}, which dash does not support.
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration (must match build_dependencies.sh so the paths line up)
# ---------------------------------------------------------------------------
# Optional cross-compile toolchain file (empty for native builds).
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-}"

# Root workspace directory. In CI this is ${{ github.workspace }}.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="${WORKSPACE:-${SCRIPT_DIR}}"
NETWORKMANAGER_DIR="${NETWORKMANAGER_DIR:-${SCRIPT_DIR}}"
INSTALL_DIR="${WORKSPACE}/install/usr"
MODULE_PATH="${WORKSPACE}/install/tools/cmake"

NPROC="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

# ---------------------------------------------------------------------------
# Build networkmanager with Gnome libnm Proxy
# ---------------------------------------------------------------------------
log "Building networkmanager with Gnome libnm Proxy"
NM_CXX_FLAGS=" -fprofile-arcs -ftest-coverage \
-I ${NETWORKMANAGER_DIR}/tests/headers \
-I ${NETWORKMANAGER_DIR}/tests/headers/rdk/iarmbus \
-I ${NETWORKMANAGER_DIR}/tests/headers/rdk/iarmmgrs-hal \
--include ${NETWORKMANAGER_DIR}/tests/mocks/Iarm.h \
--include ${NETWORKMANAGER_DIR}/tests/mocks/mfrMgr.h "

TOOLCHAIN_ARG=()
if [ -n "${TOOLCHAIN_FILE}" ]; then
  TOOLCHAIN_ARG=(-DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}")
fi

cmake \
  -S "${NETWORKMANAGER_DIR}" \
  -B "${WORKSPACE}/build/networkmanager_libnm" \
  "${TOOLCHAIN_ARG[@]}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DCMAKE_MODULE_PATH="${MODULE_PATH}" \
  -DCMAKE_CXX_FLAGS="${NM_CXX_FLAGS}" \
  -DENABLE_GNOME_NETWORKMANAGER=ON \
  -DENABLE_LEGACY_PLUGINS=OFF \
  -DENABLE_UNIT_TESTING=ON \
  -DENABLE_PLUGIN_CLI=OFF \
  -DENABLE_MIGRATION_MFRMGR_SUPPORT=ON
cmake --build "${WORKSPACE}/build/networkmanager_libnm" --target install -j"${NPROC}"
