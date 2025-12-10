#!/bin/bash
# Search for shell arithmetic operations in workflow files
echo "Searching for shell arithmetic operations in GitHub workflows..."
echo "=================================================="

cd /workspace/.github/workflows

echo "Files with \$(( arithmetic operations:"
grep -r "\$\(\(" . || echo "None found"

echo ""
echo "Files with arithmetic assignments:"
grep -r "=\$\(\(" . || echo "None found"

echo ""
echo "Files with [ arithmetic comparisons:"
grep -r "\[ .*-[gl][te] " . || echo "None found"

echo ""
echo "Files with wc -l operations:"
grep -r "wc -l" . || echo "None found"

echo ""
echo "Files with grep -c operations:"
grep -r "grep -c" . || echo "None found"