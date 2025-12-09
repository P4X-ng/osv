# OSv Kernel Customization Guide

## Table of Contents

1. [Introduction](#introduction)
2. [Build Configuration Options](#build-configuration-options)
3. [Module Selection and Management](#module-selection-and-management)
4. [Kernel Size Reduction](#kernel-size-reduction)
5. [Filesystem Selection](#filesystem-selection)
6. [Architecture-Specific Optimizations](#architecture-specific-optimizations)
7. [Performance Tuning](#performance-tuning)
8. [Advanced Customization](#advanced-customization)

---

## 1. Introduction

OSv's modular architecture allows extensive customization to tailor the kernel for specific workloads. This guide covers:

- **Build-time configuration**: Compile options, features, and optimizations
- **Module selection**: Including/excluding components
- **Size optimization**: Reducing kernel footprint
- **Performance tuning**: Maximizing performance for specific workloads

### When to Customize

Consider customization when:
- Minimizing kernel size for faster boot/smaller images
- Optimizing for specific hypervisor or hardware
- Removing unnecessary features for security
- Tuning for specific application workloads
- Reducing memory footprint

### Customization vs. Standard Build

**Standard Build:**
```bash
./scripts/build
```
- Size: ~6.8 MB (with ZFS and libstdc++)
- Boot time: ~200ms on QEMU
- Memory: ~15-20 MB minimum
- All features enabled

**Customized Build:**
```bash
./scripts/build fs=rofs conf_hide_symbols=1 conf_drivers_mmio=0
```
- Size: Can be <3 MB
- Boot time: ~5ms on Firecracker
- Memory: ~11 MB minimum
- Only necessary features

---

## 2. Build Configuration Options

### 2.1 Configuration File Locations

```
conf/
├── base.mk           # Base configuration
├── release.mk        # Release mode settings
├── debug.mk          # Debug mode settings
├── x64.mk            # x86_64 architecture
├── aarch64.mk        # ARM64 architecture
└── profiles/         # Pre-defined configurations
```

### 2.2 Core Configuration Variables

#### Build Mode

```bash
# Release mode (optimized, default)
./scripts/build mode=release

# Debug mode (symbols, no optimization)
./scripts/build mode=debug
```

**Release vs Debug:**
| Feature | Release | Debug |
|---------|---------|-------|
| Optimization | -O2/-O3 | -O0 |
| Debug symbols | Minimal | Full |
| Assertions | Disabled | Enabled |
| Size | ~3-7 MB | ~15-20 MB |
| Performance | Fast | Slow |

#### Architecture Selection

```bash
# Build for x86_64 (default on x86_64 host)
./scripts/build arch=x64

# Build for aarch64 (cross-compile or native)
./scripts/build arch=aarch64
```

### 2.3 Feature Configuration Options

#### Symbol Hiding (Size Reduction)

```bash
# Hide unused symbols (reduces size by ~30%)
./scripts/build conf_hide_symbols=1
```

**Effect:**
- Before: ~6.8 MB
- After: ~3.6 MB
- Removes: Unused library symbols, debug info
- Keep: Only symbols used by kernel and application

#### Lazy Stack Allocation

```bash
# Allocate thread stacks on demand
./scripts/build conf_lazy_stack=1
```

**Benefits:**
- Reduces memory usage for thread-heavy applications
- Saves ~4KB per thread (initial allocation)
- Good for apps with hundreds/thousands of threads

**Use Cases:**
- Java applications with many threads
- Web servers with thread-per-connection
- Applications with large thread pools

#### Memory Pool Configuration

```bash
# Customize memory pool sizes
./scripts/build conf_mem_page_size=4096 \
                conf_mem_l1_cache_size=256 \
                conf_mem_l2_cache_size=4096
```

#### Driver Selection

```bash
# Disable MMIO drivers (PCI only)
./scripts/build conf_drivers_mmio=0

# Disable specific drivers
./scripts/build conf_drivers_xen=0 \
                conf_drivers_vmware=0 \
                conf_drivers_ahci=0
```

**Available Driver Options:**
- `conf_drivers_mmio`: Memory-mapped I/O devices
- `conf_drivers_xen`: Xen paravirtual drivers
- `conf_drivers_vmware`: VMware-specific drivers
- `conf_drivers_ahci`: SATA AHCI driver
- `conf_drivers_ide`: Legacy IDE driver
- `conf_drivers_virtio`: VirtIO drivers (usually keep)

### 2.4 Compiler Optimizations

#### Optimization Level

```make
# In conf/base.mk or conf/release.mk
CFLAGS += -O3          # Maximum optimization
CFLAGS += -Os          # Optimize for size
CFLAGS += -O2          # Balanced (default)
```

#### Link-Time Optimization (LTO)

```bash
# Enable LTO for whole-program optimization
./scripts/build lto=1
```

**Benefits:**
- Smaller binary size (~10-15% reduction)
- Better performance (~5-10% improvement)
- Longer build time

**Caveats:**
- Requires more memory during build
- May increase build time significantly
- Some edge cases with plugin loading

#### Architecture-Specific Tuning

```make
# For x86_64 in conf/x64.mk
CFLAGS += -march=x86-64-v3    # Modern CPUs (AVX2)
CFLAGS += -march=x86-64-v2    # Compatibility
CFLAGS += -mtune=generic      # Generic tuning
CFLAGS += -mtune=native       # Optimize for build host
```

**x86-64 Feature Levels:**
- `x86-64-v1`: Baseline (Pentium 4 era)
- `x86-64-v2`: SSE4.2, SSSE3 (2009+)
- `x86-64-v3`: AVX2, BMI2, FMA (2015+)
- `x86-64-v4`: AVX-512 (2017+)

### 2.5 Configuration Profiles

Create custom profiles in `conf/profiles/`:

```makefile
# conf/profiles/minimal.mk
# Minimal kernel configuration

conf_hide_symbols = 1
conf_lazy_stack = 1
conf_drivers_mmio = 0
conf_drivers_xen = 0
conf_drivers_vmware = 0
conf_drivers_ahci = 0

# Use ROFS
fs = rofs

# Optimize for size
CXXFLAGS += -Os
CFLAGS += -Os
```

**Usage:**
```bash
./scripts/build --profile=minimal
```

---

## 3. Module Selection and Management

### 3.1 Understanding Modules

Modules are optional components that can be included/excluded:

```
modules/
├── httpserver-api/        # REST API server
├── httpserver-monitoring-api/  # Monitoring
├── cloud-init/            # Cloud initialization
├── ca-certificates/       # SSL certificates
├── cli/                   # Command-line interface
├── libext/               # ext4 filesystem support
├── nfs/                  # NFS client
└── ...
```

### 3.2 Module Selection

#### Default Image (CLI + REST API)

```bash
./scripts/build
# Includes: CLI, httpserver-api, ca-certificates
```

#### Empty Image (Minimal)

```bash
./scripts/build image=empty
# Includes: Only kernel, no additional modules
```

#### Custom Module Selection

```bash
# Single module
./scripts/build image=native-example

# Multiple modules (comma-separated)
./scripts/build image=httpserver-monitoring-api,cloud-init,native-example
```

### 3.3 Common Module Combinations

#### Minimal Stateless Microservice

```bash
./scripts/build image=empty fs=rofs conf_hide_symbols=1
# Size: ~3 MB
# Boot: ~5 ms on Firecracker
# Use: Single application, no shell access
```

#### Monitored Service

```bash
./scripts/build image=httpserver-monitoring-api,myapp fs=rofs
# Includes: Monitoring API for metrics
# Access: HTTP REST API for monitoring
```

#### Development Image

```bash
./scripts/build image=cli,httpserver-api,myapp
# Includes: Shell access, REST API
# Use: Interactive debugging and development
```

#### Java Application

```bash
./scripts/build JAVA_VERSION=11 \
    image=openjdk-zulu-9-and-above,myapp
```

### 3.4 Creating Custom Modules

#### Module Structure

```
modules/mymodule/
├── module.py          # Module definition
├── Makefile          # Build rules (optional)
├── usr.manifest      # Files to include
└── src/              # Source code (if any)
```

#### Module Definition (module.py)

```python
from osv.modules import api

# Module metadata
api.require('cloud-init')  # Dependencies
api.require('ca-certificates')

# Build commands (if compiling)
default = api.run('/bin/make')

# Files to include in image
usr_files = [
    '/usr/bin/myapp',
    '/usr/lib/mylib.so',
]
```

#### Manifest File (usr.manifest)

```
# Static files to copy
/usr/bin/myapp: ${MODULE_DIR}/myapp
/usr/lib/mylib.so: ${MODULE_DIR}/mylib.so
/etc/myapp.conf: ${MODULE_DIR}/config/myapp.conf

# From host system
/lib64/libssl.so.1.1: ->/lib64/libssl.so.1.1
```

#### Building with Custom Module

```bash
# If module is in modules/mymodule/
./scripts/build image=mymodule

# External module
./scripts/build modules=/path/to/mymodule
```

### 3.5 Module Dependencies

OSv automatically resolves module dependencies:

```python
# In module.py
api.require('httpserver-api')      # Requires REST API
api.require('ca-certificates')     # Requires SSL certs
api.require('libz')                # Requires zlib
```

**Dependency Resolution:**
1. Specified module
2. Direct dependencies
3. Transitive dependencies
4. Base system libraries

**Viewing Dependencies:**
```bash
# List all modules in image
./scripts/module.py query-depends --out-file usr.manifest build/last/usr.manifest
```

---

## 4. Kernel Size Reduction

### 4.1 Size Reduction Strategies

#### Strategy 1: Symbol Hiding

```bash
./scripts/build conf_hide_symbols=1
```
**Savings:** ~3 MB (from 6.8 to 3.6 MB)

#### Strategy 2: Filesystem Selection

```bash
# ZFS (default): 6.8 MB
./scripts/build fs=zfs

# ROFS (read-only): 3.8 MB
./scripts/build fs=rofs

# RAMFS (in-kernel): 3.5 MB + app size
./scripts/build fs=ramfs
```

#### Strategy 3: Compiler Optimization for Size

```bash
./scripts/build conf_hide_symbols=1 -Os
# Edit conf/base.mk to add: CFLAGS += -Os
```
**Savings:** Additional ~10-15%

#### Strategy 4: Remove Unused Drivers

```bash
./scripts/build conf_hide_symbols=1 \
    conf_drivers_xen=0 \
    conf_drivers_vmware=0 \
    conf_drivers_ahci=0 \
    conf_drivers_mmio=0
```
**Savings:** ~500 KB - 1 MB

**⚠️ Warning:** Disabling MMIO drivers may prevent boot on some systems. Test thoroughly on your target hypervisor before deploying. MMIO drivers are needed for PCI device discovery on many platforms.

#### Strategy 5: Minimal Module Set

```bash
./scripts/build image=empty \
    fs=rofs \
    conf_hide_symbols=1
```
**Result:** ~2.5-3 MB kernel

### 4.2 Size Comparison Table

| Configuration | Size | Boot Time | Use Case |
|---------------|------|-----------|----------|
| Default (ZFS) | 6.8 MB | ~200 ms | Development, stateful apps |
| Standard (ROFS) | 3.8 MB | ~40 ms | Production stateless |
| Optimized | 3.2 MB | ~20 ms | Size-critical |
| Minimal | 2.8 MB | ~5 ms | Serverless, minimal |

### 4.3 Measuring Size

```bash
# Kernel ELF size
ls -lh build/last/loader.elf

# Compressed image size
ls -lh build/last/usr.img

# Detailed breakdown
./scripts/build --size-analysis
```

### 4.4 Size vs. Features Trade-offs

**Removing ZFS:**
- Savings: ~3 MB
- Loss: Read-write filesystem, compression, snapshots
- Alternative: Use VirtioFS for persistence

**Symbol Hiding:**
- Savings: ~3 MB
- Loss: Better debugging, dynamic loading of some libs
- Impact: Minimal for most applications

**Removing Drivers:**
- Savings: ~500 KB - 1 MB
- Loss: Hardware compatibility
- Risk: May not boot on some hypervisors

---

## 5. Filesystem Selection

### 5.1 Filesystem Comparison

| Feature | ZFS | ROFS | RAMFS | VirtioFS |
|---------|-----|------|-------|----------|
| Read | ✓ | ✓ | ✓ | ✓ |
| Write | ✓ | ✗ | ✓ | ✓ |
| Compression | ✓ | ✓ | ✗ | Host |
| Snapshots | ✓ | ✗ | ✗ | Host |
| Size | 6.8 MB | 3.8 MB | 3.5 MB | 3.8 MB |
| Boot Time | 200 ms | 40 ms | 10 ms | 50 ms |
| Memory | High | Low | Medium | Low |
| Persistence | ✓ | ✗ | ✗ | ✓ |

### 5.2 ZFS Configuration

```bash
# Standard ZFS
./scripts/build fs=zfs

# ZFS with compression
./scripts/build fs=zfs zfs_compression=lz4
```

**Use Cases:**
- Database applications
- Stateful services
- Applications needing snapshots
- Data integrity critical

**Configuration:**
```bash
# Boot parameters
--mount-fs=zfs,/dev/vblk0,/

# Compression algorithms
zfs_compression=lz4      # Fast, moderate compression
zfs_compression=gzip     # Slower, better compression
zfs_compression=zstd     # Balanced
```

### 5.3 ROFS (Read-Only Filesystem)

```bash
# Build with ROFS
./scripts/build fs=rofs image=myapp
```

**Advantages:**
- Small size (~3.8 MB)
- Fast boot (~40 ms)
- Compressed
- Simple implementation

**Limitations:**
- No writes (read-only)
- No /tmp writes (use --tmpfs)
- No logs to disk

**Best Practices:**
```bash
# Use tmpfs for temporary files
./scripts/run.py -e '--tmpfs /tmp /myapp'

# Redirect logs to serial console
./scripts/run.py -e '--redirect=/dev/console /myapp'
```

### 5.4 RAMFS Configuration

```bash
# Build with RAMFS
./scripts/build fs=ramfs image=myapp
```

**Characteristics:**
- Files embedded in kernel ELF
- No disk I/O
- Fastest boot
- Increases kernel size by app size

**Use Cases:**
- Ultra-fast boot required
- Small applications (<10 MB)
- No disk available (rare)

### 5.5 VirtioFS

```bash
# Build with VirtioFS boot support
./scripts/build fs=rofs image=virtiofs,myapp

# Run with shared directory
./scripts/run.py --mount-fs=virtiofs,myshare,/shared \
    -e '/myapp'
```

**Advantages:**
- No image building for development
- Share files with host
- Good performance
- Write support

**Setup:**
```bash
# Start virtiofsd on host
virtiofsd --socket-path=/tmp/vfsd.sock \
    --shared-dir=/path/to/share \
    --cache=always

# Run OSv
./scripts/run.py --virtio-fs-tag=myshare \
    --virtio-fs-socket=/tmp/vfsd.sock
```

### 5.6 Multiple Filesystem Strategy

```bash
# Boot from ROFS, mount data on ZFS
./scripts/build fs=rofs image=zfs,myapp

# At boot
./scripts/run.py \
    --second-disk-image=data.zfs \
    -e '--mount-fs=zfs,/dev/vblk1,/data /myapp'
```

**Benefits:**
- Fast boot (ROFS)
- Data persistence (ZFS)
- Flexibility

---

## 6. Architecture-Specific Optimizations

### 6.1 x86_64 Optimizations

#### CPU Feature Selection

Add to `conf/x64.mk` or your custom profile:

```makefile
# conf/x64.mk additions

# Baseline (maximum compatibility)
CXXFLAGS += -march=x86-64
CFLAGS += -march=x86-64

# Modern CPUs (2015+)
CXXFLAGS += -march=x86-64-v3 -mfma -mavx2
CFLAGS += -march=x86-64-v3 -mfma -mavx2

# Latest CPUs (2017+)
CXXFLAGS += -march=x86-64-v4 -mavx512f
CFLAGS += -march=x86-64-v4 -mavx512f
```

#### x86-Specific Features

```bash
# Enable huge pages (2MB)
./scripts/build conf_mem_huge_page_size=2097152

# Use TSC for timekeeping (faster)
# Default, no config needed

# Enable FSGSBASE (faster TLS)
# Detected automatically on capable CPUs
```

#### Vector Instructions

```makefile
# Use SIMD for string operations
CFLAGS += -mavx2 -DUSE_AVX2_MEMCPY
```

### 6.2 AArch64 Optimizations

#### CPU Selection

```bash
# Generic ARM64
./scripts/build arch=aarch64

# With specific CPU tuning
# Edit conf/aarch64.mk:
CFLAGS += -mcpu=cortex-a72  # Raspberry Pi 4
CFLAGS += -mcpu=cortex-a76  # High-performance ARM
```

#### ARM-Specific Features

```bash
# Enable NEON SIMD
CFLAGS += -mfpu=neon

# Large page support
# Configure based on SoC capabilities
```

### 6.3 Hypervisor-Specific Optimizations

#### QEMU/KVM

```bash
# Build with optimal drivers
./scripts/build conf_drivers_virtio=1

# Boot with optimal settings
./scripts/run.py \
    --cpus=4 \
    --memsize=2048 \
    -k               # PVH boot (faster)
    --nopci          # Skip PCI scan on microvm
```

#### Firecracker

```bash
# Minimal drivers
./scripts/build fs=rofs \
    conf_hide_symbols=1 \
    image=myapp

# Run with minimal boot time
./scripts/firecracker.py \
    --boot-args="--nopci --console=serial" \
    --mem=512 \
    --cpus=2
```

#### VMware

```bash
# Include VMware drivers
./scripts/build conf_drivers_vmware=1

# Use vmxnet3 for networking
```

#### Xen

```bash
# Include Xen drivers
./scripts/build conf_drivers_xen=1
```

### 6.4 Cloud Provider Optimizations

#### AWS

```bash
# Include ENA driver for EC2
./scripts/build image=ena,myapp

# Use NVMe for storage
# Automatic in recent instances
```

#### Google Cloud Platform

```bash
# Standard virtio drivers
./scripts/build conf_drivers_virtio=1
```

#### Azure

```bash
# Hyper-V enlightenments (if supported)
# Standard virtio works well
```

---

## 7. Performance Tuning

### 7.1 Memory Tuning

#### Memory Pool Sizing

```bash
# Tune memory pools for workload
./scripts/build \
    conf_mem_l1_cache_size=512 \    # Per-CPU cache
    conf_mem_l2_cache_size=8192     # Shared pool
```

**Guidelines:**
- L1 cache: 256-512 KB per CPU (for small allocations)
- L2 cache: 4-16 MB (for larger allocations)
- Too small: Frequent refills, contention
- Too large: Wasted memory

#### Lazy Stack

```bash
# For thread-heavy applications
./scripts/build conf_lazy_stack=1
```

**Benchmarking:**
```bash
# Before
./scripts/test.py threads.so  # Measure memory

# After
./scripts/build conf_lazy_stack=1 image=tests
./scripts/test.py threads.so  # Compare
```

### 7.2 CPU Tuning

#### SMP Configuration

```bash
# Match vCPUs to application threads
./scripts/run.py --cpus=4  # For 4-threaded app

# CPU pinning (on host)
taskset -c 0-3 ./scripts/run.py ...
```

#### Scheduler Tuning

```cpp
// At application start
sched::thread::pin(2);  // Pin to CPU 2

// Priority adjustment
sched::thread::current()->set_priority(10);
```

### 7.3 Network Tuning

#### Virtio-Net Configuration

```bash
# Enable multiqueue
./scripts/run.py \
    --cpus=4 \
    -n -v \  # Vhost-net
    --virtio-net-queues=4  # Match CPU count
```

#### TCP Tuning

```bash
# Via boot parameters
./scripts/run.py -e \
    '--sysctl net.inet.tcp.sendspace=262144 \
     --sysctl net.inet.tcp.recvspace=262144 \
     /myapp'
```

### 7.4 Storage Tuning

#### Virtio-Blk

```bash
# Use multiple virtio-blk devices
./scripts/run.py \
    --disk-image=disk1.img \
    --second-disk-image=disk2.img

# Enable AIO
# Automatic with recent QEMU
```

#### NVMe

```bash
# Use NVMe device
./scripts/run.py \
    --nvme-image=disk.img \
    --nvme-queues=4
```

### 7.5 Boot Time Optimization

**Techniques:**

1. **Use ROFS:**
   ```bash
   ./scripts/build fs=rofs  # vs ZFS: 5x faster
   ```

2. **Disable PCI Scan:**
   ```bash
   --boot-args="--nopci"  # Save 10-20ms
   ```

3. **Use Serial Console:**
   ```bash
   --boot-args="--console=serial"  # Save 60-70ms
   ```

4. **PVH Boot:**
   ```bash
   ./scripts/run.py -k  # Direct kernel boot
   ```

5. **Disable Unnecessary Services:**
   ```bash
   ./scripts/build image=empty  # No CLI/API
   ```

**Measurement:**
```bash
./scripts/run.py -e '--bootchart /myapp'
```

---

## 8. Advanced Customization

### 8.1 Custom Boot Parameters

#### Defining Boot Parameters

```cpp
// In your module
#include <osv/options.hh>

namespace myapp {
    static int my_param = 10;
    
    osv::option<int> opt_my_param(
        "myapp.param",
        &my_param,
        10,  // default
        "My application parameter"
    );
}
```

#### Using Boot Parameters

```bash
./scripts/run.py -e '--myapp.param=20 /myapp'
```

### 8.2 Custom System Calls

If your application needs custom syscalls:

```cpp
// In syscalls/
// Add to syscalls.cc

extern "C" long syscall_mycall(long arg1) {
    // Implementation
    return 0;
}

// Add to syscall table
[SYS_mycall] = (void*)syscall_mycall,
```

### 8.3 Custom Drivers

#### Driver Template

```cpp
// drivers/mydriver.cc
#include <osv/device.h>
#include <osv/driver.h>

namespace mydriver {

struct mydevice : public device {
    virtual int probe() override;
    virtual int init() override;
    virtual int read(void* buf, size_t len) override;
    virtual int write(const void* buf, size_t len) override;
};

int mydevice::probe() {
    // Detect device
    return 0;
}

int mydevice::init() {
    // Initialize device
    return 0;
}

// Register driver
static driver mydriver = {
    .name = "mydriver",
    .devclass = device_class::UNKNOWN,
    .ops = &mydriver_ops,
};

}

REGISTER_DRIVER(mydriver);
```

### 8.4 Custom Filesystems

#### Filesystem Interface

```cpp
// Implement vfsops and vnops
static struct vfsops myfs_vfsops = {
    .vfs_mount = myfs_mount,
    .vfs_unmount = myfs_unmount,
    .vfs_sync = myfs_sync,
    .vfs_statfs = myfs_statfs,
};

static struct vnops myfs_vnops = {
    .vop_open = myfs_open,
    .vop_close = myfs_close,
    .vop_read = myfs_read,
    .vop_write = myfs_write,
    // ... more operations
};

// Register filesystem
extern "C" void myfs_init(void) {
    vfs_register_fs("myfs", &myfs_vfsops);
}
```

### 8.5 Link-Time Optimization

```bash
# Enable LTO
./scripts/build lto=1

# Profile-guided optimization
# 1. Build with profiling
./scripts/build profile=generate

# 2. Run workload
./scripts/run.py -e '/myapp'

# 3. Rebuild with profile data
./scripts/build profile=use
```

### 8.6 Custom Memory Allocator

```cpp
// Override allocation functions
extern "C" void* malloc(size_t size) {
    // Custom allocator
    return my_alloc(size);
}

extern "C" void free(void* ptr) {
    my_free(ptr);
}
```

---

## 9. Configuration Examples

### 9.1 Minimal Serverless Configuration

```bash
#!/bin/bash
# build-serverless.sh

./scripts/build \
    image=empty \
    fs=rofs \
    conf_hide_symbols=1 \
    conf_lazy_stack=1 \
    conf_drivers_mmio=0 \
    conf_drivers_xen=0 \
    conf_drivers_vmware=0

# Run on Firecracker
./scripts/firecracker.py \
    --boot-args="--nopci --console=serial" \
    --mem=128 \
    --cpus=1
```

### 9.2 High-Performance Web Server

```bash
#!/bin/bash
# build-webserver.sh

./scripts/build \
    image=httpserver-monitoring-api,mywebserver \
    fs=rofs \
    conf_hide_symbols=1 \
    -march=x86-64-v3

# Run with optimized networking
./scripts/run.py \
    --cpus=4 \
    --memsize=2048 \
    -n -v \
    --virtio-net-queues=4 \
    -e '--sysctl net.inet.tcp.sendspace=262144 /mywebserver'
```

### 9.3 Database Server

```bash
#!/bin/bash
# build-database.sh

./scripts/build \
    image=cli,httpserver-monitoring-api,mydb \
    fs=zfs \
    zfs_compression=lz4

# Run with multiple disks
./scripts/run.py \
    --cpus=8 \
    --memsize=4096 \
    --disk-image=os.img \
    --second-disk-image=data.zfs \
    --nvme-image=logs.img \
    -e '--mount-fs=zfs,/dev/vblk1,/data \
         --mount-fs=rofs,/dev/nvme0n1,/logs \
         /mydb'
```

### 9.4 Java Application

```bash
#!/bin/bash
# build-java.sh

./scripts/build \
    JAVA_VERSION=17 \
    image=openjdk-zulu-9-and-above,myapp \
    fs=rofs \
    conf_hide_symbols=1 \
    conf_lazy_stack=1

# Run with JVM tuning
./scripts/run.py \
    --cpus=4 \
    --memsize=2048 \
    -e 'java -Xmx1G -Xms1G -XX:+UseG1GC -jar /myapp.jar'
```

---

## 10. Troubleshooting

### 10.1 Build Issues

#### Symbol Not Found

```
ld: undefined reference to 'symbol_name'
```

**Solutions:**
- Check module dependencies in module.py
- Verify required libraries are included
- Check conf_hide_symbols didn't remove needed symbol

#### Out of Memory During Build

```
g++: fatal error: Killed signal terminated program
```

**Solutions:**
- Increase build host memory
- Use swap space
- Reduce parallel jobs: `./scripts/build -j2`
- Disable LTO if enabled

### 10.2 Boot Issues

#### Kernel Panic on Boot

**Common Causes:**
- Missing required driver
- Incorrect boot parameters
- Memory too low

**Debug:**
```bash
./scripts/run.py -e '--verbose --bootchart /myapp'
```

#### Slow Boot Time

**Check:**
- Filesystem type (ZFS vs ROFS)
- Console type (VGA vs serial)
- PCI enumeration (use --nopci if safe)
- Boot method (use -k for PVH)

### 10.3 Runtime Issues

#### Out of Memory

**Tuning:**
```bash
# Increase VM memory
./scripts/run.py --memsize=4096

# Or reduce memory pools
./scripts/build conf_mem_l1_cache_size=128
```

#### Poor Performance

**Profiling:**
```bash
# Enable tracing
./scripts/run.py -e '--trace=sched,mutex /myapp'

# Analyze trace
./scripts/trace.py build/last/trace.out
```

---

## 11. Best Practices

### 11.1 Development Workflow

1. **Start with defaults:**
   ```bash
   ./scripts/build image=myapp
   ```

2. **Profile and measure:**
   ```bash
   ./scripts/run.py -e '--bootchart --trace=* /myapp'
   ```

3. **Optimize incrementally:**
   ```bash
   # Try ROFS
   ./scripts/build fs=rofs image=myapp
   
   # Add symbol hiding
   ./scripts/build fs=rofs conf_hide_symbols=1 image=myapp
   
   # Test each change
   ```

4. **Document your configuration:**
   ```bash
   # Create build script
   cat > build.sh <<EOF
   #!/bin/bash
   ./scripts/build \
       image=myapp \
       fs=rofs \
       conf_hide_symbols=1
   EOF
   ```

### 11.2 Production Checklist

- [ ] Optimize for target hypervisor
- [ ] Choose appropriate filesystem
- [ ] Enable symbol hiding for size
- [ ] Set optimal memory size
- [ ] Configure CPU count appropriately
- [ ] Test boot time and memory usage
- [ ] Profile application performance
- [ ] Document all customizations
- [ ] Test failure scenarios
- [ ] Plan for monitoring and logging

### 11.3 Configuration Management

```bash
# Version control your build configuration
git add conf/profiles/production.mk
git add scripts/build-production.sh

# Document in README
echo "Build: ./scripts/build-production.sh" > BUILD.md
```

---

## 12. References

### Configuration Files
- `conf/base.mk` - Base configuration
- `conf/x64.mk` - x86_64 specific
- `conf/aarch64.mk` - ARM64 specific
- `Makefile` - Main build logic

### Documentation
- [OSv Wiki](https://github.com/cloudius-systems/osv/wiki)
- [Modularization](https://github.com/cloudius-systems/osv/wiki/Modularization)
- [Filesystems](https://github.com/cloudius-systems/osv/wiki/Filesystems)

### Tools
- `./scripts/build` - Main build script
- `./scripts/module.py` - Module management
- `./scripts/run.py` - Run OSv images
- `./scripts/trace.py` - Performance analysis

---

## Appendix: Quick Reference

### Common Build Commands

```bash
# Minimal build
./scripts/build image=empty fs=rofs conf_hide_symbols=1

# Development build
./scripts/build mode=debug image=cli,httpserver-api,myapp

# Production optimized
./scripts/build fs=rofs conf_hide_symbols=1 lto=1 image=myapp

# Cross-compile for ARM
./scripts/build arch=aarch64 image=myapp

# Java application
./scripts/build JAVA_VERSION=17 \
    image=openjdk-zulu-9-and-above,myapp

# Custom profile
./scripts/build --profile=minimal image=myapp
```

### Size Optimization Checklist

- [ ] `fs=rofs` (save ~3 MB vs ZFS)
- [ ] `conf_hide_symbols=1` (save ~3 MB)
- [ ] `image=empty` (save ~2-3 MB vs CLI)
- [ ] Remove unused drivers (save ~500 KB)
- [ ] Use `-Os` optimization (save ~10%)
- [ ] Enable LTO (save ~10-15%)

### Performance Optimization Checklist

- [ ] Use PVH boot (`-k`)
- [ ] Disable PCI if possible (`--nopci`)
- [ ] Use serial console (`--console=serial`)
- [ ] Enable vhost-net (`-v`)
- [ ] Use multiqueue virtio-net
- [ ] Tune memory pools
- [ ] Enable lazy_stack for thread-heavy apps
- [ ] Use appropriate filesystem (ROFS for stateless)
