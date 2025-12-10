#!/bin/bash

# Test the shell script syntax from the modified workflow
# This simulates the Python file analysis section

echo "Testing shell script syntax..."

# Simulate a Python file for testing
echo 'def test_function():
    """Test docstring"""
    pass

class TestClass:
    pass' > /tmp/test.py

file="/tmp/test.py"

# Test the modified logic
func_count=$(grep -c "^def " "$file" 2>/dev/null || echo 0)
class_count=$(grep -c "^class " "$file" 2>/dev/null || echo 0)
docstring_count=$(grep -c '"""' "$file" 2>/dev/null || echo 0)

# Ensure variables are numeric and clean
func_count=$(echo "$func_count" | tr -d '\n\r ' | grep -E '^[0-9]+$' || echo 0)
class_count=$(echo "$class_count" | tr -d '\n\r ' | grep -E '^[0-9]+$' || echo 0)
docstring_count=$(echo "$docstring_count" | tr -d '\n\r ' | grep -E '^[0-9]+$' || echo 0)

# Set defaults if empty
func_count=${func_count:-0}
class_count=${class_count:-0}
docstring_count=${docstring_count:-0}

total=$((func_count + class_count))

echo "func_count: $func_count"
echo "class_count: $class_count"
echo "docstring_count: $docstring_count"
echo "total: $total"

if [ "$total" -gt 0 ] && [ "$docstring_count" -eq 0 ]; then
  echo "⚠️ $file: $total definitions, no docstrings"
else
  echo "✅ Documentation check passed"
fi

# Clean up
rm -f /tmp/test.py

echo "Shell script syntax test completed successfully!"