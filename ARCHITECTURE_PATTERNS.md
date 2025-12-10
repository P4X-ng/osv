# Architecture and Design Patterns in OSv

## Overview

This document describes the architectural patterns, design principles, and software engineering practices used in the OSv unikernel. It addresses findings from the Amazon Q Code Review regarding design patterns usage, separation of concerns, and dependency management.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Design Patterns](#design-patterns)
3. [Separation of Concerns](#separation-of-concerns)
4. [Dependency Management](#dependency-management)
5. [Module System](#module-system)
6. [Interface Design](#interface-design)
7. [Error Handling](#error-handling)

## Architecture Overview

### Layered Architecture

OSv follows a layered architecture with clear boundaries between components:

```
┌─────────────────────────────────────┐
│     Application Layer               │
│  (User Applications)                │
├─────────────────────────────────────┤
│     C Standard Library (MUSL)       │
│  (libc, libm, libpthread, etc.)     │
├─────────────────────────────────────┤
│     System Services                 │
│  (VFS, Networking, Scheduler)       │
├─────────────────────────────────────┤
│     Core Kernel                     │
│  (MMU, Memory, Threading, Boot)     │
├─────────────────────────────────────┤
│     Hardware Abstraction Layer      │
│  (x64, aarch64)                     │
├─────────────────────────────────────┤
│     Drivers                         │
│  (VirtIO, AHCI, Network, etc.)      │
└─────────────────────────────────────┘
```

### Key Architectural Decisions

1. **Single Address Space**: No user/kernel space separation
   - Eliminates system call overhead
   - Simplifies programming model
   - Requires careful memory management

2. **Single Application per VM**: Unikernel model
   - Tightly couples OS with application
   - Optimizes for single workload
   - Improves security through isolation

3. **Linux ABI Compatibility**: Binary compatibility layer
   - Runs unmodified Linux binaries
   - Implements Linux system calls
   - Maps to OSv internal APIs

## Design Patterns

### 1. RAII (Resource Acquisition Is Initialization)

Used extensively throughout OSv for automatic resource management:

```cpp
// Example from OSv codebase
class file_handle {
    file* fp;
public:
    file_handle(const char* path, int flags) {
        fp = fopen(path, flags);
        if (!fp) throw errno_error(errno);
    }
    ~file_handle() {
        if (fp) fclose(fp);
    }
    file* get() { return fp; }
};

// Usage - automatically closes file
void read_config() {
    file_handle f("/etc/config", O_RDONLY);
    // Use f.get()
    // File automatically closed when function exits
}
```

**Benefits**:
- Automatic cleanup prevents resource leaks
- Exception-safe
- Clear ownership semantics

### 2. Smart Pointers (Unique and Shared Ownership)

OSv uses modern C++ smart pointers for memory management:

```cpp
// Unique ownership
std::unique_ptr<device> dev = std::make_unique<virtio_net_device>();

// Shared ownership (when multiple owners needed)
std::shared_ptr<vnode> root = std::make_shared<vnode>();
```

**Guidelines**:
- Prefer `unique_ptr` by default
- Use `shared_ptr` only when truly needed
- Avoid raw pointers for ownership
- Use raw pointers only for non-owning references

### 3. Factory Pattern

Used for creating polymorphic objects:

```cpp
// Driver factory
class driver_factory {
public:
    static std::unique_ptr<driver> create(const pci_device& dev) {
        switch (dev.device_id()) {
        case VIRTIO_NET_ID:
            return std::make_unique<virtio_net_driver>(dev);
        case VIRTIO_BLK_ID:
            return std::make_unique<virtio_blk_driver>(dev);
        default:
            return nullptr;
        }
    }
};
```

### 4. Strategy Pattern

Used for pluggable algorithms:

```cpp
// Scheduling policy is a strategy
class scheduler {
    std::unique_ptr<scheduling_policy> policy;
public:
    void set_policy(std::unique_ptr<scheduling_policy> p) {
        policy = std::move(p);
    }
    thread* pick_next() {
        return policy->select_thread();
    }
};
```

### 5. Observer Pattern

Used for event notification:

```cpp
// Network event observers
class network_observer {
public:
    virtual void on_packet_received(packet* pkt) = 0;
    virtual void on_link_status_change(bool up) = 0;
};

class network_interface {
    std::vector<network_observer*> observers;
public:
    void notify_packet(packet* pkt) {
        for (auto* obs : observers) {
            obs->on_packet_received(pkt);
        }
    }
};
```

### 6. Singleton Pattern (with Caution)

Used sparingly for global state that must be unique:

```cpp
// CPU-local singleton (per-CPU instance)
class per_cpu_data {
    static __thread per_cpu_data* instance;
public:
    static per_cpu_data& get() {
        if (!instance) {
            instance = new per_cpu_data();
        }
        return *instance;
    }
};
```

**Caution**: 
- Avoid global singletons when possible
- Prefer dependency injection
- Use per-CPU singletons to avoid lock contention

### 7. Template Method Pattern

Used for defining algorithm skeletons:

```cpp
// File system base class
class filesystem {
public:
    int mount(device* dev, int flags) {
        // Common mounting logic
        if (auto err = validate_device(dev)) return err;
        if (auto err = do_mount(dev, flags)) return err;
        return post_mount();
    }
protected:
    virtual int do_mount(device* dev, int flags) = 0;
    virtual int post_mount() { return 0; }
private:
    int validate_device(device* dev);
};
```

### 8. Adapter Pattern

Used for interfacing with different systems:

```cpp
// Linux syscall adapter
namespace linux {
    long sys_read(int fd, void* buf, size_t count) {
        // Adapt to OSv internal API
        auto fp = get_file(fd);
        return osv::read(fp, buf, count);
    }
}
```

### 9. Pimpl (Pointer to Implementation)

Used to hide implementation details:

```cpp
// Header file
class network_stack {
    struct impl;
    std::unique_ptr<impl> pimpl;
public:
    network_stack();
    ~network_stack();
    void send_packet(packet* p);
};

// Implementation file
struct network_stack::impl {
    tcp_stack tcp;
    udp_stack udp;
    routing_table routes;
};
```

### 10. Lock-Free Patterns

Used in performance-critical paths:

```cpp
// Lock-free ring buffer
template<typename T, size_t Size>
class spsc_queue {
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
    std::array<T, Size> buffer;
public:
    bool push(const T& item) {
        auto current_tail = tail.load(std::memory_order_relaxed);
        auto next_tail = (current_tail + 1) % Size;
        if (next_tail == head.load(std::memory_order_acquire)) {
            return false; // Full
        }
        buffer[current_tail] = item;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }
};
```

## Separation of Concerns

### Architectural Layers

#### 1. Hardware Abstraction Layer (HAL)

**Location**: `arch/x64/`, `arch/aarch64/`

**Responsibilities**:
- CPU-specific operations
- Boot sequence
- Interrupt handling
- Page table management
- Atomic operations

**Interface Example**:
```cpp
namespace arch {
    void enable_interrupts();
    void disable_interrupts();
    u64 read_msr(u32 msr);
    void write_msr(u32 msr, u64 value);
}
```

#### 2. Core Kernel Layer

**Location**: `core/`

**Responsibilities**:
- Memory management (MMU, page allocator)
- Thread scheduling
- Synchronization primitives
- Timer management
- Boot and initialization

**Clear Boundaries**:
- No direct hardware access (uses HAL)
- Platform-independent
- Provides services to upper layers

#### 3. System Services Layer

**Location**: `fs/`, `libc/`, `bsd/`

**Responsibilities**:
- File system abstraction (VFS)
- Network stack (TCP/IP)
- Device management
- System calls

**Isolation**:
- Each subsystem is independent
- Well-defined interfaces
- Minimal coupling

#### 4. Driver Layer

**Location**: `drivers/`

**Responsibilities**:
- Device-specific code
- Hardware interaction
- DMA management
- Interrupt handling

**Pattern**:
```cpp
// Generic driver interface
class driver {
public:
    virtual int probe() = 0;
    virtual int init() = 0;
    virtual int shutdown() = 0;
};

// Specific driver implementation
class virtio_net : public driver {
    // Implements interface for VirtIO network device
};
```

### Module Independence

Each major subsystem is designed to be independently testable and maintainable:

```
┌──────────────┐     ┌──────────────┐
│  Scheduler   │────▶│   Threading  │
└──────────────┘     └──────────────┘
        │                    │
        │                    │
        ▼                    ▼
┌──────────────┐     ┌──────────────┐
│  CPU Manager │     │    Memory    │
└──────────────┘     └──────────────┘
```

**Key Principles**:
- Low coupling between modules
- High cohesion within modules
- Well-defined interfaces
- Dependency injection where possible

## Dependency Management

### Internal Dependencies

#### Module Dependency Graph

OSv uses a module system to manage dependencies:

```makefile
# Module manifest (module.py)
provides = ['myapp']
requires = ['libc', 'libstdc++', 'networking']
```

**Build System**:
- Topologically sorts modules
- Ensures dependencies are built first
- Links modules in correct order

#### Header Organization

```
include/
├── osv/          # OSv-specific interfaces
├── api/          # Stable external API
├── internal/     # Internal implementation details
└── arch/         # Architecture-specific
```

**Guidelines**:
- Public API in `api/` - stable interface
- Internal headers in `internal/` - can change
- Use forward declarations to reduce coupling
- Include guards or `#pragma once`

### External Dependencies

#### Third-Party Components

OSv includes several external components:

1. **MUSL libc**: Standard C library
   - Location: `musl_1.1.24/`
   - Version: 1.1.24
   - Modifications: Minimal, mostly build system

2. **BSD Networking Stack**: TCP/IP implementation
   - Location: `bsd/`
   - Origin: FreeBSD
   - Modifications: Adapted for OSv threading model

3. **ZFS**: File system
   - Location: `fs/zfs/`
   - Origin: FreeBSD
   - Modifications: Memory allocator integration

#### Dependency Principles

1. **Minimize Dependencies**:
   - Only include what's necessary
   - Evaluate alternatives
   - Consider maintenance burden

2. **Version Pinning**:
   - Document versions explicitly
   - Test before upgrading
   - Track security advisories

3. **Isolation**:
   - Keep third-party code separate
   - Document modifications
   - Maintain upgrade path

4. **Abstraction**:
   ```cpp
   // Don't expose third-party types in interfaces
   // BAD:
   boost::shared_ptr<T> get_resource();
   
   // GOOD:
   std::shared_ptr<T> get_resource();
   ```

### Circular Dependency Prevention

**Techniques Used**:

1. **Forward Declarations**:
   ```cpp
   // header.h
   class File;  // Forward declaration
   
   class FileManager {
       File* open(const char* path);
   };
   ```

2. **Interface Segregation**:
   ```cpp
   // Split large interfaces
   class Readable {
   public:
       virtual ssize_t read(void* buf, size_t len) = 0;
   };
   
   class Writable {
   public:
       virtual ssize_t write(const void* buf, size_t len) = 0;
   };
   ```

3. **Dependency Injection**:
   ```cpp
   class Service {
       Logger* logger;
   public:
       Service(Logger* log) : logger(log) {}
   };
   ```

## Module System

### Module Structure

Each OSv module consists of:

```
module/
├── module.py      # Module metadata
├── Makefile       # Build rules
├── src/          # Source files
├── include/      # Public headers
└── tests/        # Module tests
```

### Module Manifest (module.py)

```python
from osv.modules import api

# Module metadata
api.require('libc')
api.require('libstdc++')

# Build configuration
default = api.run('/myapp')
```

### Module Loading

OSv loader resolves dependencies and loads modules:

```cpp
// Simplified loader logic
void load_modules() {
    auto sorted = topological_sort(requested_modules);
    for (auto& mod : sorted) {
        load_module(mod);
        run_module_init(mod);
    }
}
```

### Dynamic Linking

OSv supports ELF dynamic linking:
- Resolves symbols at load time
- Supports shared libraries
- Implements lazy binding for performance

## Interface Design

### API Design Principles

1. **Consistency**:
   ```cpp
   // Consistent naming and parameter order
   int open(const char* path, int flags, mode_t mode);
   int mkdir(const char* path, mode_t mode);
   int stat(const char* path, struct stat* buf);
   ```

2. **Error Handling**:
   ```cpp
   // Return error codes, set errno
   int syscall() {
       if (error_condition) {
           errno = EINVAL;
           return -1;
       }
       return 0;
   }
   ```

3. **Resource Management**:
   ```cpp
   // Acquire in constructor, release in destructor
   class ScopedLock {
       mutex& m;
   public:
       ScopedLock(mutex& m) : m(m) { m.lock(); }
       ~ScopedLock() { m.unlock(); }
   };
   ```

4. **Type Safety**:
   ```cpp
   // Use strong types instead of primitives
   class FileDescriptor {
       int fd;
   public:
       explicit FileDescriptor(int fd) : fd(fd) {}
       int get() const { return fd; }
   };
   
   // Prevents accidental misuse
   void write_to_file(FileDescriptor fd, const void* data);
   // write_to_file(42, data);  // Compile error
   ```

### Linux Compatibility Layer

OSv provides Linux system call compatibility:

```cpp
// Linux syscall interface
namespace linux {
    long sys_open(const char* path, int flags, int mode);
    long sys_read(int fd, void* buf, size_t count);
    long sys_write(int fd, const void* buf, size_t count);
}

// Internal OSv implementation
namespace osv {
    file* file_open(const char* path, int flags, mode_t mode);
    ssize_t file_read(file* fp, void* buf, size_t count);
    ssize_t file_write(file* fp, const void* buf, size_t count);
}
```

**Adapter Pattern**: Linux syscalls adapt to OSv internal APIs

## Error Handling

### Error Handling Strategies

#### 1. Error Codes (C Style)

Used in C code and Linux compatibility layer:

```cpp
int open_file(const char* path, file** out) {
    if (!path) return EINVAL;
    if (!out) return EINVAL;
    
    auto fp = internal_open(path);
    if (!fp) return ENOENT;
    
    *out = fp;
    return 0;
}
```

#### 2. Exceptions (C++ Style)

Used in modern C++ code:

```cpp
file* open_file(const char* path) {
    if (!path) {
        throw std::invalid_argument("path is null");
    }
    
    auto fp = internal_open(path);
    if (!fp) {
        throw errno_exception(ENOENT);
    }
    
    return fp;
}
```

#### 3. Optional Types

Used for operations that may not produce a value:

```cpp
std::optional<Config> load_config(const char* path) {
    auto fp = fopen(path, "r");
    if (!fp) return std::nullopt;
    
    Config cfg = parse_config(fp);
    fclose(fp);
    return cfg;
}

// Usage
if (auto cfg = load_config("/etc/config")) {
    apply_config(*cfg);
}
```

#### 4. Result Types

Used for operations that return value or error:

```cpp
template<typename T, typename E>
class Result {
    std::variant<T, E> data;
public:
    bool is_ok() const;
    bool is_err() const;
    T& value();
    E& error();
};

Result<File*, int> open_file(const char* path) {
    auto fp = internal_open(path);
    if (!fp) return Result<File*, int>::error(errno);
    return Result<File*, int>::ok(fp);
}
```

### Error Handling Guidelines

1. **Consistent Error Reporting**:
   - Choose one style per module
   - Document error handling strategy
   - Use RAII for cleanup

2. **Error Propagation**:
   ```cpp
   // Propagate errors up the call stack
   int high_level_operation() {
       int err = low_level_operation();
       if (err) return err;
       return 0;
   }
   ```

3. **Error Context**:
   ```cpp
   // Add context to errors
   class error_with_context : public std::exception {
       std::string message;
       std::string context;
   public:
       error_with_context(const std::string& msg, const std::string& ctx)
           : message(msg), context(ctx) {}
       const char* what() const noexcept override {
           static std::string full = message + " (in: " + context + ")";
           return full.c_str();
       }
   };
   ```

## Best Practices Summary

### Design Checklist

When designing a new component:

- [ ] Clear responsibility and scope
- [ ] Minimal dependencies
- [ ] Well-defined interface
- [ ] Appropriate design patterns used
- [ ] Error handling strategy documented
- [ ] Thread safety considered
- [ ] Resource management (RAII)
- [ ] Testability
- [ ] Performance considerations
- [ ] Documentation

### Code Review Checklist

When reviewing code:

- [ ] Appropriate layer for functionality
- [ ] No circular dependencies
- [ ] Consistent with existing patterns
- [ ] Proper separation of concerns
- [ ] Clean interfaces
- [ ] Error handling comprehensive
- [ ] No resource leaks
- [ ] Thread-safe if needed

## Additional Resources

### OSv Documentation
- [Components of OSv](https://github.com/cloudius-systems/osv/wiki/Components-of-OSv)
- [OSv Architecture](https://github.com/cloudius-systems/osv/wiki)
- [Module System](https://github.com/cloudius-systems/osv/wiki/Modules)

### Design Pattern References
- "Design Patterns" by Gang of Four
- "Modern C++ Design" by Andrei Alexandrescu
- "Effective C++" by Scott Meyers
- "C++ Concurrency in Action" by Anthony Williams

### Architecture Resources
- [Unikernel Design](https://en.wikipedia.org/wiki/Unikernel)
- [Clean Architecture](https://blog.cleancoder.com/uncle-bob/2012/08/13/the-clean-architecture.html)
- [SOLID Principles](https://en.wikipedia.org/wiki/SOLID)

Last Updated: 2025-12-10
