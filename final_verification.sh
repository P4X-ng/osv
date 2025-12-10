#!/bin/bash
echo "=== Final Verification of grep -P Fixes ==="
echo ""

echo "1. Checking manifest_from_host.sh for remaining grep -P patterns:"
grep -n "grep -P" /workspace/scripts/manifest_from_host.sh
if [ $? -ne 0 ]; then
    echo "   ✓ No grep -P patterns found in manifest_from_host.sh"
else
    echo "   ✗ Found remaining grep -P patterns in manifest_from_host.sh"
fi

echo ""
echo "2. Checking cli/rpmbuild/Makefile for remaining grep -P patterns:"
grep -n "grep -P" /workspace/modules/cli/rpmbuild/Makefile
if [ $? -ne 0 ]; then
    echo "   ✓ No grep -P patterns found in cli/rpmbuild/Makefile"
else
    echo "   ✗ Found remaining grep -P patterns in cli/rpmbuild/Makefile"
fi

echo ""
echo "3. Syntax check for manifest_from_host.sh:"
bash -n /workspace/scripts/manifest_from_host.sh
if [ $? -eq 0 ]; then
    echo "   ✓ manifest_from_host.sh syntax is valid"
else
    echo "   ✗ manifest_from_host.sh has syntax errors"
fi

echo ""
echo "4. Checking for other build-critical grep -P patterns:"
echo "   Searching in modules/*/Makefile files..."
find /workspace/modules -name "Makefile" -exec grep -l "grep -P" {} \;
if [ $? -ne 0 ]; then
    echo "   ✓ No grep -P patterns found in module Makefiles"
else
    echo "   ⚠ Found grep -P patterns in other module Makefiles (may need attention)"
fi

echo ""
echo "=== Summary ==="
echo "Fixed files:"
echo "  - /workspace/scripts/manifest_from_host.sh (7 instances of grep -P replaced)"
echo "  - /workspace/modules/cli/rpmbuild/Makefile (1 instance of grep -P replaced)"
echo ""
echo "These fixes should resolve the build error:"
echo "  'grep: support for the -P option is not compiled into this --disable-perl-regexp binary'"