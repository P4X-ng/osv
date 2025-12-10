#!/bin/bash
cd /workspace
echo "Testing basic regex compilation..."
g++ -std=gnu++11 -o simple_regex_test simple_regex_test.cpp
if [ $? -eq 0 ]; then
    echo "Basic regex compilation: SUCCESS"
    ./simple_regex_test
else
    echo "Basic regex compilation: FAILED"
    exit 1
fi

echo ""
echo "Testing full parser compilation..."
g++ -std=gnu++11 -o test_regex_parser test_regex_parser.cpp
if [ $? -eq 0 ]; then
    echo "Parser compilation: SUCCESS"
    ./test_regex_parser
else
    echo "Parser compilation: FAILED"
    exit 1
fi