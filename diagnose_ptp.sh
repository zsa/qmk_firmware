#!/bin/bash
# Diagnostic script for PTP trackpad on Linux

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Windows Precision Trackpad - Linux Diagnostic Tool           ║"
echo "╚════════════════════════════════════════════════════════════════╝"

echo ""
echo "=== USB Device Detection ==="
echo "Looking for ZSA Voyager (VID: 3297)..."
VOYAGER=$(lsusb | grep "3297")
if [ -z "$VOYAGER" ]; then
    echo "❌ Voyager not found!"
    echo "   Make sure the device is connected."
    exit 1
else
    echo "✓ Found: $VOYAGER"
fi

echo ""
echo "=== Input Devices ==="
INPUT_DEVICES=$(ls -la /dev/input/by-id/ 2>/dev/null | grep -i "zsa\|voyager")
if [ -z "$INPUT_DEVICES" ]; then
    echo "❌ No input devices found"
else
    echo "$INPUT_DEVICES"
fi

echo ""
echo "=== HID Raw Devices ==="
HIDRAW_FOUND=""
for hidraw in /sys/class/hidraw/hidraw*; do
    if [ -e "$hidraw/device/uevent" ] && grep -q "3297" "$hidraw/device/uevent" 2>/dev/null; then
        HIDRAW_DEV=$(basename "$hidraw")
        HIDRAW_FOUND="$HIDRAW_FOUND $HIDRAW_DEV"
        echo "✓ Found: $HIDRAW_DEV"

        # Get HID info
        if [ -e "$hidraw/device/report_descriptor" ]; then
            DESC_SIZE=$(stat -c%s "$hidraw/device/report_descriptor")
            echo "  Report descriptor size: $DESC_SIZE bytes"
        fi

        # Get usage page and usage
        if [ -e "$hidraw/device/uevent" ]; then
            HID_INFO=$(grep "HID_NAME\|HID_ID" "$hidraw/device/uevent")
            echo "  $HID_INFO" | sed 's/^/  /'
        fi
    fi
done

if [ -z "$HIDRAW_FOUND" ]; then
    echo "❌ No hidraw devices found for Voyager"
fi

echo ""
echo "=== Event Device Info ==="
EVENT_FOUND=""
for event in /sys/class/input/event*; do
    if [ -e "$event/device/name" ] && grep -qi "voyager\|trackpad" "$event/device/name" 2>/dev/null; then
        EVENT_DEV=$(basename "$event")
        EVENT_FOUND="$EVENT_FOUND $EVENT_DEV"
        echo "✓ Found: $EVENT_DEV"
        echo "  Name: $(cat "$event/device/name" 2>/dev/null)"

        # Check capabilities
        if [ -e "$event/device/capabilities/abs" ]; then
            ABS_CAP=$(cat "$event/device/capabilities/abs")
            if [ "$ABS_CAP" != "0" ]; then
                echo "  ✓ Has ABS capabilities: $ABS_CAP"
            fi
        fi

        if [ -e "$event/device/capabilities/key" ]; then
            KEY_CAP=$(cat "$event/device/capabilities/key")
            echo "  Key capabilities: $KEY_CAP"
        fi

        # Check for multi-touch
        if [ -e "$event/device/properties" ]; then
            PROPS=$(cat "$event/device/properties")
            if [ "$PROPS" != "0" ]; then
                echo "  Properties: $PROPS"
            fi
        fi
    fi
done

if [ -z "$EVENT_FOUND" ]; then
    echo "❌ No event devices found for Voyager trackpad"
fi

echo ""
echo "=== Kernel Messages ==="
echo "Recent HID/USB messages:"
dmesg | grep -i "3297\|voyager\|hid.*touchpad" | tail -10

echo ""
echo "=== Quick Tests ==="
echo ""

if [ -n "$HIDRAW_FOUND" ]; then
    HIDRAW_DEV=$(echo $HIDRAW_FOUND | awk '{print $1}')
    echo "To capture raw HID reports from $HIDRAW_DEV:"
    echo "  sudo python3 ptp_monitor.py /dev/$HIDRAW_DEV"
    echo ""
fi

if [ -n "$EVENT_FOUND" ]; then
    EVENT_DEV=$(echo $EVENT_FOUND | awk '{print $1}')
    echo "To monitor input events from $EVENT_DEV:"
    echo "  sudo evtest /dev/input/$EVENT_DEV"
    echo ""
fi

echo "To dump HID descriptor:"
if [ -n "$HIDRAW_FOUND" ]; then
    HIDRAW_DEV=$(echo $HIDRAW_FOUND | awk '{print $1}')
    echo "  sudo cat /sys/class/hidraw/$HIDRAW_DEV/device/report_descriptor | xxd"
fi

echo ""
echo "To compare with working PTP device:"
echo "  ls -la /dev/input/by-path/ | grep touchpad"
echo "  sudo evtest /dev/input/by-path/XXX-touchpad"

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  Diagnosis complete. Run the commands above to debug further.  ║"
echo "╚════════════════════════════════════════════════════════════════╝"
