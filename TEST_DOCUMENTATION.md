# OSv Test Documentation

This document describes the test infrastructure for the OSv unikernel project.

## Project Type

OSv is a **C/C++ systems programming project** (unikernel/operating system). It does not use or require web testing frameworks like Playwright. The test infrastructure is specifically designed for testing operating system components, kernel functionality, and system-level APIs.

## Test Infrastructure Overview

### Test Categories

OSv has several categories of tests:

1. **Unit Tests** - C++ tests for kernel components (located in `tests/` directory)
2. **Python Integration Tests** - Tests that boot OSv VMs and verify functionality
3. **HTTP API Tests** - Tests for the management HTTP server API
4. **Module Tests** - Tests for specific OSv modules

### Directory Structure

```
osv/
├── tests/                          # C/C++ unit tests for kernel
│   ├── tst-*.cc                   # Individual test files
│   └── misc-*.cc/cpp              # Miscellaneous tests
├── scripts/
│   ├── test.py                    # Main test orchestration script
│   ├── test-ruby.py               # Ruby-specific tests
│   └── tests/                     # Python integration tests
│       ├── test_app.py            # Application testing framework
│       ├── test_http_app.py       # HTTP application tests
│       ├── test_tracing.py        # Tracing functionality tests
│       ├── test_net.py            # Network tests
│       └── testing.py             # Test utilities and helpers
├── modules/
│   ├── httpserver-api/tests/      # HTTP server API tests
│   ├── httpserver-html5-gui/tests/# HTML5 GUI tests
│   └── nfs-tests/                 # NFS functionality tests
```

## Running Tests

### Building and Running All Tests

To build OSv and run all unit tests:

```bash
./scripts/build check
```

This command:
1. Builds the OSv kernel
2. Builds the test modules
3. Runs the unit tests in a VM

### Running Specific Tests

To run a specific test application:

```bash
./scripts/build image=tests
./scripts/run.py -e "/tests/tst-<test-name>.so"
```

### Running Python Integration Tests

The Python integration tests require a built OSv image:

```bash
# Build OSv with the default image
./scripts/build

# Run application tests
cd scripts/tests
python3 test_app.py --hypervisor qemu --image ../../build/release/usr.img

# Run HTTP tests
python3 test_http_app.py --hypervisor qemu
```

### Running HTTP API Tests

To test the HTTP server API module:

```bash
cd modules/httpserver-api/tests
python3 testhttpserver-api.py --run_script ../../../scripts/run.py
```

## Test Writing Guidelines

### C/C++ Unit Tests

Unit tests are written in C++ and located in the `tests/` directory. Each test file should:

- Be named `tst-<feature>.cc`
- Include the test framework headers
- Implement test cases that can run in the OSv environment
- Return 0 on success, non-zero on failure

Example structure:
```cpp
#include <osv/debug.h>

int main(int argc, char **argv) {
    // Test implementation
    debug("Test passed\n");
    return 0;
}
```

### Python Integration Tests

Python tests use the testing framework in `scripts/tests/testing.py`. Tests should:

- Import the testing framework: `from testing import *`
- Use `run_command_in_guest()` to execute commands in OSv
- Use `wait_for_line_contains()` to verify output
- Handle VM lifecycle properly (start, verify, cleanup)

Example:
```python
from testing import *

def test_my_feature():
    app = run_command_in_guest("/my_app", hypervisor="qemu")
    wait_for_line_contains(app, "Expected output")
    app.join()
```

## Test Dependencies

### Required Tools

- **GCC/Clang** - For building C/C++ tests
- **Python 3** - For test orchestration and integration tests
- **QEMU** or **Firecracker** - For running OSv VMs
- **Make** - Build system

### Python Packages

Integration tests may require:
- `requests` - For HTTP API tests
- Standard library modules (no external dependencies for basic tests)

## Continuous Integration

Tests are integrated into the CI/CD pipeline via Travis CI (`.travis.yml`). The CI runs:

1. Full kernel build
2. Unit test suite
3. Selected integration tests
4. Module tests

## Troubleshooting

### Tests Fail to Start

- Ensure OSv is built: `./scripts/build`
- Check hypervisor is available: `qemu-system-x86_64 --version`
- Verify image path is correct

### Tests Timeout

- Increase timeout in test scripts
- Check system resources (CPU, memory)
- Verify hypervisor performance

### HTTP API Tests Fail

- Ensure HTTP server module is included in the image
- Check port forwarding configuration
- Verify firewall settings

## Adding New Tests

When adding new functionality to OSv:

1. **For kernel features**: Add a C++ unit test in `tests/tst-<feature>.cc`
2. **For Python tools**: Add tests in `scripts/tests/`
3. **For modules**: Add tests in the module's `tests/` directory
4. **Update this documentation** if introducing new test patterns

## Test Coverage

While comprehensive test coverage is desired, not all utility scripts require tests:

- **Module configuration files** (`module.py`) - Generally don't need tests
- **Build scripts** - Tested implicitly through the build process
- **One-off utilities** - May not warrant test coverage

Focus testing efforts on:
- Core kernel functionality
- User-facing APIs
- Critical system components
- Regression-prone areas

## Additional Resources

- [OSv Wiki - Testing](https://github.com/cloudius-systems/osv/wiki)
- [OSv Wiki - Debugging](https://github.com/cloudius-systems/osv/wiki/Debugging-OSv)
- [scripts/README.md](scripts/README.md) - Build and test script documentation

---

**Note**: This project does not use Playwright or browser-based testing frameworks. Automated workflows suggesting Playwright integration should recognize this as a C/C++/systems programming project.
