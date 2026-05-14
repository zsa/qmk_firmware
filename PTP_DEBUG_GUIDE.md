# Windows Precision Trackpad Debug Guide

## Quick Debug Steps

### 1. Check Console Output (QMK)
With `CONSOLE_ENABLE = yes` in your rules.mk, you should see debug output:
```
PTP: fingers=1, range: x=1-897, y=1-895, current: f0(450,300) f1(0,0)
PTP Report: count=1, scan=12345, btn=000
  C0: id=0, conf=1, tip=1, x=450, y=300
```

Use `hid_listen` or QMK Toolbox to view console output.

### 2. Check HID Descriptor (Windows)

#### Option A: Device Manager
1. Open Device Manager
2. Find "HID-compliant touch pad" under "Human Interface Devices"
3. Right-click → Properties → Details
4. Select "Hardware Ids" - should show:
   - `HID\VID_3297&PID_1977&MI_03` (or similar)
   - `HID\VID_3297&UP:000D_U:0005` (Usage Page 0x0D, Usage 0x05 = Digitizer/Touchpad)

#### Option B: USB Device Tree Viewer
1. Download from: https://www.uwe-sieber.de/usbtreeview_e.html
2. Find your Voyager device
3. Look for interface with:
   - Usage Page: 0x0D (Digitizers)
   - Usage: 0x05 (Touch Pad)
4. Check the HID Report Descriptor

#### Option C: Python Script
```bash
pip install hidapi
python debug_hid_descriptor.py
```

### 3. Capture HID Packets (Wireshark)

1. Install Wireshark with USBPcap: https://www.wireshark.org/
2. Start capture on USB bus
3. Filter by: `usb.idVendor == 0x3297`
4. Look for URB_INTERRUPT packets to endpoint 0x85 (trackpad IN endpoint)

### 4. Compare with Working Trackpad

Run this on a working Windows Precision Trackpad:
```bash
python debug_hid_descriptor.py
```

Look for differences in:
- Contact count behavior
- Coordinate ranges
- Scan time increments
- Button states

## Expected PTP Report Structure

```
Byte 0:    Report ID (0x01)
Byte 1:    Contact 0 flags (confidence, tip, contact_id, reserved)
Byte 2-3:  Contact 0 X (little-endian, 0-900)
Byte 4-5:  Contact 0 Y (little-endian, 0-900)
Byte 6:    Contact 1 flags
Byte 7-8:  Contact 1 X
Byte 9-10: Contact 1 Y
Byte 11-12: Scan time (little-endian, in 100μs units)
Byte 13:   Contact count (0-2)
Byte 14:   Button states (button1, button2, button3, padding)
```

## Common Issues

### Gestures Not Working

**Symptom**: Cursor moves but no scrolling, pinch, rotate

**Possible Causes**:
1. **Contact ID not consistent** - Windows tracks fingers by contact_id
2. **Missing "finger lifted" report** - Need to send contact_count=0 when no touch
3. **Scan time not incrementing** - Windows uses this for velocity calculations
4. **Confidence bit always 0** - Windows may ignore uncertain contacts
5. **Physical dimensions wrong** - Affects gesture recognition

**Debug**:
```c
// Add to navigator_trackpad_task()
printf("C0: id=%d -> ", prev_contact_id[0]);
printf("%d, ", ptp.contacts[0].contact_id);
```

### Taps Not Working

**Symptom**: No click on single tap

**Possible Causes**:
1. **Feature report issues** - Windows queries device capabilities
2. **Clickpad bit not set** - Check REPORT_ID_TRACKPAD_MAX_COUNT response
3. **No button click simulation** - PTP may need button1 set on tap

**Debug**:
Check feature report responses in `usb_report_handling.c`:
- REPORT_ID_TRACKPAD_MAX_COUNT: Should return 2 contacts, clickpad mode
- REPORT_ID_TRACKPAD_PTPHQA: Should return certification blob

### Cursor Movement Slow/Fast

**Symptom**: Cursor doesn't match finger speed

**Check**:
1. Logical Maximum in descriptor (should be 900 for our sensor)
2. Physical Maximum in descriptor (should be 157 = 1.57" = 40mm)
3. Coordinate mapping in code (currently direct, no scaling)

## Verification Checklist

- [ ] Console shows finger touch/release correctly
- [ ] Console shows contact_count changing (0 → 1 → 2 → 1 → 0)
- [ ] Console shows contact_id staying consistent per finger
- [ ] Scan time is incrementing
- [ ] Coordinates in expected range (1-897 x 1-895)
- [ ] Device Manager shows Usage Page 0x0D, Usage 0x05
- [ ] Feature reports respond correctly
- [ ] Two fingers show different contact_ids (0 and 1)

## Next Steps

If console output looks correct but gestures don't work:
1. Capture USB packets with Wireshark
2. Compare with working Microsoft Precision Trackpad
3. Check Windows Event Viewer for HID errors
4. Try on different Windows version (10 vs 11)
