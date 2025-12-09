# OSv Common Issues and Pitfalls

## Critical Issues Identified

### 1. Memory Management Issues

**Deadlock in Lazy Stack Implementation**
```cpp
// core/mmu.cc:52 - Known deadlock scenario
// Page fault handling attempts to take vma_list_mutex for read
// while it's already held for write, causing deadlock
#if CONF_lazy_stack
#define PREVENT_STACK_PAGE_FAULT \
    arch::ensure_next_two_stack_pages();
#endif
```

**Memory Pool Contention**
```cpp
// core/mempool.cc - Global lock causes contention
void* malloc(size_t size) {
    std::lock_guard<mutex> lock(pool_mutex);  // Bottleneck
    return allocate_from_pool(size);
}
```

**Solutions:**
- Use per-CPU memory pools
- Implement lock-free allocation for small objects
- Add memory pressure handling

### 2. Network Stack Issues

**Lock Ordering Problems**
```cpp
// Multiple locks in network path create deadlock potential
void net_receive() {
    lock(net_lock);      // Lock 1
    lock(queue_lock);    // Lock 2  
    lock(socket_lock);   // Lock 3 - Potential deadlock
}
```

**Performance Bottlenecks**
- Global network locks limit scalability
- Inefficient mbuf management
- High interrupt overhead

**Solutions:**
- Implement per-CPU network queues
- Use lock-free ring buffers
- Add NAPI-style interrupt mitigation

### 3. Scheduler Issues

**Race Conditions in Thread Management**
```cpp
// core/sched.cc - Thread state races
void thread::wake() {
    _state = thread_state::runnable;  // Race with scheduler
}
```

**Cooperative Scheduling Limitations**
- Threads can monopolize CPU
- No preemption for long-running tasks
- Poor interactive response

**Solutions:**
- Add memory barriers for state transitions
- Implement priority inheritance
- Use preemptive scheduling for interactive workloads

### 4. Security Vulnerabilities

**Buffer Overflow Risks**
```cpp
// drivers/virtio-net.cc - No bounds checking
void process_packet(char* data, size_t len) {
    char buffer[1500];
    memcpy(buffer, data, len);  // Potential overflow
}
```

**Input Validation Issues**
```cpp
// modules/httpserver - Path traversal vulnerability
void handle_request(const std::string& path) {
    std::ifstream file(path);  // No path validation
}
```

**Solutions:**
- Add comprehensive bounds checking
- Implement input validation
- Use safe string functions

## Build and Configuration Issues

### 1. Cross-Compilation Problems

**Toolchain Inconsistencies**
```bash
# Common error: Wrong architecture binaries
error: cannot execute binary file: Exec format error
```

**Solutions:**
```bash
# Use Docker for consistent environment
docker run -it osvunikernel/osv-ubuntu-20.10-builder

# Verify toolchain
export CROSS_PREFIX=aarch64-linux-gnu-
./scripts/build arch=aarch64
```

### 2. Module Dependency Issues

**Circular Dependencies**
```python
# module.py - Can cause build failures
api.require('module_a')  # module_a requires module_b
# module_b requires module_a -> circular dependency
```

**Missing Dependencies**
```
Undefined symbol: some_function
```

**Solutions:**
```python
# Explicit dependency declaration
from osv.modules import api

api.require('java-base')
api.require('httpserver') 
api.require('openssl')

# Check dependencies
./scripts/module.py deps my_module
```

### 3. Configuration Conflicts

**Incompatible Options**
```makefile
# These can conflict
conf_preempt=1
conf_lazy_stack=1  # May not work well together
```

**Solutions:**
- Use tested configuration profiles
- Validate configuration combinations
- Document known conflicts

## Runtime Issues

### 1. Boot Failures

**Common Boot Problems**
```
Kernel panic: Unable to mount root filesystem
```

**Causes:**
- Incorrect filesystem type
- Missing kernel modules
- Wrong boot parameters

**Solutions:**
```bash
# Verify image contents
./scripts/run.py -e 'ls -la /'

# Check filesystem
./scripts/build fs=rofs image=my_app  # Try different FS

# Debug boot process
./scripts/run.py --debug -e '/my_app'
```

### 2. Application Crashes

**Segmentation Faults**
```
Segmentation fault at 0x0000000000000000
```

**Common Causes:**
- Null pointer dereference
- Stack overflow
- Use-after-free

**Debugging:**
```bash
# Enable debugging
./scripts/build mode=debug conf_debug_memory=1

# Use GDB
./scripts/run.py --debug -e '/my_app'
gdb build/debug/loader.elf
(gdb) target remote :1234
```

### 3. Memory Issues

**Out of Memory Errors**
```
Cannot allocate memory
```

**Solutions:**
```bash
# Increase memory
./scripts/run.py -m 2G -e '/my_app'

# Enable lazy stacks
./scripts/build conf_lazy_stack=1

# Monitor memory usage
./scripts/run.py -e 'free -h && /my_app'
```

## Performance Issues

### 1. Slow Boot Times

**Causes:**
- VGA console initialization (60-70ms)
- PCI enumeration (10-20ms)
- ZFS initialization

**Solutions:**
```bash
# Disable VGA console
./scripts/run.py -e '--console serial /my_app'

# Disable PCI enumeration
./scripts/run.py -e '--nopci /my_app'

# Use faster filesystem
./scripts/build fs=rofs image=my_app
```

### 2. Poor Network Performance

**Symptoms:**
- Low throughput
- High latency
- Packet drops

**Solutions:**
```bash
# Use vhost-net
sudo ./scripts/run.py -nv

# Tune buffers
setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

# Use multiple queues
./scripts/run.py --cpus 4 -e '/my_app'
```

### 3. High Memory Usage

**Causes:**
- Memory leaks
- Inefficient allocation patterns
- Large thread stacks

**Solutions:**
```cpp
// Use memory pools
memory::pool custom_pool(sizeof(MyStruct), 1024);

// Enable lazy stacks
conf_lazy_stack=1

// Monitor allocations
conf_debug_memory=1
```

## Hypervisor-Specific Issues

### 1. QEMU/KVM Issues

**Performance Problems**
```bash
# Ensure KVM is enabled
ls -la /dev/kvm
sudo usermod -aG kvm $USER

# Use appropriate machine type
./scripts/run.py --machine q35 -e '/my_app'
```

### 2. Firecracker Issues

**Network Configuration**
```bash
# Network must be explicitly enabled
./scripts/firecracker.py -n -e '/my_app'

# Configure tap device
./scripts/create_tap_device.sh natted fc_tap0 172.18.0.1
```

### 3. VMware Issues

**Driver Compatibility**
- VMware tools equivalent needed
- Specific hardware version requirements
- VMXNET3 driver issues

**Solutions:**
```bash
# Use compatible hardware version
# Configure appropriate network adapter
# Install VMware-specific drivers
```

## Development Pitfalls

### 1. Threading Mistakes

**Incorrect Synchronization**
```cpp
// BAD: Race condition
if (shared_data == expected_value) {
    shared_data = new_value;  // Another thread may change it
}

// GOOD: Atomic operation
shared_data.compare_exchange_strong(expected_value, new_value);
```

### 2. Memory Management Errors

**Use-After-Free**
```cpp
// BAD: Dangling pointer
delete ptr;
use_pointer(ptr);  // Use after free

// GOOD: Smart pointers
auto ptr = std::make_unique<MyClass>();
// Automatic cleanup
```

### 3. Error Handling Issues

**Inconsistent Error Handling**
```cpp
// BAD: Mixed error handling styles
if (result == -1) return -EINVAL;    // Sometimes
if (result == -1) return nullptr;    // Other times
if (result == -1) throw exception;   // Sometimes
```

## Debugging Strategies

### 1. Enable Debug Features

```bash
# Comprehensive debugging
./scripts/build mode=debug \
                conf_debug_memory=1 \
                conf_tracing=1 \
                conf_debug_elf=1
```

### 2. Use Tracing

```cpp
#include <osv/trace.hh>

TRACEPOINT(trace_my_function, "param=%d", int);

void my_function(int param) {
    trace_my_function(param);
}
```

### 3. Memory Analysis

```bash
# Check for leaks
./scripts/run.py -e '--trace=memory* /my_app'

# Analyze traces
./scripts/trace.py -t build/release/trace.bin
```

## Prevention Strategies

### 1. Code Review Checklist

- [ ] Bounds checking on all buffer operations
- [ ] Proper error handling
- [ ] Lock ordering documented
- [ ] Memory management using RAII
- [ ] Input validation implemented

### 2. Testing Strategy

```bash
# Unit tests
./scripts/build check

# Stress testing
./scripts/test.py --stress

# Memory testing
./scripts/build conf_debug_memory=1
./scripts/test.py --memory-check
```

### 3. Continuous Integration

```yaml
# CI pipeline should include:
- Unit tests
- Integration tests  
- Memory leak detection
- Performance regression tests
- Security scans
```

## Recovery Procedures

### 1. Boot Recovery

```bash
# If boot fails, try minimal configuration
./scripts/build fs=ramfs image=minimal
./scripts/run.py -e '/hello'

# Gradually add features back
```

### 2. Performance Recovery

```bash
# Profile to identify bottlenecks
./scripts/run.py -e '--trace=sched*,lock* /my_app'

# Analyze results
./scripts/trace.py -t build/release/trace.bin --summary
```

### 3. Memory Recovery

```bash
# Check memory usage
./scripts/run.py -e 'free -h'

# Enable memory debugging
./scripts/build conf_debug_memory=1
```

This document covers the most common issues and pitfalls encountered when working with OSv. Regular review of these patterns will help prevent many problems during development and deployment.