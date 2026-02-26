# Running .NET Applications on OSv

This guide explains how to run .NET applications on OSv, including setup, configuration, and troubleshooting.

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Quick Start](#quick-start)
4. [Symbol Hiding Requirement](#symbol-hiding-requirement)
5. [Building .NET Images](#building-net-images)
6. [Deployment Strategies](#deployment-strategies)
7. [Advanced Configuration](#advanced-configuration)
8. [Performance Considerations](#performance-considerations)
9. [Known Issues and Limitations](#known-issues-and-limitations)
10. [Troubleshooting](#troubleshooting)

## Overview

OSv supports running .NET applications (formerly .NET Core) version 5.0 and later. .NET applications run on OSv similarly to how they run on standard Linux distributions, with some important considerations regarding C++ library compatibility.

### Key Features

- ✅ .NET 5.0, 6.0, 7.0, 8.0+ support
- ✅ Framework-dependent and self-contained deployments
- ✅ ASP.NET Core web applications
- ✅ Console applications
- ✅ Background services and workers
- ✅ Full async/await and threading support

### Architecture

```
┌─────────────────────────────────────┐
│     .NET Application (.dll)          │
└─────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────┐
│   .NET Runtime (CoreCLR, CoreFX)    │
└─────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────┐
│         OSv Kernel (libc)           │
│  (with hidden C++ stdlib symbols)    │
└─────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────┐
│          Hypervisor                 │
│    (QEMU, Firecracker, etc.)        │
└─────────────────────────────────────┘
```

## Prerequisites

### On Host System

1. **Linux host** (Ubuntu 20.04+, Fedora 31+, or equivalent)
2. **.NET SDK or Runtime** (5.0 or later):
   ```bash
   # Ubuntu/Debian
   wget https://packages.microsoft.com/config/ubuntu/20.04/packages-microsoft-prod.deb
   sudo dpkg -i packages-microsoft-prod.deb
   sudo apt-get update
   sudo apt-get install -y dotnet-sdk-8.0
   
   # Fedora
   sudo dnf install dotnet-sdk-8.0
   ```

3. **OSv build environment**:
   ```bash
   git clone https://github.com/cloudius-systems/osv.git
   cd osv
   git submodule update --init --recursive
   ./scripts/setup.py
   ```

4. **Verify installations**:
   ```bash
   dotnet --version  # Should output 8.0.x or similar
   make --version    # Should output GNU Make 4.x
   ```

## Quick Start

### 1. Create a Simple .NET Application

```bash
# Create a new console application
mkdir /tmp/myapp
cd /tmp/myapp
dotnet new console

# Modify Program.cs to be more interesting (optional)
cat > Program.cs << 'EOF'
using System;
using System.Threading;

Console.WriteLine("Hello from .NET on OSv!");
Console.WriteLine($"Running on .NET {Environment.Version}");
Console.WriteLine($"OS: {Environment.OSVersion}");
Console.WriteLine("Sleeping for 2 seconds...");
Thread.Sleep(2000);
Console.WriteLine("Goodbye!");
EOF

# Publish the application
dotnet publish -c Release -o publish --self-contained false
```

### 2. Create OSv Module for Your App

```bash
cd /path/to/osv
mkdir -p modules/myapp

cat > modules/myapp/module.py << 'EOF'
from osv.modules.filemap import FileMap
from osv.modules import api

api.require('dotnet-from-host')

usr_files = FileMap()
usr_files.add('/tmp/myapp/publish').to('/app') \
    .include('**')

default = '/usr/bin/dotnet /app/myapp.dll'
EOF

cat > modules/myapp/Makefile << 'EOF'
.PHONY: module clean
include ../common.gmk
module:
	@echo "myapp module ready"
clean:
	@echo "Nothing to clean"
EOF
```

### 3. Build OSv Image with Symbol Hiding

**IMPORTANT**: Always use `conf_hide_symbols=1` when building .NET images:

```bash
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,myapp
```

### 4. Run Your Application

```bash
# Run with QEMU
./scripts/run.py

# Run with Firecracker
./scripts/firecracker.py

# Run with custom memory
./scripts/run.py -m 512M

# Run with specific command
./scripts/run.py -e '/usr/bin/dotnet /app/myapp.dll'
```

## Symbol Hiding Requirement

### Why Symbol Hiding is Necessary

The .NET runtime includes its own C++ standard library dependencies. OSv by default exports its own `libstdc++.so.6`, which can conflict with the version expected by .NET. This leads to:

- Symbol resolution errors
- Runtime crashes
- Undefined behavior

### The Solution: conf_hide_symbols=1

When you build OSv with `conf_hide_symbols=1`, it:

1. **Hides C++ stdlib symbols** from being exported by the kernel
2. **Reduces kernel size** (~30% smaller, from ~6.8 MB to ~3.6 MB)
3. **Allows .NET to use its own C++ libraries** without conflicts

### What Gets Hidden

With `conf_hide_symbols=1`:
- `libstdc++.so.6` (C++ standard library)
- `libboost_system.so` (Boost libraries, if applicable)
- Other internal kernel symbols not needed by applications

### What Stays Visible

- All standard C library functions (libc)
- POSIX APIs
- System calls
- OSv-specific APIs

### Build Command Template

```bash
# Always use this pattern for .NET applications
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,your-app-module
```

## Building .NET Images

### Framework-Dependent Deployment (Recommended)

Smaller images, relies on shared .NET runtime from `dotnet-from-host` module:

```bash
# Publish app
dotnet publish -c Release -o publish --self-contained false

# Build OSv image
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,your-app
```

**Advantages**:
- Smaller application size
- Faster build times
- Shared runtime across multiple apps
- Easier updates to .NET runtime

**Disadvantages**:
- Requires dotnet-from-host module
- All apps must use same .NET version

### Self-Contained Deployment

Includes .NET runtime with the application:

```bash
# Publish app with runtime included
dotnet publish -c Release -o publish --self-contained true -r linux-x64

# Build OSv image (still needs symbol hiding!)
./scripts/build fs=rofs conf_hide_symbols=1 image=your-app
```

**Advantages**:
- No dependency on dotnet-from-host module
- Different apps can use different .NET versions
- More portable

**Disadvantages**:
- Larger image size (30-80 MB additional)
- Slower build times
- Duplicate runtime if multiple apps

### Trimmed Deployment

Reduces size by removing unused assemblies:

```bash
# Publish with trimming
dotnet publish -c Release -o publish \
    --self-contained false \
    -p:PublishTrimmed=true

# Build normally
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,your-app
```

⚠️ **Warning**: Trimming can break reflection-heavy code. Test thoroughly!

## Deployment Strategies

### Console Applications

```python
# module.py
from osv.modules.filemap import FileMap
from osv.modules import api

api.require('dotnet-from-host')

usr_files = FileMap()
usr_files.add('/path/to/app/publish').to('/app') \
    .include('**')

default = '/usr/bin/dotnet /app/myapp.dll arg1 arg2'
```

### ASP.NET Core Web Applications

```python
# module.py
from osv.modules.filemap import FileMap
from osv.modules import api

api.require('dotnet-from-host')

usr_files = FileMap()
usr_files.add('/path/to/webapp/publish').to('/app') \
    .include('**')

# Listen on all interfaces
default = '/usr/bin/dotnet /app/webapp.dll --urls http://0.0.0.0:8080'
```

Run with networking:
```bash
sudo ./scripts/run.py -nv
# Access at http://192.168.122.15:8080
```

### Background Services / Workers

```python
# module.py
default = '/usr/bin/dotnet /app/worker.dll'
```

### Multiple Applications

```python
# module.py - Run multiple .NET apps
from osv.modules.filemap import FileMap
from osv.modules import api

api.require('dotnet-from-host')

usr_files = FileMap()
usr_files.add('/path/to/app1/publish').to('/app1') \
    .include('**')
usr_files.add('/path/to/app2/publish').to('/app2') \
    .include('**')

# Choose one as default, or create a launcher script
default = '/usr/bin/dotnet /app1/app1.dll'
```

## Advanced Configuration

### Custom .NET Installation Path

If .NET is in a non-standard location, modify `modules/dotnet-from-host/module.py`:

```python
# Force specific .NET installation
dotnet_path = '/opt/dotnet'
```

### Including .NET SDK (for runtime compilation)

By default, only the runtime is included. To include the SDK:

```python
# In modules/dotnet-from-host/module.py
usr_files.add(dotnet_path).to('/usr/lib/dotnet') \
    .include('dotnet') \
    .include('shared/**') \
    .include('host/**') \
    .include('sdk/**')  # Add SDK
```

### Environment Variables

Set .NET environment variables in your module:

```python
# module.py
from osv.modules import api

api.require('dotnet-from-host')

# ... file mappings ...

default = [
    '--env=DOTNET_ENVIRONMENT=Production',
    '--env=ASPNETCORE_URLS=http://0.0.0.0:8080',
    '/usr/bin/dotnet', '/app/myapp.dll'
]
```

Or pass at runtime:
```bash
./scripts/run.py -e '--env=DOTNET_ENVIRONMENT=Production /usr/bin/dotnet /app/myapp.dll'
```

### Memory Configuration

.NET GC can be tuned:

```bash
./scripts/run.py -e '--env=DOTNET_gcServer=1 --env=DOTNET_GCHeapCount=2 /usr/bin/dotnet /app/myapp.dll'
```

Common .NET memory settings:
- `DOTNET_gcServer=1` - Use server GC (better for multi-core)
- `DOTNET_GCHeapCount=N` - Number of GC heaps
- `DOTNET_GCHeapHardLimit=0x10000000` - Max heap size (hex bytes)

## Performance Considerations

### Boot Time

- **ROFS**: ~5-40ms boot time
- **ZFS**: ~200ms boot time
- Recommendation: Use ROFS for faster boots

### Memory Usage

- Minimum: ~50-100 MB for simple .NET apps
- Typical: ~200-512 MB for web applications
- Recommendation: Start with 512 MB, adjust based on load

### Throughput

.NET on OSv should have comparable throughput to .NET on Linux for CPU-bound workloads. I/O-bound workloads may show different characteristics due to OSv's VFS implementation.

### Optimization Tips

1. **Use Release builds**:
   ```bash
   dotnet publish -c Release
   ```

2. **Enable ReadyToRun**:
   ```bash
   dotnet publish -c Release -p:PublishReadyToRun=true
   ```

3. **Use ROFS filesystem**:
   ```bash
   ./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,myapp
   ```

4. **Minimize image size**:
   - Use framework-dependent deployment
   - Use trimming (carefully)
   - Exclude debug symbols

5. **Tune GC for your workload**:
   - Server GC for high-throughput services
   - Workstation GC for lower latency

## Known Issues and Limitations

### Symbol Hiding is Mandatory

❌ **Will NOT work**:
```bash
./scripts/build image=dotnet-from-host,myapp
```

✅ **Will work**:
```bash
./scripts/build conf_hide_symbols=1 image=dotnet-from-host,myapp
```

### Cross-Platform Limitations

- Cannot cross-compile .NET images (e.g., aarch64 from x64)
- .NET SDK/Runtime must match host architecture
- Some CPU-specific optimizations may not work

### File I/O Considerations

- ROFS is read-only - apps needing write access need ZFS
- VFS locking may affect high-concurrency disk I/O
- For write-heavy apps, consider mounting external volumes

### Networking

- IPv4 is fully supported
- IPv6 requires building from the [ipv6 branch](https://github.com/cloudius-systems/osv/tree/ipv6) (upstream)
- DPDK networking is not yet tested with .NET

### Native Interop

- P/Invoke should work for most cases
- Native libraries must be included in the image
- May need to adjust `LD_LIBRARY_PATH` for some scenarios

### Debugging

- gdb debugging of .NET code is limited
- Use logging and tracing for diagnostics
- OSv trace points work at the kernel level

## Troubleshooting

### Problem: "Could not find .NET installation"

**Cause**: .NET is not installed or not in standard location

**Solution**:
```bash
# Verify .NET is installed
which dotnet
dotnet --version

# If not installed, install it
sudo apt-get install dotnet-sdk-8.0  # Ubuntu
sudo dnf install dotnet-sdk-8.0      # Fedora
```

### Problem: Symbol resolution errors or crashes

**Cause**: Built without `conf_hide_symbols=1`

**Solution**:
```bash
# Clean and rebuild with symbol hiding
./scripts/build clean
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,myapp
```

### Problem: Out of memory errors

**Cause**: Insufficient memory allocated to OSv

**Solution**:
```bash
# Increase memory allocation
./scripts/run.py -m 512M  # or 1G, 2G, etc.
```

### Problem: Application fails to start

**Cause**: Missing dependencies or incorrect paths

**Solution**:
1. Verify publish output is complete:
   ```bash
   ls -la /path/to/publish/
   ```
2. Check module.py paths match publish location
3. Try self-contained deployment:
   ```bash
   dotnet publish -c Release -o publish --self-contained true -r linux-x64
   ```

### Problem: Slow performance

**Cause**: Debug build, GC configuration, or I/O bottlenecks

**Solution**:
1. Use Release build: `dotnet publish -c Release`
2. Tune GC: `--env=DOTNET_gcServer=1`
3. Use ROFS: `fs=rofs`
4. Increase memory: `-m 1G`

### Problem: Application crashes intermittently

**Cause**: Threading issues, async behavior, or GC behavior

**Solution**:
1. Check OSv console for kernel messages
2. Enable .NET diagnostics:
   ```bash
   ./scripts/run.py -e '--env=DOTNET_EnableDiagnostics=1 /usr/bin/dotnet /app/myapp.dll'
   ```
3. Try different GC mode:
   ```bash
   ./scripts/run.py -e '--env=DOTNET_gcServer=0 /usr/bin/dotnet /app/myapp.dll'
   ```

### Problem: Cannot access web application

**Cause**: Networking not configured, firewall, or binding issues

**Solution**:
1. Enable networking:
   ```bash
   sudo ./scripts/run.py -nv
   ```
2. Ensure app binds to 0.0.0.0:
   ```bash
   ./scripts/run.py -e '/usr/bin/dotnet /app/webapp.dll --urls http://0.0.0.0:8080'
   ```
3. Check OSv IP address in boot output

### Getting Help

If you encounter issues:

1. Check the [OSv wiki](https://github.com/cloudius-systems/osv/wiki)
2. Search [OSv issues](https://github.com/cloudius-systems/osv/issues)
3. Ask on [OSv Google Group](https://groups.google.com/forum/#!forum/osv-dev)
4. Review [original .NET discussion](https://groups.google.com/g/osv-dev/c/k69cHw7qvTg/m/3udXnsDGBwAJ)

## Examples

### Example 1: Hello World Console App

See [modules/dotnet-example/README.md](../modules/dotnet-example/README.md)

### Example 2: ASP.NET Core Web API

```bash
# Create web API
mkdir /tmp/webapi
cd /tmp/webapi
dotnet new webapi
dotnet publish -c Release -o publish --self-contained false

# Create module
mkdir -p modules/webapi
cat > modules/webapi/module.py << 'EOF'
from osv.modules.filemap import FileMap
from osv.modules import api

api.require('dotnet-from-host')

usr_files = FileMap()
usr_files.add('/tmp/webapi/publish').to('/app') \
    .include('**')

default = '/usr/bin/dotnet /app/webapi.dll --urls http://0.0.0.0:5000'
EOF

cat > modules/webapi/Makefile << 'EOF'
.PHONY: module clean
include ../common.gmk
module:
	@echo "webapi ready"
clean:
	@echo "Nothing to clean"
EOF

# Build and run
./scripts/build fs=rofs conf_hide_symbols=1 image=dotnet-from-host,webapi
sudo ./scripts/run.py -nv
# Access at http://192.168.122.15:5000/weatherforecast
```

### Example 3: Worker Service

```bash
# Create worker service
mkdir /tmp/worker
cd /tmp/worker
dotnet new worker
dotnet publish -c Release -o publish --self-contained false

# Module creation and build similar to above examples
```

## References

- [.NET Downloads](https://dotnet.microsoft.com/download)
- [.NET on Linux Guide](https://docs.microsoft.com/en-us/dotnet/core/install/linux)
- [OSv Symbol Hiding](KERNEL_CUSTOMIZATION_GUIDE.md#symbol-hiding-size-reduction)
- [Original .NET Discussion](https://groups.google.com/g/osv-dev/c/k69cHw7qvTg/m/3udXnsDGBwAJ)
- [.NET Runtime Issue #13790](https://github.com/dotnet/runtime/issues/13790)
- [OSv Modularization](https://github.com/cloudius-systems/osv/wiki/Modularization)
