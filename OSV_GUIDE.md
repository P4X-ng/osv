# OSv Comprehensive Guide

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Kernel Customization](#kernel-customization)
3. [Module System](#module-system)
4. [Networking Stack](#networking-stack)
5. [ELF Loading](#elf-loading)
6. [Size Optimization](#size-optimization)
7. [Custom Stacks](#custom-stacks)
8. [Best Practices](#best-practices)
9. [Common Pitfalls](#common-pitfalls)

## Architecture Overview

OSv is a modular unikernel designed to run unmodified Linux applications with high performance and minimal overhead.

### Core Components

- **Scheduler**: Cooperative and preemptive scheduling with per-CPU runqueues
- **Memory Management**: Virtual memory with demand paging and memory pools
- **ELF Loader**: Dynamic linking and symbol resolution
- **VFS**: Virtual filesystem with multiple backend support
- **Network Stack**: TCP/IP implementation with virtio drivers
- **Device Drivers**: Hypervisor-specific device support

### Architecture Support

- **x86_64**: Full support with optimized assembly routines
- **aarch64**: Complete ARM64 support with KVM acceleration

## Kernel Customization

OSv provides extensive customization through configuration profiles and build options.

### Configuration System

The configuration system is located in `conf/` directory:

```
conf/
├── base.mk          # Base configuration options
├── debug.mk         # Debug build settings
├── release.mk       # Release build settings
├── x64.mk          # x86_64 specific settings
├── aarch64.mk      # ARM64 specific settings
└── profiles/       # Pre-defined configuration profiles
```

### Key Configuration Options

```makefile
# Preemption control
conf_preempt=1

# Tracing support
conf_tracing=0

# Memory debugging
conf_debug_memory=0

# Lazy stack allocation
conf_lazy_stack=0

# Symbol visibility
conf_hide_symbols=0
```

### Creating Custom Profiles

Create a new profile in `conf/profiles/`:

```bash
mkdir conf/profiles/my_custom_profile
```

Add configuration files for your specific use case:

```makefile
# conf/profiles/my_custom_profile/base.mk
conf_preempt=0
conf_tracing=1
conf_hide_symbols=1
```

## Module System

OSv uses a modular architecture where functionality is provided through loadable modules.

### Module Structure

Each module is located in `modules/` directory with:

```
modules/my_module/
├── Makefile         # Build configuration
├── module.py        # Module metadata
├── usr.manifest     # Files to include
└── src/            # Source code
```

### Creating Custom Modules

1. Create module directory:
```bash
mkdir modules/my_custom_module
```

2. Create `module.py`:
```python
from osv.modules import api

api.require('java')  # Dependencies

default = api.run('/java.so -jar /my_app.jar')
```

3. Create `Makefile`:
```makefile
include ../common.gmk

module: my_app.jar

my_app.jar:
	# Build your application
```

4. Create `usr.manifest`:
```
/my_app.jar: my_app.jar
```

### Module Dependencies

Modules can declare dependencies:

```python
# In module.py
api.require('java-base')
api.require('httpserver')
```

## Networking Stack

OSv implements a custom TCP/IP stack optimized for virtualized environments.

### Network Architecture

```
Application Layer
    ↓
Socket API (BSD sockets)
    ↓
TCP/UDP Layer
    ↓
IP Layer
    ↓
Device Drivers (virtio-net, vmxnet3, etc.)
    ↓
Hypervisor
```

### Custom Network Drivers

To implement a custom network driver:

1. Create driver in `drivers/`:
```cpp
// drivers/my_net_driver.cc
#include <osv/net_channel.hh>

class my_net_driver : public net_channel {
public:
    virtual void send(mbuf* m) override;
    virtual void receive() override;
};
```

2. Register driver:
```cpp
static my_net_driver* driver;

void my_net_init() {
    driver = new my_net_driver();
    register_net_channel(driver);
}
```

### Network Configuration

Configure networking at boot:

```bash
# Static IP configuration
--ip=eth0,192.168.1.100,255.255.255.0
--defaultgw=192.168.1.1
--nameserver=8.8.8.8

# DHCP (default)
--dhcp
```

## ELF Loading

OSv supports multiple ELF loading mechanisms for maximum compatibility.

### Dynamic Linking Modes

1. **Built-in Dynamic Linker**: Fast local function calls
2. **Linux Dynamic Linker**: Full glibc compatibility
3. **Static Linking**: Direct system calls

### Custom ELF Loader

To customize ELF loading behavior:

```cpp
// In core/elf.cc
class custom_elf_loader : public elf::object {
public:
    virtual void* resolve_symbol(const char* name) override {
        // Custom symbol resolution
    }
};
```

### Symbol Resolution

OSv provides flexible symbol resolution:

```cpp
// Export symbols for applications
OSV_EXPORT(my_custom_function, "my_custom_function");

// Import symbols from modules
extern "C" void* dlsym(void* handle, const char* symbol);
```

## Size Optimization

OSv kernel size can be significantly reduced through various techniques.

### Build-time Optimization

1. **Hide Symbols**:
```makefile
conf_hide_symbols=1
```

2. **Disable Unused Features**:
```makefile
conf_tracing=0
conf_debug_memory=0
```

3. **Use Minimal Profiles**:
```bash
./scripts/build --profile=minimal
```

### Module Selection

Only include necessary modules:

```bash
./scripts/build image=my_app  # Minimal image
```

### Filesystem Optimization

Choose appropriate filesystem:

- **RAMFS**: Smallest, embedded in kernel
- **ROFS**: Read-only, compressed
- **ZFS**: Full-featured but larger

```bash
./scripts/build fs=ramfs image=my_app
```

### Link-time Optimization

Enable LTO for smaller binaries:

```makefile
CXXFLAGS += -flto
LDFLAGS += -flto
```

## Custom Stacks

OSv allows replacement of major subsystems with custom implementations.

### Custom Memory Allocator

Replace the default allocator:

```cpp
// In core/mempool.cc
class custom_allocator {
public:
    void* malloc(size_t size);
    void free(void* ptr);
};

// Register allocator
memory::set_allocator(new custom_allocator());
```

### Custom Scheduler

Implement custom scheduling policy:

```cpp
// In core/sched.cc
class custom_scheduler : public scheduler {
public:
    virtual thread* pick_next() override {
        // Custom scheduling logic
    }
};
```

### Custom Filesystem

Implement custom filesystem backend:

```cpp
// In fs/
class custom_fs : public filesystem {
public:
    virtual int mount(device* dev, const char* path) override;
    virtual int open(const char* path, file** fp) override;
};
```

## Best Practices

### Performance Optimization

1. **Use Appropriate Build Mode**:
   - Release mode for production
   - Debug mode for development

2. **Optimize Memory Usage**:
   - Enable lazy stack allocation for thread-heavy applications
   - Use memory pools for frequent allocations

3. **Network Performance**:
   - Use vhost-net for better network performance
   - Configure appropriate buffer sizes

4. **Filesystem Selection**:
   - RAMFS for read-only applications
   - ZFS for applications requiring persistence

### Security Considerations

1. **Symbol Visibility**:
   - Hide unnecessary symbols in production builds
   - Use minimal module sets

2. **Memory Protection**:
   - Enable memory debugging during development
   - Use safe pointer wrappers

3. **Input Validation**:
   - Validate all external inputs
   - Use bounds checking

### Development Workflow

1. **Use Docker for Development**:
```bash
docker run -it osvunikernel/osv-ubuntu-20.10-builder
```

2. **Incremental Building**:
```bash
./scripts/build -j$(nproc)  # Parallel build
```

3. **Testing**:
```bash
./scripts/build check  # Run unit tests
```

## Common Pitfalls

### Memory Management Issues

1. **Stack Overflow**:
   - Default stack size is limited
   - Use `conf_lazy_stack=1` for applications with many threads

2. **Memory Leaks**:
   - OSv doesn't have garbage collection
   - Use RAII and smart pointers

3. **Fragmentation**:
   - Large allocations can cause fragmentation
   - Use memory pools for fixed-size allocations

### Threading Issues

1. **Deadlocks**:
   - OSv uses cooperative scheduling by default
   - Avoid blocking operations in critical sections

2. **Race Conditions**:
   - Use proper synchronization primitives
   - Be aware of per-CPU data structures

### Build Issues

1. **Cross-compilation**:
   - Ensure proper toolchain setup
   - Use provided Docker images

2. **Module Dependencies**:
   - Declare all module dependencies
   - Check for circular dependencies

3. **Symbol Resolution**:
   - Export necessary symbols
   - Check for undefined references

### Runtime Issues

1. **Boot Failures**:
   - Check kernel command line parameters
   - Verify filesystem integrity

2. **Application Crashes**:
   - Use GDB for debugging
   - Enable core dumps

3. **Performance Issues**:
   - Profile with built-in tracing
   - Check for lock contention

### Hypervisor-Specific Issues

1. **QEMU/KVM**:
   - Enable KVM acceleration
   - Use appropriate machine type

2. **Firecracker**:
   - Configure proper network setup
   - Use minimal kernel configuration

3. **VMware**:
   - Install VMware tools equivalent
   - Configure appropriate hardware version

## Troubleshooting Guide

### Debug Build Configuration

Enable debugging features:

```makefile
mode=debug
conf_debug_memory=1
conf_tracing=1
```

### Using GDB

Debug OSv with GDB:

```bash
./scripts/run.py --debug
# In another terminal:
gdb build/release/loader.elf
(gdb) target remote :1234
```

### Tracing and Profiling

Enable tracing:

```bash
./scripts/run.py -e '--trace=sched* /my_app'
```

Analyze traces:

```bash
./scripts/trace.py -t build/release/trace.bin
```

### Memory Analysis

Check memory usage:

```bash
# In OSv shell
free
cat /proc/meminfo
```

### Network Debugging

Debug network issues:

```bash
# Check network configuration
ifconfig
route -n

# Test connectivity
ping 8.8.8.8
```

This guide provides a comprehensive overview of OSv's architecture and customization capabilities. For specific implementation details, refer to the source code and additional documentation in the `documentation/` directory.