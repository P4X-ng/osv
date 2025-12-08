# OSv Quick Reference Guide

## Build Commands

### Basic Builds

```bash
# Default build (ZFS, CLI, API server)
./scripts/build

# Minimal build
./scripts/build image=empty fs=rofs conf_hide_symbols=1

# Debug build
./scripts/build mode=debug image=myapp

# Specific module
./scripts/build image=native-example

# Multiple modules
./scripts/build image=cli,httpserver-api,myapp

# Java application
./scripts/build JAVA_VERSION=17 image=openjdk-zulu-9-and-above,myapp

# Cross-compile for ARM
./scripts/build arch=aarch64

# With custom parameters
./scripts/build fs=rofs conf_hide_symbols=1 conf_lazy_stack=1

# Clean build
./scripts/build clean
```

## Run Commands

### Basic Execution

```bash
# Default run
./scripts/run.py

# Run specific command
./scripts/run.py -e '/hello'

# With parameters
./scripts/run.py -e '/myapp --arg1 --arg2'

# Set memory and CPUs
./scripts/run.py --memsize=2048 --cpus=4

# Enable networking
./scripts/run.py -n

# Enable networking with vhost
sudo ./scripts/run.py -n -v

# PVH boot (faster)
./scripts/run.py -k

# Firecracker
./scripts/firecracker.py --mem=512 --cpus=2
```

### Boot Parameters

```bash
# Console configuration
./scripts/run.py -e '--console=serial /myapp'

# Disable PCI
./scripts/run.py -e '--nopci /myapp'

# Boot chart (timing)
./scripts/run.py -e '--bootchart /myapp'

# Static IP
./scripts/run.py -e '--ip=eth0,192.168.1.100,255.255.255.0 \
                      --defaultgw=192.168.1.1 \
                      --nameserver=8.8.8.8 \
                      /myapp'

# Redirect output
./scripts/run.py -e '--redirect=/tmp/out /myapp'

# Trace
./scripts/run.py -e '--trace=sched,mutex /myapp'

# Verbose
./scripts/run.py -e '--verbose /myapp'

# Multiple options
./scripts/run.py -e '--nopci --console=serial --bootchart /myapp'
```

## Configuration Options

### Build Configuration

| Option | Values | Description |
|--------|--------|-------------|
| `mode` | release, debug | Build mode |
| `arch` | x64, aarch64 | Target architecture |
| `fs` | zfs, rofs, ramfs | Filesystem type |
| `image` | module1,module2 | Modules to include |
| `conf_hide_symbols` | 0, 1 | Hide unused symbols |
| `conf_lazy_stack` | 0, 1 | Lazy stack allocation |
| `conf_drivers_mmio` | 0, 1 | MMIO drivers |
| `conf_drivers_xen` | 0, 1 | Xen drivers |
| `conf_drivers_vmware` | 0, 1 | VMware drivers |
| `lto` | 0, 1 | Link-time optimization |
| `JAVA_VERSION` | 8, 11, 17 | Java version |

### Runtime Configuration

| Option | Description |
|--------|-------------|
| `--memsize=N` | Memory in MB (default: 2048) |
| `--cpus=N` | Number of vCPUs (default: 4) |
| `-n` | Enable networking |
| `-v` | Enable vhost-net |
| `-k` | PVH boot mode |
| `-g` | GDB debugging |
| `--disk-image=FILE` | Disk image |
| `--second-disk-image=FILE` | Second disk |
| `--nvme-image=FILE` | NVMe disk |

## Filesystem Commands

### ZFS

```bash
# Build with ZFS
./scripts/build fs=zfs

# Mount ZFS at boot
./scripts/run.py -e '--mount-fs=zfs,/dev/vblk0,/ /myapp'

# With compression
./scripts/build fs=zfs zfs_compression=lz4
```

### ROFS (Read-Only)

```bash
# Build with ROFS
./scripts/build fs=rofs

# Use tmpfs for temporary files
./scripts/run.py -e '--tmpfs /tmp /myapp'
```

### RAMFS

```bash
# Build with RAMFS (files in kernel)
./scripts/build fs=ramfs
```

### VirtioFS

```bash
# Start virtiofsd on host
virtiofsd --socket-path=/tmp/vfsd.sock \
    --shared-dir=/path/to/share

# Mount in OSv
./scripts/run.py \
    --mount-fs=virtiofs,myshare,/shared \
    -e '/myapp'
```

## Debugging Commands

### GDB

```bash
# Start with GDB
./scripts/run.py -g

# In another terminal
gdb build/debug.x64/loader.elf
(gdb) target remote :1234
(gdb) break main
(gdb) continue

# Common GDB commands
(gdb) bt              # Backtrace
(gdb) info threads    # List threads
(gdb) thread N        # Switch thread
(gdb) print var       # Print variable
(gdb) x/10x addr      # Examine memory
```

### Tracing

```bash
# Enable tracing
./scripts/run.py -e '--trace=sched,mutex,block /myapp'

# Trace everything
./scripts/run.py -e '--trace=* /myapp'

# Analyze trace
./scripts/trace.py build/last/trace.out

# Summary
./scripts/trace.py --summary build/last/trace.out

# Flamegraph
./scripts/trace.py -f flamegraph build/last/trace.out > flame.svg
```

### Trace Points

| Trace Point | Description |
|-------------|-------------|
| `sched` | Scheduler activity |
| `mutex` | Lock contention |
| `block` | Block I/O |
| `net` | Network activity |
| `page` | Page faults |
| `malloc` | Memory allocation |
| `syscall` | System calls |

## Module Management

### Creating a Module

```python
# modules/mymodule/module.py
from osv.modules import api

# Dependencies
api.require('dependency1')

# Build
default = api.run('/bin/make -C ${MODULE_DIR}')

# Files to include
usr_files = [
    '/usr/bin/myapp: ${MODULE_DIR}/myapp',
    '/etc/config: ${MODULE_DIR}/config',
]
```

### Manifest File

```
# modules/mymodule/usr.manifest

# Copy files
/usr/bin/myapp: ${MODULE_DIR}/myapp
/usr/lib/libmy.so: ${MODULE_DIR}/libmy.so

# From host
/lib/libssl.so: ->/lib/libssl.so

# Entire directory
/myapp/data/*: ${MODULE_DIR}/data/*
```

## Performance Tuning

### Boot Time Optimization

```bash
# Target: <10ms boot
./scripts/build image=empty fs=rofs conf_hide_symbols=1

./scripts/firecracker.py \
    --boot-args="--nopci --console=serial" \
    --mem=128 \
    --cpus=1
```

### Memory Optimization

```bash
# Lazy stack for thread-heavy apps
./scripts/build conf_lazy_stack=1

# Tune memory pools
./scripts/build \
    conf_mem_l1_cache_size=256 \
    conf_mem_l2_cache_size=4096
```

### Network Optimization

```bash
# Enable vhost-net
sudo ./scripts/run.py -n -v

# Multiqueue
./scripts/run.py --cpus=4 --virtio-net-queues=4

# TCP tuning
./scripts/run.py -e \
    '--sysctl net.inet.tcp.sendspace=262144 \
     --sysctl net.inet.tcp.recvspace=262144 \
     /myapp'
```

## Size Optimization

### Minimize Kernel Size

```bash
# Minimal configuration
./scripts/build \
    image=empty \
    fs=rofs \
    conf_hide_symbols=1 \
    conf_drivers_mmio=0 \
    conf_drivers_xen=0 \
    conf_drivers_vmware=0

# Result: ~2.5-3 MB kernel
```

### Size Comparison

| Configuration | Size |
|---------------|------|
| Default (ZFS + libs) | 6.8 MB |
| ROFS + symbols hidden | 3.8 MB |
| ROFS + symbols hidden + minimal drivers | 3.2 MB |
| Fully optimized | 2.8 MB |

## Monitoring

### REST API

```bash
# Build with monitoring API
./scripts/build image=httpserver-monitoring-api,myapp

# Query metrics
curl http://192.168.122.15:8000/os/memory/stats
curl http://192.168.122.15:8000/os/cpu/stats
curl http://192.168.122.15:8000/os/threads
curl http://192.168.122.15:8000/os/network/stats
```

### Boot Analysis

```bash
# Boot chart
./scripts/run.py -e '--bootchart /myapp'

# Output shows:
# - disk read: XXms
# - uncompress: XXms
# - init: XXms
# - drivers: XXms
# Total: XXms
```

## Testing

### Unit Tests

```bash
# Run all tests
./scripts/build check

# Run with ROFS
./scripts/build check fs=rofs

# Run on Firecracker
./scripts/build image=tests
./scripts/test.py -p firecracker

# Run specific test
./scripts/run.py -e '/tests/tst-pthread.so'
```

## Common Issues

### Build Issues

```bash
# Submodules not initialized
git submodule update --init --recursive

# Out of memory during build
./scripts/build -j2  # Reduce parallel jobs

# Clean build
./scripts/build clean
```

### Runtime Issues

```bash
# Out of memory
./scripts/run.py --memsize=4096  # Increase memory

# Missing library
./scripts/manifest_from_host.sh /path/to/lib.so
./scripts/build --append-manifest

# fork() not supported
# Rewrite application to use threads

# Network not working
sudo ./scripts/run.py -n -v  # Enable vhost-net

# Slow boot
./scripts/build fs=rofs  # Use ROFS instead of ZFS
./scripts/run.py -e '--console=serial --nopci /myapp'
```

## Language-Specific

### Java

```bash
# Build
./scripts/build JAVA_VERSION=17 \
    image=openjdk-zulu-9-and-above,myapp

# Run with tuning
./scripts/run.py -e \
    'java -Xmx1G -Xms1G -XX:+UseG1GC -jar /myapp.jar'
```

### Python

```bash
# Build
./scripts/build image=python-from-host,myapp

# Run
./scripts/run.py -e 'python3 /myapp.py'

# With PYTHONPATH
./scripts/run.py -e 'PYTHONPATH=/myapp python3 /main.py'
```

### Node.js

```bash
# Build
./scripts/build image=node-from-host,myapp

# Run
./scripts/run.py -e 'node /myapp.js'

# With memory limit
./scripts/run.py -e 'node --max-old-space-size=512 /myapp.js'
```

### Go

```bash
# Static linking (recommended)
CGO_ENABLED=0 go build -o myapp

# Build image
./scripts/build image=golang,myapp

# Run
./scripts/run.py -e '/myapp'
```

### Rust

```bash
# Build statically
cargo build --release --target x86_64-unknown-linux-musl

# Build image
./scripts/build image=rust,myapp

# Run
./scripts/run.py -e '/myapp'
```

## Cloud Deployment

### AWS

```bash
# Build
./scripts/build image=myapp fs=rofs

# Convert
./scripts/convert raw

# Deploy (requires AWS CLI configured)
./scripts/deploy_to_aws.sh
```

### GCE

```bash
# Build
./scripts/build image=myapp fs=rofs

# Deploy
./scripts/deploy_to_gce.sh
```

### Firecracker

```bash
# Build minimal
./scripts/build image=empty fs=rofs conf_hide_symbols=1

# Run
./scripts/firecracker.py \
    --boot-args="--nopci --console=serial" \
    --mem=128 \
    --cpus=1 \
    -e '/myapp'
```

## Environment Variables

### Build Time

```bash
# Number of parallel jobs
export MAKEFLAGS=-j$(nproc)

# Cross compilation
export CROSS_PREFIX=aarch64-linux-gnu-

# Architecture
export ARCH=aarch64
```

### Runtime

```bash
# Set in boot command
./scripts/run.py -e 'MY_VAR=value /myapp'

# Multiple variables
./scripts/run.py -e 'VAR1=val1 VAR2=val2 /myapp'
```

## Useful Paths

### Source Tree

```
osv/
├── arch/              # Architecture-specific code
├── bsd/               # BSD networking stack
├── core/              # Core kernel components
├── drivers/           # Device drivers
├── fs/                # Filesystems
├── include/           # Header files
├── libc/              # C library (musl)
├── modules/           # Optional modules
├── scripts/           # Build and run scripts
└── tests/             # Unit tests
```

### Build Output

```
build/
├── release.x64/       # Release build for x86_64
│   ├── loader.elf     # Kernel ELF
│   └── usr.img        # Disk image
├── debug.x64/         # Debug build
└── last/              # Symlink to latest build
```

## Keyboard Shortcuts

### QEMU

- `Ctrl+A X` - Exit QEMU
- `Ctrl+A C` - QEMU monitor
- `Ctrl+A S` - Send break signal

### GDB

- `Ctrl+C` - Interrupt execution
- `Ctrl+D` - Exit GDB

## Quick Troubleshooting

| Problem | Command |
|---------|---------|
| Check OSv version | `./scripts/build --version` |
| View build log | `cat build.log` |
| Check memory usage | `curl http://$IP:8000/os/memory/stats` |
| View threads | `curl http://$IP:8000/os/threads` |
| Boot time breakdown | `./scripts/run.py -e '--bootchart /myapp'` |
| Profile performance | `./scripts/run.py -e '--trace=* /myapp'` |
| Debug with GDB | `./scripts/run.py -g` |
| View dependencies | `ldd /path/to/binary` |
| List modules | `ls modules/` |
| Check filesystem | `file build/last/usr.img` |

## Useful One-Liners

```bash
# Build and run in one command
./scripts/build image=myapp && ./scripts/run.py

# Check image size
du -h build/last/usr.img

# List files in image
./scripts/imgedit.py extract build/last/usr.img

# Copy file from host
./scripts/manifest_from_host.sh /usr/bin/curl && \
./scripts/build --append-manifest

# Quick test build
./scripts/build image=empty fs=rofs && ./scripts/run.py -e '/hello'

# Profile boot time
./scripts/run.py -e '--bootchart /hello' | grep Total

# Memory usage
./scripts/run.py -e '/hello' | grep "memory:"

# Find symbol in kernel
nm build/last/loader.elf | grep symbol_name

# Check which libraries are loaded
./scripts/run.py -e '/hello' | grep "Loaded"
```

## Additional Resources

- **Wiki**: https://github.com/cloudius-systems/osv/wiki
- **Forum**: https://groups.google.com/forum/#!forum/osv-dev
- **Issues**: https://github.com/cloudius-systems/osv/issues
- **Blog**: http://blog.osv.io/
- **Apps**: https://github.com/cloudius-systems/osv-apps
- **Capstan**: https://github.com/cloudius-systems/capstan

## Tips

1. Always test on QEMU before deploying to cloud
2. Use ROFS for faster boot and smaller images
3. Enable `conf_hide_symbols=1` to reduce size by ~50%
4. Use `conf_lazy_stack=1` for thread-heavy applications
5. Profile with `--trace` before optimizing
6. Monitor with REST API in production
7. Keep images minimal - only include what you need
8. Document your build configuration
9. Use version control for modules and manifests
10. Join the mailing list for help and updates
