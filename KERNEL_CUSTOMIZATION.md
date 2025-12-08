# OSv Kernel Customization Guide

## Overview

OSv provides extensive kernel customization capabilities through its configuration system, modular architecture, and build-time options. This guide covers advanced customization techniques for tailoring OSv to specific use cases.

## Configuration System Deep Dive

### Configuration Hierarchy

OSv's configuration system follows a hierarchical structure:

1. **Base Configuration** (`conf/base.mk`) - Default settings
2. **Architecture Configuration** (`conf/{arch}.mk`) - Architecture-specific settings  
3. **Mode Configuration** (`conf/{mode}.mk`) - Debug/Release settings
4. **Profile Configuration** (`conf/profiles/{profile}/`) - Custom profiles
5. **Command Line Overrides** - Build-time parameter overrides

### Key Configuration Parameters

#### Memory Management
```makefile
# Enable lazy stack allocation (reduces memory usage for thread-heavy apps)
conf_lazy_stack=1

# Stack invariant checking (debug builds)
conf_lazy_stack_invariant=1

# Memory debugging (tracks allocations/deallocations)
conf_debug_memory=1

# JVM balloon memory management
conf_memory_jvm_balloon=1
```

#### Scheduling and Threading
```makefile
# Enable preemptive scheduling (vs cooperative)
conf_preempt=1

# Thread-local storage optimization
conf_tls_opt=1
```

#### Debugging and Tracing
```makefile
# Enable kernel tracing infrastructure
conf_tracing=1

# Debug ELF loading process
conf_debug_elf=1

# Enable debug logging
conf_logger_debug=1
```

#### Size Optimization
```makefile
# Hide symbols to reduce kernel size
conf_hide_symbols=1

# Disable unused features
conf_tracing=0
conf_debug_memory=0
conf_logger_debug=0
```

### Creating Custom Configuration Profiles

1. **Create Profile Directory**:
```bash
mkdir -p conf/profiles/microservice
```

2. **Define Base Configuration** (`conf/profiles/microservice/base.mk`):
```makefile
# Optimized for microservices
conf_preempt=1
conf_lazy_stack=1
conf_hide_symbols=1
conf_tracing=0
conf_debug_memory=0

# Networking optimizations
conf_net_channel_lock_free=1
```

3. **Architecture-Specific Settings** (`conf/profiles/microservice/x64.mk`):
```makefile
# x64-specific optimizations
CXXFLAGS += -march=native -mtune=native
```

4. **Use Custom Profile**:
```bash
./scripts/build --profile=microservice image=my_microservice
```

## Kernel Module System

### Module Architecture

OSv modules are self-contained units that can be:
- **Statically linked** into the kernel
- **Dynamically loaded** at runtime
- **Conditionally included** based on configuration

### Module Types

1. **Core Modules**: Essential kernel functionality
2. **Driver Modules**: Hardware/hypervisor drivers  
3. **Filesystem Modules**: Filesystem implementations
4. **Application Modules**: User applications and runtimes
5. **Library Modules**: Shared libraries and utilities

### Creating Custom Kernel Modules

#### 1. Core Kernel Module

Create a new core module in `core/`:

```cpp
// core/my_custom_core.cc
#include <osv/debug.hh>
#include <osv/export.h>

namespace my_custom {

class custom_manager {
private:
    static custom_manager* _instance;
    
public:
    static custom_manager* instance() {
        if (!_instance) {
            _instance = new custom_manager();
        }
        return _instance;
    }
    
    void initialize() {
        debug("Custom manager initialized\n");
    }
    
    void process_request(void* data) {
        // Custom processing logic
    }
};

custom_manager* custom_manager::_instance = nullptr;

// Export for use by applications
extern "C" void custom_process(void* data) {
    custom_manager::instance()->process_request(data);
}

// Initialize during kernel boot
static void __attribute__((constructor)) init_custom_manager() {
    custom_manager::instance()->initialize();
}

} // namespace my_custom

OSV_EXPORT(custom_process, "custom_process");
```

#### 2. Driver Module

Create a custom driver module:

```cpp
// drivers/my_device.cc
#include <osv/device.h>
#include <osv/interrupt.hh>
#include <osv/mmu.hh>

class my_device_driver : public device {
private:
    void* _mmio_base;
    unsigned _irq;
    
public:
    my_device_driver(void* mmio_base, unsigned irq) 
        : _mmio_base(mmio_base), _irq(irq) {
        
        // Map MMIO region
        mmu::linear_map(_mmio_base, 0x1000, mmu::mattr::normal);
        
        // Register interrupt handler
        interrupt::register_handler(_irq, [this] { handle_interrupt(); });
    }
    
    void handle_interrupt() {
        // Process device interrupt
        uint32_t status = read_register(0x00);
        if (status & 0x1) {
            // Handle specific condition
        }
        
        // Clear interrupt
        write_register(0x04, status);
    }
    
private:
    uint32_t read_register(uint32_t offset) {
        return *reinterpret_cast<volatile uint32_t*>(
            static_cast<char*>(_mmio_base) + offset);
    }
    
    void write_register(uint32_t offset, uint32_t value) {
        *reinterpret_cast<volatile uint32_t*>(
            static_cast<char*>(_mmio_base) + offset) = value;
    }
};

// Device initialization
static my_device_driver* device_instance = nullptr;

extern "C" void init_my_device(void* mmio_base, unsigned irq) {
    device_instance = new my_device_driver(mmio_base, irq);
}
```

#### 3. Filesystem Module

Create a custom filesystem:

```cpp
// fs/my_fs/my_fs.cc
#include <fs/fs.hh>
#include <fs/vfs/vfs.h>

class my_filesystem : public filesystem {
public:
    my_filesystem() : filesystem("myfs") {}
    
    virtual int mount(device* dev, const char* path) override {
        // Mount filesystem
        return 0;
    }
    
    virtual int open(const char* path, file** fp) override {
        // Open file
        return 0;
    }
    
    virtual int stat(const char* path, struct stat* st) override {
        // Get file statistics
        return 0;
    }
};

// Register filesystem
static my_filesystem fs_instance;

static void __attribute__((constructor)) register_my_fs() {
    register_filesystem(&fs_instance);
}
```

### Module Integration

#### 1. Update Build System

Add module to `Makefile`:

```makefile
# Add to appropriate section
objects += core/my_custom_core.o
objects += drivers/my_device.o
objects += fs/my_fs/my_fs.o
```

#### 2. Export Symbols

Add to `exported_symbols`:

```
custom_process
init_my_device
```

#### 3. Module Dependencies

Define dependencies in module metadata:

```python
# modules/my_app/module.py
from osv.modules import api

# Require custom modules
api.require('my_custom_core')
api.require('my_device_driver')

default = api.run('/my_app')
```

## Advanced Customization Techniques

### 1. Custom Memory Allocators

Replace default allocator with custom implementation:

```cpp
// core/custom_allocator.cc
#include <osv/mempool.hh>

class custom_allocator : public memory::pool {
private:
    void* _heap_start;
    size_t _heap_size;
    
public:
    custom_allocator(void* heap_start, size_t heap_size)
        : _heap_start(heap_start), _heap_size(heap_size) {}
    
    virtual void* malloc(size_t size) override {
        // Custom allocation logic
        return nullptr;
    }
    
    virtual void free(void* ptr) override {
        // Custom deallocation logic
    }
    
    virtual size_t get_size(void* ptr) override {
        // Return allocation size
        return 0;
    }
};

// Replace default allocator
void install_custom_allocator() {
    auto* allocator = new custom_allocator(heap_start, heap_size);
    memory::set_allocator(allocator);
}
```

### 2. Custom Scheduler Policies

Implement custom scheduling algorithms:

```cpp
// core/custom_scheduler.cc
#include <osv/sched.hh>

namespace sched {

class custom_scheduler : public scheduler_policy {
public:
    virtual thread* pick_next(cpu* c) override {
        // Custom thread selection logic
        return nullptr;
    }
    
    virtual void thread_wake(thread* t) override {
        // Custom wake logic
    }
    
    virtual void thread_sleep(thread* t) override {
        // Custom sleep logic
    }
};

// Install custom scheduler
void install_custom_scheduler() {
    set_scheduler_policy(new custom_scheduler());
}

} // namespace sched
```

### 3. Custom Network Stack

Replace networking components:

```cpp
// core/custom_network.cc
#include <osv/net_channel.hh>

class custom_net_stack : public net_channel {
public:
    virtual void send(mbuf* m) override {
        // Custom packet transmission
    }
    
    virtual void receive() override {
        // Custom packet reception
    }
    
    virtual void set_ip_address(ipv4_addr addr) override {
        // Custom IP configuration
    }
};

// Replace network stack
void install_custom_network() {
    register_net_channel(new custom_net_stack());
}
```

## Size Optimization Strategies

### 1. Feature Elimination

Remove unused kernel features:

```makefile
# Disable tracing infrastructure
conf_tracing=0

# Disable debug features
conf_debug_memory=0
conf_debug_elf=0
conf_logger_debug=0

# Minimize symbol table
conf_hide_symbols=1
```

### 2. Link-Time Optimization

Enable aggressive optimization:

```makefile
# Enable LTO
CXXFLAGS += -flto=thin
LDFLAGS += -flto=thin

# Dead code elimination
LDFLAGS += -Wl,--gc-sections
CXXFLAGS += -ffunction-sections -fdata-sections

# Optimize for size
CXXFLAGS += -Os
```

### 3. Module Minimization

Create minimal module sets:

```python
# modules/minimal/module.py
from osv.modules import api

# Only essential modules
api.require('java-base')  # If running Java
# No httpserver, no CLI, no extras

default = api.run('/my_minimal_app')
```

### 4. Custom Linker Scripts

Optimize memory layout:

```ld
/* arch/x64/loader.ld.custom */
SECTIONS {
    /* Optimize section layout for size */
    .text : {
        *(.text.hot)    /* Hot code first */
        *(.text)
        *(.text.cold)   /* Cold code last */
    }
    
    /* Merge similar sections */
    .rodata : {
        *(.rodata*)
        *(.srodata*)
    }
}
```

## Performance Optimization

### 1. CPU-Specific Optimizations

```makefile
# Target specific CPU
CXXFLAGS += -march=skylake -mtune=skylake

# Enable CPU-specific features
CXXFLAGS += -msse4.2 -mavx2
```

### 2. Memory Layout Optimization

```cpp
// Optimize data structure layout
struct optimized_struct {
    // Hot data first (cache-friendly)
    volatile uint64_t counter;
    void* hot_ptr;
    
    // Cold data last
    char name[256];
} __attribute__((packed));
```

### 3. Lock-Free Data Structures

```cpp
// Use lock-free algorithms where possible
#include <osv/lockfree/ring.hh>

lockfree::ring<request*, 1024> request_queue;

void producer() {
    request* req = new request();
    while (!request_queue.push(req)) {
        // Retry or handle full queue
    }
}

void consumer() {
    request* req;
    if (request_queue.pop(req)) {
        process_request(req);
    }
}
```

## Testing Custom Kernels

### 1. Unit Testing

Create tests for custom components:

```cpp
// tests/test_custom_component.cc
#include <boost/test/unit_test.hpp>
#include "my_custom_component.hh"

BOOST_AUTO_TEST_CASE(test_custom_functionality) {
    my_custom::component comp;
    comp.initialize();
    
    BOOST_CHECK(comp.is_initialized());
    
    auto result = comp.process_data("test");
    BOOST_CHECK_EQUAL(result, expected_value);
}
```

### 2. Integration Testing

Test complete system:

```bash
# Build test image
./scripts/build image=tests,my_custom_module

# Run tests
./scripts/test.py -p qemu
```

### 3. Performance Testing

Benchmark custom components:

```cpp
// Measure performance
auto start = std::chrono::high_resolution_clock::now();
my_custom_operation();
auto end = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
    end - start).count();
```

## Debugging Custom Kernels

### 1. Debug Builds

Enable debugging for custom components:

```makefile
# Custom debug flags
ifdef DEBUG_MY_COMPONENT
CXXFLAGS += -DDEBUG_MY_COMPONENT=1
endif
```

### 2. Tracing Integration

Add tracing to custom code:

```cpp
#include <osv/trace.hh>

TRACEPOINT(trace_my_operation, "operation=%s result=%d", 
           const char*, int);

void my_operation(const char* op) {
    trace_my_operation(op, result);
}
```

### 3. GDB Integration

Debug custom kernel with GDB:

```bash
# Start OSv with debug symbols
./scripts/run.py --debug -e '/my_custom_app'

# In another terminal
gdb build/debug/loader.elf
(gdb) target remote :1234
(gdb) break my_custom_function
(gdb) continue
```

This guide provides comprehensive coverage of OSv kernel customization techniques. Use these patterns and examples as starting points for your specific customization needs.