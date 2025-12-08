# Custom Stack Integration Guide

## Table of Contents

1. [Introduction](#introduction)
2. [Custom Networking Stack](#custom-networking-stack)
3. [Custom ELF Loader](#custom-elf-loader)
4. [Custom Filesystem Implementation](#custom-filesystem-implementation)
5. [Custom Device Drivers](#custom-device-drivers)
6. [Custom Memory Allocator](#custom-memory-allocator)
7. [Integration Patterns](#integration-patterns)
8. [Testing and Validation](#testing-and-validation)

---

## 1. Introduction

OSv's modular architecture allows replacing or extending core components with custom implementations. This guide covers integrating custom:

- **Networking stacks** (alternatives to BSD stack)
- **ELF loaders** (custom dynamic linking strategies)
- **Filesystems** (new filesystem types)
- **Device drivers** (new hardware support)
- **Memory allocators** (specialized allocation strategies)

### Why Custom Stacks?

**Use Cases:**
- Research and experimentation
- Specialized performance requirements
- Proprietary protocols or standards
- Integration with existing codebases
- Hardware-specific optimizations

### Integration Approaches

1. **Module-based**: Package as loadable module
2. **Build-time**: Compile into kernel
3. **Hybrid**: Core in kernel, extensions as modules
4. **Runtime**: Dynamic loading and switching

---

## 2. Custom Networking Stack

### 2.1 Networking Architecture Overview

Current OSv networking stack:

```
┌─────────────────────────────────────┐
│   Socket API (POSIX)                │
├─────────────────────────────────────┤
│   BSD Socket Layer                  │
├─────────────────────────────────────┤
│   Protocol Stack (TCP/IP)           │
│   ├── TCP                           │
│   ├── UDP                           │
│   └── IP                            │
├─────────────────────────────────────┤
│   Network Interface Layer           │
├─────────────────────────────────────┤
│   Device Drivers                    │
│   ├── virtio-net                    │
│   ├── vmxnet3                       │
│   └── ena                           │
└─────────────────────────────────────┘
```

### 2.2 Integration Points

#### Option 1: Replace BSD Stack

**Location:** `bsd/sys/net/`, `bsd/sys/netinet/`

**Steps:**

1. **Create Custom Stack Module:**
```cpp
// modules/mynetstack/netstack.hh
#pragma once

namespace mynetstack {

class NetworkStack {
public:
    virtual int init() = 0;
    virtual int socket(int domain, int type, int protocol) = 0;
    virtual int bind(int fd, const struct sockaddr* addr, socklen_t len) = 0;
    virtual int listen(int fd, int backlog) = 0;
    virtual int accept(int fd, struct sockaddr* addr, socklen_t* len) = 0;
    virtual int connect(int fd, const struct sockaddr* addr, socklen_t len) = 0;
    virtual ssize_t send(int fd, const void* buf, size_t len, int flags) = 0;
    virtual ssize_t recv(int fd, void* buf, size_t len, int flags) = 0;
    // ... more socket operations
};

class MyStack : public NetworkStack {
    // Implementation
};

// Global instance
extern NetworkStack* g_netstack;

} // namespace mynetstack
```

2. **Implement Socket Operations:**
```cpp
// modules/mynetstack/socket.cc
#include "netstack.hh"
#include <osv/file.h>
#include <osv/poll.h>

namespace mynetstack {

struct socket_file : public special_file {
    socket_file(int domain, int type, int protocol);
    virtual int read(struct uio* uio, int flags) override;
    virtual int write(struct uio* uio, int flags) override;
    virtual int ioctl(u_long cmd, void* data) override;
    virtual int poll(int events) override;
    // ... more operations
private:
    int _domain, _type, _protocol;
    void* _impl;  // Your socket implementation
};

int MyStack::socket(int domain, int type, int protocol) {
    auto* sf = new socket_file(domain, type, protocol);
    return sf->fd;
}

int MyStack::bind(int fd, const struct sockaddr* addr, socklen_t len) {
    auto* fp = file_get(fd);
    if (!fp) return -EBADF;
    
    auto* sf = dynamic_cast<socket_file*>(fp);
    if (!sf) {
        file_put(fp);
        return -ENOTSOCK;
    }
    
    // Your bind implementation
    int ret = my_bind_impl(sf->_impl, addr, len);
    
    file_put(fp);
    return ret;
}

// Implement other operations...

} // namespace mynetstack
```

3. **Hook into System Calls:**
```cpp
// modules/mynetstack/syscalls.cc
#include <osv/fcntl.h>
#include <sys/socket.h>

extern "C" {

int socket(int domain, int type, int protocol) {
    return mynetstack::g_netstack->socket(domain, type, protocol);
}

int bind(int fd, const struct sockaddr* addr, socklen_t len) {
    return mynetstack::g_netstack->bind(fd, addr, len);
}

int connect(int fd, const struct sockaddr* addr, socklen_t len) {
    return mynetstack::g_netstack->connect(fd, addr, len);
}

// ... more syscalls

} // extern "C"
```

4. **Initialize Network Stack:**
```cpp
// modules/mynetstack/init.cc
#include <osv/initialize.hh>
#include "netstack.hh"

namespace mynetstack {

NetworkStack* g_netstack = nullptr;

static void init_networking() {
    g_netstack = new MyStack();
    g_netstack->init();
}

// Register initialization
OSV_INIT_PRIO(init_networking, netstack_init);

} // namespace mynetstack
```

5. **Module Configuration:**
```python
# modules/mynetstack/module.py
from osv.modules import api

# Dependencies
api.require('drivers')

# Build
default = api.run('/bin/make -C ${MODULE_DIR}')

# Export shared library
api.export_library('libmynetstack.so')
```

#### Option 2: Add as Alternative Stack

Keep BSD stack, add yours as option:

```cpp
// modules/mynetstack/netstack_select.cc
#include <osv/options.hh>

namespace netstack {

enum class StackType {
    BSD,
    CUSTOM
};

static StackType selected_stack = StackType::BSD;

osv::option<StackType> opt_stack_type(
    "netstack",
    &selected_stack,
    StackType::BSD,
    "Network stack type (bsd|custom)"
);

NetworkStack* get_network_stack() {
    switch (selected_stack) {
    case StackType::BSD:
        return &bsd_stack;
    case StackType::CUSTOM:
        return &custom_stack;
    }
}

} // namespace netstack
```

**Usage:**
```bash
./scripts/run.py -e '--netstack=custom /myapp'
```

### 2.3 Integration Examples

#### Example 1: DPDK Integration

```cpp
// modules/dpdk-net/dpdk_stack.cc
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

namespace dpdk {

class DPDKStack : public NetworkStack {
public:
    int init() override {
        // Initialize DPDK EAL
        char* argv[] = {
            "osv",
            "-c", "0x3",  // Core mask
            "-n", "4",     // Memory channels
            nullptr
        };
        int argc = 5;
        
        int ret = rte_eal_init(argc, argv);
        if (ret < 0) {
            return -1;
        }
        
        // Initialize ports
        uint16_t nb_ports = rte_eth_dev_count_avail();
        for (uint16_t port = 0; port < nb_ports; port++) {
            init_port(port);
        }
        
        // Start packet processing threads
        start_rx_threads();
        start_tx_threads();
        
        return 0;
    }
    
private:
    void init_port(uint16_t port);
    void start_rx_threads();
    void start_tx_threads();
    
    struct rte_mempool* mbuf_pool;
};

} // namespace dpdk
```

#### Example 2: mTCP Integration

```cpp
// modules/mtcp/mtcp_stack.cc
#include <mtcp_api.h>
#include <mtcp_epoll.h>

namespace mtcp {

class mTCPStack : public NetworkStack {
public:
    int init() override {
        // Initialize mTCP
        char* conf_file = "/etc/mtcp.conf";
        int ret = mtcp_init(conf_file);
        if (ret) {
            return -1;
        }
        
        // Create mTCP context per CPU
        for (int cpu = 0; cpu < num_cpus(); cpu++) {
            mctx_t mctx = mtcp_create_context(cpu);
            _contexts.push_back(mctx);
        }
        
        return 0;
    }
    
    int socket(int domain, int type, int protocol) override {
        int cpu = sched::current_cpu();
        mctx_t mctx = _contexts[cpu];
        return mtcp_socket(mctx, domain, type, protocol);
    }
    
    // Implement other operations using mTCP API
    
private:
    std::vector<mctx_t> _contexts;
};

} // namespace mtcp
```

### 2.4 Performance Considerations

**Packet Processing:**
```cpp
// Zero-copy packet handling
class PacketBuffer {
public:
    // Map directly to NIC memory
    void* get_buffer() { return _dma_addr; }
    
    // Avoid copies
    void forward_to_app(socket* sk) {
        sk->receive_queue.push(_dma_addr);
    }
    
private:
    void* _dma_addr;
};
```

**Lock-Free Queues:**
```cpp
// Use OSv's lock-free queue
#include <lockfree/ring.hh>

ring_spsc<packet*, 4096> rx_queue;  // Single producer, single consumer

// RX thread
void rx_thread() {
    while (running) {
        packet* pkt = receive_packet();
        rx_queue.push(pkt);
    }
}

// Processing thread
void process_thread() {
    while (running) {
        packet* pkt;
        if (rx_queue.pop(pkt)) {
            process_packet(pkt);
        }
    }
}
```

---

## 3. Custom ELF Loader

### 3.1 ELF Loader Architecture

Current OSv ELF loader:

```
┌─────────────────────────────────────┐
│   Application Entry                 │
├─────────────────────────────────────┤
│   Dynamic Linker (core/elf.cc)     │
│   ├── Load ELF segments            │
│   ├── Relocations                   │
│   ├── Symbol resolution             │
│   └── TLS setup                     │
├─────────────────────────────────────┤
│   Memory Management (core/mmu.cc)   │
└─────────────────────────────────────┘
```

### 3.2 Custom Loader Implementation

#### Use Case: JIT-Compiled Code Loader

```cpp
// modules/jit-loader/jit_loader.hh
#pragma once

#include <osv/elf.hh>

namespace jit {

class JITLoader {
public:
    // Load JIT-compiled code
    void* load_jit_code(const void* code, size_t size);
    
    // Setup symbol table for JIT code
    void register_symbol(const char* name, void* addr);
    
    // Resolve symbols from JIT code
    void* resolve_symbol(const char* name);
    
private:
    // Code cache
    struct code_region {
        void* base;
        size_t size;
        bool executable;
    };
    
    std::vector<code_region> _code_regions;
    std::unordered_map<std::string, void*> _symbols;
};

} // namespace jit
```

**Implementation:**
```cpp
// modules/jit-loader/jit_loader.cc
#include "jit_loader.hh"
#include <osv/mmu.hh>

namespace jit {

void* JITLoader::load_jit_code(const void* code, size_t size) {
    // Allocate executable memory
    size_t aligned_size = align_up(size, mmu::page_size);
    void* addr = mmu::alloc_page_aligned(aligned_size);
    
    // Copy code
    memcpy(addr, code, size);
    
    // Make executable
    mmu::protect(addr, aligned_size, 
                 mmu::perm_read | mmu::perm_write | mmu::perm_exec);
    
    // Flush instruction cache (ARM)
    #ifdef __aarch64__
    __builtin___clear_cache(addr, (char*)addr + size);
    #endif
    
    // Register region
    _code_regions.push_back({addr, aligned_size, true});
    
    return addr;
}

void JITLoader::register_symbol(const char* name, void* addr) {
    _symbols[name] = addr;
}

void* JITLoader::resolve_symbol(const char* name) {
    auto it = _symbols.find(name);
    if (it != _symbols.end()) {
        return it->second;
    }
    
    // Fall back to main symbol table
    return elf::lookup_symbol(name);
}

} // namespace jit
```

#### Use Case: Plugin System

```cpp
// modules/plugin-loader/plugin.hh
#pragma once

namespace plugin {

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual int init() = 0;
    virtual int fini() = 0;
    virtual const char* name() = 0;
};

class PluginLoader {
public:
    // Load plugin from ELF
    Plugin* load(const char* path);
    
    // Unload plugin
    void unload(Plugin* plugin);
    
    // Get loaded plugins
    std::vector<Plugin*> get_plugins();
    
private:
    struct plugin_info {
        Plugin* plugin;
        elf::object* elf_obj;
        std::vector<void*> allocations;
    };
    
    std::vector<plugin_info> _plugins;
};

} // namespace plugin
```

**Implementation:**
```cpp
// modules/plugin-loader/loader.cc
#include "plugin.hh"
#include <osv/elf.hh>

namespace plugin {

Plugin* PluginLoader::load(const char* path) {
    // Load ELF
    auto* obj = new elf::object(path);
    if (!obj->load()) {
        delete obj;
        return nullptr;
    }
    
    // Find plugin entry point
    void* entry = obj->lookup_symbol("plugin_create");
    if (!entry) {
        delete obj;
        return nullptr;
    }
    
    // Create plugin instance
    typedef Plugin* (*create_fn)();
    auto create = reinterpret_cast<create_fn>(entry);
    Plugin* plugin = create();
    
    if (!plugin) {
        delete obj;
        return nullptr;
    }
    
    // Initialize plugin
    if (plugin->init() != 0) {
        delete plugin;
        delete obj;
        return nullptr;
    }
    
    // Register plugin
    _plugins.push_back({plugin, obj, {}});
    
    return plugin;
}

void PluginLoader::unload(Plugin* plugin) {
    for (auto it = _plugins.begin(); it != _plugins.end(); ++it) {
        if (it->plugin == plugin) {
            // Finalize plugin
            plugin->fini();
            
            // Unload ELF
            delete it->elf_obj;
            
            // Free allocations
            for (void* ptr : it->allocations) {
                free(ptr);
            }
            
            // Remove from list
            _plugins.erase(it);
            break;
        }
    }
}

} // namespace plugin
```

### 3.3 Custom Symbol Resolution

```cpp
// modules/custom-loader/symbol_resolver.cc
#include <osv/elf.hh>

namespace custom {

class CustomSymbolResolver {
public:
    // Register symbol provider
    void register_provider(const char* name, 
                          std::function<void*()> provider) {
        _providers[name] = provider;
    }
    
    // Resolve symbol
    void* resolve(const char* name) {
        // Check custom providers first
        auto it = _providers.find(name);
        if (it != _providers.end()) {
            return it->second();
        }
        
        // Fall back to standard resolution
        return elf::lookup_symbol(name);
    }
    
private:
    std::unordered_map<std::string, std::function<void*()>> _providers;
};

// Global resolver
CustomSymbolResolver g_resolver;

// Hook into ELF loader
extern "C" void* osv_resolve_symbol(const char* name) {
    return g_resolver.resolve(name);
}

} // namespace custom
```

---

## 4. Custom Filesystem Implementation

### 4.1 Filesystem Interface

OSv VFS interface:

```cpp
// From fs/vfs/vfs.h

struct vfsops {
    int (*vfs_mount)(struct mount*, const char*, int, const void*);
    int (*vfs_unmount)(struct mount*, int);
    int (*vfs_sync)(struct mount*);
    int (*vfs_statfs)(struct mount*, struct statfs*);
    // ... more operations
};

struct vnops {
    int (*vop_open)(struct file*);
    int (*vop_close)(struct file*);
    int (*vop_read)(struct vnode*, struct file*, struct uio*, int);
    int (*vop_write)(struct vnode*, struct file*, struct uio*, int);
    int (*vop_ioctl)(struct vnode*, struct file*, u_long, void*);
    // ... more operations
};
```

### 4.2 Example: Simple In-Memory Filesystem

```cpp
// modules/memfs/memfs.hh
#pragma once

#include <fs/vfs/vfs.h>
#include <unordered_map>
#include <vector>

namespace memfs {

struct memfs_node {
    enum class type { FILE, DIRECTORY };
    
    type node_type;
    std::string name;
    std::vector<uint8_t> data;  // For files
    std::unordered_map<std::string, memfs_node*> children;  // For directories
    memfs_node* parent;
    
    // Metadata
    mode_t mode;
    uid_t uid;
    gid_t gid;
    timespec atime, mtime, ctime;
};

class MemFS {
public:
    MemFS();
    ~MemFS();
    
    // Mount operations
    int mount(const char* dev, int flags, const void* data);
    int unmount(int flags);
    
    // File operations
    int open(const char* path, int flags, mode_t mode);
    int read(int fd, void* buf, size_t count);
    int write(int fd, const void* buf, size_t count);
    int close(int fd);
    
    // Directory operations
    int mkdir(const char* path, mode_t mode);
    int rmdir(const char* path);
    int readdir(const char* path, std::vector<std::string>& entries);
    
private:
    memfs_node* _root;
    memfs_node* lookup_node(const char* path);
    memfs_node* create_node(const char* path, memfs_node::type type, mode_t mode);
};

} // namespace memfs
```

**Implementation:**
```cpp
// modules/memfs/memfs.cc
#include "memfs.hh"
#include <osv/mount.h>

namespace memfs {

// VFS operations
static int memfs_mount(struct mount* mp, const char* dev, 
                       int flags, const void* data) {
    auto* fs = new MemFS();
    int ret = fs->mount(dev, flags, data);
    if (ret == 0) {
        mp->m_data = fs;
    } else {
        delete fs;
    }
    return ret;
}

static int memfs_unmount(struct mount* mp, int flags) {
    auto* fs = static_cast<MemFS*>(mp->m_data);
    int ret = fs->unmount(flags);
    delete fs;
    return ret;
}

static int memfs_sync(struct mount* mp) {
    // In-memory, nothing to sync
    return 0;
}

static int memfs_statfs(struct mount* mp, struct statfs* buf) {
    // Fill filesystem statistics
    buf->f_type = 0x4D454D46;  // "MEMF"
    buf->f_bsize = 4096;
    buf->f_blocks = 0;  // No fixed size
    buf->f_bfree = 0;
    buf->f_bavail = 0;
    return 0;
}

static struct vfsops memfs_vfsops = {
    memfs_mount,
    memfs_unmount,
    memfs_sync,
    memfs_statfs,
    // ... initialize other ops
};

// Vnode operations
static int memfs_open(struct file* fp) {
    auto* node = static_cast<memfs_node*>(fp->f_dentry->d_vnode->v_data);
    // Handle open
    return 0;
}

static int memfs_close(struct file* fp) {
    // Handle close
    return 0;
}

static int memfs_read(struct vnode* vp, struct file* fp,
                      struct uio* uio, int ioflag) {
    auto* node = static_cast<memfs_node*>(vp->v_data);
    
    if (node->node_type != memfs_node::type::FILE) {
        return EISDIR;
    }
    
    // Calculate read bounds
    off_t offset = uio->uio_offset;
    size_t len = uio->uio_resid;
    
    if (offset >= node->data.size()) {
        return 0;  // EOF
    }
    
    if (offset + len > node->data.size()) {
        len = node->data.size() - offset;
    }
    
    // Copy data to user buffer
    int error = uiomove(node->data.data() + offset, len, uio);
    
    return error;
}

static int memfs_write(struct vnode* vp, struct file* fp,
                       struct uio* uio, int ioflag) {
    auto* node = static_cast<memfs_node*>(vp->v_data);
    
    if (node->node_type != memfs_node::type::FILE) {
        return EISDIR;
    }
    
    off_t offset = uio->uio_offset;
    size_t len = uio->uio_resid;
    
    // Expand file if necessary
    if (offset + len > node->data.size()) {
        node->data.resize(offset + len);
    }
    
    // Copy data from user buffer
    int error = uiomove(node->data.data() + offset, len, uio);
    
    // Update mtime
    clock_gettime(CLOCK_REALTIME, &node->mtime);
    
    return error;
}

static struct vnops memfs_vnops = {
    memfs_open,
    memfs_close,
    memfs_read,
    memfs_write,
    // ... initialize other ops
};

// Registration
extern "C" void memfs_init() {
    vfs_register_fs("memfs", &memfs_vfsops);
}

} // namespace memfs
```

### 4.3 Advanced Filesystem Features

#### Copy-on-Write

```cpp
// modules/cowfs/cow.cc
namespace cowfs {

struct cow_block {
    uint64_t block_id;
    uint64_t refcount;
    std::vector<uint8_t> data;
};

class COWFilesystem {
public:
    // Write creates new block, updates mapping
    int write(uint64_t inode, off_t offset, const void* buf, size_t len) {
        uint64_t block_id = offset / block_size;
        
        // Get current block
        auto* block = get_block(inode, block_id);
        
        if (block->refcount > 1) {
            // Copy-on-write: create new block
            auto* new_block = new cow_block();
            new_block->data = block->data;  // Copy data
            new_block->refcount = 1;
            
            // Decrease refcount on old block
            block->refcount--;
            
            // Update mapping
            update_mapping(inode, block_id, new_block);
            block = new_block;
        }
        
        // Write to block
        size_t block_offset = offset % block_size;
        memcpy(block->data.data() + block_offset, buf, len);
        
        return len;
    }
    
private:
    static constexpr size_t block_size = 4096;
    std::unordered_map<uint64_t, cow_block*> _blocks;
};

} // namespace cowfs
```

#### Caching Layer

```cpp
// modules/cachefs/cache.cc
namespace cachefs {

class CachedFilesystem {
public:
    int read(struct vnode* vp, off_t offset, void* buf, size_t len) {
        // Check cache
        auto* cached = _cache.get(vp->v_ino, offset, len);
        if (cached) {
            memcpy(buf, cached, len);
            return len;
        }
        
        // Read from backing store
        int ret = _backing_fs->read(vp, offset, buf, len);
        if (ret > 0) {
            // Cache result
            _cache.put(vp->v_ino, offset, buf, ret);
        }
        
        return ret;
    }
    
private:
    struct cache_entry {
        uint64_t ino;
        off_t offset;
        size_t len;
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    // LRU cache
    class Cache {
    public:
        void* get(uint64_t ino, off_t offset, size_t len);
        void put(uint64_t ino, off_t offset, const void* data, size_t len);
        void evict_lru();
        
    private:
        std::list<cache_entry> _entries;
        std::unordered_map<uint64_t, std::list<cache_entry>::iterator> _index;
        size_t _max_size = 64 * 1024 * 1024;  // 64 MB
        size_t _current_size = 0;
    };
    
    Cache _cache;
    FilesystemOps* _backing_fs;
};

} // namespace cachefs
```

---

## 5. Custom Device Drivers

### 5.1 Driver Framework

```cpp
// drivers/mydriver.hh
#pragma once

#include <osv/device.h>
#include <osv/driver.h>
#include <osv/interrupt.hh>

namespace mydriver {

class MyDevice : public device {
public:
    explicit MyDevice(struct device* bus_dev);
    virtual ~MyDevice();
    
    // Device operations
    virtual int probe() override;
    virtual int init() override;
    virtual int open() override;
    virtual int close() override;
    virtual int read(void* buf, size_t len) override;
    virtual int write(const void* buf, size_t len) override;
    virtual int ioctl(u_long cmd, void* arg) override;
    
    // Interrupt handler
    void handle_interrupt();
    
private:
    // Hardware registers
    volatile uint32_t* _regs;
    
    // DMA buffers
    void* _dma_buffer;
    phys_addr_t _dma_phys;
    
    // Interrupt
    interrupt_manager* _irq;
    
    // Device state
    enum class state { IDLE, BUSY, ERROR };
    state _state;
    
    // Hardware interaction
    void write_reg(unsigned offset, uint32_t value);
    uint32_t read_reg(unsigned offset);
    void reset_device();
};

} // namespace mydriver
```

**Implementation:**
```cpp
// drivers/mydriver.cc
#include "mydriver.hh"
#include <osv/mmu.hh>
#include <osv/pci.hh>

namespace mydriver {

MyDevice::MyDevice(struct device* bus_dev)
    : device(bus_dev)
    , _regs(nullptr)
    , _dma_buffer(nullptr)
    , _state(state::IDLE)
{
}

int MyDevice::probe() {
    // Check if device is present
    // For PCI devices:
    auto* pci_dev = static_cast<pci::device*>(_bus_dev);
    
    if (pci_dev->get_vendor_id() != MY_VENDOR_ID ||
        pci_dev->get_device_id() != MY_DEVICE_ID) {
        return -ENODEV;
    }
    
    return 0;
}

int MyDevice::init() {
    // Map device registers
    auto* pci_dev = static_cast<pci::device*>(_bus_dev);
    auto bar = pci_dev->get_bar(0);
    
    _regs = static_cast<volatile uint32_t*>(
        mmu::map_phys(bar.addr, bar.size, mmu::perm_read | mmu::perm_write)
    );
    
    // Allocate DMA buffer
    _dma_buffer = memory::alloc_phys_contiguous_aligned(
        PAGE_SIZE, PAGE_SIZE, &_dma_phys
    );
    
    // Setup interrupt
    _irq = new interrupt_manager([this] {
        this->handle_interrupt();
    });
    
    int vector = pci_dev->setup_irq(0);
    _irq->setup(vector);
    
    // Reset and initialize device
    reset_device();
    
    // Enable device
    write_reg(REG_CONTROL, CONTROL_ENABLE);
    
    return 0;
}

void MyDevice::handle_interrupt() {
    // Read interrupt status
    uint32_t status = read_reg(REG_INT_STATUS);
    
    // Handle different interrupt types
    if (status & INT_RX_COMPLETE) {
        // RX complete
        handle_rx_complete();
    }
    
    if (status & INT_TX_COMPLETE) {
        // TX complete
        handle_tx_complete();
    }
    
    if (status & INT_ERROR) {
        // Error handling
        handle_error();
    }
    
    // Clear interrupt
    write_reg(REG_INT_STATUS, status);
}

int MyDevice::read(void* buf, size_t len) {
    // Wait for device ready
    while (_state != state::IDLE) {
        sched::thread::yield();
    }
    
    _state = state::BUSY;
    
    // Setup DMA read
    write_reg(REG_DMA_ADDR, _dma_phys);
    write_reg(REG_DMA_LEN, len);
    write_reg(REG_COMMAND, CMD_DMA_READ);
    
    // Wait for completion
    while (_state == state::BUSY) {
        sched::thread::wait();
    }
    
    if (_state == state::ERROR) {
        _state = state::IDLE;
        return -EIO;
    }
    
    // Copy from DMA buffer
    memcpy(buf, _dma_buffer, len);
    
    _state = state::IDLE;
    return len;
}

// Driver registration
static struct driver mydriver = {
    .name = "mydriver",
    .devclass = device_class::UNKNOWN,
    .ops = nullptr,
};

static void mydriver_init(void* arg) {
    // Register driver
    driver_register(&mydriver);
}

OSV_DRIVER_INIT(mydriver_init, mydriver);

} // namespace mydriver
```

### 5.2 VirtIO Driver Example

```cpp
// drivers/virtio-mydevice.cc
#include <osv/virtio.hh>

namespace virtio {

class MyVirtIODevice : public virtio_driver {
public:
    explicit MyVirtIODevice(virtio_device& dev);
    
    virtual void probe() override;
    virtual void init() override;
    
    // Queue handling
    void fill_rx_queue();
    void handle_rx();
    void handle_tx();
    
private:
    // VirtIO queues
    std::unique_ptr<vring> _rx_queue;
    std::unique_ptr<vring> _tx_queue;
    
    // Buffers
    std::vector<void*> _rx_buffers;
};

MyVirtIODevice::MyVirtIODevice(virtio_device& dev)
    : virtio_driver(dev)
{
}

void MyVirtIODevice::init() {
    // Setup VirtIO device
    _dev.reset();
    _dev.set_status(VIRTIO_CONFIG_S_ACKNOWLEDGE);
    _dev.set_status(VIRTIO_CONFIG_S_DRIVER);
    
    // Negotiate features
    uint64_t features = _dev.get_features();
    // Select features you support
    _dev.set_features(features);
    
    // Setup queues
    _rx_queue = std::make_unique<vring>(&_dev, 0, 256);
    _tx_queue = std::make_unique<vring>(&_dev, 1, 256);
    
    // Fill RX queue with buffers
    fill_rx_queue();
    
    // Setup interrupt handlers
    _rx_queue->set_callback([this] { this->handle_rx(); });
    _tx_queue->set_callback([this] { this->handle_tx(); });
    
    // Mark device ready
    _dev.set_status(VIRTIO_CONFIG_S_DRIVER_OK);
}

void MyVirtIODevice::fill_rx_queue() {
    for (int i = 0; i < 256; i++) {
        void* buf = memory::alloc_page();
        _rx_buffers.push_back(buf);
        
        // Add buffer to RX queue
        _rx_queue->add_buf(buf, PAGE_SIZE, false);
    }
    
    _rx_queue->kick();
}

void MyVirtIODevice::handle_rx() {
    void* buf;
    uint32_t len;
    
    while (_rx_queue->get_buf(&buf, &len)) {
        // Process received data
        process_rx_data(buf, len);
        
        // Return buffer to queue
        _rx_queue->add_buf(buf, PAGE_SIZE, false);
    }
    
    _rx_queue->kick();
}

} // namespace virtio
```

---

## 6. Custom Memory Allocator

### 6.1 Allocator Interface

```cpp
// modules/myalloc/allocator.hh
#pragma once

#include <cstddef>

namespace myalloc {

class Allocator {
public:
    virtual ~Allocator() = default;
    
    virtual void* malloc(size_t size) = 0;
    virtual void* calloc(size_t nmemb, size_t size) = 0;
    virtual void* realloc(void* ptr, size_t size) = 0;
    virtual void free(void* ptr) = 0;
    
    virtual size_t malloc_usable_size(void* ptr) = 0;
    
    // Optional: aligned allocation
    virtual void* memalign(size_t alignment, size_t size);
    
    // Statistics
    virtual size_t get_allocated() = 0;
    virtual size_t get_freed() = 0;
};

// Global allocator
extern Allocator* g_allocator;

} // namespace myalloc
```

### 6.2 Example: Slab Allocator

```cpp
// modules/slab-alloc/slab.cc
#include "allocator.hh"
#include <vector>
#include <osv/mmu.hh>

namespace myalloc {

class SlabAllocator : public Allocator {
public:
    SlabAllocator();
    
    void* malloc(size_t size) override;
    void free(void* ptr) override;
    
private:
    struct slab {
        void* base;
        size_t obj_size;
        size_t obj_count;
        std::vector<bool> allocated;
        
        void* alloc() {
            for (size_t i = 0; i < obj_count; i++) {
                if (!allocated[i]) {
                    allocated[i] = true;
                    return (char*)base + i * obj_size;
                }
            }
            return nullptr;
        }
        
        void free(void* ptr) {
            size_t idx = ((char*)ptr - (char*)base) / obj_size;
            allocated[idx] = false;
        }
    };
    
    // Slabs for common sizes
    std::vector<slab> _slabs;
    
    // Size classes: 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
    static constexpr size_t SIZE_CLASSES[] = {
        16, 32, 64, 128, 256, 512, 1024, 2048, 4096
    };
    
    slab* get_slab_for_size(size_t size);
    slab* create_slab(size_t obj_size);
};

SlabAllocator::SlabAllocator() {
    // Create slabs for each size class
    for (size_t size : SIZE_CLASSES) {
        _slabs.push_back(*create_slab(size));
    }
}

slab* SlabAllocator::create_slab(size_t obj_size) {
    size_t page_size = mmu::page_size;
    void* base = mmu::alloc_page_aligned(page_size);
    
    slab* s = new slab{
        .base = base,
        .obj_size = obj_size,
        .obj_count = page_size / obj_size,
        .allocated = std::vector<bool>(page_size / obj_size, false)
    };
    
    return s;
}

void* SlabAllocator::malloc(size_t size) {
    // Find appropriate slab
    slab* s = get_slab_for_size(size);
    if (!s) {
        // Fall back to page allocator for large sizes
        return mmu::alloc_page_aligned(align_up(size, mmu::page_size));
    }
    
    // Try to allocate from slab
    void* ptr = s->alloc();
    if (ptr) {
        return ptr;
    }
    
    // Slab full, create new one
    slab* new_slab = create_slab(s->obj_size);
    _slabs.push_back(*new_slab);
    
    return new_slab->alloc();
}

void SlabAllocator::free(void* ptr) {
    // Find which slab this belongs to
    for (auto& s : _slabs) {
        if (ptr >= s.base && 
            ptr < (char*)s.base + s.obj_size * s.obj_count) {
            s.free(ptr);
            return;
        }
    }
    
    // Not in any slab, must be page-allocated
    mmu::free_page(ptr);
}

} // namespace myalloc
```

### 6.3 Hooking Custom Allocator

```cpp
// modules/myalloc/hooks.cc
#include "allocator.hh"

namespace myalloc {

Allocator* g_allocator = nullptr;

// Override standard allocation functions
extern "C" {

void* malloc(size_t size) {
    if (g_allocator) {
        return g_allocator->malloc(size);
    }
    // Fall back to OSv allocator
    return memory::alloc(size);
}

void free(void* ptr) {
    if (g_allocator) {
        g_allocator->free(ptr);
    } else {
        memory::free(ptr);
    }
}

void* calloc(size_t nmemb, size_t size) {
    if (g_allocator) {
        return g_allocator->calloc(nmemb, size);
    }
    void* ptr = memory::alloc(nmemb * size);
    if (ptr) {
        memset(ptr, 0, nmemb * size);
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (g_allocator) {
        return g_allocator->realloc(ptr, size);
    }
    return memory::realloc(ptr, size);
}

} // extern "C"

// Initialize custom allocator
static void init_allocator() {
    g_allocator = new SlabAllocator();
}

OSV_INIT_PRIO(init_allocator, memory_init);

} // namespace myalloc
```

---

## 7. Integration Patterns

### 7.1 Module-Based Integration

**Advantages:**
- Clean separation
- Easy to enable/disable
- Maintainable

**Pattern:**
```
modules/mycustomstack/
├── module.py          # Module definition
├── Makefile          # Build rules
├── include/
│   └── mystack.hh    # Public API
├── src/
│   ├── init.cc       # Initialization
│   ├── stack.cc      # Implementation
│   └── syscalls.cc   # System call hooks
└── usr.manifest      # Files to include
```

### 7.2 Build-Time Integration

**Advantages:**
- No runtime overhead
- Smaller kernel
- Compile-time optimization

**Pattern:**
```makefile
# In Makefile or conf/base.mk

ifeq ($(use_custom_stack),1)
    CXXFLAGS += -DUSE_CUSTOM_STACK
    src += $(src)/custom-stack/stack.cc
    src += $(src)/custom-stack/syscalls.cc
endif
```

### 7.3 Runtime Selection

**Advantages:**
- Flexibility
- A/B testing
- Easy fallback

**Pattern:**
```cpp
// In core/options.cc
namespace osv {

enum class StackType { BSD, CUSTOM };

static StackType stack_type = StackType::BSD;

option<StackType> opt_stack(
    "stack",
    &stack_type,
    StackType::BSD,
    "Network stack type"
);

NetworkStack* get_stack() {
    switch (stack_type) {
    case StackType::BSD:
        return &bsd_stack;
    case StackType::CUSTOM:
        return &custom_stack;
    }
}

} // namespace osv
```

---

## 8. Testing and Validation

### 8.1 Unit Testing

```cpp
// tests/tst-mystack.cc
#include <osv/mystack.hh>

void test_socket_creation() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    close(fd);
}

void test_bind_connect() {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    assert(bind(server, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    assert(listen(server, 10) == 0);
    
    // More tests...
    
    close(server);
}

int main() {
    test_socket_creation();
    test_bind_connect();
    
    printf("All tests passed!\n");
    return 0;
}
```

### 8.2 Integration Testing

```bash
#!/bin/bash
# tests/test-custom-stack.sh

# Build with custom stack
./scripts/build image=mystack-tests fs=rofs conf_hide_symbols=1

# Run tests
./scripts/run.py -e '/tests/tst-mystack'

# Check result
if [ $? -eq 0 ]; then
    echo "Tests passed"
else
    echo "Tests failed"
    exit 1
fi
```

### 8.3 Performance Benchmarking

```cpp
// benchmarks/bench-mystack.cc
#include <chrono>
#include <osv/mystack.hh>

void benchmark_throughput() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Run benchmark
    for (int i = 0; i < 1000000; i++) {
        // Send/receive data
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start
    );
    
    double throughput = 1000000.0 / (duration.count() / 1000000.0);
    printf("Throughput: %.2f ops/sec\n", throughput);
}

void benchmark_latency() {
    std::vector<double> latencies;
    
    for (int i = 0; i < 10000; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Single operation
        
        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start
        ).count();
        
        latencies.push_back(latency);
    }
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    
    printf("Min latency: %.2f ns\n", latencies.front());
    printf("Median latency: %.2f ns\n", latencies[latencies.size()/2]);
    printf("P99 latency: %.2f ns\n", latencies[latencies.size()*99/100]);
    printf("Max latency: %.2f ns\n", latencies.back());
}

int main() {
    benchmark_throughput();
    benchmark_latency();
    return 0;
}
```

---

## 9. Best Practices

### 9.1 Design Guidelines

1. **Follow OSv conventions**: Use existing patterns and idioms
2. **Minimize kernel changes**: Implement as modules when possible
3. **Thread-safe**: Use proper synchronization
4. **Error handling**: Return proper error codes
5. **Memory management**: Clean up resources properly
6. **Performance**: Profile and optimize hot paths
7. **Documentation**: Document APIs and usage

### 9.2 Common Pitfalls

**Memory Leaks:**
```cpp
// Bad
void* allocate() {
    return malloc(1024);  // Never freed
}

// Good
class RAII_Buffer {
    void* _buf;
public:
    RAII_Buffer(size_t size) : _buf(malloc(size)) {}
    ~RAII_Buffer() { free(_buf); }
    void* get() { return _buf; }
};
```

**Race Conditions:**
```cpp
// Bad
int global_counter = 0;

void increment() {
    global_counter++;  // Not atomic
}

// Good
#include <atomic>
std::atomic<int> global_counter{0};

void increment() {
    global_counter.fetch_add(1, std::memory_order_relaxed);
}
```

**Deadlocks:**
```cpp
// Bad
mutex_t lock1, lock2;

void thread1() {
    mutex_lock(&lock1);
    mutex_lock(&lock2);  // Can deadlock with thread2
}

void thread2() {
    mutex_lock(&lock2);
    mutex_lock(&lock1);
}

// Good
void thread1() {
    // Always lock in same order
    mutex_lock(&lock1);
    mutex_lock(&lock2);
}

void thread2() {
    mutex_lock(&lock1);  // Same order
    mutex_lock(&lock2);
}
```

---

## 10. Appendix: Reference Implementations

### A. Minimal Network Stack Skeleton

See `examples/minimal-netstack/` for a complete minimal implementation.

### B. Filesystem Template

See `examples/template-fs/` for a filesystem template.

### C. Driver Template

See `examples/template-driver/` for a driver template.

### D. Build Configurations

See `conf/profiles/` for example custom profiles.

---

## 11. Resources

### Documentation
- [OSv Source Code](https://github.com/cloudius-systems/osv)
- [OSv Wiki](https://github.com/cloudius-systems/osv/wiki)
- [VFS Documentation](https://github.com/cloudius-systems/osv/wiki/VFS)

### Community
- [OSv Mailing List](https://groups.google.com/forum/#!forum/osv-dev)
- [GitHub Issues](https://github.com/cloudius-systems/osv/issues)

### Example Projects
- [DPDK Integration](https://github.com/cloudius-systems/osv/tree/master/modules/dpdk)
- [NFS Client](https://github.com/cloudius-systems/osv/tree/master/modules/nfs)
- [VirtioFS](https://github.com/cloudius-systems/osv/tree/master/modules/virtiofs)
