/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_DTB_HH_
#define ARCH_DTB_HH_

// Device Tree Blob support for RISC-V
// Similar to AArch64, RISC-V platforms use device trees to describe hardware

namespace dtb {

void arch_setup_dtb(void* dtb_addr);
void arch_parse_dtb();

}

#endif /* ARCH_DTB_HH_ */
