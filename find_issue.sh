#!/bin/bash

echo "=== Comprehensive search for the CI issue ==="
echo ""

# First, let's reproduce the exact CI command sequence
echo "1. Reproducing CI command sequence..."

# The CI does this find command:
find . -name "*.py" \
  ! -path "*/.venv/*" \
  ! -path "*/node_modules/*" \
  ! -path "*/dist/*" \
  ! -name "__init__.py" \
  -type f > /tmp/python_files.txt

echo "Found $(wc -l < /tmp/python_files.txt) Python files"
echo ""

# Process each file exactly like the CI does
echo "2. Processing files like CI does..."
counter=0
while read -r file; do
  counter=$((counter + 1))
  if [ $counter -gt 5 ]; then break; fi  # Only test first 5 files
  
  echo "File $counter: '$file'"
  
  # Exact CI commands
  func_count=$(grep -c "^def " "$file" 2>/dev/null || echo 0)
  class_count=$(grep -c "^class " "$file" 2>/dev/null || echo 0)
  docstring_count=$(grep -c '"""' "$file" 2>/dev/null || echo 0)
  
  echo "  Raw results: func='$func_count' class='$class_count' docstring='$docstring_count'"
  
  # CI variable assignments
  func_count=${func_count:-0}
  class_count=${class_count:-0}
  docstring_count=${docstring_count:-0}
  
  echo "  After assignment: func='$func_count' class='$class_count' docstring='$docstring_count'"
  
  # Check for spaces or unusual characters
  if [[ "$func_count" =~ [[:space:]] ]]; then
    echo "  ERROR: func_count contains whitespace: '$func_count'"
  fi
  if [[ "$class_count" =~ [[:space:]] ]]; then
    echo "  ERROR: class_count contains whitespace: '$class_count'"
  fi
  
  # The failing arithmetic operation
  echo "  Testing arithmetic: total=\$((func_count + class_count))"
  if total=$((func_count + class_count)) 2>/dev/null; then
    echo "  Success: total=$total"
  else
    echo "  ARITHMETIC FAILED! func_count='$func_count' class_count='$class_count'"
    # This is the error we're looking for!
  fi
  
  # The failing conditional test
  echo "  Testing conditional: [ \"\$total\" -gt 0 ]"
  if [ "$total" -gt 0 ] 2>/dev/null; then
    echo "  Conditional success"
  else
    echo "  CONDITIONAL FAILED! total='$total'"
    # This could also be the error
  fi
  
  echo ""
done < /tmp/python_files.txt

echo ""
echo "3. Looking for specific problematic patterns..."

# Check for files that might have unusual grep output
while read -r file; do
  # Check if grep returns anything unusual
  func_output=$(grep -c "^def " "$file" 2>&1)
  class_output=$(grep -c "^class " "$file" 2>&1)
  
  # Check if output contains anything other than a single number
  if [[ ! "$func_output" =~ ^[0-9]+$ ]]; then
    echo "Unusual func output for $file: '$func_output'"
  fi
  if [[ ! "$class_output" =~ ^[0-9]+$ ]]; then
    echo "Unusual class output for $file: '$class_output'"
  fi
done < /tmp/python_files.txt

echo ""
echo "4. Checking file properties..."

# Check for files with unusual properties
head -3 /tmp/python_files.txt | while read -r file; do
  echo "Checking file: $file"
  
  # File type
  file_type=$(file "$file" 2>/dev/null)
  echo "  Type: $file_type"
  
  # Check for binary content
  if file "$file" | grep -q "binary"; then
    echo "  WARNING: File appears to be binary"
  fi
  
  # Check for unusual line endings
  if file "$file" | grep -q "CRLF"; then
    echo "  WARNING: File has Windows line endings"
  fi
  
  # Check first few bytes for unusual characters
  if head -c 100 "$file" | grep -q $'\0'; then
    echo "  WARNING: File contains null bytes"
  fi
  
  echo ""
done

rm -f /tmp/python_files.txt