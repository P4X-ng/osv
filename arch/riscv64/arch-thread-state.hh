/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_THREAD_STATE_HH_
#define ARCH_THREAD_STATE_HH_

struct thread_state {
    // RISC-V saved registers for context switching
    void* sp;      // Stack pointer
    void* ra;      // Return address
    void* s[12];   // Saved registers s0-s11
    void* tp;      // Thread pointer
};

#endif /* ARCH_THREAD_STATE_HH_ */
