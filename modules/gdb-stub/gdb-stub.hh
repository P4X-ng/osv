/*
 * Copyright (C) 2024 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef GDB_STUB_HH
#define GDB_STUB_HH

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace gdb {

// Configuration constants
constexpr size_t MAX_PACKET_SIZE = 4096;        // Maximum packet size for GDB protocol
constexpr size_t MAX_MEMORY_ACCESS_SIZE = 4096; // Maximum memory read/write size
constexpr uint16_t MIN_TCP_PORT = 1;            // Minimum valid TCP port
constexpr uint16_t MAX_TCP_PORT = 65535;        // Maximum valid TCP port

// Forward declarations
class Transport;
struct CpuState;

// GDB Remote Serial Protocol packet
class Packet {
public:
    Packet() = default;
    explicit Packet(const std::string& data) : _data(data) {}
    
    const std::string& data() const { return _data; }
    void set_data(const std::string& data) { _data = data; }
    
    // Calculate checksum for packet
    uint8_t checksum() const;
    
    // Format packet with $ prefix, # suffix and checksum
    std::string format() const;
    
    // Parse packet from raw data (returns true if valid)
    bool parse(const std::string& raw);
    
private:
    std::string _data;
};

// Abstract transport layer for GDB protocol
class Transport {
public:
    virtual ~Transport() = default;
    
    // Send raw data
    virtual bool send(const std::string& data) = 0;
    
    // Receive raw data (blocking)
    virtual std::string receive() = 0;
    
    // Check if transport is connected
    virtual bool is_connected() const = 0;
};

// TCP transport implementation
class TcpTransport : public Transport {
public:
    explicit TcpTransport(uint16_t port);
    ~TcpTransport() override;
    
    bool send(const std::string& data) override;
    std::string receive() override;
    bool is_connected() const override;
    
    // Start listening for connections
    bool start();
    
    // Stop and cleanup
    void stop();
    
private:
    uint16_t _port;
    int _listen_fd;
    int _client_fd;
    bool _running;
};

// Serial transport implementation
class SerialTransport : public Transport {
public:
    explicit SerialTransport(const std::string& device);
    ~SerialTransport() override;
    
    bool send(const std::string& data) override;
    std::string receive() override;
    bool is_connected() const override;
    
    bool start();
    void stop();
    
private:
    std::string _device;
    int _fd;
    bool _running;
};

// CPU state representation for GDB
struct CpuState {
    // x86_64 general purpose registers
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
    
    // Segment registers
    uint64_t cs, ss, ds, es, fs, gs;
    
    // Read current CPU state
    static CpuState capture();
    
    // Restore CPU state
    void restore() const;
};

// Main GDB stub server
class GdbStub {
public:
    explicit GdbStub(std::unique_ptr<Transport> transport);
    ~GdbStub();
    
    // Start the GDB stub server
    bool start();
    
    // Stop the GDB stub server
    void stop();
    
    // Main protocol loop (run in separate thread)
    void run();
    
    // Check if stub is running
    bool is_running() const { return _running; }
    
private:
    // Handle individual GDB commands
    std::string handle_command(const std::string& cmd);
    
    // Command handlers
    std::string handle_query(const std::string& params);
    std::string handle_read_registers();
    std::string handle_write_registers(const std::string& data);
    std::string handle_read_memory(uint64_t addr, size_t length);
    std::string handle_write_memory(uint64_t addr, const std::string& data);
    std::string handle_continue();
    std::string handle_step();
    std::string handle_insert_breakpoint(uint64_t addr);
    std::string handle_remove_breakpoint(uint64_t addr);
    
    // Helper functions
    bool send_packet(const Packet& packet);
    Packet receive_packet();
    std::string format_hex(uint64_t value, size_t bytes = 8);
    uint64_t parse_hex(const std::string& hex);
    
    std::unique_ptr<Transport> _transport;
    bool _running;
    bool _extended_mode;
    CpuState _cpu_state;
};

// Global GDB stub instance management
class GdbStubManager {
public:
    static GdbStubManager& instance();
    
    // Initialize GDB stub with specified transport
    bool init_tcp(uint16_t port);
    bool init_serial(const std::string& device);
    
    // Start/stop the stub
    bool start();
    void stop();
    
    // Get current stub instance
    GdbStub* get_stub() const { return _stub.get(); }
    
private:
    GdbStubManager() = default;
    std::unique_ptr<GdbStub> _stub;
};

} // namespace gdb

#endif // GDB_STUB_HH
