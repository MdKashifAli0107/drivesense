#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Ensure kernel module is loaded
if [ ! -e /dev/drivesense ]; then
    echo "==> /dev/drivesense not found. Loading driver..."
    sudo "$SCRIPT_DIR/load.sh"
fi

# Ensure binaries are built
if [ ! -f "$REPO_ROOT/build/drivesense_web" ]; then
    echo "==> Building drivesense_web..."
    "$SCRIPT_DIR/build_all.sh"
fi

# Free port 8080 if already in use
fuser -k 8080/tcp 2>/dev/null || true

# Get host/VM IP
VM_IP=$(ip route get 1.1.1.1 2>/dev/null | awk '{print $7}' || hostname -I | awk '{print $1}')
if [ -z "$VM_IP" ]; then
    VM_IP="192.168.1.3"
fi

echo "=========================================================="
echo "   DRIVESENSE -- Live Browser Dashboard & Telemetry"
echo "=========================================================="
echo "Kernel Module:   /dev/drivesense (Active)"
echo "Web Server Port: 8080"
echo ""
echo "Open this URL in Chrome, Edge, Firefox, or Brave:"
echo "👉  http://${VM_IP}:8080"
echo "=========================================================="
echo "Press Ctrl+C to terminate the web server."
echo ""

exec "$REPO_ROOT/build/drivesense_web" --port 8080
