#!/bin/bash
cd /workspace
g++ -std=c++11 -o test_parser test_parser.cpp
if [ $? -eq 0 ]; then
    echo "Compilation successful, running tests..."
    ./test_parser
else
    echo "Compilation failed"
fi