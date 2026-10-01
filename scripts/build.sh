#!/usr/bin/env bash
# ProcessPilot Pro - One-Click Build Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

echo "=================================================="
echo "      Building ProcessPilot Pro Engine & Driver   "
echo "=================================================="

mkdir -p build bin logs

# 1. Build Userspace
echo "[1/3] Compiling Userspace Binaries (C++17 / C)..."
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc 2>/dev/null || echo 2)
cd ..

# Copy binaries to top-level ./bin/
cp -f build/bin/* bin/ 2>/dev/null || cp -f build/* bin/ 2>/dev/null || true

# 2. Build Driver if kernel headers exist
echo "[2/3] Checking Linux Kernel Module build prerequisites..."
if [ -d "/lib/modules/$(uname -r)/build" ]; then
    echo "Found kernel build tree for Linux $(uname -r). Building pilot_driver.ko..."
    make -C src/driver
else
    echo "Kernel headers (/lib/modules/$(uname -r)/build) not found. Skipping .ko build."
fi

# 3. Run Unit Tests
echo "[3/3] Running Verification Unit Tests..."
./bin/test_dag
./bin/test_parser
./bin/test_healer

echo "=================================================="
echo " Build & Verification Succeeded!"
echo " Binaries located in: ${ROOT_DIR}/bin"
echo "=================================================="
