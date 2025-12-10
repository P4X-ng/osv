#!/usr/bin/env python3

import re

# Read the Makefile
with open('/workspace/Makefile', 'r') as f:
    content = f.read()

# Split into lines
lines = content.split('\n')

# Fix indentation issues - replace 8 spaces at the beginning of lines with tabs
# This is specifically for Makefile recipe lines that should be indented with tabs
fixed_lines = []
for line in lines:
    # Check if line starts with exactly 8 spaces (common pattern in the problematic lines)
    if line.startswith('        ') and not line.startswith('         '):
        # Replace the 8 spaces with a single tab
        fixed_line = '\t' + line[8:]
        fixed_lines.append(fixed_line)
    else:
        fixed_lines.append(line)

# Write the fixed content back
with open('/workspace/Makefile', 'w') as f:
    f.write('\n'.join(fixed_lines))

print("Fixed Makefile indentation issues")