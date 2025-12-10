/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef GDB_PROTOCOL_HH
#define GDB_PROTOCOL_HH

#include "gdb_stub.hh"
#include <string>
#include <vector>

namespace gdb_stub {

// GDB protocol constants
const char GDB_PACKET_START = '$';
const char GDB_PACKET_END = '#';
const char GDB_ACK = '+';
const char GDB_NACK = '-';
const char GDB_INTERRUPT = '\x03';

class protocol_handler {
private:
    gdb_stub& stub_;
    transport& transport_;
    bool ack_mode_;
    
public:
    protocol_handler(gdb_stub& stub);
    ~protocol_handler() = default;
    
    // Packet operations
    bool receive_packet(gdb_packet& packet);
    bool send_packet(const std::string& data);
    void send_ack();
    void send_nack();
    
    // Protocol state
    void set_ack_mode(bool enabled) { ack_mode_ = enabled; }
    bool get_ack_mode() const { return ack_mode_; }
    
private:
    // Low-level protocol functions
    uint8_t calculate_checksum(const std::string& data);
    bool verify_checksum(const std::string& data, uint8_t expected_checksum);
    std::string escape_data(const std::string& data);
    std::string unescape_data(const std::string& data);
    
    // Packet parsing
    bool parse_packet(const std::string& raw_data, gdb_packet& packet);
    std::string format_packet(const std::string& data);
};

} // namespace gdb_stub

#endif // GDB_PROTOCOL_HH