#!/bin/bash
set -e

if [ "$EUID" -ne 0 ]; then
    echo "Error: scripts/unload.sh must be run with sudo/root privileges." >&2
    exit 1
fi

if lsmod | grep -q "^drivesense\b"; then
    echo "==> Unloading drivesense module..."
    rmmod drivesense
    echo "==> Module unloaded."
else
    echo "==> drivesense module is not currently loaded."
fi

echo "==> Kernel log (dmesg):"
dmesg | tail -n 5
