#!/bin/bash

# Simple test to identify the exact issue
echo "=== Simple CI reproduction test ==="

# Test just one file first
test_file="./scripts/test.py"
if [[ -f "$test_file" ]]; then
  echo "Testing file: $test_file"
  
  # Run the exact commands from CI
  echo "Running: grep -c \"^def \" \"$test_file\" 2>/dev/null || echo 0"
  func_count=$(grep -c "^def " "$test_file" 2>/dev/null || echo 0)
  echo "Result: '$func_count'"
  
  echo "Running: grep -c \"^class \" \"$test_file\" 2>/dev/null || echo 0"
  class_count=$(grep -c "^class " "$test_file" 2>/dev/null || echo 0)
  echo "Result: '$class_count'"
  
  # Check for issues
  if [[ "$func_count" =~ [[:space:]] ]]; then
    echo "ERROR: func_count contains whitespace!"
  fi
  if [[ "$class_count" =~ [[:space:]] ]]; then
    echo "ERROR: class_count contains whitespace!"
  fi
  
  # Test assignments
  func_count=${func_count:-0}
  class_count=${class_count:-0}
  echo "After assignment: func_count='$func_count' class_count='$class_count'"
  
  # Test arithmetic
  echo "Testing arithmetic..."
  if total=$((func_count + class_count)) 2>/dev/null; then
    echo "Success: total=$total"
  else
    echo "FAILED: Cannot do arithmetic with func_count='$func_count' class_count='$class_count'"
  fi
fi

echo ""
echo "=== Testing find command ==="
# Test the find command
echo "Running find command..."
find . -name "*.py" \
  ! -path "*/.venv/*" \
  ! -path "*/node_modules/*" \
  ! -path "*/dist/*" \
  ! -name "__init__.py" \
  -type f | head -3

echo ""
echo "=== Testing while loop ==="
# Test the while loop structure
find . -name "*.py" \
  ! -path "*/.venv/*" \
  ! -path "*/node_modules/*" \
  ! -path "*/dist/*" \
  ! -name "__init__.py" \
  -type f | head -2 | while read -r file; do
  
  echo "Processing: '$file'"
  
  # Check if file variable looks correct
  if [[ "$file" =~ [[:space:]] ]]; then
    echo "  WARNING: file variable contains spaces: '$file'"
  fi
  
  # Test grep
  func_count=$(grep -c "^def " "$file" 2>/dev/null || echo 0)
  echo "  func_count: '$func_count'"
  
  # Check result
  if [[ "$func_count" =~ [[:space:]] ]]; then
    echo "  ERROR: func_count has spaces!"
  fi
done