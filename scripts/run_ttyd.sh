#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ ! -e /dev/drivesense ]; then
    echo "==> Loading driver..."
    sudo "$SCRIPT_DIR/load.sh"
fi

if [ ! -f "$REPO_ROOT/build/drivesense_app" ]; then
    "$SCRIPT_DIR/build_all.sh"
fi

fuser -k 8081/tcp 2>/dev/null || true

VM_IP=$(ip route get 1.1.1.1 2>/dev/null | awk '{print $7}' || hostname -I | awk '{print $1}')
if [ -z "$VM_IP" ]; then
    VM_IP="192.168.1.3"
fi

echo "=========================================================="
echo "   DRIVESENSE -- Terminal TUI in Browser (via ttyd)"
echo "=========================================================="
echo "Open in browser:  http://${VM_IP}:8081"
echo "Press Ctrl+C to terminate."
echo "=========================================================="

exec ttyd -p 8081 "$REPO_ROOT/build/drivesense_app"
