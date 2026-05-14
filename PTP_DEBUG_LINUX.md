# Linux PTP Debugging Guide

## Quick Setup (Arch Linux)

### 1. Install Required Tools
```bash
# HID debugging tools
sudo pacman -S usbutils evtest libevdev python-pyudev

# Optional: For USB packet capture
sudo pacman -S wireshark-qt
```

### 2. Find Your Trackpad Device

#### List all HID devices:
```bash
# Find Voyager device
lsusb | grep 3297

# List all input devices
ls -la /dev/input/by-id/ | grep -i zsa

# Show detailed HID info
sudo udevadm info --query=all --name=/dev/input/eventX
```

#### Find the trackpad event device:
```bash
# This will show all input devices with their capabilities
for device in /dev/input/event*; do
    echo "=== $device ==="
    sudo evtest --grab $device &
    sleep 0.1
    sudo pkill evtest
done | grep -A 5 "Touch"
```

### 3. Monitor Events with evtest

```bash
# Find the event number for your trackpad
ls -la /dev/input/by-id/ | grep -i voyager

# Monitor all events (replace eventX with your device)
sudo evtest /dev/input/eventX

# Or use by-id path
sudo evtest /dev/input/by-id/usb-ZSA_Voyager-event-if03
```

**Expected output for working PTP:**
```
Event: time 1234.567890, type 3 (EV_ABS), code 47 (ABS_MT_SLOT), value 0
Event: time 1234.567890, type 3 (EV_ABS), code 57 (ABS_MT_TRACKING_ID), value 0
Event: time 1234.567890, type 3 (EV_ABS), code 53 (ABS_MT_POSITION_X), value 450
Event: time 1234.567890, type 3 (EV_ABS), code 54 (ABS_MT_POSITION_Y), value 300
Event: time 1234.567890, type 0 (EV_SYN), code 0 (SYN_REPORT), value 0
```

### 4. Check HID Report Descriptor

```bash
# Find the hidraw device
ls -la /sys/class/hidraw/

# Get the report descriptor (replace hidrawX)
sudo cat /sys/class/hidraw/hidrawX/device/report_descriptor | xxd

# Or use hexdump
sudo hexdump -C /sys/class/hidraw/hidrawX/device/report_descriptor
```

#### Parse the descriptor:
```bash
# Install hidrd if available
yay -S hidrd  # or from AUR

# Parse descriptor
sudo cat /sys/class/hidraw/hidrawX/device/report_descriptor | hidrd-convert -o spec
```

### 5. Capture Raw HID Reports

```bash
# Find your hidraw device
ls -la /sys/class/hidraw/ | grep -i 3297

# Read raw HID reports (Ctrl+C to stop)
sudo cat /dev/hidrawX | xxd

# Or with timeout
sudo timeout 5 cat /dev/hidrawX | xxd > ptp_reports.txt
```

### 6. Compare with Working PTP Device

#### Capture working device:
```bash
# Find working trackpad (e.g., laptop touchpad)
xinput list | grep -i touchpad

# Get its event device
ls -la /dev/input/by-path/ | grep touchpad

# Monitor it
sudo evtest /dev/input/eventY > working_ptp.log
```

#### Capture Voyager:
```bash
sudo evtest /dev/input/eventX > voyager_ptp.log
```

#### Compare:
```bash
# Look for differences in event types and values
diff -u working_ptp.log voyager_ptp.log
```

### 7. Check Device Capabilities

```bash
# Show device capabilities
sudo evtest /dev/input/eventX --query

# Or check sysfs
cat /sys/class/input/eventX/device/capabilities/*
```

**Expected capabilities for PTP:**
- `EV_ABS`: Absolute positioning
- `ABS_MT_SLOT`: Multi-touch slot
- `ABS_MT_TRACKING_ID`: Finger tracking
- `ABS_MT_POSITION_X/Y`: Coordinates
- `BTN_TOUCH`: Touch detection
- `BTN_TOOL_FINGER`: Finger count

### 8. Monitor with Python (More Detailed)

```python
#!/usr/bin/env python3
# ptp_monitor.py
import struct
import sys

def parse_ptp_report(data):
    """Parse our PTP report format"""
    if len(data) < 15:
        return None

    report_id = data[0]
    if report_id != 0x01:
        return None

    # Contact 0
    c0_flags = data[1]
    c0_x = struct.unpack('<H', data[2:4])[0]
    c0_y = struct.unpack('<H', data[4:6])[0]

    # Contact 1
    c1_flags = data[6]
    c1_x = struct.unpack('<H', data[7:9])[0]
    c1_y = struct.unpack('<H', data[9:11])[0]

    scan_time = struct.unpack('<H', data[11:13])[0]
    contact_count = data[13]
    buttons = data[14]

    return {
        'contact_count': contact_count,
        'scan_time': scan_time,
        'c0': {'tip': (c0_flags >> 1) & 1, 'x': c0_x, 'y': c0_y},
        'c1': {'tip': (c1_flags >> 1) & 1, 'x': c1_x, 'y': c1_y},
        'buttons': buttons
    }

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: sudo python3 ptp_monitor.py /dev/hidrawX")
        sys.exit(1)

    with open(sys.argv[1], 'rb') as f:
        while True:
            data = f.read(64)
            if not data:
                break

            report = parse_ptp_report(data)
            if report:
                print(f"Count: {report['contact_count']}, "
                      f"Scan: {report['scan_time']}, "
                      f"C0: ({report['c0']['x']}, {report['c0']['y']}), "
                      f"C1: ({report['c1']['x']}, {report['c1']['y']})")
```

Save as `ptp_monitor.py` and run:
```bash
sudo python3 ptp_monitor.py /dev/hidrawX
```

### 9. Check Kernel Messages

```bash
# Watch kernel messages for HID errors
sudo dmesg -w | grep -i hid

# Check for initialization messages
sudo dmesg | grep -i "3297\|voyager\|touchpad"
```

### 10. libinput Debug Mode

```bash
# Stop display manager first
sudo systemctl stop gdm  # or sddm, lightdm, etc.

# Run libinput in debug mode
sudo libinput debug-events --device /dev/input/eventX

# Or debug all devices
sudo libinput debug-events

# Restart display manager when done
sudo systemctl start gdm
```

## Quick Diagnostic Script

```bash
#!/bin/bash
# diagnose_ptp.sh

echo "=== Finding Voyager Device ==="
lsusb | grep 3297

echo -e "\n=== Input Devices ==="
ls -la /dev/input/by-id/ | grep -i zsa

echo -e "\n=== HID Raw Devices ==="
for hidraw in /sys/class/hidraw/hidraw*; do
    if grep -q "3297" "$hidraw/device/uevent" 2>/dev/null; then
        echo "Found: $(basename $hidraw)"
        cat "$hidraw/device/uevent"
    fi
done

echo -e "\n=== Event Device Info ==="
for event in /sys/class/input/event*; do
    if grep -q "Voyager" "$event/device/name" 2>/dev/null; then
        echo "Device: $(basename $event)"
        echo "Name: $(cat $event/device/name)"
        echo "Capabilities:"
        cat "$event/device/capabilities/abs" 2>/dev/null || echo "  No ABS"
        cat "$event/device/capabilities/key" 2>/dev/null || echo "  No KEY"
    fi
done
```

Save as `diagnose_ptp.sh`, make executable, and run:
```bash
chmod +x diagnose_ptp.sh
./diagnose_ptp.sh
```

## Comparison Checklist

Compare Voyager vs Working PTP:

- [ ] Same event types (EV_ABS, EV_KEY, EV_SYN)
- [ ] Same ABS codes (ABS_MT_SLOT, ABS_MT_TRACKING_ID, ABS_MT_POSITION_X/Y)
- [ ] Similar coordinate ranges
- [ ] Tracking ID appears/disappears correctly
- [ ] SLOT changes for multi-touch
- [ ] BTN_TOUCH state changes

## What to Look For

### Good Signs:
- Kernel recognizes as input device
- `evtest` shows MT (multi-touch) events
- Tracking IDs increment/stay consistent
- Coordinates in expected range

### Bad Signs:
- Only sees button events, no coordinates
- No MT events (ABS_MT_*)
- Tracking IDs always -1
- Coordinates always 0 or out of range
- "hid-generic" driver instead of "hid-multitouch"

## Next Steps

After gathering data, we can:
1. Fix HID descriptor if kernel doesn't recognize it
2. Adjust coordinate mapping if range is wrong
3. Fix contact tracking if IDs are inconsistent
4. Add missing events if capabilities are incomplete
