/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "gdb_protocol.hh"
#include <osv/debug.h>
#include <sstream>
#include <iomanip>

namespace gdb_stub {

protocol_handler::protocol_handler(gdb_stub& stub)
    : stub_(stub)
    , transport_(stub.get_transport())
    , ack_mode_(true)
{
}

bool protocol_handler::receive_packet(gdb_packet& packet)
{
    std::string buffer;
    char ch;
    
    // Read until we find a packet start
    while (true) {
        ssize_t bytes_read = transport_.read(&ch, 1);
        if (bytes_read <= 0) {
            return false;
        }
        
        if (ch == GDB_PACKET_START) {
            break;
        } else if (ch == GDB_INTERRUPT) {
            // Handle interrupt signal
            packet.data = "\x03";
            packet.valid = true;
            return true;
        } else if (ch == GDB_ACK || ch == GDB_NACK) {
            // Handle acknowledgments
            continue;
        }
    }
    
    // Read packet data until '#'
    while (true) {
        ssize_t bytes_read = transport_.read(&ch, 1);
        if (bytes_read <= 0) {
            return false;
        }
        
        if (ch == GDB_PACKET_END) {
            break;
        }
        
        buffer += ch;
    }
    
    // Read checksum (2 hex digits)
    char checksum_str[3] = {0};
    ssize_t bytes_read = transport_.read(checksum_str, 2);
    if (bytes_read != 2) {
        return false;
    }
    
    uint8_t received_checksum = static_cast<uint8_t>(std::stoul(checksum_str, nullptr, 16));
    uint8_t calculated_checksum = calculate_checksum(buffer);
    
    if (received_checksum != calculated_checksum) {
        debug("gdb-stub: Checksum mismatch: received %02x, calculated %02x\n",
              received_checksum, calculated_checksum);
        if (ack_mode_) {
            send_nack();
        }
        return false;
    }
    
    if (ack_mode_) {
        send_ack();
    }
    
    // Unescape the data
    packet.data = unescape_data(buffer);
    packet.valid = true;
    packet.checksum = received_checksum;
    
    return true;
}

bool protocol_handler::send_packet(const std::string& data)
{
    std::string escaped_data = escape_data(data);
    std::string formatted_packet = format_packet(escaped_data);
    
    ssize_t bytes_written = transport_.write(formatted_packet.c_str(), formatted_packet.length());
    if (bytes_written != static_cast<ssize_t>(formatted_packet.length())) {
        return false;
    }
    
    if (ack_mode_) {
        // Wait for acknowledgment
        char ack;
        ssize_t bytes_read = transport_.read(&ack, 1);
        if (bytes_read != 1) {
            return false;
        }
        
        if (ack == GDB_NACK) {
            // Resend packet
            return send_packet(data);
        } else if (ack != GDB_ACK) {
            debug("gdb-stub: Unexpected acknowledgment: %c\n", ack);
            return false;
        }
    }
    
    return true;
}

void protocol_handler::send_ack()
{
    char ack = GDB_ACK;
    transport_.write(&ack, 1);
}

void protocol_handler::send_nack()
{
    char nack = GDB_NACK;
    transport_.write(&nack, 1);
}

uint8_t protocol_handler::calculate_checksum(const std::string& data)
{
    uint8_t checksum = 0;
    for (char ch : data) {
        checksum += static_cast<uint8_t>(ch);
    }
    return checksum;
}

bool protocol_handler::verify_checksum(const std::string& data, uint8_t expected_checksum)
{
    return calculate_checksum(data) == expected_checksum;
}

std::string protocol_handler::escape_data(const std::string& data)
{
    std::string escaped;
    for (char ch : data) {
        if (ch == '#' || ch == '$' || ch == '}' || ch == '*') {
            escaped += '}';
            escaped += static_cast<char>(ch ^ 0x20);
        } else {
            escaped += ch;
        }
    }
    return escaped;
}

std::string protocol_handler::unescape_data(const std::string& data)
{
    std::string unescaped;
    bool escape_next = false;
    
    for (char ch : data) {
        if (escape_next) {
            unescaped += static_cast<char>(ch ^ 0x20);
            escape_next = false;
        } else if (ch == '}') {
            escape_next = true;
        } else {
            unescaped += ch;
        }
    }
    
    return unescaped;
}

std::string protocol_handler::format_packet(const std::string& data)
{
    uint8_t checksum = calculate_checksum(data);
    
    std::ostringstream oss;
    oss << GDB_PACKET_START << data << GDB_PACKET_END;
    oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(checksum);
    
    return oss.str();
}

} // namespace gdb_stub