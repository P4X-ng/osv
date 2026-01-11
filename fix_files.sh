#!/bin/bash

echo "=== Checking for and fixing potential file issues ==="
echo ""

# Find all Python files that the CI would process
find . -name "*.py" \
  ! -path "*/.venv/*" \
  ! -path "*/node_modules/*" \
  ! -path "*/dist/*" \
  ! -name "__init__.py" \
  -type f > /tmp/all_python_files.txt

echo "Found $(wc -l < /tmp/all_python_files.txt) Python files to check"
echo ""

# Check each file for potential issues
while read -r file; do
  echo "Checking: $file"
  
  # Check if file exists and is readable
  if [[ ! -r "$file" ]]; then
    echo "  ERROR: File not readable, skipping"
    continue
  fi
  
  # Check for Windows line endings
  if file "$file" | grep -q "CRLF"; then
    echo "  WARNING: File has Windows line endings"
    # Convert to Unix line endings
    if command -v dos2unix >/dev/null 2>&1; then
      echo "  Converting to Unix line endings..."
      dos2unix "$file" 2>/dev/null
    else
      # Manual conversion
      sed -i 's/\r$//' "$file" 2>/dev/null
    fi
  fi
  
  # Check for null bytes
  if grep -q $'\0' "$file" 2>/dev/null; then
    echo "  WARNING: File contains null bytes"
    # Remove null bytes
    tr -d '\0' < "$file" > "${file}.tmp" && mv "${file}.tmp" "$file"
  fi
  
  # Check for very long lines that might cause issues
  max_line_length=$(wc -L "$file" 2>/dev/null | cut -d' ' -f1)
  if [[ "$max_line_length" -gt 10000 ]]; then
    echo "  WARNING: File has very long lines (max: $max_line_length chars)"
  fi
  
  # Test the grep commands that are failing in CI
  func_result=$(grep -c "^def " "$file" 2>/dev/null || echo 0)
  class_result=$(grep -c "^class " "$file" 2>/dev/null || echo 0)
  
  # Check if results are valid integers
  if [[ ! "$func_result" =~ ^[0-9]+$ ]]; then
    echo "  ERROR: Invalid func_result: '$func_result'"
  fi
  if [[ ! "$class_result" =~ ^[0-9]+$ ]]; then
    echo "  ERROR: Invalid class_result: '$class_result'"
  fi
  
  # Check if results contain spaces (the main issue)
  if [[ "$func_result" =~ [[:space:]] ]]; then
    echo "  ERROR: func_result contains whitespace: '$func_result'"
  fi
  if [[ "$class_result" =~ [[:space:]] ]]; then
    echo "  ERROR: class_result contains whitespace: '$class_result'"
  fi
  
done < /tmp/all_python_files.txt

echo ""
echo "=== Checking for files with problematic names ==="

# Check for files with spaces in names
find . -name "*.py" -type f | while IFS= read -r file; do
  if [[ "$file" == *" "* ]]; then
    echo "File with space in name: '$file'"
    # Rename file to remove spaces
    new_name=$(echo "$file" | tr ' ' '_')
    if [[ "$file" != "$new_name" ]]; then
      echo "  Renaming to: '$new_name'"
      mv "$file" "$new_name" 2>/dev/null || echo "  Failed to rename"
    fi
  fi
done

echo ""
echo "=== Final verification ==="

# Test the exact CI workflow on a few files
find . -name "*.py" \
  ! -path "*/.venv/*" \
  ! -path "*/node_modules/*" \
  ! -path "*/dist/*" \
  ! -name "__init__.py" \
  -type f | head -3 | while read -r file; do
  
  echo "Final test on: $file"
  
  # Exact CI commands
  func_count=$(grep -c "^def " "$file" 2>/dev/null || echo 0)
  class_count=$(grep -c "^class " "$file" 2>/dev/null || echo 0)
  docstring_count=$(grep -c '"""' "$file" 2>/dev/null || echo 0)
  
  # Variable assignments
  func_count=${func_count:-0}
  class_count=${class_count:-0}
  docstring_count=${docstring_count:-0}
  
  # Arithmetic operation
  if total=$((func_count + class_count)) 2>/dev/null; then
    echo "  Arithmetic success: $func_count + $class_count = $total"
    
    # Conditional test
    if [ "$total" -gt 0 ] && [ "$docstring_count" -eq 0 ] 2>/dev/null; then
      echo "  Conditional test success"
    else
      echo "  Conditional test result: total=$total, docstring=$docstring_count"
    fi
  else
    echo "  ERROR: Arithmetic still failing!"
  fi
done

rm -f /tmp/all_python_files.txt
echo ""
echo "File checking and fixing complete."