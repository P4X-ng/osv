/*
 * Copyright (C) 2024 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "gdb-stub.hh"
#include <osv/debug.h>
#include <osv/sched.hh>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

namespace gdb {

// Packet implementation
uint8_t Packet::checksum() const {
    uint8_t sum = 0;
    for (char c : _data) {
        sum += static_cast<uint8_t>(c);
    }
    return sum;
}

std::string Packet::format() const {
    std::ostringstream oss;
    oss << '$' << _data << '#' 
        << std::hex << std::setw(2) << std::setfill('0') 
        << static_cast<int>(checksum());
    return oss.str();
}

bool Packet::parse(const std::string& raw) {
    if (raw.length() < 4) return false;
    if (raw[0] != '$') return false;
    
    size_t hash_pos = raw.find('#');
    if (hash_pos == std::string::npos || hash_pos + 3 > raw.length()) {
        return false;
    }
    
    _data = raw.substr(1, hash_pos - 1);
    
    // Verify checksum
    std::string checksum_str = raw.substr(hash_pos + 1, 2);
    uint8_t expected_checksum = std::stoi(checksum_str, nullptr, 16);
    
    return checksum() == expected_checksum;
}

// TcpTransport implementation
TcpTransport::TcpTransport(uint16_t port)
    : _port(port), _listen_fd(-1), _client_fd(-1), _running(false) {
}

TcpTransport::~TcpTransport() {
    stop();
}

bool TcpTransport::start() {
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd < 0) {
        debug("GDB stub: Failed to create socket: %s\n", strerror(errno));
        return false;
    }
    
    int opt = 1;
    if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        debug("GDB stub: Failed to set SO_REUSEADDR: %s\n", strerror(errno));
        close(_listen_fd);
        _listen_fd = -1;
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);
    
    if (bind(_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        debug("GDB stub: Failed to bind to port %d: %s\n", _port, strerror(errno));
        close(_listen_fd);
        _listen_fd = -1;
        return false;
    }
    
    if (listen(_listen_fd, 1) < 0) {
        debug("GDB stub: Failed to listen: %s\n", strerror(errno));
        close(_listen_fd);
        _listen_fd = -1;
        return false;
    }
    
    debug("GDB stub: Listening on port %d\n", _port);
    _running = true;
    
    // Accept connection (blocking)
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    _client_fd = accept(_listen_fd, (struct sockaddr*)&client_addr, &client_len);
    
    if (_client_fd < 0) {
        debug("GDB stub: Failed to accept connection: %s\n", strerror(errno));
        return false;
    }
    
    debug("GDB stub: Client connected from %s\n", 
          inet_ntoa(client_addr.sin_addr));
    
    return true;
}

void TcpTransport::stop() {
    _running = false;
    if (_client_fd >= 0) {
        close(_client_fd);
        _client_fd = -1;
    }
    if (_listen_fd >= 0) {
        close(_listen_fd);
        _listen_fd = -1;
    }
}

bool TcpTransport::send(const std::string& data) {
    if (_client_fd < 0) return false;
    
    ssize_t sent = ::send(_client_fd, data.c_str(), data.length(), 0);
    if (sent < 0) {
        debug("GDB stub: Failed to send data: %s\n", strerror(errno));
        return false;
    }
    
    return sent == static_cast<ssize_t>(data.length());
}

std::string TcpTransport::receive() {
    if (_client_fd < 0) return "";
    
    char buffer[4096];
    ssize_t received = recv(_client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (received <= 0) {
        if (received < 0) {
            debug("GDB stub: Failed to receive data: %s\n", strerror(errno));
        }
        return "";
    }
    
    buffer[received] = '\0';
    return std::string(buffer, received);
}

bool TcpTransport::is_connected() const {
    return _running && _client_fd >= 0;
}

// SerialTransport implementation
SerialTransport::SerialTransport(const std::string& device)
    : _device(device), _fd(-1), _running(false) {
}

SerialTransport::~SerialTransport() {
    stop();
}

bool SerialTransport::start() {
    _fd = open(_device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (_fd < 0) {
        debug("GDB stub: Failed to open serial device %s: %s\n", 
              _device.c_str(), strerror(errno));
        return false;
    }
    
    // Configure serial port
    struct termios tty;
    if (tcgetattr(_fd, &tty) != 0) {
        debug("GDB stub: Failed to get serial attributes: %s\n", strerror(errno));
        close(_fd);
        _fd = -1;
        return false;
    }
    
    // Set 115200 baud, 8N1, no flow control
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 5;
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    
    if (tcsetattr(_fd, TCSANOW, &tty) != 0) {
        debug("GDB stub: Failed to set serial attributes: %s\n", strerror(errno));
        close(_fd);
        _fd = -1;
        return false;
    }
    
    debug("GDB stub: Opened serial device %s\n", _device.c_str());
    _running = true;
    return true;
}

void SerialTransport::stop() {
    _running = false;
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
}

bool SerialTransport::send(const std::string& data) {
    if (_fd < 0) return false;
    
    ssize_t written = write(_fd, data.c_str(), data.length());
    if (written < 0) {
        debug("GDB stub: Failed to write to serial: %s\n", strerror(errno));
        return false;
    }
    
    return written == static_cast<ssize_t>(data.length());
}

std::string SerialTransport::receive() {
    if (_fd < 0) return "";
    
    char buffer[4096];
    ssize_t bytes_read = read(_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read <= 0) {
        if (bytes_read < 0) {
            debug("GDB stub: Failed to read from serial: %s\n", strerror(errno));
        }
        return "";
    }
    
    buffer[bytes_read] = '\0';
    return std::string(buffer, bytes_read);
}

bool SerialTransport::is_connected() const {
    return _running && _fd >= 0;
}

// CpuState implementation
CpuState CpuState::capture() {
    CpuState state;
    // This is a simplified version - actual implementation would need
    // to capture registers from the current thread context
    memset(&state, 0, sizeof(state));
    
    // In a real implementation, we would:
    // 1. Get current thread
    // 2. Extract register state from thread context
    // 3. Handle different CPU states (running, stopped, etc.)
    
    return state;
}

void CpuState::restore() const {
    // This would restore the CPU state
    // Implementation depends on OSv's thread/CPU state management
}

// GdbStub implementation
GdbStub::GdbStub(std::unique_ptr<Transport> transport)
    : _transport(std::move(transport))
    , _running(false)
    , _extended_mode(false) {
}

GdbStub::~GdbStub() {
    stop();
}

bool GdbStub::start() {
    if (_running) return true;
    
    if (!_transport->is_connected()) {
        debug("GDB stub: Transport not connected\n");
        return false;
    }
    
    _running = true;
    debug("GDB stub: Started\n");
    
    return true;
}

void GdbStub::stop() {
    if (!_running) return;
    
    _running = false;
    debug("GDB stub: Stopped\n");
}

void GdbStub::run() {
    debug("GDB stub: Entering main loop\n");
    
    while (_running && _transport->is_connected()) {
        Packet packet = receive_packet();
        
        if (packet.data().empty()) {
            // Connection closed or error
            break;
        }
        
        std::string response = handle_command(packet.data());
        
        if (!response.empty()) {
            Packet response_packet(response);
            if (!send_packet(response_packet)) {
                debug("GDB stub: Failed to send response\n");
                break;
            }
        }
    }
    
    debug("GDB stub: Exiting main loop\n");
    stop();
}

std::string GdbStub::handle_command(const std::string& cmd) {
    if (cmd.empty()) return "";
    
    debug("GDB stub: Received command: %s\n", cmd.c_str());
    
    char command = cmd[0];
    std::string params = cmd.length() > 1 ? cmd.substr(1) : "";
    
    switch (command) {
        case '?':
            // Return last signal
            return "S05"; // SIGTRAP
            
        case 'q':
        case 'Q':
            return handle_query(params);
            
        case 'g':
            return handle_read_registers();
            
        case 'G':
            return handle_write_registers(params);
            
        case 'm':
            {
                // Read memory: m<addr>,<length>
                size_t comma = params.find(',');
                if (comma == std::string::npos) return "E01";
                
                uint64_t addr = parse_hex(params.substr(0, comma));
                size_t length = parse_hex(params.substr(comma + 1));
                
                return handle_read_memory(addr, length);
            }
            
        case 'M':
            {
                // Write memory: M<addr>,<length>:<data>
                size_t comma = params.find(',');
                size_t colon = params.find(':');
                if (comma == std::string::npos || colon == std::string::npos) {
                    return "E01";
                }
                
                uint64_t addr = parse_hex(params.substr(0, comma));
                std::string data = params.substr(colon + 1);
                
                return handle_write_memory(addr, data);
            }
            
        case 'c':
            return handle_continue();
            
        case 's':
            return handle_step();
            
        case 'Z':
            {
                // Insert breakpoint: Z<type>,<addr>,<kind>
                size_t comma1 = params.find(',');
                size_t comma2 = params.find(',', comma1 + 1);
                if (comma1 == std::string::npos) return "E01";
                
                uint64_t addr = parse_hex(params.substr(comma1 + 1, 
                    comma2 != std::string::npos ? comma2 - comma1 - 1 : std::string::npos));
                
                return handle_insert_breakpoint(addr);
            }
            
        case 'z':
            {
                // Remove breakpoint: z<type>,<addr>,<kind>
                size_t comma1 = params.find(',');
                size_t comma2 = params.find(',', comma1 + 1);
                if (comma1 == std::string::npos) return "E01";
                
                uint64_t addr = parse_hex(params.substr(comma1 + 1,
                    comma2 != std::string::npos ? comma2 - comma1 - 1 : std::string::npos));
                
                return handle_remove_breakpoint(addr);
            }
            
        case 'k':
            // Kill request
            stop();
            return "";
            
        default:
            // Unsupported command
            return "";
    }
}

std::string GdbStub::handle_query(const std::string& params) {
    if (params.substr(0, 9) == "Supported") {
        // Report supported features
        return "PacketSize=4096;qXfer:features:read+";
    } else if (params == "C") {
        // Return current thread ID
        return "QC1";
    } else if (params.substr(0, 8) == "Attached") {
        // Attached to existing process
        return "1";
    } else if (params.substr(0, 7) == "Symbol:") {
        // Symbol lookup
        return "OK";
    } else if (params.substr(0, 6) == "TStatus") {
        // Tracepoint status
        return "";
    }
    
    return "";
}

std::string GdbStub::handle_read_registers() {
    _cpu_state = CpuState::capture();
    
    std::ostringstream oss;
    
    // Format all registers as hex (little-endian)
    oss << format_hex(_cpu_state.rax)
        << format_hex(_cpu_state.rbx)
        << format_hex(_cpu_state.rcx)
        << format_hex(_cpu_state.rdx)
        << format_hex(_cpu_state.rsi)
        << format_hex(_cpu_state.rdi)
        << format_hex(_cpu_state.rbp)
        << format_hex(_cpu_state.rsp)
        << format_hex(_cpu_state.r8)
        << format_hex(_cpu_state.r9)
        << format_hex(_cpu_state.r10)
        << format_hex(_cpu_state.r11)
        << format_hex(_cpu_state.r12)
        << format_hex(_cpu_state.r13)
        << format_hex(_cpu_state.r14)
        << format_hex(_cpu_state.r15)
        << format_hex(_cpu_state.rip)
        << format_hex(_cpu_state.rflags);
    
    return oss.str();
}

std::string GdbStub::handle_write_registers(const std::string& data) {
    // Parse register data and update CPU state
    // This is a simplified version
    return "OK";
}

std::string GdbStub::handle_read_memory(uint64_t addr, size_t length) {
    // Read memory and return as hex
    std::ostringstream oss;
    
    // Validate address range for safety
    // In a production implementation, should check:
    // 1. Address is within valid memory regions
    // 2. Memory is readable (not protected)
    // 3. Length is reasonable (cap at some maximum)
    
    if (length == 0 || length > 4096) {
        return "E01"; // Invalid length
    }
    
    // Basic check: ensure address is not null and reasonably aligned
    if (addr == 0) {
        return "E02"; // Invalid address
    }
    
    try {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(addr);
        
        // TODO: Add proper memory region validation
        // For now, attempt to read and catch exceptions
        for (size_t i = 0; i < length; i++) {
            oss << std::hex << std::setw(2) << std::setfill('0') 
                << static_cast<int>(ptr[i]);
        }
    } catch (...) {
        return "E03"; // Memory access error
    }
    
    return oss.str();
}

std::string GdbStub::handle_write_memory(uint64_t addr, const std::string& data) {
    // Write hex data to memory
    
    // Validate address and data length
    if (addr == 0) {
        return "E01"; // Invalid address
    }
    
    if (data.length() == 0 || data.length() % 2 != 0) {
        return "E02"; // Invalid data format
    }
    
    size_t length = data.length() / 2;
    if (length > 4096) {
        return "E03"; // Data too large
    }
    
    // TODO: Add proper memory region validation
    // Should check:
    // 1. Address range is writable
    // 2. Not writing to protected/kernel memory
    // 3. Proper permissions check
    
    try {
        uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
        for (size_t i = 0; i < data.length(); i += 2) {
            std::string byte_str = data.substr(i, 2);
            ptr[i/2] = std::stoi(byte_str, nullptr, 16);
        }
        return "OK";
    } catch (...) {
        return "E04"; // Memory write error
    }
}

std::string GdbStub::handle_continue() {
    // Continue execution
    return "S05"; // SIGTRAP
}

std::string GdbStub::handle_step() {
    // Single step
    return "S05"; // SIGTRAP
}

std::string GdbStub::handle_insert_breakpoint(uint64_t addr) {
    // Insert software breakpoint at address
    // Would need to save original byte and replace with int3 (0xCC)
    debug("GDB stub: Insert breakpoint at 0x%lx\n", addr);
    return "OK";
}

std::string GdbStub::handle_remove_breakpoint(uint64_t addr) {
    // Remove software breakpoint at address
    debug("GDB stub: Remove breakpoint at 0x%lx\n", addr);
    return "OK";
}

bool GdbStub::send_packet(const Packet& packet) {
    std::string formatted = packet.format();
    debug("GDB stub: Sending: %s\n", formatted.c_str());
    return _transport->send(formatted);
}

Packet GdbStub::receive_packet() {
    std::string data = _transport->receive();
    
    if (data.empty()) {
        return Packet();
    }
    
    debug("GDB stub: Received raw: %s\n", data.c_str());
    
    // Handle acknowledgments
    if (data[0] == '+' || data[0] == '-') {
        // ACK/NACK handling
        if (data.length() > 1) {
            data = data.substr(1);
        } else {
            return receive_packet(); // Get next packet
        }
    }
    
    Packet packet;
    if (!packet.parse(data)) {
        debug("GDB stub: Failed to parse packet\n");
        // Send NACK
        _transport->send("-");
        return Packet();
    }
    
    // Send ACK
    _transport->send("+");
    
    return packet;
}

std::string GdbStub::format_hex(uint64_t value, size_t bytes) {
    std::ostringstream oss;
    
    // Output in little-endian format
    for (size_t i = 0; i < bytes; i++) {
        uint8_t byte = (value >> (i * 8)) & 0xFF;
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(byte);
    }
    
    return oss.str();
}

uint64_t GdbStub::parse_hex(const std::string& hex) {
    return std::stoull(hex, nullptr, 16);
}

// GdbStubManager implementation
GdbStubManager& GdbStubManager::instance() {
    static GdbStubManager manager;
    return manager;
}

bool GdbStubManager::init_tcp(uint16_t port) {
    auto transport = std::make_unique<TcpTransport>(port);
    if (!transport->start()) {
        return false;
    }
    
    _stub = std::make_unique<GdbStub>(std::move(transport));
    return true;
}

bool GdbStubManager::init_serial(const std::string& device) {
    auto transport = std::make_unique<SerialTransport>(device);
    if (!transport->start()) {
        return false;
    }
    
    _stub = std::make_unique<GdbStub>(std::move(transport));
    return true;
}

bool GdbStubManager::start() {
    if (!_stub) {
        debug("GDB stub: Not initialized\n");
        return false;
    }
    
    return _stub->start();
}

void GdbStubManager::stop() {
    if (_stub) {
        _stub->stop();
    }
}

} // namespace gdb
