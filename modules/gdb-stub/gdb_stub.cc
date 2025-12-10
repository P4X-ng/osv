/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "gdb_stub.hh"
#include "gdb_protocol.hh"
#include "gdb_transport.hh"
#include "gdb_arch.hh"
#include <osv/debug.hh>
#include <osv/sched.hh>
#include <osv/mmu.hh>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace gdb_stub {

std::unique_ptr<gdb_stub> g_gdb_stub;

gdb_stub::gdb_stub()
    : running_(false)
    , attached_(false)
    , current_thread_(nullptr)
    , non_stop_mode_(false)
    , extended_mode_(false)
{
}

gdb_stub::~gdb_stub()
{
    shutdown();
}

bool gdb_stub::initialize(std::unique_ptr<transport> transport)
{
    transport_ = std::move(transport);
    arch_ = create_arch_interface();
    protocol_ = std::make_unique<protocol_handler>(*this);
    
    if (!transport_->initialize()) {
        debug("gdb-stub: Failed to initialize transport\n");
        return false;
    }
    
    debug("gdb-stub: Initialized successfully\n");
    return true;
}

void gdb_stub::shutdown()
{
    WITH_LOCK(state_lock_) {
        running_ = false;
    }
    
    if (transport_) {
        transport_->shutdown();
    }
    
    debug("gdb-stub: Shutdown complete\n");
}

void gdb_stub::run()
{
    WITH_LOCK(state_lock_) {
        running_ = true;
    }
    
    debug("gdb-stub: Starting main loop\n");
    
    while (running_) {
        if (!transport_->wait_for_connection()) {
            debug("gdb-stub: Failed to wait for connection\n");
            break;
        }
        
        debug("gdb-stub: Client connected\n");
        attached_ = true;
        
        // Main packet processing loop
        while (running_ && transport_->is_connected()) {
            gdb_packet packet;
            if (protocol_->receive_packet(packet)) {
                handle_packet(packet);
            } else {
                // Connection lost or error
                break;
            }
        }
        
        attached_ = false;
        debug("gdb-stub: Client disconnected\n");
    }
}

void gdb_stub::handle_packet(const gdb_packet& packet)
{
    if (packet.data.empty()) {
        return;
    }
    
    char command = packet.data[0];
    std::string params = packet.data.substr(1);
    
    debug("gdb-stub: Received command '%c' with params '%s'\n", 
          command, params.c_str());
    
    switch (command) {
        case 'q':
            handle_query(params);
            break;
        case 'g':
            handle_read_registers();
            break;
        case 'G':
            handle_write_registers(params);
            break;
        case 'm':
            handle_read_memory(params);
            break;
        case 'M':
            handle_write_memory(params);
            break;
        case 'c':
            handle_continue(params);
            break;
        case 's':
            handle_step(params);
            break;
        case 'Z':
        case 'z':
            handle_breakpoint(packet.data);
            break;
        case 'H':
            handle_thread_selection(params);
            break;
        case 'T':
            handle_thread_alive(params);
            break;
        case '?':
            handle_halt_reason();
            break;
        case 'D':
            handle_detach();
            break;
        case 'k':
            handle_kill();
            break;
        default:
            // Unsupported command
            send_packet("");
            break;
    }
}

void gdb_stub::send_packet(const std::string& data)
{
    if (protocol_) {
        protocol_->send_packet(data);
    }
}

void gdb_stub::handle_query(const std::string& query)
{
    if (query.substr(0, 9) == "Supported") {
        // Report supported features
        std::string response = "PacketSize=1000;qXfer:features:read+;qXfer:memory-map:read+";
        send_packet(response);
    } else if (query == "C") {
        // Current thread ID
        send_packet("QC1");
    } else if (query.substr(0, 8) == "fThreadInfo") {
        handle_thread_info();
    } else if (query == "sThreadInfo") {
        send_packet("l"); // End of thread list
    } else if (query.substr(0, 11) == "Xfer:features:read:") {
        handle_target_xml_query(query);
    } else if (query.substr(0, 15) == "Xfer:memory-map:read:") {
        handle_memory_map_query(query);
    } else {
        // Unsupported query
        send_packet("");
    }
}

void gdb_stub::handle_read_registers()
{
    if (!current_thread_) {
        send_packet("E01");
        return;
    }
    
    std::vector<uint8_t> reg_data;
    if (arch_->read_registers(current_thread_, reg_data)) {
        send_packet(format_hex(reg_data));
    } else {
        send_packet("E02");
    }
}

void gdb_stub::handle_write_registers(const std::string& data)
{
    if (!current_thread_) {
        send_packet("E01");
        return;
    }
    
    std::vector<uint8_t> reg_data = parse_hex(data);
    if (arch_->write_registers(current_thread_, reg_data)) {
        send_packet("OK");
    } else {
        send_packet("E02");
    }
}

void gdb_stub::handle_read_memory(const std::string& params)
{
    size_t comma_pos = params.find(',');
    if (comma_pos == std::string::npos) {
        send_packet("E01");
        return;
    }
    
    uintptr_t addr = std::stoull(params.substr(0, comma_pos), nullptr, 16);
    size_t length = std::stoull(params.substr(comma_pos + 1), nullptr, 16);
    
    std::vector<uint8_t> data;
    if (read_memory(addr, length, data)) {
        send_packet(format_hex(data));
    } else {
        send_packet("E03");
    }
}

void gdb_stub::handle_write_memory(const std::string& params)
{
    size_t comma_pos = params.find(',');
    size_t colon_pos = params.find(':');
    
    if (comma_pos == std::string::npos || colon_pos == std::string::npos) {
        send_packet("E01");
        return;
    }
    
    uintptr_t addr = std::stoull(params.substr(0, comma_pos), nullptr, 16);
    size_t length = std::stoull(params.substr(comma_pos + 1, colon_pos - comma_pos - 1), nullptr, 16);
    std::vector<uint8_t> data = parse_hex(params.substr(colon_pos + 1));
    
    if (data.size() != length) {
        send_packet("E02");
        return;
    }
    
    if (write_memory(addr, data)) {
        send_packet("OK");
    } else {
        send_packet("E03");
    }
}

bool gdb_stub::read_memory(uintptr_t addr, size_t length, std::vector<uint8_t>& data)
{
    data.resize(length);
    
    try {
        // Use OSv's memory management to safely read memory
        memcpy(data.data(), reinterpret_cast<void*>(addr), length);
        return true;
    } catch (...) {
        return false;
    }
}

bool gdb_stub::write_memory(uintptr_t addr, const std::vector<uint8_t>& data)
{
    try {
        // Use OSv's memory management to safely write memory
        memcpy(reinterpret_cast<void*>(addr), data.data(), data.size());
        return true;
    } catch (...) {
        return false;
    }
}

std::string gdb_stub::format_hex(const std::vector<uint8_t>& data)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        oss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> gdb_stub::parse_hex(const std::string& hex_str)
{
    std::vector<uint8_t> data;
    for (size_t i = 0; i < hex_str.length(); i += 2) {
        if (i + 1 < hex_str.length()) {
            std::string byte_str = hex_str.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
            data.push_back(byte);
        }
    }
    return data;
}

// Additional handler methods will be implemented in the next part...

void gdb_stub::handle_continue(const std::string& params)
{
    // Continue execution
    send_packet("OK");
    // In a real implementation, this would resume thread execution
}

void gdb_stub::handle_step(const std::string& params)
{
    if (!current_thread_) {
        send_packet("E01");
        return;
    }
    
    if (arch_->single_step(current_thread_)) {
        send_packet("OK");
    } else {
        send_packet("E02");
    }
}

void gdb_stub::handle_breakpoint(const std::string& params)
{
    if (params.length() < 4) {
        send_packet("E01");
        return;
    }
    
    char action = params[0]; // 'Z' to set, 'z' to remove
    char type_char = params[1];
    
    // Parse type,addr,length
    size_t comma1 = params.find(',', 2);
    size_t comma2 = params.find(',', comma1 + 1);
    
    if (comma1 == std::string::npos || comma2 == std::string::npos) {
        send_packet("E01");
        return;
    }
    
    int type = type_char - '0';
    uintptr_t addr = std::stoull(params.substr(comma1 + 1, comma2 - comma1 - 1), nullptr, 16);
    size_t length = std::stoull(params.substr(comma2 + 1), nullptr, 16);
    
    breakpoint_type bp_type = static_cast<breakpoint_type>(type);
    
    if (action == 'Z') {
        if (set_breakpoint(bp_type, addr, length)) {
            send_packet("OK");
        } else {
            send_packet("E03");
        }
    } else {
        if (remove_breakpoint(bp_type, addr, length)) {
            send_packet("OK");
        } else {
            send_packet("E03");
        }
    }
}

void gdb_stub::handle_thread_selection(const std::string& params)
{
    if (params.empty()) {
        send_packet("E01");
        return;
    }
    
    char operation = params[0];
    std::string thread_id_str = params.substr(1);
    
    if (thread_id_str == "0" || thread_id_str == "-1") {
        // Use any thread
        send_packet("OK");
        return;
    }
    
    int thread_id = std::stoi(thread_id_str);
    sched::thread* thread = get_thread_by_id(thread_id);
    
    if (!thread) {
        send_packet("E02");
        return;
    }
    
    if (operation == 'c' || operation == 'g') {
        current_thread_ = thread;
        send_packet("OK");
    } else {
        send_packet("E01");
    }
}

void gdb_stub::handle_thread_alive(const std::string& params)
{
    int thread_id = std::stoi(params);
    sched::thread* thread = get_thread_by_id(thread_id);
    
    if (thread) {
        send_packet("OK");
    } else {
        send_packet("E01");
    }
}

void gdb_stub::handle_halt_reason()
{
    // Report that we're stopped due to a signal
    send_packet("S05"); // SIGTRAP
}

void gdb_stub::handle_detach()
{
    send_packet("OK");
    attached_ = false;
}

void gdb_stub::handle_kill()
{
    send_packet("OK");
    running_ = false;
}

void gdb_stub::handle_thread_info()
{
    std::ostringstream oss;
    oss << "m";
    
    bool first = true;
    WITH_LOCK(state_lock_) {
        for (const auto& pair : threads_) {
            if (!first) {
                oss << ",";
            }
            oss << std::hex << pair.first;
            first = false;
        }
    }
    
    send_packet(oss.str());
}

void gdb_stub::handle_target_xml_query(const std::string& query)
{
    // Extract offset and length from query
    size_t colon_pos = query.find(':', 11);
    size_t comma_pos = query.find(',', colon_pos);
    
    if (colon_pos == std::string::npos || comma_pos == std::string::npos) {
        send_packet("E01");
        return;
    }
    
    std::string target_name = query.substr(11, colon_pos - 11);
    size_t offset = std::stoull(query.substr(colon_pos + 1, comma_pos - colon_pos - 1), nullptr, 16);
    size_t length = std::stoull(query.substr(comma_pos + 1), nullptr, 16);
    
    if (target_name == "target.xml") {
        std::string xml = arch_->get_target_xml();
        
        if (offset >= xml.length()) {
            send_packet("l");
            return;
        }
        
        size_t remaining = xml.length() - offset;
        size_t chunk_size = std::min(length, remaining);
        std::string chunk = xml.substr(offset, chunk_size);
        
        if (offset + chunk_size >= xml.length()) {
            send_packet("l" + chunk);
        } else {
            send_packet("m" + chunk);
        }
    } else {
        send_packet("E01");
    }
}

void gdb_stub::handle_memory_map_query(const std::string& query)
{
    // Simple memory map - in a real implementation this would query OSv's memory layout
    std::string memory_map = R"(<?xml version="1.0"?>
<!DOCTYPE memory-map PUBLIC "+//IDN gnu.org//DTD GDB Memory Map V1.0//EN" "http://sourceware.org/gdb/gdb-memory-map.dtd">
<memory-map>
  <memory type="ram" start="0x0" length="0x100000000"/>
</memory-map>)";
    
    // Handle offset and length like target XML
    size_t colon_pos = query.find(':', 15);
    size_t comma_pos = query.find(',', colon_pos);
    
    if (colon_pos == std::string::npos || comma_pos == std::string::npos) {
        send_packet("E01");
        return;
    }
    
    size_t offset = std::stoull(query.substr(colon_pos + 1, comma_pos - colon_pos - 1), nullptr, 16);
    size_t length = std::stoull(query.substr(comma_pos + 1), nullptr, 16);
    
    if (offset >= memory_map.length()) {
        send_packet("l");
        return;
    }
    
    size_t remaining = memory_map.length() - offset;
    size_t chunk_size = std::min(length, remaining);
    std::string chunk = memory_map.substr(offset, chunk_size);
    
    if (offset + chunk_size >= memory_map.length()) {
        send_packet("l" + chunk);
    } else {
        send_packet("m" + chunk);
    }
}

bool gdb_stub::set_breakpoint(breakpoint_type type, uintptr_t addr, size_t length)
{
    breakpoint bp;
    bp.type = type;
    bp.address = addr;
    bp.length = length;
    bp.enabled = false;
    
    if (arch_->set_breakpoint(bp)) {
        WITH_LOCK(state_lock_) {
            breakpoints_[addr] = bp;
        }
        return true;
    }
    
    return false;
}

bool gdb_stub::remove_breakpoint(breakpoint_type type, uintptr_t addr, size_t length)
{
    WITH_LOCK(state_lock_) {
        auto it = breakpoints_.find(addr);
        if (it != breakpoints_.end()) {
            if (arch_->remove_breakpoint(it->second)) {
                breakpoints_.erase(it);
                return true;
            }
        }
    }
    
    return false;
}

int gdb_stub::get_thread_id(sched::thread* thread)
{
    // Simple mapping - use thread pointer as ID
    return static_cast<int>(reinterpret_cast<uintptr_t>(thread) & 0xFFFF);
}

sched::thread* gdb_stub::get_thread_by_id(int thread_id)
{
    WITH_LOCK(state_lock_) {
        auto it = threads_.find(thread_id);
        if (it != threads_.end()) {
            return it->second.thread;
        }
    }
    return nullptr;
}

void gdb_stub::add_thread(sched::thread* thread)
{
    int thread_id = get_thread_id(thread);
    thread_info info;
    info.thread = thread;
    info.state = thread_state::RUNNING;
    info.signal = 0;
    info.name = "osv-thread";
    
    WITH_LOCK(state_lock_) {
        threads_[thread_id] = info;
        if (!current_thread_) {
            current_thread_ = thread;
        }
    }
}

void gdb_stub::remove_thread(sched::thread* thread)
{
    int thread_id = get_thread_id(thread);
    
    WITH_LOCK(state_lock_) {
        threads_.erase(thread_id);
        if (current_thread_ == thread) {
            current_thread_ = nullptr;
        }
    }
}

// Main entry point for the GDB stub module
extern "C" void gdb_stub_main()
{
    debug("gdb-stub: Starting GDB stub module\n");
    
    // Create transport based on configuration
    // For now, default to TCP on port 1234
    auto transport = create_tcp_transport(1234);
    if (!transport) {
        debug("gdb-stub: Failed to create transport\n");
        return;
    }
    
    // Create and initialize the GDB stub
    g_gdb_stub = std::make_unique<gdb_stub>();
    if (!g_gdb_stub->initialize(std::move(transport))) {
        debug("gdb-stub: Failed to initialize GDB stub\n");
        g_gdb_stub.reset();
        return;
    }
    
    // Add current thread to the stub
    g_gdb_stub->add_thread(sched::thread::current());
    
    // Run the main loop
    g_gdb_stub->run();
    
    debug("gdb-stub: GDB stub module exiting\n");
}

} // namespace gdb_stub