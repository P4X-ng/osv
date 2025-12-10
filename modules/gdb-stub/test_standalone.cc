/*
 * Simple standalone test for GDB packet handling logic
 * This can be compiled and run independently from OSv
 */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cstdint>

// Simplified packet class for testing
class TestPacket {
public:
    TestPacket() = default;
    explicit TestPacket(const std::string& data) : _data(data) {}
    
    const std::string& data() const { return _data; }
    void set_data(const std::string& data) { _data = data; }
    
    uint8_t checksum() const {
        uint8_t sum = 0;
        for (char c : _data) {
            sum += static_cast<uint8_t>(c);
        }
        return sum;
    }
    
    std::string format() const {
        std::ostringstream oss;
        oss << '$' << _data << '#' 
            << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(checksum());
        return oss.str();
    }
    
    bool parse(const std::string& raw) {
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
    
private:
    std::string _data;
};

void test_packet_checksum() {
    std::cout << "Testing packet checksum... ";
    
    TestPacket packet("qSupported");
    uint8_t checksum = packet.checksum();
    
    // Manually calculated checksum for "qSupported"
    // Sum of ASCII values: q(113) + S(83) + u(117) + p(112) + p(112) + 
    // o(111) + r(114) + t(116) + e(101) + d(100) = 879 = 0x37 (mod 256)
    assert(checksum == 0x37);
    
    std::cout << "PASSED\n";
}

void test_packet_format() {
    std::cout << "Testing packet formatting... ";
    
    TestPacket packet("qSupported");
    std::string formatted = packet.format();
    
    assert(formatted == "$qSupported#37");
    
    std::cout << "PASSED\n";
}

void test_packet_parse() {
    std::cout << "Testing packet parsing... ";
    
    TestPacket packet;
    bool result = packet.parse("$qSupported#37");
    
    assert(result == true);
    assert(packet.data() == "qSupported");
    
    // Test invalid checksum
    TestPacket invalid_packet;
    bool invalid_result = invalid_packet.parse("$qSupported#00");
    assert(invalid_result == false);
    
    std::cout << "PASSED\n";
}

void test_various_packets() {
    std::cout << "Testing various packet types... ";
    
    // Test signal query
    TestPacket p1("?");
    assert(p1.format() == "$?#3f");
    
    // Test memory read command
    TestPacket p2("m1000,10");
    std::string formatted = p2.format();
    assert(formatted.substr(0, 1) == "$");
    assert(formatted.find("#") != std::string::npos);
    
    // Parse it back
    TestPacket p3;
    assert(p3.parse(formatted));
    assert(p3.data() == "m1000,10");
    
    std::cout << "PASSED\n";
}

void test_hex_conversion() {
    std::cout << "Testing hex conversion... ";
    
    // Test formatting a value as hex (little-endian)
    uint64_t value = 0x1234;
    std::ostringstream oss;
    
    // Output in little-endian format (byte by byte)
    for (size_t i = 0; i < 2; i++) {
        uint8_t byte = (value >> (i * 8)) & 0xFF;
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(byte);
    }
    
    // 0x1234 in little-endian: 34 12
    assert(oss.str() == "3412");
    
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "GDB Stub Standalone Tests\n";
    std::cout << "==========================\n\n";
    
    try {
        test_packet_checksum();
        test_packet_format();
        test_packet_parse();
        test_various_packets();
        test_hex_conversion();
        
        std::cout << "\n✓ All tests PASSED!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test FAILED: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Test FAILED with unknown exception\n";
        return 1;
    }
}
