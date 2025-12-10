/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "gdb_stub.hh"
#include <osv/debug.h>

// Entry point for the GDB stub shared library
extern "C" int main(int argc, char** argv)
{
    debug("gdb-stub: Module loaded\n");
    
    // Start GDB stub main function directly for now
    // In a real implementation, this might be in a separate thread
    gdb_stub::gdb_stub_main();
    
    debug("gdb-stub: GDB stub module exiting\n");
    return 0;
}