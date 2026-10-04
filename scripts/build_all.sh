#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================================="
echo "            Building DriveSense Subsystems                "
echo "=========================================================="

echo "==> 1. Compiling Linux Kernel Driver..."
make -C "$REPO_ROOT/driver"

echo "==> 2. Configuring and Building C++ Application & Tests..."
mkdir -p "$REPO_ROOT/build"
cd "$REPO_ROOT/build"
cmake ..
make -j4

echo "=========================================================="
echo " Build Succeeded: All driver and userland targets ready.  "
echo "=========================================================="
