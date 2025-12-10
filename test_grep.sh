#!/bin/bash

# Test script to reproduce the documentation analysis issue
echo "Testing grep commands on first few Python files..."

# Find first 5 Python files and test grep commands on them
find . -name "*.py" \
  ! -path "*/.venv/*" \
  ! -path "*/node_modules/*" \
  ! -path "*/dist/*" \
  ! -name "__init__.py" \
  -type f | head -5 | while read -r file; do
  
  echo "Testing file: $file"
  
  # Count functions and classes like the CI script does
  func_count=$(grep -c "^def " "$file" 2>/dev/null || echo 0)
  class_count=$(grep -c "^class " "$file" 2>/dev/null || echo 0)
  
  echo "  func_count: '$func_count'"
  echo "  class_count: '$class_count'"
  
  # Check if either contains spaces or multiple values
  if [[ "$func_count" == *" "* ]]; then
    echo "  ERROR: func_count contains spaces: '$func_count'"
  fi
  
  if [[ "$class_count" == *" "* ]]; then
    echo "  ERROR: class_count contains spaces: '$class_count'"
  fi
  
  # Try the arithmetic that would fail
  if ! total=$((func_count + class_count)) 2>/dev/null; then
    echo "  ERROR: Arithmetic failed with func_count='$func_count' class_count='$class_count'"
  else
    echo "  total: $total"
  fi
  
  echo ""
done

echo "Checking for any files with unusual content..."
# Look for files that might have multiple newlines or unusual patterns
find . -name "*.py" -type f | head -3 | while read -r file; do
  echo "Checking $file for unusual patterns..."
  # Check if file has any lines that might confuse grep
  if grep -n "def.*def" "$file" 2>/dev/null; then
    echo "  Found multiple 'def' on same line in $file"
  fi
  if grep -n "class.*class" "$file" 2>/dev/null; then
    echo "  Found multiple 'class' on same line in $file"
  fi
done