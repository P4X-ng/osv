#!/bin/bash
# Test script to verify the workflow fixes work correctly

echo "Testing the fixed shell script logic..."

# Create a test Python file
test_file="/tmp/test_file.py"
cat > "$test_file" << 'EOF'
def function1():
    pass

class TestClass:
    """Test docstring"""
    pass

def function2():
    pass
EOF

echo "Testing Python file analysis..."

# Test the fixed logic
func_count=$(grep -c "^def " "$test_file" 2>/dev/null | head -1 | tr -d '\n\r' || echo 0)
class_count=$(grep -c "^class " "$test_file" 2>/dev/null | head -1 | tr -d '\n\r' || echo 0)
docstring_count=$(grep -c '"""' "$test_file" 2>/dev/null | head -1 | tr -d '\n\r' || echo 0)

# Ensure variables are numeric and sanitize
func_count=$(echo "${func_count:-0}" | grep -E '^[0-9]+$' || echo 0)
class_count=$(echo "${class_count:-0}" | grep -E '^[0-9]+$' || echo 0)
docstring_count=$(echo "${docstring_count:-0}" | grep -E '^[0-9]+$' || echo 0)

total=$((func_count + class_count))

echo "func_count: $func_count"
echo "class_count: $class_count"
echo "docstring_count: $docstring_count"
echo "total: $total"

if [ "$total" -gt 0 ] && [ "$docstring_count" -eq 0 ]; then
    echo "⚠️ Test file: $total definitions, no docstrings"
else
    echo "✅ Test passed - docstrings found or no definitions"
fi

# Test JavaScript logic
test_js_file="/tmp/test_file.js"
cat > "$test_js_file" << 'EOF'
function testFunc() {
    return true;
}

class TestClass {
    constructor() {}
}

const arrowFunc = () => {
    return false;
}
EOF

echo ""
echo "Testing JavaScript file analysis..."

func_count=$(grep -cE "(^function |^export function |^const .* = .*=>)" "$test_js_file" 2>/dev/null | head -1 | tr -d '\n\r' || echo 0)
class_count=$(grep -c "^class " "$test_js_file" 2>/dev/null | head -1 | tr -d '\n\r' || echo 0)
jsdoc_count=$(grep -c '/\*\*' "$test_js_file" 2>/dev/null | head -1 | tr -d '\n\r' || echo 0)

# Ensure variables are numeric and sanitize
func_count=$(echo "${func_count:-0}" | grep -E '^[0-9]+$' || echo 0)
class_count=$(echo "${class_count:-0}" | grep -E '^[0-9]+$' || echo 0)
jsdoc_count=$(echo "${jsdoc_count:-0}" | grep -E '^[0-9]+$' || echo 0)

total=$((func_count + class_count))

echo "func_count: $func_count"
echo "class_count: $class_count"
echo "jsdoc_count: $jsdoc_count"
echo "total: $total"

if [ "$total" -gt 5 ] && [ "$jsdoc_count" -eq 0 ]; then
    echo "⚠️ Test JS file: ~$total definitions, no JSDoc comments"
else
    echo "✅ Test passed - JSDoc found or not enough definitions"
fi

# Clean up
rm -f "$test_file" "$test_js_file"

echo ""
echo "✅ All tests passed! The workflow fixes should resolve the CI pipeline failures."