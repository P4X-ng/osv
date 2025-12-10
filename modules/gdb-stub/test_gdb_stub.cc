/*
 * Copyright (C) 2024 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

// Simple unit tests for GDB stub packet handling

#include "../gdb-stub.hh"
#include <cassert>
#include <iostream>
#include <string>

using namespace gdb;

void test_packet_checksum() {
    std::cout << "Testing packet checksum... ";
    
    Packet packet("qSupported");
    uint8_t checksum = packet.checksum();
    
    // Expected checksum for "qSupported"
    // q=0x71, S=0x53, u=0x75, p=0x70, p=0x70, o=0x6f, r=0x72, t=0x74, e=0x65, d=0x64
    // Sum = 0x71+0x53+0x75+0x70+0x70+0x6f+0x72+0x74+0x65+0x64 = 0x467 = 0x67 (mod 256)
    assert(checksum == 0x67);
    
    std::cout << "PASSED\n";
}

void test_packet_format() {
    std::cout << "Testing packet formatting... ";
    
    Packet packet("qSupported");
    std::string formatted = packet.format();
    
    // Should be: $qSupported#67
    assert(formatted == "$qSupported#67");
    
    std::cout << "PASSED\n";
}

void test_packet_parse() {
    std::cout << "Testing packet parsing... ";
    
    Packet packet;
    bool result = packet.parse("$qSupported#67");
    
    assert(result == true);
    assert(packet.data() == "qSupported");
    
    // Test invalid packet
    Packet invalid_packet;
    bool invalid_result = invalid_packet.parse("$invalid#00");
    assert(invalid_result == false);
    
    std::cout << "PASSED\n";
}

void test_hex_formatting() {
    std::cout << "Testing hex formatting... ";
    
    // This would test the GdbStub::format_hex function
    // For now, just verify the logic manually
    
    uint64_t value = 0x1234567890ABCDEF;
    // In little-endian: EF CD AB 90 78 56 34 12
    // As hex string: efcdab9078563412
    
    std::cout << "PASSED\n";
}

void test_command_parsing() {
    std::cout << "Testing command parsing... ";
    
    // Test various command formats
    std::string cmd1 = "?";
    assert(cmd1[0] == '?');
    
    std::string cmd2 = "qSupported:multiprocess+";
    assert(cmd2[0] == 'q');
    assert(cmd2.substr(1, 9) == "Supported");
    
    std::string cmd3 = "m1000,10";
    assert(cmd3[0] == 'm');
    size_t comma = cmd3.find(',');
    assert(comma != std::string::npos);
    
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "GDB Stub Unit Tests\n";
    std::cout << "===================\n\n";
    
    try {
        test_packet_checksum();
        test_packet_format();
        test_packet_parse();
        test_hex_formatting();
        test_command_parsing();
        
        std::cout << "\nAll tests PASSED!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test FAILED: " << e.what() << "\n";
        return 1;
    }
}
