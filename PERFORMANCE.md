# Performance Optimization Guide for OSv

## Overview

This document provides guidance on performance optimization strategies for the OSv unikernel, covering both kernel development and application deployment. It addresses findings from the Amazon Q Code Review regarding algorithm efficiency, resource management, and caching opportunities.

## Table of Contents

1. [Performance Philosophy](#performance-philosophy)
2. [Memory Management Optimization](#memory-management-optimization)
3. [CPU and Scheduling](#cpu-and-scheduling)
4. [I/O Performance](#io-performance)
5. [Network Performance](#network-performance)
6. [Caching Strategies](#caching-strategies)
7. [Profiling and Measurement](#profiling-and-measurement)
8. [Common Performance Pitfalls](#common-performance-pitfalls)

## Performance Philosophy

OSv is designed with performance as a primary goal:

- **Minimal Overhead**: Unikernel architecture eliminates kernel-user space transitions
- **Hypervisor Optimization**: Leverages paravirtualization for better performance
- **Lock-Free Data Structures**: Used extensively in hot paths
- **CPU Affinity**: Per-CPU data structures minimize cache coherency traffic
- **Zero-Copy I/O**: Reduces memory copying in network and file operations

### Design Principles

1. **Measure First**: Always profile before optimizing
2. **Optimize Hot Paths**: Focus on frequently executed code
3. **Avoid Premature Optimization**: Keep code readable until profiling proves otherwise
4. **Use Appropriate Data Structures**: Choose the right tool for the job
5. **Minimize Allocations**: Reduce memory allocation frequency in critical paths

## Memory Management Optimization

### Memory Allocation Strategies

OSv provides multiple allocators optimized for different use cases:

```cpp
// For kernel structures - fast, minimal overhead
void* kernel_alloc = memory::alloc_phys_contiguous_aligned(size, align);

// For general purpose allocation
void* ptr = malloc(size);

// For page-aligned allocations
void* pages = memory::alloc_page();
```

### Best Practices

1. **Pool Allocation for Fixed-Size Objects**:
   ```cpp
   // Create object pool to avoid repeated malloc/free
   class ObjectPool {
       std::vector<Object*> free_list;
   public:
       Object* allocate() {
           if (!free_list.empty()) {
               auto obj = free_list.back();
               free_list.pop_back();
               return obj;
           }
           return new Object();
       }
       void deallocate(Object* obj) {
           free_list.push_back(obj);
       }
   };
   ```

2. **Pre-allocation**:
   ```cpp
   // Pre-allocate buffers for repeated use
   class BufferManager {
       std::unique_ptr<char[]> buffer;
       size_t buffer_size;
   public:
       BufferManager(size_t size) 
           : buffer(new char[size]), buffer_size(size) {}
       
       char* get_buffer() { return buffer.get(); }
   };
   ```

3. **Avoid Fragmentation**:
   - Use fixed-size allocations when possible
   - Group allocations by lifetime
   - Consider using arenas for temporary allocations

4. **Memory Alignment**:
   ```cpp
   // Align data structures to cache lines (64 bytes)
   struct alignas(64) CacheLine {
       // Prevents false sharing between CPUs
       atomic<int> counter;
       char padding[60];
   };
   ```

### Memory Leak Prevention

- Use RAII and smart pointers
- Run with Valgrind in development
- Monitor memory usage in production
- Use OSv's built-in memory statistics

### Monitoring Memory Usage

```bash
# View memory statistics
scripts/osv/client.py --api GET /os/memory/stats

# View per-thread memory
scripts/osv/client.py --api GET /os/threads
```

## CPU and Scheduling

### Thread Management

OSv uses a sophisticated scheduler with per-CPU runqueues:

```cpp
// Create thread with specific CPU affinity
sched::thread::attr attr;
attr.pin(sched::cpu::from_id(cpu_id));
sched::thread* t = sched::thread::make([](){ work(); }, attr);
```

### Optimization Techniques

1. **CPU Pinning**:
   ```cpp
   // Pin thread to specific CPU to improve cache locality
   sched::thread::current()->pin(cpu);
   ```

2. **Avoid Unnecessary Context Switches**:
   ```cpp
   // Use spinlocks for very short critical sections
   mutex::lock l(short_lived_lock);
   
   // Use sleeping locks for longer operations
   mutex m;
   WITH_LOCK(m) {
       longer_operation();
   }
   ```

3. **Batch Processing**:
   ```cpp
   // Process items in batches to reduce overhead
   while (!queue.empty()) {
       auto batch = queue.dequeue_batch(BATCH_SIZE);
       process_batch(batch);
   }
   ```

4. **Work Stealing**:
   - OSv scheduler implements work stealing
   - Distributes load across CPUs automatically
   - Configure workqueue parameters for specific workloads

### Lock-Free Programming

OSv uses lock-free data structures in performance-critical code:

```cpp
// Example: Lock-free queue
template<typename T>
class LockFreeQueue {
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    
public:
    void enqueue(T value) {
        Node* node = new Node(value);
        Node* prev_tail = tail.exchange(node, std::memory_order_acq_rel);
        prev_tail->next.store(node, std::memory_order_release);
    }
};
```

**Guidelines**:
- Use lock-free structures only when profiling shows locking overhead
- Ensure proper memory ordering (acquire/release semantics)
- Test thoroughly on multiple architectures
- Document the algorithm used

## I/O Performance

### File System Performance

OSv supports multiple file systems with different performance characteristics:

| File System | Use Case | Performance |
|-------------|----------|-------------|
| ROFS | Read-only, embedded apps | Fastest read |
| ZFS | Full-featured, pooled storage | Good overall |
| VirtioFS | Host file sharing | Variable (host-dependent) |
| RAM FS | Temporary files | Fastest write |

### Optimization Strategies

1. **Page Cache Usage**:
   ```cpp
   // OSv automatically caches file pages
   // Frequent reads benefit from cache
   // Cache size configurable via OSv parameters
   ```

2. **Direct I/O for Large Transfers**:
   ```cpp
   // Bypass page cache for large sequential I/O
   int fd = open("/path", O_RDONLY | O_DIRECT);
   ```

3. **Asynchronous I/O**:
   ```cpp
   // Use AIO for overlapping I/O operations
   struct aiocb cb;
   cb.aio_fildes = fd;
   cb.aio_buf = buffer;
   cb.aio_nbytes = size;
   aio_read(&cb);
   // Do other work
   aio_suspend(&cb, 1, nullptr);
   ```

4. **Read-Ahead Optimization**:
   - OSv implements automatic read-ahead
   - Benefits sequential read patterns
   - Configured via VFS parameters

### Known Performance Characteristics

From CODE_REVIEW.md:
> OSv lags behind Linux in disk-I/O-intensive workloads partially due to 
> coarse-grained locking in VFS around read/write operations

**Mitigation**:
- Use appropriate file system for workload
- Consider multiple smaller files vs. one large file
- Leverage page cache for repeated reads
- Use memory-mapped I/O when appropriate

## Network Performance

### VirtIO Network Optimization

OSv's network stack is optimized for VirtIO:

```cpp
// Enable multi-queue networking for better scaling
// Configure in VM: -device virtio-net-pci,queues=4
```

### Performance Techniques

1. **Zero-Copy Networking**:
   ```cpp
   // OSv supports zero-copy socket operations
   // Use sendfile() for file-to-socket transfers
   ssize_t sent = sendfile(sock_fd, file_fd, &offset, count);
   ```

2. **TCP Tuning**:
   ```bash
   # Increase socket buffer sizes
   sysctl -w net.inet.tcp.sendbuf_max=4194304
   sysctl -w net.inet.tcp.recvbuf_max=4194304
   ```

3. **Batched Network Operations**:
   ```cpp
   // Use vectored I/O for multiple buffers
   struct iovec iov[IOV_MAX];
   // Fill iov array
   writev(socket, iov, count);
   ```

4. **Connection Pooling**:
   - Reuse TCP connections
   - Implement connection pooling for client applications
   - Reduce connection setup overhead

### Network Stack Features

- **TCP Segmentation Offload (TSO)**: Reduces CPU usage
- **Large Receive Offload (LRO)**: Improves receive performance
- **Checksum Offload**: Offloads checksum calculation to hardware
- **Multi-queue Support**: Distributes packet processing across CPUs

## Caching Strategies

### Page Cache

OSv implements a unified page cache:

```cpp
// Page cache is automatic for file I/O
// Monitor cache effectiveness
auto stats = get_page_cache_stats();
auto hit_rate = stats.hits / (stats.hits + stats.misses);
```

### Application-Level Caching

1. **Result Caching**:
   ```cpp
   class ResultCache {
       std::unordered_map<Key, Value> cache;
       std::mutex mutex;
   public:
       std::optional<Value> get(const Key& k) {
           std::lock_guard<std::mutex> lock(mutex);
           auto it = cache.find(k);
           return it != cache.end() ? std::optional(it->second) : std::nullopt;
       }
       void put(const Key& k, const Value& v) {
           std::lock_guard<std::mutex> lock(mutex);
           cache[k] = v;
       }
   };
   ```

2. **Lazy Initialization**:
   ```cpp
   class ExpensiveResource {
       std::unique_ptr<Resource> resource;
   public:
       Resource* get() {
           if (!resource) {
               resource = std::make_unique<Resource>();
           }
           return resource.get();
       }
   };
   ```

3. **Memoization**:
   ```cpp
   template<typename F>
   class Memoize {
       F func;
       mutable std::map<Args, Result> cache;
   public:
       Result operator()(Args... args) const {
           auto key = std::make_tuple(args...);
           auto it = cache.find(key);
           if (it != cache.end()) return it->second;
           auto result = func(args...);
           cache[key] = result;
           return result;
       }
   };
   ```

### Cache Considerations

- **Cache Invalidation**: Implement proper invalidation strategies
- **Cache Size**: Monitor memory usage, implement eviction policies
- **Cache Coherency**: Consider multi-CPU caching implications
- **False Sharing**: Align cached data to cache line boundaries

## Profiling and Measurement

### Built-in Profiling Tools

OSv includes several profiling capabilities:

1. **CPU Profiling**:
   ```bash
   # Enable CPU profiling
   scripts/osv/prof.py --uri=<osv-ip>:8000 --time=10
   
   # Generate flame graph
   scripts/osv/prof.py --flamegraph
   ```

2. **Tracing**:
   ```bash
   # Trace specific events
   scripts/osv/trace.py --uri=<osv-ip>:8000
   
   # Trace file system operations
   scripts/osv/trace.py --filter=vfs
   ```

3. **Memory Profiling**:
   ```bash
   # Track memory allocations
   scripts/osv/trace.py --filter=memory
   ```

### Performance Counters

```cpp
// Use OSv's tracepoint infrastructure
#include <osv/trace.hh>

TRACEPOINT(trace_my_function, "function=%s time=%d", const char*, int);

void my_function() {
    auto start = std::chrono::steady_clock::now();
    // Do work
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    trace_my_function("my_function", duration.count());
}
```

### External Tools

- **perf**: Linux perf can profile OSv in some configurations
- **gprof**: Traditional profiling (limited support)
- **Valgrind**: Memory profiling and leak detection
- **SystemTap**: Dynamic tracing (with limitations)

### Performance Metrics to Monitor

1. **System Metrics**:
   - CPU utilization per core
   - Memory usage and page cache hit rate
   - Context switches per second
   - Interrupts per second

2. **Application Metrics**:
   - Request latency (p50, p95, p99)
   - Throughput (requests/second)
   - Error rates
   - Resource usage per request

3. **I/O Metrics**:
   - Disk IOPS
   - Network throughput
   - Buffer queue depths
   - Cache hit rates

## Common Performance Pitfalls

### 1. Excessive Memory Allocation

**Problem**:
```cpp
// Allocating in hot path
void process_items(const std::vector<Item>& items) {
    for (const auto& item : items) {
        auto buffer = new char[BUFFER_SIZE];  // BAD: Allocates every iteration
        process_with_buffer(item, buffer);
        delete[] buffer;
    }
}
```

**Solution**:
```cpp
// Reuse buffer
void process_items(const std::vector<Item>& items) {
    auto buffer = std::make_unique<char[]>(BUFFER_SIZE);
    for (const auto& item : items) {
        process_with_buffer(item, buffer.get());
    }
}
```

### 2. Lock Contention

**Problem**:
```cpp
// Global lock causes contention
std::mutex global_mutex;
void worker() {
    std::lock_guard<std::mutex> lock(global_mutex);
    // Long operation
}
```

**Solution**:
```cpp
// Per-CPU or striped locking
std::array<std::mutex, NUM_CPUS> per_cpu_mutex;
void worker() {
    auto cpu = sched::cpu::current();
    std::lock_guard<std::mutex> lock(per_cpu_mutex[cpu->id]);
    // Operation
}
```

### 3. False Sharing

**Problem**:
```cpp
// Adjacent variables cause cache line bouncing
struct Counters {
    std::atomic<int> counter1;  // Same cache line
    std::atomic<int> counter2;  // Same cache line
};
```

**Solution**:
```cpp
// Pad to separate cache lines
struct Counters {
    alignas(64) std::atomic<int> counter1;
    alignas(64) std::atomic<int> counter2;
};
```

### 4. Unnecessary Copies

**Problem**:
```cpp
// Copies data unnecessarily
void process(std::vector<int> data) {  // BAD: Copies vector
    for (auto x : data) {  // BAD: Copies each element
        // Process
    }
}
```

**Solution**:
```cpp
// Pass by const reference, use const reference in loop
void process(const std::vector<int>& data) {
    for (const auto& x : data) {
        // Process
    }
}
```

### 5. Synchronous I/O in Event Loop

**Problem**:
```cpp
// Blocks event loop
void handle_request(Request& req) {
    auto data = read_file_sync(req.filename);  // Blocks!
    send_response(data);
}
```

**Solution**:
```cpp
// Use async I/O or separate I/O thread pool
void handle_request(Request& req) {
    async_read_file(req.filename, [](auto data) {
        send_response(data);
    });
}
```

## Performance Checklist

When optimizing code:

- [ ] Profile to identify actual bottlenecks
- [ ] Focus on hot paths (top 20% of time)
- [ ] Reduce memory allocations in critical sections
- [ ] Use appropriate locking granularity
- [ ] Avoid false sharing with cache line alignment
- [ ] Use lock-free structures where appropriate
- [ ] Batch operations to reduce overhead
- [ ] Pre-allocate and reuse buffers
- [ ] Use const references to avoid copies
- [ ] Leverage OSv's zero-copy capabilities
- [ ] Monitor cache hit rates
- [ ] Pin threads to CPUs for cache locality
- [ ] Use vectorized I/O for multiple operations
- [ ] Consider read-ahead for sequential access
- [ ] Implement appropriate caching strategies

## Additional Resources

### OSv Documentation
- [OSv Performance Monitoring](https://github.com/cloudius-systems/osv/wiki/Monitoring-OSv)
- [Profiling OSv Applications](https://github.com/cloudius-systems/osv/wiki/Profiling)
- [Tracing Framework](https://github.com/cloudius-systems/osv/wiki/Trace-analysis-using-trace.py)

### Performance Analysis Papers
- [OSv USENIX ATC'14 Paper](https://www.usenix.org/conference/atc14/technical-sessions/presentation/kivity)
- [Seastar Framework (used with OSv)](http://seastar.io/)

### Tools
- [FlameGraph](https://github.com/brendangregg/FlameGraph)
- [Linux perf](https://perf.wiki.kernel.org/)
- [Intel VTune](https://software.intel.com/content/www/us/en/develop/tools/oneapi/components/vtune-profiler.html)

## Contributing Performance Improvements

When submitting performance-related changes:

1. Include benchmark results showing the improvement
2. Profile before and after to demonstrate impact
3. Document the optimization technique used
4. Consider trade-offs (code complexity vs. performance gain)
5. Ensure the optimization doesn't break functionality
6. Update this guide with new techniques learned

Last Updated: 2025-12-10
