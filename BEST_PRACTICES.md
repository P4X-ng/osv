# OSv Best Practices and Common Pitfalls

## Development Best Practices

### 1. Build Environment Setup

**Recommended Approach: Use Docker**
```bash
# Use official OSv builder image
docker run -it --privileged \
  -v $(pwd):/workspace \
  osvunikernel/osv-ubuntu-20.10-builder

# Or build your own
cd docker && docker build -t osv-builder .
```

**Benefits:**
- Consistent build environment
- All dependencies pre-installed
- Isolated from host system
- Reproducible builds

### 2. Development Workflow

**Incremental Development:**
```bash
# Fast incremental builds
export MAKEFLAGS=-j$(nproc)
./scripts/build

# Quick testing
./scripts/run.py -e '/my_app'

# Debug mode for development
./scripts/build mode=debug
./scripts/run.py --debug
```

**Testing Strategy:**
```bash
# Unit tests
./scripts/build check

# Specific test suites
./scripts/build image=tests
./scripts/test.py -p qemu

# Performance testing
./scripts/build image=tests
./scripts/test.py -p firecracker --benchmark
```

### 3. Code Organization

**Module Structure:**
```
modules/my_app/
├── Makefile          # Build configuration
├── module.py         # Module metadata and dependencies
├── usr.manifest      # Files to include in image
├── src/             # Source code
│   ├── main.cc
│   └── utils.cc
├── include/         # Headers
└── tests/           # Unit tests
```

**Dependency Management:**
```python
# module.py - Declare dependencies explicitly
from osv.modules import api

api.require('java-base')      # For Java applications
api.require('httpserver')     # For REST APIs
api.require('openssl')        # For crypto operations

default = api.run('/my_app --config /config.json')
```

### 4. Memory Management

**Use RAII and Smart Pointers:**
```cpp
#include <memory>

// Prefer smart pointers
std::unique_ptr<MyClass> obj = std::make_unique<MyClass>();
std::shared_ptr<Resource> resource = std::make_shared<Resource>();

// Avoid raw pointers for ownership
// BAD: MyClass* obj = new MyClass();
// GOOD: auto obj = std::make_unique<MyClass>();
```

**Memory Pool Usage:**
```cpp
#include <osv/mempool.hh>

// For frequent allocations of same size
memory::pool custom_pool(sizeof(MyStruct), 1024);

void* ptr = custom_pool.alloc();
// Use ptr...
custom_pool.free(ptr);
```

### 5. Threading Best Practices

**Thread Creation:**
```cpp
#include <osv/sched.hh>

// Create threads with proper stack size
sched::thread* t = sched::thread::make([]{
    // Thread work
}, sched::thread::attr().stack(64*1024));  // 64KB stack

t->start();
```

**Synchronization:**
```cpp
#include <osv/mutex.h>
#include <osv/condvar.h>

// Use OSv synchronization primitives
mutex mtx;
condvar cv;

// Producer
{
    std::lock_guard<mutex> lock(mtx);
    // Modify shared data
    cv.notify_one();
}

// Consumer
{
    std::unique_lock<mutex> lock(mtx);
    cv.wait(lock, []{ return condition_met(); });
}
```

## Performance Optimization

### 1. Build Optimizations

**Release Builds:**
```bash
# Always use release mode for production
./scripts/build mode=release

# Enable additional optimizations
./scripts/build CXXFLAGS="-O3 -march=native"
```

**Link-Time Optimization:**
```makefile
# In custom Makefile
CXXFLAGS += -flto
LDFLAGS += -flto
```

### 2. Memory Optimizations

**Lazy Stack Allocation:**
```makefile
# For applications with many threads
conf_lazy_stack=1
```

**Memory Pool Tuning:**
```cpp
// Tune pool sizes for your workload
memory::set_pool_size(memory::pool_type::small, 1024*1024);
memory::set_pool_size(memory::pool_type::large, 16*1024*1024);
```

### 3. Network Performance

**Use vhost-net:**
```bash
# Better network performance
sudo ./scripts/run.py -nv
```

**Buffer Tuning:**
```cpp
// Increase network buffer sizes
setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
```

### 4. Filesystem Performance

**Choose Right Filesystem:**
```bash
# RAMFS for read-only, small applications
./scripts/build fs=ramfs image=my_app

# ROFS for read-only, larger applications  
./scripts/build fs=rofs image=my_app

# ZFS for read-write applications
./scripts/build fs=zfs image=my_app
```

## Common Pitfalls and Solutions

### 1. Memory Issues

**Problem: Stack Overflow**
```
Symptoms: Random crashes, segmentation faults
Cause: Default stack size too small for deep recursion
```

**Solution:**
```cpp
// Increase stack size for threads
sched::thread::attr attr;
attr.stack(256*1024);  // 256KB instead of default 64KB
sched::thread* t = sched::thread::make(work_func, attr);
```

**Problem: Memory Leaks**
```
Symptoms: Increasing memory usage over time
Cause: Missing delete/free calls
```

**Solution:**
```cpp
// Use RAII and smart pointers
{
    auto resource = std::make_unique<Resource>();
    // Automatic cleanup when scope exits
}

// For C-style APIs
struct ResourceDeleter {
    void operator()(Resource* r) { resource_free(r); }
};
std::unique_ptr<Resource, ResourceDeleter> resource(resource_alloc());
```

### 2. Threading Issues

**Problem: Deadlocks**
```
Symptoms: Application hangs
Cause: Lock ordering issues, blocking in critical sections
```

**Solution:**
```cpp
// Always acquire locks in same order
void transfer(Account& from, Account& to, int amount) {
    // Avoid deadlock by ordering locks
    if (&from < &to) {
        std::lock_guard<mutex> lock1(from.mtx);
        std::lock_guard<mutex> lock2(to.mtx);
        // Transfer logic
    } else {
        std::lock_guard<mutex> lock1(to.mtx);
        std::lock_guard<mutex> lock2(from.mtx);
        // Transfer logic
    }
}
```

**Problem: Race Conditions**
```
Symptoms: Inconsistent behavior, data corruption
Cause: Unsynchronized access to shared data
```

**Solution:**
```cpp
// Use atomic operations for simple cases
std::atomic<int> counter{0};
counter.fetch_add(1);

// Use locks for complex operations
mutex data_mutex;
std::lock_guard<mutex> lock(data_mutex);
// Access shared data safely
```

### 3. Build Issues

**Problem: Cross-compilation Failures**
```
Symptoms: Build errors with wrong architecture
Cause: Incorrect toolchain setup
```

**Solution:**
```bash
# Use Docker for consistent environment
docker run -it osvunikernel/osv-ubuntu-20.10-builder

# Or set up cross-compilation properly
export CROSS_PREFIX=aarch64-linux-gnu-
./scripts/build arch=aarch64
```

**Problem: Module Dependency Errors**
```
Symptoms: Undefined symbols, missing files
Cause: Missing or incorrect module dependencies
```

**Solution:**
```python
# In module.py, declare all dependencies
from osv.modules import api

api.require('java-base')      # For Java
api.require('httpserver')     # For HTTP
api.require('openssl')        # For SSL/TLS

# Check dependency chain
./scripts/module.py query my_module
```

### 4. Runtime Issues

**Problem: Boot Failures**
```
Symptoms: Kernel panic, boot hangs
Cause: Incorrect kernel parameters, missing files
```

**Solution:**
```bash
# Check kernel command line
./scripts/run.py -e '--help'

# Verify image contents
./scripts/run.py -e 'ls -la /'

# Use debug mode
./scripts/build mode=debug
./scripts/run.py --debug
```

**Problem: Application Crashes**
```
Symptoms: Segmentation faults, abort signals
Cause: Various - memory corruption, null pointers, etc.
```

**Solution:**
```bash
# Enable debugging
./scripts/build mode=debug conf_debug_memory=1

# Use GDB
./scripts/run.py --debug -e '/my_app'
# In another terminal:
gdb build/debug/loader.elf
(gdb) target remote :1234
(gdb) continue
```

### 5. Performance Issues

**Problem: Poor Network Performance**
```
Symptoms: Low throughput, high latency
Cause: Suboptimal network configuration
```

**Solution:**
```bash
# Use vhost-net
sudo ./scripts/run.py -nv

# Tune network parameters
./scripts/run.py -e '--ip=eth0,192.168.1.100,255.255.255.0 \
                      --defaultgw=192.168.1.1 \
                      --nameserver=8.8.8.8 \
                      /my_app'
```

**Problem: High Memory Usage**
```
Symptoms: Out of memory errors
Cause: Memory leaks, inefficient allocation
```

**Solution:**
```bash
# Enable lazy stacks
./scripts/build conf_lazy_stack=1

# Use memory debugging
./scripts/build conf_debug_memory=1

# Monitor memory usage
./scripts/run.py -e 'free && /my_app'
```

## Security Best Practices

### 1. Symbol Visibility

**Hide Internal Symbols:**
```makefile
# Reduce attack surface
conf_hide_symbols=1
```

### 2. Input Validation

**Validate All Inputs:**
```cpp
bool validate_input(const std::string& input) {
    if (input.empty() || input.size() > MAX_INPUT_SIZE) {
        return false;
    }
    
    // Check for malicious patterns
    if (input.find("../") != std::string::npos) {
        return false;
    }
    
    return true;
}
```

### 3. Safe String Handling

**Use Safe String Functions:**
```cpp
#include <osv/string_utils.hh>

// Use safe string operations
char buffer[256];
safe_strncpy(buffer, source, sizeof(buffer));

// Or use C++ strings
std::string safe_str = sanitize_input(user_input);
```

## Debugging Techniques

### 1. Enable Debug Features

```bash
# Debug build with all features
./scripts/build mode=debug \
                conf_debug_memory=1 \
                conf_tracing=1 \
                conf_debug_elf=1
```

### 2. Use Tracing

```cpp
#include <osv/trace.hh>

TRACEPOINT(trace_my_function, "param=%d result=%s", int, const char*);

void my_function(int param) {
    const char* result = process(param);
    trace_my_function(param, result);
}
```

### 3. Memory Debugging

```bash
# Check for memory leaks
./scripts/run.py -e '--trace=memory* /my_app'

# Analyze memory usage
./scripts/trace.py -t build/release/trace.bin
```

### 4. GDB Debugging

```bash
# Start with debugging
./scripts/run.py --debug -e '/my_app'

# Connect GDB
gdb build/debug/loader.elf
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

## Monitoring and Profiling

### 1. Built-in Monitoring

```bash
# Use REST API for monitoring
./scripts/build image=httpserver-monitoring-api,my_app
./scripts/run.py -e '/my_app'

# Access monitoring data
curl http://192.168.122.15:8000/os/memory
curl http://192.168.122.15:8000/os/threads
```

### 2. Performance Profiling

```bash
# Enable tracing for profiling
./scripts/run.py -e '--trace=sched*,lock* /my_app'

# Analyze performance
./scripts/trace.py -t build/release/trace.bin --summary
```

This guide covers the most important best practices and common pitfalls when developing with OSv. Following these guidelines will help you build robust, performant, and maintainable OSv applications.