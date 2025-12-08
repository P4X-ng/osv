# OSv Best Practices and Common Pitfalls

## Table of Contents

1. [Introduction](#introduction)
2. [Application Porting Best Practices](#application-porting-best-practices)
3. [Performance Optimization](#performance-optimization)
4. [Security Considerations](#security-considerations)
5. [Common Pitfalls and Solutions](#common-pitfalls-and-solutions)
6. [Debugging Strategies](#debugging-strategies)
7. [Production Deployment](#production-deployment)
8. [Troubleshooting Guide](#troubleshooting-guide)

---

## 1. Introduction

This document consolidates best practices, common issues, and solutions based on experience from the OSv community and the upstream cloudius-systems/osv repository.

### Key Insights from Upstream Issues

**Major Known Issues:**
1. **Issue #450**: VFS coarse-grained locking causing disk I/O performance bottleneck
2. **Issue #1013**: Memory pool auto-tuning needed for optimal performance
3. **Issue #987**: VGA console initialization adds 60-70ms to boot time
4. **Limited fork/exec support**: Not all POSIX applications work
5. **Memory requirements**: Minimum 11MB RAM, higher than other unikernels

---

## 2. Application Porting Best Practices

### 2.1 Assessing Application Compatibility

#### Compatibility Checklist

Before porting an application to OSv, verify:

- [ ] **No fork/exec usage**: OSv doesn't support multi-process model
- [ ] **Minimal process interactions**: No IPC via pipes, signals limited
- [ ] **Syscall compatibility**: Check if all required syscalls are implemented
- [ ] **File I/O patterns**: Consider VFS locking limitations (issue #450)
- [ ] **Thread-based design**: Application should use threads, not processes
- [ ] **Library dependencies**: All dependencies must be available

#### Quick Compatibility Test

```bash
# Check for fork/exec usage (whole words only)
grep -rw 'fork\|exec' myapp/src/

# Check syscall usage with strace (on Linux)
strace -c ./myapp

# List shared library dependencies
ldd ./myapp

# Check for problematic patterns
grep -rw 'pipe\|signal\|setuid' myapp/src/
```

### 2.2 Porting Strategies

#### Strategy 1: Direct Port (Easiest)

**Best For:** Applications that are already thread-based and self-contained

```bash
# Copy binary from host
./scripts/manifest_from_host.sh /usr/bin/myapp

# Build image
./scripts/build --append-manifest image=empty

# Run
./scripts/run.py -e '/usr/bin/myapp'
```

**Example - Python application:**
```bash
# Copy Python and dependencies
./scripts/manifest_from_host.sh -w python3 -w /usr/lib/python3.*

# Build
./scripts/build image=python-from-host,myapp --append-manifest

# Run
./scripts/run.py -e 'python3 /myapp.py'
```

#### Strategy 2: Recompilation

**Best For:** C/C++ applications, better optimization

```bash
# Cross-compile for OSv
export CXX=x86_64-linux-gnu-g++
export CC=x86_64-linux-gnu-gcc

# Compile with OSv toolchain
make

# Create module
cat > module.py <<EOF
from osv.modules import api

usr_files = api.to_manifest([
    '/usr/bin/myapp: ${MODULE_DIR}/myapp',
])
EOF

# Build image
./scripts/build image=mymodule
```

#### Strategy 3: Modification Required

**For applications using fork/exec:**

```cpp
// Before: Multi-process
pid_t pid = fork();
if (pid == 0) {
    // Child process
    execl("/usr/bin/worker", "worker", NULL);
}

// After: Multi-threaded
#include <thread>
std::thread worker([] {
    // Worker code
    worker_main();
});
worker.detach();
```

### 2.3 Language-Specific Guidelines

#### Java Applications

**Best Practices:**
```bash
# Use appropriate Java version
./scripts/build JAVA_VERSION=17 \
    image=openjdk-zulu-9-and-above,myapp

# Optimize JVM for container
./scripts/run.py -e \
    'java -Xmx1G -Xms1G \
     -XX:+UseG1GC \
     -XX:MaxGCPauseMillis=200 \
     -jar /myapp.jar'
```

**JVM Tuning:**
- Set heap size explicitly (-Xmx, -Xms)
- Use G1GC for predictable pauses
- Enable container support flags
- Monitor GC with `-verbose:gc`

**Common Issues:**
```bash
# Out of memory
# Solution: Increase --memsize and set appropriate -Xmx

# Slow startup
# Solution: Use lazy_stack for thread-heavy applications
./scripts/build conf_lazy_stack=1 JAVA_VERSION=17 ...
```

#### Go Applications

**Best Practices:**
```bash
# Static linking (recommended)
CGO_ENABLED=0 go build -o myapp

# Build OSv image
./scripts/build image=golang,myapp

# Run
./scripts/run.py -e '/myapp'
```

**Go-Specific Issues:**
- **Goroutines**: Work well with OSv's scheduler
- **cgo**: Can cause issues, prefer pure Go
- **File I/O**: Be aware of VFS locking (issue #450)

#### Python Applications

**Best Practices:**
```bash
# Use Python from host
./scripts/build image=python-from-host,myapp

# Or compile Python into image
./scripts/build image=python3x,myapp
```

**Virtual Environment:**
```bash
# Include virtualenv in image
usr_files = [
    '/myapp/': '${MODULE_DIR}/myapp/',
    '/myapp/venv/': '${MODULE_DIR}/venv/',
]
```

**Common Issues:**
- **Import errors**: Ensure all dependencies copied
- **File paths**: Use absolute paths or set PYTHONPATH
- **C extensions**: May need recompilation

#### Node.js Applications

**Best Practices:**
```bash
# Copy Node.js from host
./scripts/build image=node-from-host,myapp

# Run
./scripts/run.py -e 'node /myapp.js'
```

**Performance:**
- Use --max-old-space-size to limit memory
- Enable clustering with worker threads (not processes)
- Monitor event loop lag

### 2.4 Dependency Management

#### Identifying Dependencies

```bash
# For dynamically linked executables
ldd /path/to/myapp

# For Python
pip freeze > requirements.txt

# For Node.js
npm list --depth=0

# For Java
mvn dependency:tree
```

#### Including Dependencies

```python
# In module.py
from osv.modules import api

# Copy specific libraries
usr_files = api.to_manifest([
    '/usr/lib/libmylib.so.1: ->/usr/lib/libmylib.so.1',
    '/usr/lib/libother.so: ->/usr/lib/libother.so',
])

# Copy entire directory
usr_files = [
    '/myapp/lib/': '${MODULE_DIR}/lib/',
]
```

---

## 3. Performance Optimization

### 3.1 Boot Time Optimization

**Target: <10ms boot time**

#### Technique 1: Filesystem Selection

```bash
# ZFS (default): ~200ms on QEMU
./scripts/build fs=zfs

# ROFS: ~40ms on QEMU, ~5ms on Firecracker
./scripts/build fs=rofs

# RAMFS: ~10ms on QEMU
./scripts/build fs=ramfs
```

**Impact:** 5-20x faster boot with ROFS vs ZFS

#### Technique 2: Disable VGA Console

```bash
# Add boot parameter
./scripts/run.py -e '--console=serial /myapp'
```

**Impact:** Save 60-70ms (issue #987)

#### Technique 3: Skip PCI Enumeration

```bash
# When using minimal devices (Firecracker, microvm)
./scripts/run.py -e '--nopci /myapp'
```

**Impact:** Save 10-20ms

#### Technique 4: Use PVH Boot

```bash
# Direct kernel boot (x86_64 only)
./scripts/run.py -k -e '/myapp'
```

**Impact:** Skip bootloader, faster startup

#### Combined Optimization

```bash
#!/bin/bash
# fast-boot.sh - Achieve <10ms boot

./scripts/build image=empty fs=rofs conf_hide_symbols=1

./scripts/firecracker.py \
    --boot-args="--nopci --console=serial" \
    --mem=128 \
    --cpus=1 \
    -e '/myapp'
```

**Result:** ~5ms boot time on Firecracker

### 3.2 Runtime Performance

#### Memory Optimization

**Issue:** High memory overhead (min 11MB)

**Solutions:**

1. **Lazy Stack Allocation**
```bash
./scripts/build conf_lazy_stack=1
```
**Benefit:** Save 4KB per thread

2. **Tune Memory Pools**
```bash
# Reduce for memory-constrained environments
./scripts/build \
    conf_mem_l1_cache_size=128 \
    conf_mem_l2_cache_size=2048
```

3. **Minimize Loaded Modules**
```bash
# Only include essentials
./scripts/build image=empty
```

#### CPU Optimization

**1. CPU Pinning**
```bash
# Pin OSv to specific CPUs on host
taskset -c 0-3 ./scripts/run.py --cpus=4
```

**2. vCPU Configuration**
```bash
# Match application thread count
./scripts/run.py --cpus=4  # For 4-threaded app

# Don't over-provision
# Bad: --cpus=16 for single-threaded app
# Good: --cpus=1 or --cpus=2
```

**3. Priority Tuning** (in application code)
```cpp
#include <sched.hh>

// Set high priority for latency-critical thread
sched::thread::current()->set_priority(10);

// Pin to specific CPU
sched::thread::pin(2);
```

#### Disk I/O Optimization

**Known Issue:** VFS coarse-grained locking (issue #450)

**Workarounds:**

1. **Use Memory Filesystem When Possible**
```bash
./scripts/build fs=ramfs  # or fs=rofs for read-only
```

2. **Minimize Concurrent File Access**
```cpp
// Bad: Multiple threads reading same file
for (int i = 0; i < 10; i++) {
    std::thread([]{
        std::ifstream f("/data/file.txt");
        // Read...
    }).detach();
}

// Better: Read once, share data
std::string data;
{
    std::ifstream f("/data/file.txt");
    std::stringstream ss;
    ss << f.rdbuf();
    data = ss.str();
}
// Share data among threads
```

3. **Use VirtioFS for Development**
```bash
# Better performance, no locking issues
./scripts/run.py --mount-fs=virtiofs,mydata,/data
```

#### Network Optimization

**1. Enable vhost-net**
```bash
sudo ./scripts/run.py -n -v  # -v enables vhost
```
**Benefit:** 2-3x throughput improvement

**2. Use Multiqueue**
```bash
./scripts/run.py --cpus=4 --virtio-net-queues=4
```
**Benefit:** Better multicore scaling

**3. Tune TCP Parameters**
```bash
./scripts/run.py -e \
    '--sysctl net.inet.tcp.sendspace=262144 \
     --sysctl net.inet.tcp.recvspace=262144 \
     /myapp'
```

**4. Disable Nagle Algorithm** (for low-latency)
```cpp
int flag = 1;
setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
```

### 3.3 Profiling and Benchmarking

#### Using trace.py

```bash
# Enable tracing
./scripts/run.py -e '--trace=sched,mutex,block /myapp'

# Analyze trace
./scripts/trace.py build/last/trace.out

# Generate flamegraph
./scripts/trace.py -f flamegraph build/last/trace.out > flame.svg
```

#### Identifying Bottlenecks

```bash
# Trace points
--trace=sched       # Scheduler activity
--trace=mutex       # Lock contention
--trace=block       # Block I/O
--trace=net         # Network activity
--trace=page        # Page faults
--trace=malloc      # Memory allocation
```

#### Boot Time Analysis

```bash
./scripts/run.py -e '--bootchart /myapp'

# Output shows timing breakdown:
# disk read, decompression, init, driver load, etc.
```

---

## 4. Security Considerations

### 4.1 Security Model

**OSv Characteristics:**
- Single address space (no process isolation)
- No user accounts or permissions
- Smaller attack surface vs Linux
- No privilege escalation (single privilege level)

### 4.2 Security Best Practices

#### 1. Minimize Kernel Size

```bash
# Remove unnecessary components
./scripts/build image=empty fs=rofs conf_hide_symbols=1
```

**Benefit:** Fewer potential vulnerabilities

#### 2. Use Read-Only Filesystem

```bash
./scripts/build fs=rofs
```

**Benefit:** Prevents runtime tampering with code

#### 3. Disable Unnecessary Services

```bash
# Don't include CLI or API servers in production
./scripts/build image=myapp  # Not image=cli,httpserver-api,myapp
```

#### 4. Network Isolation

```bash
# Use firewall rules at hypervisor level
# Limit exposed ports
# Use separate network namespace
```

#### 5. Secret Management

```bash
# Don't embed secrets in image
# Use environment variables or external secret store

# Bad:
usr_files = [
    '/etc/secrets/api_key.txt: ${MODULE_DIR}/api_key.txt',
]

# Good: Mount secrets at runtime
./scripts/run.py \
    --mount-fs=virtiofs,secrets,/secrets \
    -e '/myapp'
```

### 4.3 Common Vulnerabilities

#### Memory Safety Issues

**Buffer Overflows:**
```cpp
// Bad
char buf[100];
strcpy(buf, user_input);  // No bounds checking

// Good
char buf[100];
strncpy(buf, user_input, sizeof(buf) - 1);
buf[sizeof(buf) - 1] = '\0';

// Better
std::string buf = user_input;
```

**Use-After-Free:**
```cpp
// Bad
char* ptr = malloc(100);
free(ptr);
strcpy(ptr, "data");  // Use after free

// Good: Use RAII
std::unique_ptr<char[]> ptr(new char[100]);
// Automatic cleanup
```

#### Integer Overflows

```cpp
// Bad
size_t size = user_size;
void* buf = malloc(size * sizeof(int));  // Can overflow

// Good
size_t size = user_size;
if (size > SIZE_MAX / sizeof(int)) {
    return -EINVAL;
}
void* buf = malloc(size * sizeof(int));
```

### 4.4 Security Checklist

Production Security Checklist:

- [ ] Minimize image size (fewer components)
- [ ] Use ROFS for immutable applications
- [ ] Don't include CLI/API in production
- [ ] Keep OSv updated (security patches)
- [ ] Use network firewall rules
- [ ] Don't embed secrets in image
- [ ] Enable compiler security flags
- [ ] Regular security audits
- [ ] Monitor for CVEs in dependencies
- [ ] Implement application-level authentication
- [ ] Use TLS for network communication
- [ ] Validate all external inputs

---

## 5. Common Pitfalls and Solutions

### 5.1 Build Issues

#### Pitfall: Missing Dependencies

**Symptom:**
```
./scripts/build: error: Module 'mymodule' not found
```

**Solution:**
```bash
# Check module dependencies
cat modules/mymodule/module.py

# Install missing modules
./scripts/build image=dependency1,dependency2,mymodule

# Or check for typos in module name
ls modules/
```

#### Pitfall: Out of Memory During Build

**Symptom:**
```
g++: fatal error: Killed signal terminated program cc1plus
```

**Solution:**
```bash
# Reduce parallel jobs
./scripts/build -j2

# Or increase system memory/swap

# For link-time optimization
./scripts/build lto=0  # Disable LTO
```

#### Pitfall: Cross-Compilation Failures

**Symptom:**
```
./scripts/build arch=aarch64: toolchain not found
```

**Solution:**
```bash
# Install aarch64 toolchain
./scripts/download_aarch64_packages.py

# Or on Debian/Ubuntu
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

### 5.2 Runtime Issues

#### Pitfall: Application Won't Start

**Symptom:**
```
ELF load error: /myapp: cannot open shared object file
```

**Solution:**
```bash
# Check dependencies
ldd /path/to/myapp

# Include missing libraries
./scripts/manifest_from_host.sh /path/to/libmissing.so

# Or use static linking
go build -ldflags="-extldflags=-static" myapp.go
```

#### Pitfall: "fork: Function not implemented"

**Symptom:**
```
OSv error: fork() not supported
```

**Solution:**
```cpp
// Rewrite to use threads instead of fork
// Before:
pid_t pid = fork();
if (pid == 0) {
    // Child work
    exit(0);
}

// After:
std::thread worker([] {
    // Child work
});
worker.join();
```

#### Pitfall: Out of Memory at Runtime

**Symptom:**
```
std::bad_alloc
malloc: Cannot allocate memory
```

**Solution:**
```bash
# Increase VM memory
./scripts/run.py --memsize=2048  # 2GB

# Or optimize application memory usage

# For Java
./scripts/run.py -e 'java -Xmx1G ...'

# Check actual memory usage
# (via REST API or monitoring)
```

#### Pitfall: Slow Boot Time

**Symptom:**
Boot takes >1 second

**Solution:**
```bash
# Check current boot time
./scripts/run.py -e '--bootchart /myapp'

# Optimize:
# 1. Use ROFS instead of ZFS
./scripts/build fs=rofs

# 2. Disable VGA console
./scripts/run.py -e '--console=serial /myapp'

# 3. Skip PCI scan
./scripts/run.py -e '--nopci /myapp'

# 4. Use Firecracker
./scripts/firecracker.py
```

#### Pitfall: Poor Disk I/O Performance

**Symptom:**
Low throughput, high latency on file operations

**Cause:** VFS coarse-grained locking (issue #450)

**Solutions:**
```bash
# 1. Use ROFS for read-only workloads
./scripts/build fs=rofs

# 2. Use VirtioFS for better performance
./scripts/run.py --mount-fs=virtiofs,data,/data

# 3. Minimize concurrent file access in application

# 4. Use memory filesystem when possible
./scripts/build fs=ramfs
```

#### Pitfall: Network Connectivity Issues

**Symptom:**
```
Cannot connect to network
DHCP timeout
```

**Solutions:**
```bash
# 1. Check network configuration
./scripts/run.py -n  # Enable networking

# 2. Use vhost-net for better reliability
sudo ./scripts/run.py -n -v

# 3. Set static IP if DHCP fails
./scripts/run.py -e \
    '--ip=eth0,192.168.122.100,255.255.255.0 \
     --defaultgw=192.168.122.1 \
     --nameserver=8.8.8.8 \
     /myapp'

# 4. Check hypervisor network configuration
# Ensure tap device or bridge is set up correctly
```

### 5.3 Application-Specific Issues

#### Java: Class Not Found

**Symptom:**
```
java.lang.ClassNotFoundException
```

**Solution:**
```python
# In module.py, include all JARs
usr_files = [
    '/myapp/lib/*.jar: ${MODULE_DIR}/lib/',
    '/myapp/app.jar: ${MODULE_DIR}/app.jar',
]

# Set classpath
./scripts/run.py -e \
    'java -cp /myapp/lib/*:/myapp/app.jar Main'
```

#### Python: Import Errors

**Symptom:**
```
ImportError: No module named 'mymodule'
```

**Solution:**
```python
# Include Python packages
usr_files = [
    '/myapp/': '${MODULE_DIR}/myapp/',
    '/usr/lib/python3.8/': '->/usr/lib/python3.8/',
]

# Set PYTHONPATH
./scripts/run.py -e \
    'PYTHONPATH=/myapp python3 main.py'
```

#### Go: Undefined Symbol

**Symptom:**
```
undefined symbol: __vdso_clock_gettime
```

**Solution:**
```bash
# Use static linking
CGO_ENABLED=0 go build -o myapp

# Or disable cgo
go build -tags netgo -o myapp
```

---

## 6. Debugging Strategies

### 6.1 Debug Build

```bash
# Build in debug mode
./scripts/build mode=debug image=myapp

# Includes:
# - Debug symbols
# - Assertions enabled
# - No optimization
# - Larger binary size
```

### 6.2 Using GDB

```bash
# Start OSv under GDB
./scripts/run.py -g -e '/myapp'

# In another terminal
gdb build/debug.x64/kernel.elf
(gdb) target remote :1234
(gdb) symbol-file build/debug.x64/loader.elf
(gdb) break main
(gdb) continue
```

**Common GDB Commands:**
```
(gdb) bt           # Backtrace
(gdb) info threads # List threads
(gdb) thread 3     # Switch to thread 3
(gdb) p variable   # Print variable
(gdb) x/10x addr   # Examine memory
```

### 6.3 Tracing

```bash
# Enable specific trace points
./scripts/run.py -e '--trace=sched,mutex /myapp'

# Trace everything (verbose)
./scripts/run.py -e '--trace=* /myapp'

# Analyze trace
./scripts/trace.py build/last/trace.out

# Generate report
./scripts/trace.py --summary build/last/trace.out
```

### 6.4 Logging

```bash
# Redirect output to file
./scripts/run.py -e '--redirect=/tmp/log /myapp'

# Increase verbosity
./scripts/run.py -e '--verbose /myapp'

# Application logging
# Use fprintf(stderr, ...) or std::cerr
# Output appears on serial console
```

### 6.5 Memory Debugging

**AddressSanitizer (ASAN):**
```bash
# Build with ASAN
make conf_sanitize=1

# Run
./scripts/run.py
# Detects: use-after-free, buffer overflows, etc.
```

**Memory Leak Detection:**
```bash
# Enable allocation tracking
./scripts/run.py -e '--trace=malloc /myapp'

# Analyze allocation patterns
./scripts/trace.py build/last/trace.out | grep malloc
```

### 6.6 Network Debugging

```bash
# Trace network activity
./scripts/run.py -e '--trace=net /myapp'

# Use tcpdump on host (for external networking)
sudo tcpdump -i tap0

# Check network interface status
# Via CLI (if included):
# ifconfig
# netstat -an
```

### 6.7 REST API Monitoring

If httpserver-monitoring-api is included:

```bash
# Build with monitoring
./scripts/build image=httpserver-monitoring-api,myapp

# Run
./scripts/run.py -e '/myapp' &

# Query metrics
curl http://192.168.122.15:8000/os/memory/stats
curl http://192.168.122.15:8000/os/threads
curl http://192.168.122.15:8000/os/trace
```

---

## 7. Production Deployment

### 7.1 Pre-Deployment Checklist

- [ ] **Testing complete**: All functionality tested on OSv
- [ ] **Performance profiled**: Meets requirements
- [ ] **Memory sized**: Appropriate --memsize set
- [ ] **CPU allocated**: Appropriate --cpus set
- [ ] **Image optimized**: Minimal size, fast boot
- [ ] **Secrets external**: Not embedded in image
- [ ] **Monitoring configured**: Metrics and logging
- [ ] **Rollback plan**: Can revert to previous version
- [ ] **Documentation**: Usage and configuration documented
- [ ] **Security reviewed**: No known vulnerabilities

### 7.2 Cloud Deployment

#### AWS EC2

```bash
# Build image
./scripts/build image=myapp fs=rofs

# Convert to raw
./scripts/convert raw

# Upload to S3 and create AMI
./scripts/deploy_to_aws.sh
```

**Recommendations:**
- Use instance types with ENA support
- Include ena driver in image
- Use EBS-optimized instances for storage
- Configure security groups appropriately

#### Google Cloud Platform

```bash
# Build image
./scripts/build image=myapp fs=rofs

# Convert and upload
./scripts/deploy_to_gce.sh
```

#### Firecracker

```bash
# Build minimal image
./scripts/build image=empty fs=rofs conf_hide_symbols=1

# Run with Firecracker
./scripts/firecracker.py \
    --boot-args="--nopci --console=serial" \
    --mem=128 \
    --cpus=1
```

### 7.3 Monitoring in Production

#### Metrics to Monitor

1. **Memory Usage**
```bash
# Via REST API
curl http://$IP:8000/os/memory/stats

# Key metrics:
# - memory.free
# - memory.allocated
# - memory.buffers
```

2. **CPU Usage**
```bash
# CPU utilization per vCPU
curl http://$IP:8000/os/cpu/stats
```

3. **Network Traffic**
```bash
# Network interface statistics
curl http://$IP:8000/os/network/stats

# Metrics:
# - rx_bytes, tx_bytes
# - rx_packets, tx_packets
# - rx_errors, tx_errors
```

4. **Application Metrics**
- Response time
- Request rate
- Error rate
- Custom business metrics

#### External Monitoring

```bash
# Prometheus integration
# Export metrics from REST API

# Example exporter
while true; do
    curl -s http://$IP:8000/os/memory/stats | \
        jq '.memory.free' | \
        sed 's/^/osv_memory_free /' | \
        curl --data-binary @- http://pushgateway:9091/metrics/job/osv
    sleep 10
done
```

### 7.4 Logging

```bash
# Redirect logs to external system
./scripts/run.py -e '--redirect=/dev/console /myapp'

# On host, capture serial output
# Forward to log aggregation system (ELK, Splunk, etc.)
```

### 7.5 Updates and Rollback

#### Blue-Green Deployment

```bash
# 1. Build new version
./scripts/build image=myapp-v2

# 2. Deploy to "green" environment

# 3. Test "green" environment

# 4. Switch traffic to "green"

# 5. Keep "blue" for rollback
```

#### Versioning

```bash
# Tag images with version
mv build/last/usr.img build/myapp-v1.0.0.img

# Create manifest
cat > versions.txt <<EOF
v1.0.0: myapp-v1.0.0.img
v0.9.0: myapp-v0.9.0.img
EOF
```

---

## 8. Troubleshooting Guide

### 8.1 Diagnostic Steps

When encountering issues:

1. **Check logs**: Look for error messages
2. **Verify configuration**: Double-check all parameters
3. **Test on QEMU**: Simplifies debugging vs cloud
4. **Enable verbose mode**: `--verbose`
5. **Use debug build**: `mode=debug`
6. **Trace execution**: `--trace=*`
7. **Check resources**: Memory, CPU, disk space
8. **Review recent changes**: What changed?

### 8.2 Quick Reference

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Won't boot | Missing driver | Check hypervisor compatibility |
| Slow boot | ZFS + VGA console | Use ROFS + --console=serial |
| Out of memory | Too little allocated | Increase --memsize |
| App won't start | Missing library | Include in manifest |
| fork error | Not supported | Rewrite using threads |
| Poor I/O | VFS locking | Use ROFS or VirtioFS |
| No network | DHCP failure | Set static IP or check network |
| High CPU | Busy loop | Profile with trace.py |
| Crash | Segfault | Use GDB or debug build |
| Memory leak | Not freeing | Trace malloc/free |

### 8.3 Getting Help

**Community Resources:**
- [OSv Mailing List](https://groups.google.com/forum/#!forum/osv-dev)
- [GitHub Issues](https://github.com/cloudius-systems/osv/issues)
- [Wiki](https://github.com/cloudius-systems/osv/wiki)

**When Asking for Help:**
1. Describe the problem clearly
2. Include error messages
3. Provide OSv version: Check `git describe --tags` or README.md
4. Share build command
5. Share run command
6. Include relevant logs
7. Describe what you've tried

---

## 9. Summary

### Key Best Practices

1. **Start simple**: Test with default settings first
2. **Profile before optimizing**: Measure, don't guess
3. **Use appropriate filesystem**: ROFS for stateless, ZFS for stateful
4. **Minimize image size**: Only include necessary components
5. **Test on QEMU first**: Before deploying to cloud
6. **Monitor in production**: Metrics and logging
7. **Document your setup**: For future reference
8. **Keep OSv updated**: Security and bug fixes

### Critical Issues to Remember

1. **No fork/exec**: Application must be thread-based
2. **VFS locking (issue #450)**: May impact disk I/O performance
3. **Memory overhead**: Minimum 11MB RAM required
4. **VGA console**: Adds 60-70ms to boot (use --console=serial)
5. **Single address space**: No process isolation
6. **Limited syscall coverage**: Check compatibility

### When to Use OSv

**Good Fit:**
- Stateless microservices
- Single-application containers
- Low-latency services
- Fast boot requirements
- Cloud-native applications

**Not Recommended:**
- Multi-process applications
- Fork/exec-heavy workloads
- Heavy disk I/O (until issue #450 resolved)
- Applications needing process isolation
- Complete POSIX compatibility required

### Quick Start Template

```bash
#!/bin/bash
# quickstart.sh - Template for OSv deployment

# 1. Build image
./scripts/build \
    image=myapp \
    fs=rofs \
    conf_hide_symbols=1

# 2. Test locally
./scripts/run.py \
    --cpus=2 \
    --memsize=512 \
    -e '--console=serial /myapp'

# 3. Profile
./scripts/run.py -e \
    '--bootchart --trace=sched /myapp'

# 4. Optimize based on profile

# 5. Deploy to production
./scripts/convert raw
# Upload and deploy...
```

---

## Appendix: Common Error Messages

### Build Errors

```
Error: Module 'X' not found
→ Check module name, verify it exists in modules/

Error: Submodule not initialized
→ git submodule update --init --recursive

Error: Killed signal during build
→ Out of memory, reduce -j or add swap
```

### Runtime Errors

```
ELF load error: cannot open shared object
→ Missing library, include in manifest

fork: Function not implemented
→ Not supported, use threads instead

std::bad_alloc / Cannot allocate memory
→ Increase --memsize or reduce usage

Cannot mount filesystem
→ Check filesystem type and boot parameters
```

### Network Errors

```
DHCP timeout
→ Check network config, try static IP

Connection refused
→ Check firewall, security groups, port bindings

No route to host
→ Check default gateway, routing
```

---

## References

- [OSv Documentation](https://github.com/cloudius-systems/osv/wiki)
- [Upstream Issues](https://github.com/cloudius-systems/osv/issues)
- [OSv Blog](http://blog.osv.io/)
- [Community Forum](https://groups.google.com/forum/#!forum/osv-dev)
