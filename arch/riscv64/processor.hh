/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef RISCV64_PROCESSOR_HH
#define RISCV64_PROCESSOR_HH

#include <osv/types.h>
#include <osv/debug.h>
#include <cstddef>

namespace processor {

// RISC-V sstatus register bit for supervisor interrupt enable
constexpr unsigned long sstatus_sie = 1UL << 1;

inline void wfi()
{
    asm volatile ("wfi" ::: "memory");
}

inline void irq_enable()
{
    // Set SIE bit in sstatus to enable supervisor interrupts
    asm volatile ("csrsi sstatus, %0" :: "i"(sstatus_sie) : "memory");
}

inline void irq_disable()
{
    // Clear SIE bit in sstatus to disable supervisor interrupts
    asm volatile ("csrci sstatus, %0" :: "i"(sstatus_sie) : "memory");
}

__attribute__((no_instrument_function))
inline void irq_disable_notrace();

inline void irq_disable_notrace()
{
    asm volatile ("csrci sstatus, %0" :: "i"(sstatus_sie) : "memory");
}

inline void wait_for_interrupt() {
    // Must be called when interrupts are disabled
    // Enter 'Wait For Interrupt' mode
    // On exit from 'Wait For Interrupt' mode enable interrupts
    asm volatile( "wfi; csrsi sstatus, %0" :: "i"(sstatus_sie) : "memory");
}

inline void halt_no_interrupts() {
    irq_disable();
    while (1) {
        wfi();
    }
}

// RISC-V uses satp (Supervisor Address Translation and Protection) register
inline u64 read_satp() {
    u64 val;
    asm volatile("csrr %0, satp" : "=r"(val) :: "memory");
    return val;
}

inline void write_satp(u64 val) {
    asm volatile("csrw satp, %0" :: "r"(val) : "memory");
    // Flush TLB after changing page table
    asm volatile("sfence.vma" ::: "memory");
}

inline u64 read_hartid()
{
    // TODO: Proper implementation of hardware thread ID reading
    // In RISC-V, the hardware thread ID (hart ID) is typically:
    // 1. Passed by firmware in a0 register during boot
    // 2. Read from mhartid CSR in M-mode (not accessible in S-mode)
    // 3. Stored in a per-CPU data structure after boot
    // This placeholder returns 0, which works for single-CPU systems
    // but needs proper implementation for SMP support
    u64 hartid = 0;
    // Actual implementation should read from a per-CPU variable:
    // asm volatile("ld %0, hartid_offset(tp)" : "=r" (hartid));
    return hartid;
}

/* Read the cycle counter for high resolution timing */
inline u64 ticks()
{
    u64 cycles;
    asm volatile ("rdcycle %0" : "=r"(cycles));
    return cycles;
}

// RISC-V FPU state (F and D extensions)
struct fpu_state {
    u64 fregs[32];  // f0-f31 floating point registers (64-bit for D extension)
    u32 fcsr;       // Floating-point control and status register
    u32 padding;    // For alignment
};
static_assert(sizeof(struct fpu_state) == 264, "Wrong size for struct fpu_state");
static_assert(offsetof(fpu_state, fcsr) == 256, "Wrong offset for fcsr");

extern "C" {
void fpu_state_save(fpu_state *s);
void fpu_state_load(fpu_state *s);
}

}

#endif /* RISCV64_PROCESSOR_HH */
