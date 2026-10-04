#!/bin/bash
set -e

if [ "$EUID" -ne 0 ]; then
    echo "Error: scripts/load.sh must be run with sudo/root privileges." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DRIVER_DIR="$REPO_ROOT/driver"

# Build driver if .ko is missing
if [ ! -f "$DRIVER_DIR/drivesense.ko" ]; then
    echo "==> Building kernel driver..."
    make -C "$DRIVER_DIR"
fi

# Remove existing module if already loaded
if lsmod | grep -q "^drivesense\b"; then
    echo "==> Removing existing drivesense module..."
    rmmod drivesense
fi

# Insert the module
echo "==> Inserting drivesense.ko..."
insmod "$DRIVER_DIR/drivesense.ko"

# Wait for device node creation
COUNT=0
while [ ! -e /dev/drivesense ]; do
    sleep 0.1
    COUNT=$((COUNT + 1))
    if [ $COUNT -gt 50 ]; then
        echo "Error: /dev/drivesense failed to appear within 5 seconds." >&2
        exit 1
    fi
done

# Set permissions so unprivileged users and app can read/write
chmod 666 /dev/drivesense

echo "==> DriveSense driver loaded successfully:"
ls -l /dev/drivesense
echo "==> Kernel log (dmesg):"
dmesg | tail -n 5
