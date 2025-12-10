#!/usr/bin/env python3

# Read line 30 from Makefile to analyze whitespace
with open('/workspace/Makefile', 'r') as f:
    lines = f.readlines()

line30 = lines[29]  # 0-indexed
print(f"Line 30 content: '{line30}'")
print(f"Line 30 repr: {repr(line30)}")
print(f"Starts with 8 spaces: {line30.startswith('        ')}")
print(f"Length: {len(line30)}")

# Show character by character
for i, char in enumerate(line30):
    if char == ' ':
        print(f"Position {i}: SPACE")
    elif char == '\t':
        print(f"Position {i}: TAB")
    else:
        print(f"Position {i}: '{char}'")