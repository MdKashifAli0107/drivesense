#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================================="
echo "          DRIVESENSE AUTOMATED TEST HARNESS               "
echo "=========================================================="

cd "$REPO_ROOT"

# Step 1: Build everything from scratch
echo "==> Step 1: Building all components..."
./scripts/build_all.sh

# Step 2: Run Unit Tests
echo -e "\n==> Step 2: Executing C++ Unit Tests (AlertManager, Logger)..."
./build/tests/unit_tests

# Step 3: Verify Friendly Error when Driver is Unloaded
echo -e "\n==> Step 3: Verifying graceful error handling without driver..."
if lsmod | grep -q "^drivesense\b"; then
    sudo ./scripts/unload.sh >/dev/null
fi

set +e
ERR_OUT=$(./build/app/drivesense_app --once 2>&1)
ERR_CODE=$?
set -e

if [ $ERR_CODE -ne 0 ] && echo "$ERR_OUT" | grep -q "Please load the driver first"; then
    echo "[PASS] Application cleanly handles unloaded driver with helpful message."
else
    echo "[FAIL] Expected friendly failure message, got exit code $ERR_CODE: $ERR_OUT"
    exit 1
fi

# Step 4: Load Driver and Run Integration Tests
echo -e "\n==> Step 4: Loading kernel driver for Integration Testing..."
sudo ./scripts/load.sh

echo "==> Running Integration Tests against live /dev/drivesense..."
./build/tests/integration_test

echo "==> Unloading kernel driver..."
sudo ./scripts/unload.sh

# Step 5: Run 20x Stress Load/Unload Cycle
echo -e "\n==> Step 5: Running 20x Stress Load/Unload verification..."
sudo ./scripts/stress_load_unload.sh

# Step 6: Static Analysis via Cppcheck
echo -e "\n==> Step 6: Running Cppcheck static code analysis..."
cppcheck --enable=warning,style,performance --error-exitcode=1 --std=c++17 app tests

# Step 7: Optional Valgrind Memory Safety Check on Unit Tests
if command -v valgrind >/dev/null 2>&1; then
    echo -e "\n==> Step 7: Running Valgrind memory leak verification on Unit Tests..."
    valgrind --leak-check=full --error-exitcode=1 ./build/tests/unit_tests >/dev/null 2>&1
    echo "[PASS] Valgrind reports ZERO memory leaks."
fi

echo -e "\n=========================================================="
echo "    ALL TEST SUITES PASSED (100% SUCCESSFUL)              "
echo "=========================================================="
