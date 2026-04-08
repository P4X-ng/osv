/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_ELF_HH
#define ARCH_ELF_HH

// RISC-V ELF relocation types
enum {
    R_RISCV_NONE = 0,
    R_RISCV_32 = 1,
    R_RISCV_64 = 2,
    R_RISCV_RELATIVE = 3,
    R_RISCV_COPY = 4,
    R_RISCV_JUMP_SLOT = 5,
    R_RISCV_TLS_DTPMOD32 = 6,
    R_RISCV_TLS_DTPMOD64 = 7,
    R_RISCV_TLS_DTPREL32 = 8,
    R_RISCV_TLS_DTPREL64 = 9,
    R_RISCV_TLS_TPREL32 = 10,
    R_RISCV_TLS_TPREL64 = 11,
    R_RISCV_IRELATIVE = 58
};

/* for pltgot relocation */
#define ARCH_JUMP_SLOT R_RISCV_JUMP_SLOT
#define ARCH_IRELATIVE R_RISCV_IRELATIVE

#define ELF_KERNEL_MACHINE_TYPE 243  // EM_RISCV

static constexpr unsigned SAFETY_BUFFER = 256;
#include <osv/align.hh>

inline void run_entry_point(void* ep, int argc, char** argv, int argv_size)
{
    // RISC-V calling convention: arguments in a0-a7, stack 16-byte aligned
    int argc_plus_argv_stack_size = argv_size + 1;

    // Capture current stack pointer
    void *stack;
    asm volatile ("mv %0, sp" : "=r"(stack));

    // Leave safety buffer between current stack pointer and where we write
    stack -= (SAFETY_BUFFER + argc_plus_argv_stack_size * sizeof(char*));

    // Stack pointer should be 16-byte aligned
    stack = align_down(stack, 16);

    *reinterpret_cast<u64*>(stack) = argc;
    memcpy(stack + sizeof(char*), argv, argv_size * sizeof(char*));

    // Set stack pointer and jump to the ELF entry point
    asm volatile (
        "mv sp, %1\n\t"     // set stack pointer
        "li a0, 0\n\t"      // set atexit pointer to 0
        "jalr %0\n\t"       // jump to entry point
        :
        : "r"(ep), "r"(stack)
        : "a0", "sp");
}

#endif /* ARCH_ELF_HH */
