#!/bin/bash
echo "=== Checking for remaining grep -P patterns ==="
grep -n "grep -P" /workspace/scripts/manifest_from_host.sh
if [ $? -ne 0 ]; then
    echo "✓ No grep -P patterns found"
else
    echo "✗ Found remaining grep -P patterns"
fi

echo ""
echo "=== Syntax check ==="
bash -n /workspace/scripts/manifest_from_host.sh
if [ $? -eq 0 ]; then
    echo "✓ Script syntax is valid"
else
    echo "✗ Script has syntax errors"
fi

echo ""
echo "=== Summary of changes made ==="
echo "1. Replaced grep -P with grep -E for simple patterns"
echo "2. Replaced Perl regex \\d with [0-9] for digit matching"
echo "3. Replaced complex Perl regex extractions with awk field parsing"
echo "4. All grep -Pv changed to grep -Ev"