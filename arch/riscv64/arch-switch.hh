/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_SWITCH_HH_
#define ARCH_SWITCH_HH_

#include <osv/barrier.hh>
#include <osv/kernel_config_preempt.h>
#include <osv/kernel_config_threads_default_kernel_stack_size.h>
#include <string.h>
#include "arch-setup.hh"

extern "C" {
void thread_main(void);
void thread_main_c(sched::thread* t);
}

namespace sched {

void thread::switch_to_first()
{
    // Set thread pointer (tp) register to point to TCB
    asm volatile ("mv tp, %0" :: "r"(_tcb) : "memory");

    /* check that the tls variable preempt_counter is correct */
    assert(sched::get_preempt_counter() == 1);

    s_current = this;
    current_cpu = _detached_state->_cpu;
    remote_thread_local_var(percpu_base) = _detached_state->_cpu->percpu_base;

    // Switch to the new thread's stack and call thread_main
    asm volatile(
        "mv sp, %0\n"
        "jr %1\n"
        :
        : "r"(this->_state.sp), "r"(this->_state.pc)
        : "memory");
}

void thread::init_stack()
{
    auto& stack = _attr._stack;
    if (!stack.size) {
        stack.size = CONF_threads_default_kernel_stack_size;
    }
    if (!stack.begin) {
        stack.begin = malloc(stack.size);
        stack.deleter = stack.default_deleter;
    } else {
        // Pre-fault the top of the stack
        (void) *((volatile char*)stack.begin + stack.size - 1);
    }
    void** stacktop = reinterpret_cast<void**>(stack.begin + stack.size);
    _state.thread = this;
    _state.sp = stacktop;
    _state.pc = reinterpret_cast<void*>(thread_main);
}

void thread::setup_tcb()
{
    // Setup thread control block for RISC-V
    // The TCB is pointed to by the tp register
    _tcb = _detached_state->_cpu->tcb;
}

void thread::switch_to()
{
    // TODO: Implement full context switch between running threads
    // This should:
    // 1. Save current thread's register state (sp, ra, s0-s11, tp)
    // 2. Save FPU state if needed
    // 3. Switch to new thread's stack
    // 4. Restore new thread's register state
    // 5. Update current thread pointer
    // 6. Return to new thread's saved program counter
    // For now, this is a stub that needs assembly implementation
    // similar to arch/aarch64/sched.S or arch/x64/arch-switch.hh
}

void thread::switch_to_from_privileged()
{
    // TODO: Implement context switch from privileged mode
    // This is used when switching from interrupt/exception handlers
    switch_to();
}

}

#endif /* ARCH_SWITCH_HH_ */
