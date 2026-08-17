#!/usr/bin/env bash
#
# build_dependencies.sh - Install system packages and build the Thunder
# ecosystem (ThunderTools, Thunder, ThunderInterfaces) plus the mock
# dependency files required to build networkmanager for Coverity.
#
# This is the first half of the old cov_build.sh: everything that prepares
# the environment. The component itself is built by cov_build.sh, which is
# run afterwards from the same workspace.
#
# Usage:
#   ./build_dependencies.sh
#
# Override any of the environment variables below on the command line, e.g.:
#   THUNDER_REF=R4.4.3 ./build_dependencies.sh
#

# Re-exec under bash if started with a non-bash shell (e.g. `sh build_dependencies.sh`,
# where sh is dash). This script relies on bash features such as `pipefail`
# and ${BASH_SOURCE[0]}, which dash does not support.
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration (mirrors the workflow `env:` block)
# ---------------------------------------------------------------------------
BUILD_TYPE="${BUILD_TYPE:-Debug}"
THUNDER_REF="${THUNDER_REF:-R4.4.3}"
THUNDERTOOLS_REF="${THUNDERTOOLS_REF:-${THUNDER_REF}}"
INTERFACES_REF="${INTERFACES_REF:-${THUNDER_REF}}"

# Root workspace directory. In CI this is ${{ github.workspace }}.
# The Thunder repositories and install tree live inside the networkmanager
# checkout so the layout is self-contained and shared with cov_build.sh.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="${WORKSPACE:-${SCRIPT_DIR}}"
NETWORKMANAGER_DIR="${NETWORKMANAGER_DIR:-${SCRIPT_DIR}}"
INSTALL_DIR="${WORKSPACE}/install/usr"
MODULE_PATH="${WORKSPACE}/install/tools/cmake"

NPROC="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

# Use sudo only when it exists (CI build containers usually run as root and
# do not ship sudo). Otherwise run the command directly.
if command -v sudo >/dev/null 2>&1; then
  SUDO="sudo"
else
  SUDO=""
fi

# ---------------------------------------------------------------------------
# Install system packages
# ---------------------------------------------------------------------------
log "Installing system packages"
${SUDO} apt-get update
${SUDO} apt-get install -y \
  build-essential \
  cmake \
  pkg-config \
  libglib2.0-dev \
  libnm-dev \
  libcurl4-openssl-dev \
  lcov \
  ninja-build

# ---------------------------------------------------------------------------
# Install Python dependencies
# ---------------------------------------------------------------------------
log "Installing Python dependencies"
if command -v pip >/dev/null 2>&1; then
  pip install --break-system-packages jsonref || pip install jsonref || true
elif command -v pip3 >/dev/null 2>&1; then
  pip3 install --break-system-packages jsonref || pip3 install jsonref || true
else
  echo "pip not found; skipping jsonref install"
fi

# ---------------------------------------------------------------------------
# Clone Thunder repositories
# ---------------------------------------------------------------------------
clone_repo() {
  # clone_repo <url> <path> <ref>
  local url="$1" path="$2" ref="$3"
  if [ ! -d "${path}/.git" ]; then
    log "Cloning ${url} (${ref}) into ${path}"
    git clone --branch "${ref}" "${url}" "${path}"
  else
    log "Repository ${path} already present; fetching ${ref}"
    git -C "${path}" fetch origin "${ref}"
    git -C "${path}" checkout "${ref}"
  fi
}

clone_repo "https://github.com/rdkcentral/ThunderTools" \
  "${WORKSPACE}/ThunderTools" "${THUNDERTOOLS_REF}"
clone_repo "https://github.com/rdkcentral/Thunder" \
  "${WORKSPACE}/Thunder" "${THUNDER_REF}"
clone_repo "https://github.com/rdkcentral/ThunderInterfaces" \
  "${WORKSPACE}/ThunderInterfaces" "${INTERFACES_REF}"

# ---------------------------------------------------------------------------
# Apply Thunder SubscribeStub patch
# ---------------------------------------------------------------------------
log "Applying Thunder SubscribeStub patch"
(
  cd "${WORKSPACE}/Thunder"
  git apply "${NETWORKMANAGER_DIR}/tests/patches/thunder/SubscribeStub.patch" 2>/dev/null \
    || echo "Patch already applied or not needed"
)

# ---------------------------------------------------------------------------
# Build ThunderTools
# ---------------------------------------------------------------------------
log "Building ThunderTools"
cmake \
  -S "${WORKSPACE}/ThunderTools" \
  -B "${WORKSPACE}/build/ThunderTools" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DCMAKE_MODULE_PATH="${MODULE_PATH}" \
  -DGENERIC_CMAKE_MODULE_PATH="${MODULE_PATH}"
cmake --build "${WORKSPACE}/build/ThunderTools" --target install -j"${NPROC}"

# ---------------------------------------------------------------------------
# Build Thunder
# ---------------------------------------------------------------------------
log "Building Thunder"
cmake \
  -S "${WORKSPACE}/Thunder" \
  -B "${WORKSPACE}/build/Thunder" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DCMAKE_MODULE_PATH="${MODULE_PATH}" \
  -DBUILD_TYPE="${BUILD_TYPE}" \
  -DBINDING=127.0.0.1 \
  -DPORT=9998
cmake --build "${WORKSPACE}/build/Thunder" --target install -j"${NPROC}"

# ---------------------------------------------------------------------------
# Build ThunderInterfaces
# ---------------------------------------------------------------------------
log "Building ThunderInterfaces"
cmake \
  -S "${WORKSPACE}/ThunderInterfaces" \
  -B "${WORKSPACE}/build/ThunderInterfaces" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DCMAKE_MODULE_PATH="${MODULE_PATH}"
cmake --build "${WORKSPACE}/build/ThunderInterfaces" --target install -j"${NPROC}"

# ---------------------------------------------------------------------------
# Install IPowerManager header into the Thunder interfaces include path
# ---------------------------------------------------------------------------
log "Installing IPowerManager header"
IFACE_DIR="$(find "${INSTALL_DIR}/include" -maxdepth 2 -name "interfaces" -type d | head -1)"
if [ -z "${IFACE_DIR}" ] || [ ! -d "${IFACE_DIR}" ]; then
   echo "Error: Thunder interfaces include dir not found under ${INSTALL_DIR}/include" >&2
   exit 1
fi
cp "${NETWORKMANAGER_DIR}/tests/mocks/thunder/IPowerManager.h" "${IFACE_DIR}/"

# ---------------------------------------------------------------------------
# Generate dependency files
# ---------------------------------------------------------------------------
log "Generating /etc/device.properties"
${SUDO} tee /etc/device.properties >/dev/null <<'EOF'
ETHERNET_INTERFACE=eth0
WIFI_INTERFACE=wlan0
DEFAULT_HOSTNAME=rdk_test_device
EOF

# ---------------------------------------------------------------------------
# Generate IARM headers/stubs
# ---------------------------------------------------------------------------
log "Generating IARM headers"
mkdir -p "${INSTALL_DIR}/lib"
printf 'void __nm_cov_stub(void){}\n' | ${CC:-cc} -fPIC -shared -x c - -o "${INSTALL_DIR}/lib/libIARMBus.so"
printf 'void __nm_cov_stub(void){}\n' | ${CC:-cc} -fPIC -shared -x c - -o "${INSTALL_DIR}/lib/libmfrlib.so"
mkdir -p "${INSTALL_DIR}/include/rdk/iarmbus"
mkdir -p "${INSTALL_DIR}/include/rdk/iarmmgrs-hal"
touch "${INSTALL_DIR}/include/rdk/iarmbus/libIARM.h"
touch "${INSTALL_DIR}/include/rdk/iarmmgrs-hal/mfrMgr.h"
(
  cd "${NETWORKMANAGER_DIR}/tests"
  mkdir -p headers/rdk/iarmbus
  mkdir -p headers/rdk/iarmmgrs-hal
  touch headers/rdk/iarmmgrs-hal/mfrMgr.h
  touch headers/rdk/iarmbus/libIARM.h headers/rdk/iarmbus/libIBus.h
)

log "Dependencies ready. Run cov_build.sh to build networkmanager."
