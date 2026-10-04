#!/bin/bash
set -e

if [ "$EUID" -ne 0 ]; then
    echo "Error: scripts/stress_load_unload.sh must be run with sudo/root privileges." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DRIVER_DIR="$REPO_ROOT/driver"

if [ ! -f "$DRIVER_DIR/drivesense.ko" ]; then
    echo "==> Building kernel driver..."
    make -C "$DRIVER_DIR"
fi

echo "=========================================================="
echo " Starting DriveSense 20x Driver Stress Load/Unload Cycle  "
echo "=========================================================="

# Unload first if currently loaded
if lsmod | grep -q "^drivesense\b"; then
    rmmod drivesense
fi

# Record current kernel log position
DMESG_START_LINES=$(dmesg | wc -l)

CYCLES=20
for i in $(seq 1 $CYCLES); do
    echo -n "[Cycle $i/$CYCLES] Loading... "
    insmod "$DRIVER_DIR/drivesense.ko"
    
    # Wait for device node
    WAIT=0
    while [ ! -e /dev/drivesense ]; do
        sleep 0.02
        WAIT=$((WAIT + 1))
        if [ $WAIT -gt 50 ]; then
            echo "FAILED: /dev/drivesense did not appear"
            exit 1
        fi
    done
    chmod 666 /dev/drivesense

    # Quick sanity check on procfs
    if [ ! -f /proc/drivesense ]; then
        echo "FAILED: /proc/drivesense missing"
        exit 1
    fi

    # Small delay for timer tick
    sleep 0.05

    echo -n "Unloading... "
    rmmod drivesense
    
    if [ -e /dev/drivesense ]; then
        echo "FAILED: /dev/drivesense remained after rmmod"
        exit 1
    fi
    echo "OK"
done

echo "=========================================================="
echo " Checking dmesg for kernel anomalies..."
DMESG_DIFF=$(dmesg | tail -n "+$((DMESG_START_LINES + 1))")

# Check specifically for driver-related bugs or critical kernel alerts
if echo "$DMESG_DIFF" | grep -Ei "drivesense.*(BUG|Oops|WARNING|Call Trace)" >/dev/null; then
    echo "ERROR: Kernel warnings or faults detected during stress cycle:"
    echo "$DMESG_DIFF" | grep -Ei "drivesense.*(BUG|Oops|WARNING|Call Trace)"
    exit 1
fi

echo "PASS: 20/20 load/unload cycles completed with zero kernel warnings or faults."
echo "=========================================================="
