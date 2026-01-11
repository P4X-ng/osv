#!/bin/bash

echo "Checking for Python files with spaces in names..."
find . -name "*.py" -type f | while IFS= read -r file; do
  if [[ "$file" == *" "* ]]; then
    echo "Found file with space: '$file'"
  fi
done

echo ""
echo "Checking for any unusual Python files..."
find . -name "*.py" -type f | head -10 | while IFS= read -r file; do
  echo "File: '$file'"
  # Check if file exists and is readable
  if [[ ! -r "$file" ]]; then
    echo "  ERROR: File not readable"
  fi
  
  # Check file size
  size=$(wc -c < "$file" 2>/dev/null || echo "unknown")
  echo "  Size: $size bytes"
  
  # Test the exact grep commands that are failing
  func_count=$(grep -c "^def " "$file" 2>/dev/null || echo 0)
  class_count=$(grep -c "^class " "$file" 2>/dev/null || echo 0)
  
  echo "  func_count result: '$func_count'"
  echo "  class_count result: '$class_count'"
  
  # Check if results contain spaces
  if [[ "$func_count" == *" "* ]]; then
    echo "  ERROR: func_count contains spaces!"
  fi
  if [[ "$class_count" == *" "* ]]; then
    echo "  ERROR: class_count contains spaces!"
  fi
  
  echo ""
done

echo "Checking for any files that might cause issues..."
# Look for files with unusual names or content
find . -name "*.py" -type f -print0 | while IFS= read -r -d '' file; do
  # Check if filename has unusual characters
  if [[ "$file" =~ [^[:print:]] ]]; then
    echo "File with non-printable characters: '$file'"
  fi
done