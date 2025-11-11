#!/bin/bash
# Upload script for Thermostat project

SKETCH_PATH="/home/fanstaf/Arduino/Thermostat"
FQBN="esp8266:esp8266:d1_mini_clone"
NETWORK_PORT="192.168.1.67"

echo "=== Compiling and Uploading Thermostat ==="

# Compile
echo "Compiling..."
arduino-cli compile --fqbn "$FQBN" "$SKETCH_PATH"

if [ $? -ne 0 ]; then
    echo "✗ Compilation failed!"
    exit 1
fi

echo ""
echo "✓ Compilation successful!"
echo ""

# Detect if USB or OTA upload
if [ "$1" = "usb" ]; then
    # USB upload
    echo "Uploading via USB..."
    USB_PORT=$(arduino-cli board list | grep "Serial Port" | awk '{print $1}' | head -n1)

    if [ -z "$USB_PORT" ]; then
        echo "✗ No USB device found. Please specify port: ./upload.sh usb /dev/ttyUSB0"
        exit 1
    fi

    if [ ! -z "$2" ]; then
        USB_PORT="$2"
    fi

    echo "Using port: $USB_PORT"
    arduino-cli upload --fqbn "$FQBN" --port "$USB_PORT" "$SKETCH_PATH"
else
    # OTA upload (default)
    echo "Uploading via OTA to $NETWORK_PORT..."
    arduino-cli upload --fqbn "$FQBN" --port "$NETWORK_PORT" "$SKETCH_PATH"
fi

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Upload successful!"
else
    echo ""
    echo "✗ Upload failed!"
    exit 1
fi
