#!/usr/bin/env python3
"""
embed.py <binary>

Converts a binary file into a mutable C byte array (no const).
The array lands in .data — writable — so callers can explicit_bzero()
it after use to wipe the payload from memory.
"""
import sys

with open(sys.argv[1], 'rb') as f:
    data = f.read()

# no 'const' → compiler places this in .data (writable), not .rodata
print("static unsigned char payload[] = {")
for i in range(0, len(data), 12):
    chunk = data[i:i+12]
    print("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
print("};")
print(f"static unsigned int payload_len = {len(data)};")
