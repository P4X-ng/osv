/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_INTERRUPT_HH_
#define ARCH_INTERRUPT_HH_

// RISC-V interrupt handling
// Interrupts in RISC-V are handled through the stvec register

namespace sched {
    class exception_frame;
}

void interrupt_init();

#endif /* ARCH_INTERRUPT_HH_ */
