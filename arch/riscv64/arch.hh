/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_HH_
#define ARCH_HH_

#include "processor.hh"
#include <osv/kernel_config_lazy_stack.h>
#include <osv/kernel_config_lazy_stack_invariant.h>

// architecture independent interface for architecture dependent operations

namespace arch {

#define CACHELINE_ALIGNED __attribute__((aligned(64)))
#define INSTR_SIZE_MIN 4
#define ELF_IMAGE_START (OSV_KERNEL_VM_BASE + 0x10000)

#if CONF_lazy_stack
inline void ensure_next_stack_page() {
    u64 i, offset = -4096;
    asm volatile("ld %0, %1(sp)" : "=r"(i) : "i"(offset));
}

inline void ensure_next_two_stack_pages() {
    u64 i, offset = -4096;
    asm volatile("ld %0, %1(sp)" : "=r"(i) : "i"(offset));
    offset = -8192;
    asm volatile("ld %0, %1(sp)" : "=r"(i) : "i"(offset));
}
#endif

inline void irq_disable()
{
    processor::irq_disable();
}

__attribute__((no_instrument_function))
inline void irq_disable_notrace();

inline void irq_disable_notrace()
{
    processor::irq_disable_notrace();
}

inline void irq_enable()
{
    processor::irq_enable();
}

inline void wait_for_interrupt()
{
    processor::wait_for_interrupt();
}

inline void halt_no_interrupts()
{
    processor::halt_no_interrupts();
}

class irq_flag {
public:
    void save() {
        asm volatile("csrr %0, sstatus" : "=r"(sstatus) :: "memory");
    }

    void restore() {
        asm volatile("csrw sstatus, %0" :: "r"(sstatus) : "memory");
    }
    
    bool enabled() const {
        return (sstatus & processor::sstatus_sie);
    }

private:
    unsigned long sstatus;
};

class irq_flag_notrace {
public:
    __attribute__((no_instrument_function)) void save();
    __attribute__((no_instrument_function)) void restore();
    __attribute__((no_instrument_function)) bool enabled() const;
private:
    unsigned long sstatus;
};

inline void irq_flag_notrace::save() {
    asm volatile("csrr %0, sstatus" : "=r"(sstatus) :: "memory");
}

inline void irq_flag_notrace::restore() {
    asm volatile("csrw sstatus, %0" :: "r"(sstatus) : "memory");
}

inline bool irq_flag_notrace::enabled() const {
    return (sstatus & processor::sstatus_sie);
}

inline bool irq_enabled()
{
    irq_flag f;
    f.save();
    return f.enabled();
}

extern bool tls_available() __attribute__((no_instrument_function));

inline bool tls_available()
{
    // RISC-V uses tp (thread pointer) register for TLS
    // Check if tp register is set
    unsigned long tp;
    asm("mv %0, tp" : "=r"(tp));
    return tp != 0;
}

} // namespace arch

#endif /* ARCH_HH_ */
