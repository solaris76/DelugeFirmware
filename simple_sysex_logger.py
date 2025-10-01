#!/usr/bin/env python3
"""
Simple sysex logger for Deluge debug messages
Uses mido library which has prebuilt wheels (easier to install than python-rtmidi)
"""

import time

try:
    import mido
except ImportError:
    print("Installing mido library...")
    import subprocess
    import sys

    subprocess.check_call(
        [sys.executable, "-m", "pip", "install", "--user", "mido", "python-rtmidi"]
    )
    import mido


def unpack_7bit_to_8bit(data):
    """Convert 7-bit MIDI data to 8-bit"""
    output = bytearray()
    for b in data:
        value = b & 0x7F
        if value & 0x40:
            value |= 0x80
        output.append(value)
    return output


def main():
    print("Available MIDI ports:")
    print("\nOUTPUT ports (for sending to Deluge):")
    for i, name in enumerate(mido.get_output_names()):
        print(f"  {i}: {name}")

    print("\nINPUT ports (for receiving from Deluge):")
    for i, name in enumerate(mido.get_input_names()):
        print(f"  {i}: {name}")

    # Find Deluge ports
    deluge_out = None
    deluge_in = None

    for name in mido.get_output_names():
        if "deluge" in name.lower():
            deluge_out = name
            break

    for name in mido.get_input_names():
        if "deluge" in name.lower():
            deluge_in = name
            break

    if not deluge_out or not deluge_in:
        print("\n❌ Could not find Deluge MIDI ports!")
        print("Make sure Deluge is connected via USB.")
        return

    print(f"\n✅ Found Deluge:")
    print(f"   Output: {deluge_out}")
    print(f"   Input:  {deluge_in}")

    # Open ports
    print("\n🔧 Enabling debug logging on Deluge...")
    outport = mido.open_output(deluge_out)
    inport = mido.open_input(deluge_in)

    # Send enable command
    enable_msg = mido.Message(
        "sysex",
        data=[
            0x00,
            0x21,
            0x7B,
            0x01,  # Deluge header
            0x03,  # Debug namespace
            0x00,  # Sysex logging config command
            0x01,  # Enable
        ],
    )
    outport.send(enable_msg)
    print("✅ Debug logging enabled\n")
    print("=" * 60)
    print("DELUGE DEBUG OUTPUT:")
    print("=" * 60)

    # Listen for messages
    try:
        while True:
            for msg in inport.iter_pending():
                if msg.type == "sysex":
                    data = msg.data
                    # Check for Deluge debug message
                    if len(data) > 8 and data[0:7] == (
                        0x00,
                        0x21,
                        0x7B,
                        0x01,
                        0x03,
                        0x40,
                        0x00,
                    ):
                        try:
                            # Unpack and decode
                            target_bytes = unpack_7bit_to_8bit(data[5:])
                            decoded = target_bytes.decode(
                                "ascii", errors="replace"
                            ).replace("\n", "")
                            if decoded.strip():
                                print(decoded, flush=True)
                        except Exception as e:
                            print(f"[decode error: {e}]")
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("\n\n🛑 Stopping debug logger...")
    finally:
        outport.close()
        inport.close()


if __name__ == "__main__":
    main()
