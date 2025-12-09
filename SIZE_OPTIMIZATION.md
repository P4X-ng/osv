# OSv Size Optimization Guide

## Overview

OSv kernel size optimization is crucial for microservices and serverless deployments where memory footprint and boot time are critical. This guide provides comprehensive strategies to minimize OSv kernel and image sizes.

## Current Size Baseline

### Default OSv Kernel Sizes (as of 2024)

- **Minimal kernel** (`loader.elf` with symbols hidden): ~3.6 MB
- **Full kernel** (with libstdc++.so.6 and ZFS): ~6.8 MB
- **Complete image** (kernel + filesystem + app): 10-50 MB (varies by application)

### Size Comparison Context

OSv provides functionality equivalent to these Linux libraries:
- `libc.so.6` (~2 MB)
- `libstdc++.so.6` (~2 MB)  
- `libm.so.6` (~1.4 MB)
- `ld-linux-x86-64.so.2` (~184 KB)
- `libpthread.so.0` (~156 KB)
- Plus kernel functionality, drivers, and networking stack

## Configuration-Based Optimization

### 1. Build Mode Selection

```bash
# Release mode (optimized for size and performance)
./scripts/build mode=release

# Additional size optimizations
./scripts/build mode=release \
                CXXFLAGS="-Os -ffunction-sections -fdata-sections" \
                LDFLAGS="-Wl,--gc-sections"
```

### 2. Core Configuration Options

```makefile
# conf/profiles/minimal/base.mk

# Disable debugging features
conf_debug_memory=0
conf_debug_elf=0
conf_logger_debug=0

# Disable tracing infrastructure
conf_tracing=0

# Hide symbols to reduce size
conf_hide_symbols=1

# Optimize for size
conf_optimize_size=1

# Disable unused features
conf_preempt=0  # If cooperative scheduling is sufficient
```

### 3. Architecture-Specific Optimizations

```makefile
# conf/profiles/minimal/x64.mk

# Optimize for specific CPU (if deployment target is known)
CXXFLAGS += -march=skylake -mtune=skylake

# Enable size optimizations
CXXFLAGS += -Os -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections -Wl,--strip-all

# Remove debug information
CXXFLAGS += -g0
```

## Module-Level Optimization

### 1. Minimal Module Selection

Create ultra-minimal images by selecting only essential modules:

```python
# modules/minimal-app/module.py
from osv.modules import api

# Only include absolutely necessary modules
# NO httpserver, NO cli, NO monitoring

# For Java applications
api.require('java-base')  # Instead of full 'java'

# For native applications - no additional modules needed

default = api.run('/my_minimal_app')
```

### 2. Custom Minimal Modules

Create stripped-down versions of existing modules:

```python
# modules/java-minimal/module.py
from osv.modules import api

# Minimal Java runtime without management tools
api.require('java-base')
# Skip: java-mgmt, java-cmd, httpserver-jvm-plugin

default = api.run_java(
    classpath=['/app.jar'],
    args=['-Xmx128m', '-XX:+UseSerialGC']  # Minimal heap, simple GC
)
```

### 3. Module Dependency Analysis

Analyze and minimize module dependencies:

```bash
# Check what modules are actually needed
./scripts/module.py query my_app

# Build dependency graph
./scripts/module.py deps my_app --graph

# Remove unnecessary dependencies
./scripts/module.py optimize my_app
```

## Filesystem Optimization

### 1. Filesystem Type Selection

Choose the most appropriate filesystem for size:

```bash
# RAMFS: Smallest, embedded in kernel
./scripts/build fs=ramfs image=my_app
# Pros: Minimal size, fast access
# Cons: Limited to small applications, no persistence

# ROFS: Read-only, compressed
./scripts/build fs=rofs image=my_app  
# Pros: Good compression, reasonable size
# Cons: Read-only, slower than RAMFS

# ZFS: Full-featured but largest
./scripts/build fs=zfs image=my_app
# Pros: Full filesystem features, persistence
# Cons: Largest size, highest memory usage
```

### 2. File Inclusion Optimization

Minimize files included in the image:

```makefile
# usr.manifest - Only include essential files
/my_app: my_app
/lib/libc.so.6: $(ROOTFS)/lib/libc.so.6

# Exclude unnecessary files
# /usr/share/doc/*     # Documentation
# /usr/share/man/*     # Manual pages  
# /usr/share/locale/*  # Localization files
# /tmp/*               # Temporary files
```

### 3. Binary Stripping

Strip debug information and symbols:

```bash
# Strip application binaries
strip --strip-all my_app

# Strip shared libraries
strip --strip-unneeded lib/*.so

# Use in manifest
/my_app: my_app
    strip --strip-all $<
```

## Compiler and Linker Optimization

### 1. Compiler Flags for Size

```makefile
# Optimize for size
CXXFLAGS += -Os

# Function and data sections (enables dead code elimination)
CXXFLAGS += -ffunction-sections -fdata-sections

# Remove unused inline functions
CXXFLAGS += -fno-inline-functions-called-once

# Optimize string literals
CXXFLAGS += -fmerge-all-constants

# Remove frame pointers (x86_64)
CXXFLAGS += -fomit-frame-pointer
```

### 2. Linker Optimization

```makefile
# Dead code elimination
LDFLAGS += -Wl,--gc-sections

# Strip symbols
LDFLAGS += -Wl,--strip-all

# Optimize for size
LDFLAGS += -Wl,-O2

# Remove unused dynamic symbols
LDFLAGS += -Wl,--as-needed

# Compress debug sections (if debug info needed)
LDFLAGS += -Wl,--compress-debug-sections=zlib
```

### 3. Link-Time Optimization (LTO)

```makefile
# Enable LTO for better optimization
CXXFLAGS += -flto=thin
LDFLAGS += -flto=thin

# Additional LTO optimizations
CXXFLAGS += -fwhole-program-vtables
LDFLAGS += -Wl,--lto-O3
```

## Advanced Size Reduction Techniques

### 1. Custom Linker Scripts

Create optimized memory layout:

```ld
/* arch/x64/loader-minimal.ld */
SECTIONS {
    /* Merge similar sections */
    .text : {
        *(.text.hot)     /* Hot code first for better cache usage */
        *(.text)
        *(.text.unlikely) /* Cold code last */
    }
    
    /* Combine read-only data */
    .rodata : {
        *(.rodata*)
        *(.srodata*)
    }
    
    /* Optimize data layout */
    .data : {
        *(.data.hot)
        *(.data)
        *(.data.cold)
    }
    
    /* Remove unnecessary sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.debug*)
    }
}
```

### 2. Symbol Table Optimization

Minimize symbol table size:

```cpp
// Use anonymous namespaces instead of static
namespace {
    void internal_function() { }  // Not exported
}

// Mark functions as internal
__attribute__((visibility("internal")))
void helper_function() { }

// Use function-level visibility
#pragma GCC visibility push(hidden)
void private_api() { }
#pragma GCC visibility pop
```

### 3. Template Instantiation Control

Reduce template bloat:

```cpp
// Explicit template instantiation to control code generation
template class std::vector<int>;
template class std::string;

// Avoid template instantiation in headers
// Use extern template declarations
extern template class std::vector<MyClass>;
```

### 4. Library Optimization

Create minimal library versions:

```cpp
// Custom minimal libc++ implementation
namespace std {
    // Only implement used functionality
    template<typename T>
    class minimal_vector {
        // Minimal implementation
    };
}

// Replace heavy libraries with lightweight alternatives
// Instead of boost::regex, use simple string matching
// Instead of full JSON library, use minimal parser
```

## Application-Specific Optimization

### 1. Java Applications

```bash
# Use minimal Java runtime
./scripts/build image=java-base,my_java_app

# Optimize JVM settings for size
java -Xmx64m -XX:+UseSerialGC -XX:+TieredCompilation \
     -XX:TieredStopAtLevel=1 MyApp
```

### 2. Node.js Applications

```bash
# Use minimal Node.js build
./scripts/build image=node-minimal,my_node_app

# Optimize Node.js for size
node --max-old-space-size=64 --optimize-for-size app.js
```

### 3. Native C++ Applications

```cpp
// Minimize standard library usage
#include <cstdio>     // Instead of <iostream>
#include <cstring>    // Instead of <string>

// Use stack allocation instead of heap when possible
char buffer[256];     // Instead of std::string
int array[100];       // Instead of std::vector

// Avoid exceptions if possible (reduces code size)
#define MINIMAL_BUILD
#ifdef MINIMAL_BUILD
    #define throw_error(msg) std::abort()
#else
    #define throw_error(msg) throw std::runtime_error(msg)
#endif
```

## Measurement and Analysis

### 1. Size Analysis Tools

```bash
# Analyze kernel size
size build/release/loader.elf

# Detailed section analysis
objdump -h build/release/loader.elf

# Symbol size analysis
nm --size-sort build/release/loader.elf | tail -20

# Bloat analysis
bloaty build/release/loader.elf
```

### 2. Image Size Analysis

```bash
# Check image size
ls -lh build/release/usr.img

# Analyze filesystem contents
./scripts/run.py -e 'du -sh /*'

# Check for large files
./scripts/run.py -e 'find / -size +1M -ls'
```

### 3. Memory Usage Analysis

```bash
# Runtime memory usage
./scripts/run.py -e 'free -h'

# Detailed memory breakdown
./scripts/run.py -e 'cat /proc/meminfo'

# Application memory usage
./scripts/run.py -e 'ps aux'
```

## Size Optimization Profiles

### 1. Microservice Profile

```makefile
# conf/profiles/microservice/base.mk
conf_hide_symbols=1
conf_tracing=0
conf_debug_memory=0
conf_preempt=1
conf_lazy_stack=1

# Minimal feature set
conf_net_minimal=1
conf_fs_minimal=1
```

### 2. Serverless Profile

```makefile
# conf/profiles/serverless/base.mk
conf_hide_symbols=1
conf_tracing=0
conf_debug_memory=0
conf_preempt=0  # Cooperative scheduling for minimal overhead
conf_lazy_stack=1

# Ultra-minimal configuration
conf_boot_minimal=1
conf_drivers_minimal=1
```

### 3. IoT Profile

```makefile
# conf/profiles/iot/base.mk
conf_hide_symbols=1
conf_tracing=0
conf_debug_memory=0
conf_preempt=0
conf_lazy_stack=1

# Minimal memory footprint
conf_memory_minimal=1
conf_net_basic=1
```

## Build Scripts for Size Optimization

### 1. Automated Size Optimization

```bash
#!/bin/bash
# scripts/build-minimal.sh

# Build with size optimizations
./scripts/build mode=release \
                profile=minimal \
                fs=ramfs \
                CXXFLAGS="-Os -ffunction-sections -fdata-sections" \
                LDFLAGS="-Wl,--gc-sections -Wl,--strip-all" \
                image=$1

# Strip additional symbols
strip --strip-all build/release/loader.elf

# Compress image if supported
if command -v upx >/dev/null 2>&1; then
    upx --best build/release/loader.elf
fi

echo "Optimized image size:"
ls -lh build/release/usr.img
```

### 2. Size Comparison Script

```bash
#!/bin/bash
# scripts/size-compare.sh

echo "Building different configurations..."

# Default build
./scripts/build image=$1
DEFAULT_SIZE=$(stat -c%s build/release/usr.img)

# Optimized build
./scripts/build-minimal.sh $1
OPTIMIZED_SIZE=$(stat -c%s build/release/usr.img)

# Calculate savings
SAVINGS=$((DEFAULT_SIZE - OPTIMIZED_SIZE))
PERCENT=$((SAVINGS * 100 / DEFAULT_SIZE))

echo "Size comparison:"
echo "Default:   $(numfmt --to=iec $DEFAULT_SIZE)"
echo "Optimized: $(numfmt --to=iec $OPTIMIZED_SIZE)"
echo "Savings:   $(numfmt --to=iec $SAVINGS) ($PERCENT%)"
```

## Validation and Testing

### 1. Functionality Testing

```bash
# Ensure optimized build still works
./scripts/build-minimal.sh my_app
./scripts/run.py -e '/my_app --test'

# Run basic functionality tests
./scripts/test.py -p qemu --minimal
```

### 2. Performance Impact Assessment

```bash
# Measure boot time
time ./scripts/run.py -e '/my_app --benchmark'

# Memory usage comparison
./scripts/run.py -e 'free && /my_app' | grep Mem
```

## Best Practices Summary

### 1. Development Workflow

1. **Start with minimal profile** during development
2. **Add features incrementally** and measure size impact
3. **Regular size audits** to catch size regressions
4. **Automated size testing** in CI/CD pipeline

### 2. Size vs. Functionality Trade-offs

1. **Identify must-have features** vs. nice-to-have
2. **Use feature flags** for optional functionality
3. **Consider runtime loading** for rarely-used features
4. **Profile actual usage** to guide optimization decisions

### 3. Maintenance Considerations

1. **Document size requirements** and constraints
2. **Monitor size growth** over time
3. **Regular optimization reviews** as part of development process
4. **Size budgets** for new features

This guide provides comprehensive strategies for optimizing OSv kernel and image sizes. The key is to systematically apply these techniques while maintaining the functionality required for your specific use case.