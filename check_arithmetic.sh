#!/bin/bash
echo "Checking for arithmetic operations in workflow files..."
cd /workspace/.github/workflows

for file in *.yml *.yaml; do
    if [ -f "$file" ]; then
        echo "=== Checking $file ==="
        
        # Check for $((arithmetic))
        if grep -n '\$\(\(' "$file" 2>/dev/null; then
            echo "  Found arithmetic operations in $file"
        fi
        
        # Check for wc -l (potential source of multi-line output)
        if grep -n 'wc -l' "$file" 2>/dev/null; then
            echo "  Found wc -l operations in $file"
        fi
        
        # Check for grep -c (potential source of multi-line output)
        if grep -n 'grep -c' "$file" 2>/dev/null; then
            echo "  Found grep -c operations in $file"
        fi
        
        echo ""
    fi
done