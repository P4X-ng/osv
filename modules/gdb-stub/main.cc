/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "gdb_stub.hh"
#include <osv/debug.hh>
#include <thread>

// Entry point for the GDB stub shared library
extern "C" int main(int argc, char** argv)
{
    debug("gdb-stub: Module loaded\n");
    
    // Start GDB stub in a separate thread so it doesn't block
    std::thread gdb_thread(gdb_stub::gdb_stub_main);
    gdb_thread.detach();
    
    debug("gdb-stub: GDB stub thread started\n");
    return 0;
}