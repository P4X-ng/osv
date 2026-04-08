/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "arch-dtb.hh"

// Stub implementation for RISC-V device tree parsing

namespace dtb {

void arch_setup_dtb(void* dtb_addr)
{
    // TODO: Parse device tree to discover hardware
    // This will include memory regions, CPU info, devices, etc.
}

void arch_parse_dtb()
{
    // TODO: Parse device tree structure
}

}
