#!/usr/bin/env python3
"""
Generate SYSEX firmware file for Deluge
Usage: python3 generate_sysex.py <firmware.bin> <hex_key>
"""

import sys


def generate_sysex(firmware_path, hex_key):
    """Generate SYSEX file from firmware binary and hex key"""

    # Read the firmware binary
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()

    print(f"Firmware size: {len(firmware_data)} bytes")

    # Convert hex key to bytes
    key_bytes = bytes.fromhex(hex_key)
    print(f"Key: {hex_key}")

    # SYSEX header for Deluge firmware
    sysex_header = b"\xf0\x00\x21\x2d\x00\x00\x01"

    # Create SYSEX data
    sysex_data = sysex_header + key_bytes + firmware_data

    # Add SYSEX end marker
    sysex_data += b"\xf7"

    # Write SYSEX file
    sysex_filename = firmware_path.replace(".bin", ".syx")
    with open(sysex_filename, "wb") as f:
        f.write(sysex_data)

    print(f"SYSEX file created: {sysex_filename}")
    print(f"SYSEX size: {len(sysex_data)} bytes")

    return sysex_filename


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 generate_sysex.py <firmware.bin> <hex_key>")
        print("Example: python3 generate_sysex.py build/Debug/deluge.bin 1BF83947")
        sys.exit(1)

    firmware_path = sys.argv[1]
    hex_key = sys.argv[2]

    try:
        sysex_file = generate_sysex(firmware_path, hex_key)
        print(f"\n✅ Success! SYSEX file ready: {sysex_file}")
        print("\nNext steps:")
        print("1. Load the .bin file to your Deluge via SD card first")
        print("2. Then you can use the .syx file for future USB updates")
        print("3. Use a MIDI app like MIDI Monitor to send the .syx file")

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
