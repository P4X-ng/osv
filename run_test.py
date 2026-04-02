#!/usr/bin/env python3
import subprocess
import os

os.chdir('/workspace')

# Compile the test
compile_result = subprocess.run(['g++', '-std=c++11', '-o', 'test_rofs_fix', 'test_rofs_fix.cc'], 
                               capture_output=True, text=True)

if compile_result.returncode != 0:
    print("Compilation failed:")
    print(compile_result.stderr)
    exit(1)

print("Compilation successful!")

# Run the test
run_result = subprocess.run(['./test_rofs_fix'], capture_output=True, text=True)

print("Test output:")
print(run_result.stdout)

if run_result.stderr:
    print("Test errors:")
    print(run_result.stderr)

print(f"Test exit code: {run_result.returncode}")