/*
 * Copyright (C) 2024 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "gdb-stub.hh"
#include <osv/debug.h>
#include <osv/sched.hh>
#include <thread>
#include <cstdlib>
#include <cstring>

using namespace gdb;

// Configuration constants
constexpr uint16_t MIN_TCP_PORT = 1;
constexpr uint16_t MAX_TCP_PORT = 65535;

static sched::thread* gdb_thread = nullptr;

// Parse command line arguments for GDB stub configuration
static bool parse_args(int argc, char** argv, std::string& transport_type, 
                       std::string& param) {
    // Default configuration
    transport_type = "tcp";
    param = "1234"; // Default GDB port
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--gdb-tcp") == 0 && i + 1 < argc) {
            transport_type = "tcp";
            param = argv[++i];
        } else if (strcmp(argv[i], "--gdb-serial") == 0 && i + 1 < argc) {
            transport_type = "serial";
            param = argv[++i];
        } else if (strcmp(argv[i], "--gdb-help") == 0) {
            debug("GDB Stub Module Usage:\n");
            debug("  --gdb-tcp <port>        Start GDB stub on TCP port (default: 1234)\n");
            debug("  --gdb-serial <device>   Start GDB stub on serial device\n");
            debug("  --gdb-help              Show this help\n");
            return false;
        }
    }
    
    return true;
}

// GDB stub thread function
static void gdb_stub_thread_fn() {
    debug("GDB stub: Thread started\n");
    
    auto* stub = GdbStubManager::instance().get_stub();
    if (stub) {
        stub->run();
    }
    
    debug("GDB stub: Thread exiting\n");
}

extern "C" int gdb_stub_main(int argc, char** argv) {
    debug("GDB Stub Module v1.0\n");
    debug("==================\n");
    
    std::string transport_type;
    std::string param;
    
    if (!parse_args(argc, argv, transport_type, param)) {
        return 1;
    }
    
    bool success = false;
    
    if (transport_type == "tcp") {
        try {
            int port_int = std::stoi(param);
            if (port_int < MIN_TCP_PORT || port_int > MAX_TCP_PORT) {
                debug("GDB stub: Invalid port number %d (must be %d-%d)\n", 
                      port_int, MIN_TCP_PORT, MAX_TCP_PORT);
                return 1;
            }
            uint16_t port = static_cast<uint16_t>(port_int);
            debug("GDB stub: Initializing TCP transport on port %d\n", port);
            success = GdbStubManager::instance().init_tcp(port);
        } catch (const std::exception& e) {
            debug("GDB stub: Invalid port parameter '%s': %s\n", param.c_str(), e.what());
            return 1;
        }
    } else if (transport_type == "serial") {
        debug("GDB stub: Initializing serial transport on %s\n", param.c_str());
        success = GdbStubManager::instance().init_serial(param);
    }
    
    if (!success) {
        debug("GDB stub: Failed to initialize transport\n");
        return 1;
    }
    
    if (!GdbStubManager::instance().start()) {
        debug("GDB stub: Failed to start\n");
        return 1;
    }
    
    // Start GDB stub in a separate thread
    gdb_thread = sched::thread::make([=] {
        gdb_stub_thread_fn();
    });
    gdb_thread->start();
    
    debug("GDB stub: Ready and waiting for debugger connection...\n");
    debug("GDB stub: Connect with: gdb -ex 'target remote :%s'\n", param.c_str());
    
    // Keep main thread alive
    gdb_thread->join();
    
    return 0;
}

// Module initialization for OSv
extern "C" void gdb_stub_init() {
    debug("GDB Stub Module: Initialization\n");
}

// Module cleanup
extern "C" void gdb_stub_fini() {
    debug("GDB Stub Module: Cleanup\n");
    GdbStubManager::instance().stop();
    
    if (gdb_thread) {
        gdb_thread->wake();
    }
}
