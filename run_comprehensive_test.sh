#!/bin/bash
cd /workspace

echo "Compiling comprehensive test..."
g++ -std=gnu++11 -o comprehensive_test comprehensive_test.cpp
if [ $? -eq 0 ]; then
    echo "Compilation successful, running tests..."
    ./comprehensive_test
else
    echo "Compilation failed"
    exit 1
fi