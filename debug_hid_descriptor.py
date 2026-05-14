#!/usr/bin/env python3
"""
HID Descriptor Parser for Windows Precision Trackpad

This script helps verify the HID descriptor is correct.
Run on Windows with: python debug_hid_descriptor.py

Requires: pip install hidapi
"""

import hid
import struct

def parse_hid_report(data):
    """Parse a PTP HID report"""
    if len(data) < 15:
        print(f"Report too short: {len(data)} bytes")
        return

    report_id = data[0]
    print(f"\nReport ID: 0x{report_id:02x}")

    if report_id == 0x01:  # REPORT_ID_TRACKPAD
        # Parse contact 0 (byte 1-5)
        c0_flags = data[1]
        c0_confidence = (c0_flags >> 0) & 1
        c0_tip = (c0_flags >> 1) & 1
        c0_contact_id = (c0_flags >> 2) & 3
        c0_x = struct.unpack('<H', data[2:4])[0]
        c0_y = struct.unpack('<H', data[4:6])[0]

        # Parse contact 1 (byte 6-10)
        c1_flags = data[6]
        c1_confidence = (c1_flags >> 0) & 1
        c1_tip = (c1_flags >> 1) & 1
        c1_contact_id = (c1_flags >> 2) & 3
        c1_x = struct.unpack('<H', data[7:9])[0]
        c1_y = struct.unpack('<H', data[9:11])[0]

        # Parse scan time and contact count
        scan_time = struct.unpack('<H', data[11:13])[0]
        contact_count = data[13]

        # Parse buttons
        button_byte = data[14]
        button1 = (button_byte >> 0) & 1
        button2 = (button_byte >> 1) & 1
        button3 = (button_byte >> 2) & 1

        print(f"Contact Count: {contact_count}")
        print(f"Scan Time: {scan_time} (x100μs = {scan_time/10:.1f}ms)")
        print(f"Buttons: B1={button1} B2={button2} B3={button3}")

        if c0_tip:
            print(f"Contact 0: ID={c0_contact_id}, Conf={c0_confidence}, Tip={c0_tip}, X={c0_x}, Y={c0_y}")
        if c1_tip:
            print(f"Contact 1: ID={c1_contact_id}, Conf={c1_confidence}, Tip={c1_tip}, X={c1_x}, Y={c1_y}")

def find_trackpad():
    """Find the Voyager trackpad device"""
    print("Searching for HID devices...")
    for device_info in hid.enumerate():
        # Look for ZSA Voyager
        if device_info['vendor_id'] == 0x3297:  # ZSA VID
            print(f"\nFound: {device_info['product_string']}")
            print(f"  VID: 0x{device_info['vendor_id']:04x}")
            print(f"  PID: 0x{device_info['product_id']:04x}")
            print(f"  Interface: {device_info['interface_number']}")
            print(f"  Usage Page: 0x{device_info['usage_page']:04x}")
            print(f"  Usage: 0x{device_info['usage']:04x}")

            # Check if it's the trackpad interface (usage page 0x0D, usage 0x05)
            if device_info['usage_page'] == 0x0D and device_info['usage'] == 0x05:
                print("  --> This is the Precision Trackpad interface!")
                return device_info

    print("\nNo trackpad interface found. Make sure the device is connected.")
    return None

def monitor_reports(device_info, count=10):
    """Monitor HID reports from the trackpad"""
    try:
        device = hid.device()
        device.open_path(device_info['path'])
        device.set_nonblocking(True)

        print(f"\nMonitoring {count} reports (move your finger on the trackpad)...")
        print("=" * 60)

        reports_received = 0
        while reports_received < count:
            data = device.read(64)
            if data:
                print(f"\nRaw bytes ({len(data)}): {' '.join(f'{b:02x}' for b in data)}")
                parse_hid_report(data)
                reports_received += 1

        device.close()
        print("\n" + "=" * 60)
        print(f"Received {reports_received} reports")

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    print("Windows Precision Trackpad HID Monitor")
    print("=" * 60)

    device_info = find_trackpad()
    if device_info:
        monitor_reports(device_info, count=20)
    else:
        print("\nTo manually check the descriptor:")
        print("1. Open Device Manager")
        print("2. Find 'HID-compliant touch pad' under 'Human Interface Devices'")
        print("3. Right-click -> Properties -> Details -> Hardware Ids")
        print("4. Look for the VID/PID and interface number")
