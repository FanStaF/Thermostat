#!/bin/bash
# Build script for Thermostat project

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_PATH="$SCRIPT_DIR"
FQBN="esp8266:esp8266:d1_mini_clone"

echo "=== Compiling Thermostat ==="
arduino-cli compile --fqbn "$FQBN" "$SKETCH_PATH"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Compilation successful!"
    echo ""
    echo "To upload:"
    echo "  Via OTA:  ./build.sh upload"
    echo "  Via USB:  ./build.sh upload-usb"
else
    echo ""
    echo "✗ Compilation failed!"
    exit 1
fi
