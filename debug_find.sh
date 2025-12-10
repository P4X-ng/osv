#!/bin/bash

echo "=== Debugging the find command that CI uses ==="
echo ""

echo "1. Testing the exact find command from CI:"
echo "find . -name \"*.py\" ! -path \"*/.venv/*\" ! -path \"*/node_modules/*\" ! -path \"*/dist/*\" ! -name \"__init__.py\" -type f"
echo ""

find . -name "*.py" \
  ! -path "*/.venv/*" \
  ! -path "*/node_modules/*" \
  ! -path "*/dist/*" \
  ! -name "__init__.py" \
  -type f | head -5 | while IFS= read -r file; do
  
  echo "Processing file: '$file'"
  
  # Check if it's a regular file
  if [[ ! -f "$file" ]]; then
    echo "  ERROR: Not a regular file!"
  fi
  
  # Check if it's readable
  if [[ ! -r "$file" ]]; then
    echo "  ERROR: Not readable!"
  fi
  
  # Run the exact grep commands from CI
  echo "  Running: grep -c \"^def \" \"$file\" 2>/dev/null || echo 0"
  func_count=$(grep -c "^def " "$file" 2>/dev/null || echo 0)
  echo "  Result: '$func_count'"
  
  echo "  Running: grep -c \"^class \" \"$file\" 2>/dev/null || echo 0"
  class_count=$(grep -c "^class " "$file" 2>/dev/null || echo 0)
  echo "  Result: '$class_count'"
  
  # Test the variable assignments that are failing
  echo "  Testing: func_count=\${func_count:-0}"
  func_count=${func_count:-0}
  echo "  After assignment: '$func_count'"
  
  echo "  Testing: class_count=\${class_count:-0}"
  class_count=${class_count:-0}
  echo "  After assignment: '$class_count'"
  
  # Test the arithmetic that's failing
  echo "  Testing: total=\$((func_count + class_count))"
  if total=$((func_count + class_count)) 2>/dev/null; then
    echo "  Arithmetic success: total=$total"
  else
    echo "  ERROR: Arithmetic failed with func_count='$func_count' class_count='$class_count'"
  fi
  
  echo ""
done

echo ""
echo "2. Checking for any files that might have unusual output:"

# Test a few specific files that might be problematic
test_files=(
  "./scripts/test.py"
  "./scripts/run.py" 
  "./scripts/setup.py"
)

for file in "${test_files[@]}"; do
  if [[ -f "$file" ]]; then
    echo "Testing specific file: $file"
    
    # Show first few lines to see if there's anything unusual
    echo "  First 3 lines:"
    head -3 "$file" | sed 's/^/    /'
    
    # Test grep commands
    func_result=$(grep -c "^def " "$file" 2>/dev/null || echo 0)
    class_result=$(grep -c "^class " "$file" 2>/dev/null || echo 0)
    
    echo "  grep results: func='$func_result' class='$class_result'"
    
    # Check for any unusual characters in the results
    if [[ "$func_result" =~ [^0-9] ]]; then
      echo "  WARNING: func_result contains non-numeric characters"
    fi
    if [[ "$class_result" =~ [^0-9] ]]; then
      echo "  WARNING: class_result contains non-numeric characters"
    fi
    
    echo ""
  fi
done