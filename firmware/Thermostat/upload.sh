#!/bin/bash
# Upload script for Thermostat project
#
#   ./upload.sh                 OTA upload; finds the "thermostat" board via mDNS
#   ./upload.sh <host>          OTA upload to a specific IP/hostname
#   ./upload.sh ota <host>      (same)
#   ./upload.sh usb [port]      USB upload (auto-detects port if omitted)
#
# OTA_HOST / OTA_PASSWORD env vars override discovery / Credentials.h.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_PATH="$SCRIPT_DIR"
FQBN="esp8266:esp8266:d1_mini_clone"
OTA_NAME="thermostat"   # must match OTA_HOSTNAME in Config.h

echo "=== Compiling and Uploading Thermostat ==="

# Compile
echo "Compiling..."
if ! arduino-cli compile --fqbn "$FQBN" "$SKETCH_PATH"; then
    echo "✗ Compilation failed!"
    exit 1
fi

echo ""
echo "✓ Compilation successful!"
echo ""

if [ "$1" = "usb" ]; then
    echo "Uploading via USB..."
    if [ -n "$2" ]; then
        USB_PORT="$2"
    else
        USB_PORT=$(arduino-cli board list | grep "Serial Port (USB)" | awk '{print $1}' | head -n1)
    fi

    if [ -z "$USB_PORT" ]; then
        echo "✗ No USB device found. Please specify port: ./upload.sh usb /dev/ttyUSB0"
        exit 1
    fi

    echo "Using port: $USB_PORT"
    arduino-cli upload --fqbn "$FQBN" --port "$USB_PORT" "$SKETCH_PATH"
else
    # OTA upload (default). Host: "ota <host>" or bare "<host>" > OTA_HOST > discovery.
    if [ "$1" = "ota" ]; then HOST="$2"; else HOST="$1"; fi
    HOST="${HOST:-$OTA_HOST}"
    if [ -z "$HOST" ]; then
        # Only accept a board advertising the "thermostat" OTA hostname, so other
        # ESP nodes on the LAN (floor heat, door hub) are never flashed by mistake.
        echo "Looking for '$OTA_NAME' on the network..."
        HOST=$(arduino-cli board list --discovery-timeout 5s --format json | python3 -I -c '
import json, sys
name = sys.argv[1]
for p in json.load(sys.stdin).get("detected_ports", []):
    port = p.get("port", {})
    if port.get("protocol") == "network" and port.get("label", "").split(" ")[0] == name:
        print(port["address"]); break
' "$OTA_NAME")
    fi
    if [ -z "$HOST" ]; then
        echo "✗ '$OTA_NAME' not found on the network. Use: ./upload.sh <ip-address>"
        exit 1
    fi

    # Password: OTA_PASSWORD env var, else the define in Credentials.h.
    PASSWORD="${OTA_PASSWORD:-$(sed -n 's/^#define[[:space:]]\+OTA_PASSWORD[[:space:]]\+"\(.*\)".*/\1/p' "$SCRIPT_DIR/Credentials.h" 2>/dev/null)}"

    echo "Uploading via OTA to $HOST..."
    if [ -n "$PASSWORD" ]; then
        arduino-cli upload --fqbn "$FQBN" --port "$HOST" --protocol network \
            --upload-field "password=$PASSWORD" "$SKETCH_PATH"
    else
        arduino-cli upload --fqbn "$FQBN" --port "$HOST" --protocol network "$SKETCH_PATH"
    fi
fi

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Upload successful!"
else
    echo ""
    echo "✗ Upload failed!"
    exit 1
fi
