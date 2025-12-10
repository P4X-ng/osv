/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef GDB_STUB_HH
#define GDB_STUB_HH

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <osv/sched.hh>
#include <osv/mutex.h>

namespace gdb_stub {

// Forward declarations
class transport;
class protocol_handler;

// GDB packet structure
struct gdb_packet {
    std::string data;
    bool valid;
    uint8_t checksum;
    
    gdb_packet() : valid(false), checksum(0) {}
    gdb_packet(const std::string& d) : data(d), valid(true), checksum(0) {}
};

// Register information
struct register_info {
    std::string name;
    size_t size;
    size_t offset;
    bool available;
};

// Memory region information
struct memory_region {
    uintptr_t start;
    uintptr_t end;
    bool readable;
    bool writable;
    bool executable;
    std::string name;
};

// Breakpoint types
enum class breakpoint_type {
    SOFTWARE = 0,
    HARDWARE = 1,
    WRITE_WATCHPOINT = 2,
    READ_WATCHPOINT = 3,
    ACCESS_WATCHPOINT = 4
};

// Breakpoint structure
struct breakpoint {
    breakpoint_type type;
    uintptr_t address;
    size_t length;
    uint8_t original_instruction;
    bool enabled;
};

// Thread state
enum class thread_state {
    RUNNING,
    STOPPED,
    TERMINATED
};

// Thread information
struct thread_info {
    sched::thread* thread;
    thread_state state;
    int signal;
    std::string name;
};

// Architecture-specific interface
class arch_interface {
public:
    virtual ~arch_interface() = default;
    
    // Register operations
    virtual std::vector<register_info> get_register_info() = 0;
    virtual bool read_registers(sched::thread* thread, std::vector<uint8_t>& data) = 0;
    virtual bool write_registers(sched::thread* thread, const std::vector<uint8_t>& data) = 0;
    virtual bool read_register(sched::thread* thread, int reg_num, std::vector<uint8_t>& data) = 0;
    virtual bool write_register(sched::thread* thread, int reg_num, const std::vector<uint8_t>& data) = 0;
    
    // Breakpoint operations
    virtual bool set_breakpoint(breakpoint& bp) = 0;
    virtual bool remove_breakpoint(const breakpoint& bp) = 0;
    virtual bool is_breakpoint_instruction(uintptr_t addr) = 0;
    
    // Single step
    virtual bool single_step(sched::thread* thread) = 0;
    
    // Architecture info
    virtual std::string get_target_xml() = 0;
    virtual size_t get_instruction_size(uintptr_t addr) = 0;
};

// Transport interface
class transport {
public:
    virtual ~transport() = default;
    
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool is_connected() = 0;
    virtual ssize_t read(void* buffer, size_t size) = 0;
    virtual ssize_t write(const void* buffer, size_t size) = 0;
    virtual bool wait_for_connection() = 0;
};

// Main GDB stub class
class gdb_stub {
private:
    std::unique_ptr<transport> transport_;
    std::unique_ptr<arch_interface> arch_;
    std::unique_ptr<protocol_handler> protocol_;
    
    // State management
    mutex state_lock_;
    bool running_;
    bool attached_;
    sched::thread* current_thread_;
    std::map<int, thread_info> threads_;
    std::map<uintptr_t, breakpoint> breakpoints_;
    
    // Configuration
    bool non_stop_mode_;
    bool extended_mode_;
    
public:
    gdb_stub();
    ~gdb_stub();
    
    // Initialization
    bool initialize(std::unique_ptr<transport> transport);
    void shutdown();
    
    // Main loop
    void run();
    
    // Thread management
    void add_thread(sched::thread* thread);
    void remove_thread(sched::thread* thread);
    void thread_stopped(sched::thread* thread, int signal);
    
    // Memory operations
    bool read_memory(uintptr_t addr, size_t length, std::vector<uint8_t>& data);
    bool write_memory(uintptr_t addr, const std::vector<uint8_t>& data);
    
    // Breakpoint management
    bool set_breakpoint(breakpoint_type type, uintptr_t addr, size_t length);
    bool remove_breakpoint(breakpoint_type type, uintptr_t addr, size_t length);
    
    // Execution control
    bool continue_execution();
    bool step_execution();
    bool interrupt_execution();
    
    // Query operations
    std::vector<memory_region> get_memory_map();
    std::string get_target_xml();
    
private:
    void handle_packet(const gdb_packet& packet);
    void send_packet(const std::string& data);
    void send_ack();
    void send_nack();
    
    // Packet handlers
    void handle_query(const std::string& query);
    void handle_read_registers();
    void handle_write_registers(const std::string& data);
    void handle_read_memory(const std::string& params);
    void handle_write_memory(const std::string& params);
    void handle_continue(const std::string& params);
    void handle_step(const std::string& params);
    void handle_breakpoint(const std::string& params);
    void handle_thread_info();
    void handle_thread_selection(const std::string& params);
    void handle_thread_alive(const std::string& params);
    void handle_halt_reason();
    void handle_detach();
    void handle_kill();
    void handle_target_xml_query(const std::string& query);
    void handle_memory_map_query(const std::string& query);
    
    // Utility functions
    std::string format_hex(const std::vector<uint8_t>& data);
    std::vector<uint8_t> parse_hex(const std::string& hex_str);
    int get_thread_id(sched::thread* thread);
    sched::thread* get_thread_by_id(int thread_id);
    
    // Accessor for transport (needed by protocol handler)
    transport& get_transport() { return *transport_; }
};

// Global instance
extern std::unique_ptr<gdb_stub> g_gdb_stub;

// Transport factory functions
std::unique_ptr<transport> create_tcp_transport(int port);
std::unique_ptr<transport> create_serial_transport(const std::string& device);
std::unique_ptr<transport> create_vsock_transport(int port);

// Architecture factory function
std::unique_ptr<arch_interface> create_arch_interface();

} // namespace gdb_stub

#endif // GDB_STUB_HH